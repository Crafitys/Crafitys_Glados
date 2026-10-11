#!/usr/bin/env python3
"""Synthesize a short test sentence in memory, optionally playing it through the PC speakers."""

import argparse
import io
from pathlib import Path
import wave

from piper.voice import PiperVoice
import soundfile as sf


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--language", choices=("en", "fr"), default="en")
    parser.add_argument(
        "--no-play", action="store_true", help="Check synthesis without an audio device"
    )
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    prefix = "en_US" if args.language == "en" else "fr_FR"
    model = root / "voices" / f"{prefix}-glados-medium.onnx"
    voice = PiperVoice.load(
        str(model), config_path=str(model) + ".json", use_cuda=False
    )
    sentence = (
        "Testing complete. Your survival remains statistically interesting."
        if args.language == "en"
        else "Test terminé. Votre survie reste statistiquement intéressante."
    )
    buffer = io.BytesIO()
    with wave.open(buffer, "wb") as wav:
        voice.synthesize_wav(sentence, wav_file=wav)
    buffer.seek(0)
    data, rate = sf.read(buffer, dtype="float32")
    if len(data) == 0:
        raise RuntimeError("Piper returned empty audio.")
    print(f"Voice OK: {prefix}, {rate} Hz, {len(data) / rate:.2f} seconds.")
    if not args.no_play:
        import sounddevice as sd

        sd.play(data, rate)
        sd.wait()


if __name__ == "__main__":
    main()
