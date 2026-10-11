# Asset and dependency notices

## Project files

The Crafitys project sources and documentation are supplied here for the animatronic build. No blanket reuse license has been selected for these project files in this package. Public availability alone should not be read as a grant of every reuse right. The STL files are distributed through [Crafitys on Cults3D](https://cults3d.com/fr/utilisateurs/Crafitys/fichiers-3d), under the terms of the relevant listing. Crafitys supplied the assembly manual and original shopping-list PDF for inclusion here. The README logo and robot render are extracted from the supplied manual cover; they are Crafitys project artwork. This repository does not grant a separate blanket reuse license for those images or documents.

## Included voices

| Asset | Role |
| --- | --- |
| `en_US-glados-medium.onnx` and its JSON | English GLaDOS-style speech synthesis |
| `fr_FR-glados-medium.onnx` and its JSON | French GLaDOS-style speech synthesis |

These are the voice files supplied with the project. Their supplied JSON files do not include model cards, training-source attribution or a redistribution license. The author should add the original source and applicable terms when available. No license for the voice weights is inferred from the Piper engine's license.

## External software and downloaded models

The project installs or uses these upstream components; it does not vendor their source or redistribute Ollama/Whisper model weights in this repository:

- [Ollama](https://github.com/ollama/ollama)
- [Huihui Qwen3.5 Abliterated 4B](https://ollama.com/huihui_ai/qwen3.5-abliterated:4B)
- [faster-whisper](https://github.com/SYSTRAN/faster-whisper)
- [Piper](https://github.com/OHF-Voice/piper1-gpl)
- [Arduino-ESP32](https://github.com/espressif/arduino-esp32)
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo)
- [Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library)
- [Adafruit GC9A01A](https://github.com/adafruit/Adafruit_GC9A01A)
- [Adafruit BusIO](https://github.com/adafruit/Adafruit_BusIO)

Other Python dependencies are listed in `requirements.txt`. Consult the actual installed releases and model pages for their terms. GLaDOS, Portal and related character names belong to their respective rights holders; this is a fan-made build.
