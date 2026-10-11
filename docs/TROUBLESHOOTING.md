# Troubleshooting

Begin with `python tools/check_setup.py` from the project folder, with the venv active. Work on one subsystem at a time: voices, audio, Ollama, then USB/motors.

## Python or packages

| Symptom | Check |
| --- | --- |
| `ModuleNotFoundError` | Activate `.venv`, then run `python -m pip install -r requirements.txt` |
| `No module named venv` | On Ubuntu/Debian, install `python3-venv` |
| “externally managed environment” | You are likely installing outside the venv; activate it first |
| Native package compilation fails | Use a supported 64-bit Python 3.10–3.12 with compatible wheels |
| The script stops with “interactive terminal” | Run it in a terminal, not a redirected/background input session |

## Microphone and speakers

| Symptom | Check |
| --- | --- |
| `PortAudio library not found` on Linux | Install `libportaudio2` |
| Missing native sound-file library | Install `libsndfile1` on Ubuntu/Debian |
| No microphone input | OS permission, selected default input and `python -m sounddevice` |
| Invalid sample rate | Choose an input/backend accepting 16 kHz mono |
| Listening times out | Press Enter before speaking; lower `SILENCE_THRESHOLD` if speech RMS is below it |
| Recording never ends | Background noise is above the threshold; raise it and reduce speaker/microphone feedback |
| Poor recognition language | Use `glados.py` for English or `glados_fr.py` for French |
| Slow recognition | Try `WHISPER_MODEL = 'base'`; CPU transcription speed depends on the PC |
| Piper loads but nothing is heard | Run `tools/test_voice.py`, check output selection and OS volume |
| Piper reports missing file | Check the ONNX and matching `.onnx.json` filenames in `voices/` |

The script closes the microphone while it speaks. Wait for the next Enter prompt before beginning another question. Its microphone RMS display lets you tune sensitivity from actual input levels.

## Ollama

| Symptom | Check |
| --- | --- |
| Connection refused on port 11434 | Start the Ollama app/service or `ollama serve` |
| Model not found | Run `ollama pull huihui_ai/qwen3.5-abliterated:4B` and `ollama list` |
| A very large model is downloading | Check that you used the explicit `:4B` tag |
| Slow replies | Check `ollama ps`; GPU offloading depends on supported hardware and drivers |
| Insufficient RAM/VRAM | Close other applications, reduce context, or choose a smaller installed model |
| Whisper's first launch fails downloading | Check internet access and retry; the model is cached after download |

## ESP32-C3 upload and serial

| Symptom | Check |
| --- | --- |
| Firmware requests USB CDC | **Tools → USB CDC On Boot → Enabled** |
| No port | USB data cable; try BOOT held while tapping RESET |
| Upload stuck at “Connecting” | Select the correct C3 board and port; manually enter download mode |
| Linux permission denied | Add your current user to `dialout`, then log out/in |
| Port busy | Close Arduino Serial Monitor, the Python console or another GLaDOS instance |
| Several serial devices | Set `ESP32_PORT` to the verified device |
| Repeated USB reconnects | Check motor supply stability and grounding; test with motor power disconnected |
| `ANIM_DONE` is not received | Inspect serial output and firmware; Python keeps waiting rather than stacking new movements |

The PC conversation can work without an ESP32, but the terminal will report serial retries and there will be no animation. `PING` should return `ESP32_MOTORS_OK` from the uploaded firmware.

## Motor and display behavior

| Symptom | Check |
| --- | --- |
| Eye and head roll behave as swapped axes | Eye signal is GPIO 6; head roll signal is GPIO 7 |
| Stepper does not move | STEP/DIR wiring, enabled driver, RESET/SLEEP requirements, driver supply and current setting |
| Pitch direction is wrong | Calibrate `NEMA_INVERT_DIR`, then re-establish the physical reference |
| Head pose drifts | Open-loop stepper may miss steps; re-reference and inspect load, current and speed |
| Mechanism strikes a stop | Disconnect motor power; correct the physical start pose and calibrate the limits |
| Head moves unexpectedly on reset | Check `NEMA_BOOT_TRAVEL_STEPS`; boot travel is separate from estimated-position limits |
| Animation commands rejected | `CALIBRATION_ON_BOOT` or `CAL ON` may still be active; send `CAL OFF` only after calibration |
| Calibration target rejected | Value must be inside compiled bounds; `HEAD` accepts whole pulse counts |
| Display stays blank | GC9A01A module, power rating, common GND and all five SPI/control signals |
| Board only boots with peripherals disconnected | Check GPIO 2/9 strapping levels and external LED circuitry |

Software `0` is a pose return, not a reference search. It cannot recover an unknown physical stepper origin. See the [calibration guide](CALIBRATION.md) before further movement.
