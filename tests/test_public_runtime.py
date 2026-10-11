"""Behavior checks that do not need audio hardware, Ollama or an ESP32."""

from collections import deque
import json
from pathlib import Path
import sys
import threading
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import glados


class FakeReply:
    def __enter__(self):
        return self

    def __exit__(self, *args):
        pass

    def raise_for_status(self):
        pass

    def iter_lines(self, **kwargs):
        events = [
            {"message": {"thinking": "This must never be spoken."}},
            {"message": {"content": "The value is 3."}},
            {"message": {"content": "14. Next sentence. "}},
            {"done": True, "done_reason": "stop"},
        ]
        for event in events:
            yield json.dumps(event).encode()


class FakeSession:
    def post(self, url, **kwargs):
        self.request = kwargs["json"]
        return FakeReply()


class FakeSpeech:
    def __init__(self):
        self.spoken = []
        self.motors = type("Motors", (), {"speech_session": threading.Event()})()

    def say(self, text, cancel=None):
        self.spoken.append(text)


class PublicRuntimeTests(unittest.TestCase):
    def test_decimal_number_is_kept_in_one_sentence(self):
        parts, pending = glados.sentence_parts(
            "Measured 3.14. Another sentence. unfinished"
        )
        self.assertEqual(parts, ["Measured 3.14.", "Another sentence."])
        self.assertEqual(pending, "unfinished")

    def test_context_keeps_whole_recent_turns_and_can_be_disabled(self):
        history = deque(
            [
                {"role": "user", "content": "a" * 800},
                {"role": "assistant", "content": "b" * 800},
                {"role": "user", "content": "Recent question"},
                {"role": "assistant", "content": "Recent answer"},
            ]
        )
        messages = glados.build_messages("Next", history)
        self.assertEqual(
            [m["content"] for m in messages[1:-1]], ["Recent question", "Recent answer"]
        )
        with patch.object(glados, "HISTORY_TURNS", 0):
            self.assertEqual(len(glados.build_messages("Next", history)), 2)

    def test_french_launcher_selects_french_recognition_and_voice(self):
        with patch.multiple(
            glados,
            STT_LANGUAGE="en",
            SYSTEM_PROMPT="English",
            IDLE_PROMPT="English",
            PIPER_MODEL=Path("unused"),
            PIPER_CONFIG=Path("unused"),
        ):
            import importlib
            import glados_fr

            importlib.reload(glados_fr)
            self.assertEqual(glados.STT_LANGUAGE, "fr")
            self.assertEqual(glados.PIPER_MODEL.name, "fr_FR-glados-medium.onnx")
            self.assertIn("français", glados.SYSTEM_PROMPT)

    def test_transcription_has_selected_language_without_personal_prompt(self):
        class Model:
            def transcribe(self, audio, **kwargs):
                self.options = kwargs
                return (
                    iter([type("Segment", (), {"text": "An ordinary question"})()]),
                    None,
                )

        model = Model()
        with patch.object(glados, "STT_LANGUAGE", "fr"):
            result = glados.transcribe_audio(model, [])
        self.assertEqual(result, "An ordinary question")
        self.assertEqual(model.options["language"], "fr")
        self.assertEqual(model.options["initial_prompt"], "GLaDOS, Aperture Science.")

    def test_streamed_speech_excludes_thinking_and_releases_motor_session(self):
        session, speech = FakeSession(), FakeSpeech()
        history = deque(maxlen=6)
        result = glados.generate_and_speak(session, "Question", history, speech)
        self.assertEqual(result, "The value is 3.14. Next sentence.")
        self.assertEqual(speech.spoken, ["The value is 3.14.", "Next sentence."])
        self.assertFalse(speech.motors.speech_session.is_set())
        self.assertEqual(len(history), 2)
        self.assertFalse(session.request["think"])

    def test_idle_thought_does_not_become_a_user_conversation_turn(self):
        history = deque(maxlen=6)
        glados.generate_and_speak(FakeSession(), "", history, FakeSpeech(), idle=True)
        self.assertEqual(len(history), 0)


if __name__ == "__main__":
    unittest.main()
