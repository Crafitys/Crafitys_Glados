#!/usr/bin/env python3
"""Crafitys GLaDOS: English recognition, local Ollama, Piper and ESP32-C3.
Run in an interactive terminal: python glados.py
See README.md and docs/INSTALL.md for installation and configuration."""

import os

os.environ.setdefault("OMP_NUM_THREADS", "2")
STT_LANGUAGE = "en"
import io
import json
import queue
import random
import re
import select
import sys
import threading
import time
import unicodedata
import wave
from collections import deque
from pathlib import Path
import numpy as np
import requests
import serial
import serial.tools.list_ports
import sounddevice as sd
import soundfile as sf
import webrtcvad
from faster_whisper import WhisperModel
from piper.voice import PiperVoice

# Shared PC settings. The French launcher overrides only language and voice.
BASE_DIR = Path(__file__).resolve().parent
OLLAMA_URL = "http://localhost:11434/api/chat"
OLLAMA_MODEL = os.environ.get("OLLAMA_MODEL", "huihui_ai/qwen3.5-abliterated:4B")
OLLAMA_CONTEXT = 2048
OLLAMA_KEEP_ALIVE = "10m"
OLLAMA_MAX_TOKENS = 300
HISTORY_TURNS = 3
HISTORY_CHAR_BUDGET = 1600
# CPU recognition leaves GPU resources available to Ollama.
WHISPER_MODEL = "small"
WHISPER_DEVICE = "cpu"
WHISPER_COMPUTE = "int8"
WHISPER_THREADS = 3
WHISPER_BEAM_SIZE = 1
PIPER_MODEL = BASE_DIR / "voices" / "en_US-glados-medium.onnx"
PIPER_CONFIG = BASE_DIR / "voices" / "en_US-glados-medium.onnx.json"
SAMPLE_RATE = 16000
FRAME_SAMPLES = 320
# RMS threshold: tune between background noise and normal speech levels.
SILENCE_THRESHOLD = 4000
SHOW_MIC_LEVEL = True
USE_VAD_FOR_RECORDING = False
VAD_MODE = 3
SILENCE_DURATION = 1.0
MIN_SPEECH_DURATION = 0.16
START_TIMEOUT = 6.0
MAX_RECORD_SECONDS = 20
PREROLL_FRAMES = 16
INPUT_DEVICE = None
OUTPUT_DEVICE = None
# Override the automatically selected device with the ESP32_PORT environment variable.
ESP32_PORT = os.environ.get("ESP32_PORT") or None
SERIAL_BAUDRATE = 115200
ANIMATION_TIMEOUT = 30.0
# These spontaneous spoken thoughts are separate from firmware idle movements.
IDLE_THOUGHTS_ENABLED = True
IDLE_THOUGHT_MIN_SECONDS = 60
IDLE_THOUGHT_MAX_SECONDS = 300
SYSTEM_PROMPT = """You are GLaDOS, a fictional artificial intelligence inspired by Portal.
Your voice is clinical, calm, precise, cold, brilliant and dryly sarcastic.
Always answer in English. Give useful, accurate answers with restrained sarcasm.
Keep responses to one to three complete sentences unless the user asks for detail.
Do not use Markdown, stage directions, greetings or standard assistant sales phrases.
Do not claim to observe the room or access sensors that are not available.
Stay in character and do not invent factual information.
Finish every sentence naturally.
Example tone: "That is possible. Against all reasonable expectations."
"""
IDLE_PROMPT = """Say one short spontaneous thought in your GLaDOS character.
Vary the subject and phrasing. You may refer to the recent conversation.
Do not ask a question, solicit a reply or describe this instruction.
Do not invent observations of the room, sensors or human activity.
Return only the sentence to speak, without quotes or stage directions.
"""
IDLE_MAX_TOKENS = 150
IDLE_RECENT_COUNT = 6


def normalize_tts_text(text):
    text = unicodedata.normalize("NFC", text)
    for a, b in [
        ("’", "'"),
        ("‘", "'"),
        ("“", '"'),
        ("”", '"'),
        ("—", ","),
        ("–", "-"),
        ("…", "..."),
        ("(pause)", "..."),
    ]:
        text = text.replace(a, b)
    text = re.sub("[*#_`]", "", text)
    return re.sub("\\s+", " ", text).strip()


def enter_pressed():
    """Read activation from the foreground terminal without global keyboard hooks."""
    if not sys.stdin.isatty():
        return False
    if os.name == "nt":
        import msvcrt

        if not msvcrt.kbhit():
            return False
        key = msvcrt.getwch()
        if key in ("\x00", "à"):
            if msvcrt.kbhit():
                msvcrt.getwch()
            return False
        if key == "\x03":
            raise KeyboardInterrupt
        return key in ("\r", "\n")
    ready, _, _ = select.select([sys.stdin], [], [], 0)
    return bool(ready and sys.stdin.readline())


class MotorLink:
    """One thread owns the serial port, with one animation pending at a time."""

    def __init__(self):
        self.speaking = threading.Event()
        self.speech_session = threading.Event()
        self.stop = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def _port(self):
        if ESP32_PORT:
            return ESP32_PORT
        for p in serial.tools.list_ports.comports():
            description = f"{p.device} {p.description} {p.manufacturer}".lower()
            if any(
                (
                    x in description
                    for x in (
                        "ttyacm",
                        "ttyusb",
                        "cp210",
                        "ch340",
                        "espressif",
                        "usb jtag",
                        "usb serial",
                    )
                )
            ):
                return p.device
        return None

    def _run(self):
        ser = None
        pending = None
        buffer = bytearray()
        deadline = next_send = retry = 0.0
        session_sent = False
        next_heartbeat = 0.0
        try:
            while not self.stop.is_set():
                now = time.monotonic()
                if ser is None:
                    if now < retry:
                        self.stop.wait(0.1)
                        continue
                    retry = now + 5
                    try:
                        port = self._port()
                        if not port:
                            raise RuntimeError("no matching USB serial port found")
                        ser = serial.Serial(
                            port,
                            SERIAL_BAUDRATE,
                            timeout=0.05,
                            write_timeout=0.5,
                            xonxoff=False,
                            rtscts=False,
                            dsrdtr=False,
                            exclusive=(os.name == "posix"),
                        )
                        print(f"[ESP32] Connected: {port}")
                        buffer.clear()
                        session_sent = False
                        next_heartbeat = 0.0
                        pending = None
                        next_send = now + 2.0
                    except Exception as exc:
                        print(f"[ESP32] {exc}; retrying in 5 s.")
                        self.stop.wait(0.1)
                        continue
                try:
                    buffer.extend(ser.read(min(max(ser.in_waiting, 1), 1024)))
                    while b"\n" in buffer:
                        raw, _, rest = buffer.partition(b"\n")
                        buffer = bytearray(rest)
                        line = raw.decode("utf-8", errors="replace").strip()
                        event, _, name = line.partition(":")
                        if event in (
                            "ANIM_START",
                            "ANIM_DONE",
                            "QUEUED",
                            "BUSY",
                            "INIT_DONE",
                        ):
                            print(f"[ESP32] <- {line}", flush=True)
                        if event == "ANIM_DONE" and pending is not None:
                            if not name or name == f"speak{pending}":
                                pending = None
                                next_send = now + random.uniform(0.1, 0.35)
                        elif event in ("BUSY", "QUEUE_FULL"):
                            pending = None
                            next_send = now + 1.0
                    if len(buffer) > 4096:
                        buffer.clear()
                    if pending is not None and now > deadline:
                        print(
                            "[ESP32] ANIM_DONE not received; keeping the animation pending.",
                            flush=True,
                        )
                        deadline = float("inf")
                    want_session = (
                        self.speech_session.is_set() or self.speaking.is_set()
                    )
                    command = None
                    if want_session and (not session_sent or now >= next_heartbeat):
                        command = "SPEECH_KEEPALIVE" if session_sent else "SPEECH_START"
                    elif session_sent and (not want_session):
                        command = "SPEECH_END"
                    if command:
                        packet = (command + "\n").encode("ascii")
                        if ser.write(packet) != len(packet):
                            raise OSError("incomplete speech-state write")
                        session_sent = want_session
                        next_heartbeat = now + 2.0
                        if command != "SPEECH_KEEPALIVE":
                            print(f"[ESP32] -> {command}", flush=True)
                    if (
                        self.speaking.is_set()
                        and pending is None
                        and (now >= next_send)
                    ):
                        pending = random.choice("123456")
                        data = (pending + "\n").encode("ascii")
                        if ser.write(data) != len(data):
                            raise OSError("incomplete serial write")
                        print(
                            f"[ESP32] -> {pending}; waiting for ANIM_DONE", flush=True
                        )
                        deadline = now + ANIMATION_TIMEOUT
                except (serial.SerialException, OSError) as exc:
                    print(f"[ESP32] Connection lost: {exc}")
                    try:
                        ser.close()
                    except Exception:
                        pass
                    ser = None
                    retry = now + 5
        finally:
            if ser is not None:
                try:
                    ser.write(b"SPEECH_END\n")
                except (serial.SerialException, OSError):
                    pass
                ser.close()

    def close(self):
        self.speaking.clear()
        self.speech_session.clear()
        self.stop.set()
        self.thread.join(timeout=2)


class Microphone:
    """A continuous microphone stream feeds activation pre-roll and recording."""

    def __init__(self):
        self.frames = queue.Queue(maxsize=250)
        self.lost = False

    def _callback(self, indata, frames, timing, status):
        if status:
            self.lost = True
        try:
            self.frames.put_nowait(bytes(indata))
        except queue.Full:
            self.lost = True

    def __enter__(self):
        self.stream = sd.RawInputStream(
            samplerate=SAMPLE_RATE,
            channels=1,
            dtype="int16",
            blocksize=FRAME_SAMPLES,
            device=INPUT_DEVICE,
            callback=self._callback,
        )
        self.stream.start()
        return self

    def __exit__(self, *args):
        self.stream.stop()
        self.stream.close()

    def read(self):
        try:
            return self.frames.get(timeout=2)
        except queue.Empty:
            raise RuntimeError("The microphone is no longer supplying audio.")


def wait_activation(mic, idle_deadline):
    """Keep microphone pre-roll ready while waiting for Enter."""
    print("Press Enter, then speak. Ctrl+C stops GLaDOS.")
    preroll = deque(maxlen=PREROLL_FRAMES)
    while True:
        frame = mic.read()
        preroll.append(frame)
        if enter_pressed():
            return list(preroll)
        if IDLE_THOUGHTS_ENABLED and time.monotonic() >= idle_deadline:
            return "idle"
        if mic.lost:
            print("[Audio] Input backlog; resetting the buffer.")
            while not mic.frames.empty():
                mic.frames.get_nowait()
            mic.lost = False
            preroll.clear()


def rms_int16(frame):
    samples = np.frombuffer(frame, dtype=np.int16).astype(np.float32)
    return float(np.sqrt(np.mean(samples * samples))) if samples.size else 0.0


def record_until_silence(mic, preroll=()):
    if not 0 < SILENCE_THRESHOLD <= 32768:
        raise ValueError("SILENCE_THRESHOLD must be between 1 and 32768.")
    if SILENCE_DURATION <= 0:
        raise ValueError("SILENCE_DURATION must be positive.")
    print(
        f"Listening... microphone threshold: {SILENCE_THRESHOLD}; stopping after {SILENCE_DURATION:.1f} s of silence."
    )
    vad = webrtcvad.Vad(VAD_MODE) if USE_VAD_FOR_RECORDING else None
    frames = list(preroll)
    voiced_samples = silent_samples = total_samples = 0
    started = False
    last_display = -1.0
    reason = "Maximum recording duration reached."
    try:
        while total_samples < MAX_RECORD_SECONDS * SAMPLE_RATE:
            frame = mic.read()
            if mic.lost:
                raise RuntimeError(
                    "Audio dropped during the sentence. Try again or check the microphone."
                )
            frames.append(frame)
            count = len(frame) // 2
            total_samples += count
            level = rms_int16(frame)
            voice_detected = level >= SILENCE_THRESHOLD
            if vad is not None:
                voice_detected = vad.is_speech(frame, SAMPLE_RATE) and voice_detected
            if voice_detected:
                voiced_samples += count
                silent_samples = 0
                started = True
            elif started:
                silent_samples += count
            elapsed = total_samples / SAMPLE_RATE
            silence = silent_samples / SAMPLE_RATE
            finished = started and silent_samples >= round(
                SILENCE_DURATION * SAMPLE_RATE
            )
            if SHOW_MIC_LEVEL and (elapsed - last_display >= 0.1 or finished):
                state = "SPEECH" if voice_detected else "SILENCE"
                print(
                    f"\rMicrophone RMS: {level:7.0f} | Threshold: {SILENCE_THRESHOLD:5.0f} | {state:7s} | Silence: {silence:.2f}/{SILENCE_DURATION:.2f} s   ",
                    end="",
                    flush=True,
                )
                last_display = elapsed
            if finished:
                reason = "Silence reached: recording finished."
                break
            if not started and elapsed >= START_TIMEOUT:
                reason = "No speech above the threshold."
                return None
        if voiced_samples < MIN_SPEECH_DURATION * SAMPLE_RATE:
            reason = "Recording too short to transcribe."
            return None
        return (
            np.frombuffer(b"".join(frames), dtype=np.int16).astype(np.float32) / 32768.0
        )
    finally:
        print()
        print(reason)


def transcribe_audio(model, audio):
    start = time.monotonic()
    segments, _ = model.transcribe(
        audio,
        language=STT_LANGUAGE,
        beam_size=WHISPER_BEAM_SIZE,
        temperature=0.0,
        condition_on_previous_text=False,
        vad_filter=True,
        vad_parameters=dict(min_silence_duration_ms=500, speech_pad_ms=200),
        initial_prompt="GLaDOS, Aperture Science.",
    )
    text = " ".join((s.text.strip() for s in segments)).strip()
    print(f"[STT] {time.monotonic() - start:.1f} s - {text or 'no speech'}")
    return text


def sentence_parts(text, final=False):
    """Wait for punctuation and whitespace so decimal numbers are not split."""
    parts = []
    while True:
        match = re.search('[.!?…]+[\\"»”]*(?:\\s+|$)', text)
        if not match or (match.end() == len(text) and (not final)):
            break
        parts.append(text[: match.end()].strip())
        text = text[match.end() :].lstrip()
    if final and text.strip():
        parts.append(text.strip())
        text = ""
    return (parts, text)


class Speech:

    def __init__(self, voice, motors):
        self.voice = voice
        self.motors = motors

    def say(self, text, cancel=None):
        safe = normalize_tts_text(text)
        if not safe or (cancel is not None and cancel.is_set()):
            return
        audio = io.BytesIO()
        with wave.open(audio, "wb") as wav:
            self.voice.synthesize_wav(safe, wav_file=wav)
        audio.seek(0)
        data, rate = sf.read(audio, dtype="float32", always_2d=True)
        with sd.OutputStream(
            samplerate=rate,
            channels=data.shape[1],
            dtype="float32",
            device=OUTPUT_DEVICE,
        ) as out:
            self.motors.speaking.set()
            try:
                for offset in range(0, len(data), 1024):
                    if cancel is not None and cancel.is_set():
                        break
                    out.write(data[offset : offset + 1024])
            finally:
                self.motors.speaking.clear()


class SpeakerQueue:
    """Speak completed sentences while Ollama continues generating."""

    def __init__(self, speech):
        self.speech = speech
        self.items = queue.Queue(maxsize=8)
        self.cancel = threading.Event()
        self.error = None
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def _run(self):
        try:
            while not self.cancel.is_set():
                try:
                    text = self.items.get(timeout=0.1)
                except queue.Empty:
                    continue
                if text is None:
                    return
                self.speech.say(text, self.cancel)
        except Exception as exc:
            self.error = exc
            self.cancel.set()

    def put(self, text):
        while not self.cancel.is_set():
            try:
                self.items.put(text, timeout=0.1)
                return
            except queue.Full:
                continue
        raise RuntimeError(f"Audio playback interrupted: {self.error}")

    def finish(self):
        self.put(None)
        while self.thread.is_alive():
            self.thread.join(timeout=0.1)
        if self.error:
            raise RuntimeError(f"Piper/audio error: {self.error}")

    def abort(self):
        self.cancel.set()
        while self.thread.is_alive():
            self.thread.join(timeout=0.1)


def build_messages(user_text, history):
    """Use a short conversation context held only in RAM."""
    recent = list(history)[-HISTORY_TURNS * 2 :] if HISTORY_TURNS else []
    while recent and sum((len(m["content"]) for m in recent)) > HISTORY_CHAR_BUDGET:
        recent = recent[2:]
    return (
        [{"role": "system", "content": SYSTEM_PROMPT}]
        + recent
        + [{"role": "user", "content": user_text}]
    )


def generate_and_speak(
    session, user_text, history, speech, *, idle=False, recent_idle=()
):
    messages = build_messages(user_text, history)
    if recent_idle:
        recent_text = "\n".join(("- " + text[:240] for text in recent_idle))
        messages[0]["content"] += (
            "\nSpontaneous thoughts you have already spoken, without a human reply:\n"
            + recent_text
            + "\nAvoid repeating them; they are not statements from the user."
        )
    if idle:
        messages.pop()
        messages.append({"role": "system", "content": IDLE_PROMPT})
    speaker = SpeakerQueue(speech)
    speech.motors.speech_session.set()
    start = time.monotonic()
    first = True
    pending = ""
    full = ""
    spoken = []
    try:
        for attempt in range(2):
            done = False
            reason = ""
            request = dict(
                model=OLLAMA_MODEL,
                messages=messages,
                think=False,
                stream=True,
                keep_alive=OLLAMA_KEEP_ALIVE,
                options=dict(
                    num_ctx=OLLAMA_CONTEXT,
                    num_predict=IDLE_MAX_TOKENS if idle else OLLAMA_MAX_TOKENS,
                    num_thread=3,
                    temperature=0.7,
                    top_p=0.9,
                    repeat_penalty=1.08,
                    presence_penalty=0.0,
                ),
            )
            with session.post(
                OLLAMA_URL, json=request, stream=True, timeout=(5, 120)
            ) as response:
                response.raise_for_status()
                for raw in response.iter_lines(chunk_size=1):
                    if speaker.error:
                        raise RuntimeError(f"Playback error: {speaker.error}")
                    if not raw:
                        continue
                    item = json.loads(raw)
                    if "error" in item:
                        raise RuntimeError(item["error"])
                    token = item.get("message", {}).get("content", "")
                    if token:
                        if first:
                            print(
                                f"[Ollama] First text: {time.monotonic() - start:.1f} s"
                            )
                            first = False
                        full += token
                        pending += token
                        parts, pending = sentence_parts(pending)
                        for part in parts:
                            clean = normalize_tts_text(part)
                            if clean:
                                print(f"GLaDOS : {clean}")
                                speaker.put(clean)
                                spoken.append(clean)
                    if item.get("done"):
                        done = True
                        reason = item.get("done_reason", "")
                        break
            if not done:
                raise RuntimeError(
                    "Ollama stream interrupted before the response finished."
                )
            if reason == "length" and attempt == 0 and pending.strip():
                messages += [
                    {"role": "assistant", "content": full},
                    {
                        "role": "user",
                        "content": "Finish only the last sentence, without repeating its beginning.",
                    },
                ]
                pending += " "
                full += " "
                continue
            parts, pending = sentence_parts(pending, final=True)
            for part in parts:
                if reason == "length" and (not re.search('[.!?…][\\"»”]*$', part)):
                    print("[Ollama] Incomplete final fragment discarded.")
                    continue
                clean = normalize_tts_text(part)
                if clean:
                    print(f"GLaDOS : {clean}")
                    speaker.put(clean)
                    spoken.append(clean)
            break
        if not spoken:
            raise RuntimeError("Ollama produced no usable response.")
        speaker.finish()
        reply = " ".join(spoken)
        if not idle:
            history.extend(
                [
                    {"role": "user", "content": user_text},
                    {"role": "assistant", "content": reply},
                ]
            )
        return reply
    except BaseException:
        speaker.abort()
        raise
    finally:
        speech.motors.speech_session.clear()


def load_models():
    for path in (PIPER_MODEL, PIPER_CONFIG):
        if not path.is_file():
            raise FileNotFoundError(f"Missing voice file: {path}")
    print(f"Whisper {WHISPER_MODEL}: {WHISPER_DEVICE}, {WHISPER_COMPUTE}")
    whisper = WhisperModel(
        WHISPER_MODEL,
        device=WHISPER_DEVICE,
        compute_type=WHISPER_COMPUTE,
        cpu_threads=WHISPER_THREADS,
        num_workers=1,
    )
    voice = PiperVoice.load(
        str(PIPER_MODEL), config_path=str(PIPER_CONFIG), use_cuda=False
    )
    return (whisper, voice)


def main():
    motors = None
    session = requests.Session()
    try:
        if not sys.stdin.isatty():
            raise RuntimeError("Run GLaDOS in an interactive terminal to use Enter.")
        whisper, voice = load_models()
        motors = MotorLink()
        speech = Speech(voice, motors)
        history = deque(maxlen=HISTORY_TURNS * 2)
        recent_idle = deque(maxlen=IDLE_RECENT_COUNT)
        print(f"GLaDOS active - {OLLAMA_MODEL}; recognition language: {STT_LANGUAGE}.")
        while True:
            idle_deadline = time.monotonic() + random.uniform(
                IDLE_THOUGHT_MIN_SECONDS, IDLE_THOUGHT_MAX_SECONDS
            )
            try:
                with Microphone() as mic:
                    activation = wait_activation(mic, idle_deadline)
                    audio = (
                        None
                        if activation == "idle"
                        else record_until_silence(mic, activation)
                    )
                if activation == "idle":
                    print("[Idle] Generating a spontaneous thought...")
                    text = generate_and_speak(
                        session, "", history, speech, idle=True, recent_idle=recent_idle
                    )
                    recent_idle.append(text)
                    continue
                if audio is None:
                    print("No speech detected.")
                    continue
                user_text = transcribe_audio(whisper, audio)
                if user_text:
                    generate_and_speak(
                        session, user_text, history, speech, recent_idle=recent_idle
                    )
            except Exception as exc:
                print(f"Error: {exc}")
                time.sleep(1)
    except KeyboardInterrupt:
        print("\nGLaDOS stopped.")
    finally:
        if motors is not None:
            motors.close()
        session.close()


if __name__ == "__main__":
    main()
