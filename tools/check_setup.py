#!/usr/bin/env python3
"""Check Python, voice files, Ollama, audio devices and serial ports without moving motors."""

import importlib
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    failures = 0
    print(f"Python: {sys.version.split()[0]}")
    print(f"Virtual environment active: {sys.prefix != sys.base_prefix}")
    for name in (
        "numpy",
        "requests",
        "serial",
        "sounddevice",
        "soundfile",
        "webrtcvad",
        "faster_whisper",
        "piper.voice",
    ):
        try:
            importlib.import_module(name)
            print(f"OK import: {name}")
        except Exception as exc:
            failures += 1
            print(f"FAIL import: {name}: {exc}")
    for language in ("en_US", "fr_FR"):
        model = ROOT / "voices" / f"{language}-glados-medium.onnx"
        config = Path(str(model) + ".json")
        try:
            if not model.is_file() or model.stat().st_size < 1_000_000:
                raise RuntimeError(f"Missing or incomplete ONNX model: {model.name}")
            data = json.loads(config.read_text())
            print(f'OK voice: {model.name}, {data["audio"]["sample_rate"]} Hz')
        except Exception as exc:
            failures += 1
            print(f"FAIL voice: {exc}")
    try:
        import sounddevice as sd

        print("\nAudio devices:")
        print(sd.query_devices())
    except Exception as exc:
        failures += 1
        print(f"FAIL audio devices: {exc}")
    try:
        from serial.tools import list_ports

        ports = list(list_ports.comports())
        print("\nUSB serial ports:")
        for port in ports:
            print(f"{port.device}: {port.description}")
        if not ports:
            print("No serial port detected. Connect the ESP32-C3 to use the motors.")
    except Exception as exc:
        failures += 1
        print(f"FAIL serial ports: {exc}")
    try:
        import os
        import requests

        response = requests.get("http://localhost:11434/api/tags", timeout=5)
        response.raise_for_status()
        models = [model["name"] for model in response.json().get("models", [])]
        wanted = os.environ.get("OLLAMA_MODEL", "huihui_ai/qwen3.5-abliterated:4B")
        print(f"\nOllama models: {models}")
        if wanted not in models:
            failures += 1
            print(f"FAIL model missing: run ollama pull {wanted}")
        else:
            print(f"OK Ollama model: {wanted}")
    except Exception as exc:
        failures += 1
        print(f"FAIL Ollama: {exc}")
    print(f"\nChecks needing attention: {failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
