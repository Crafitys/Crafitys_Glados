# Configuration reference

Shared PC settings are near the top of `glados.py`. The French launcher imports that module and changes the recognition language, voice paths and character prompts. Edit shared settings in `glados.py`, even when you use `glados_fr.py`.

## PC settings

| Setting | Default | Purpose |
| --- | --- | --- |
| `OLLAMA_MODEL` | `huihui_ai/qwen3.5-abliterated:4B` | Installed local AI model; overridable by the environment |
| `OLLAMA_URL` | `http://localhost:11434/api/chat` | Local Ollama chat endpoint |
| `OLLAMA_CONTEXT` | `2048` | Ollama context budget |
| `OLLAMA_MAX_TOKENS` | `300` | Normal reply generation limit |
| `WHISPER_MODEL` | `small` | Recognition model; try `base` for a slower PC |
| `WHISPER_DEVICE` / `WHISPER_COMPUTE` | `cpu` / `int8` | Recognition execution mode |
| `WHISPER_THREADS` | `3` | Recognition CPU threads |
| `INPUT_DEVICE` / `OUTPUT_DEVICE` | `None` / `None` | Default system microphone/speakers |
| `SILENCE_THRESHOLD` | `4000` | RMS level distinguishing speech from background noise |
| `SILENCE_DURATION` | `1.0` | Seconds of silence before recording stops |
| `START_TIMEOUT` | `6.0` | Maximum wait for speech after pressing Enter |
| `MAX_RECORD_SECONDS` | `20` | Maximum recorded question duration |
| `SHOW_MIC_LEVEL` | `True` | Show microphone levels during recording |
| `USE_VAD_FOR_RECORDING` | `False` | Optional WebRTC VAD in addition to the RMS threshold |
| `ESP32_PORT` | Automatic matching USB port | Can be overridden by the `ESP32_PORT` environment variable |
| `SERIAL_BAUDRATE` | `115200` | Must match firmware `Serial.begin()` |
| `HISTORY_TURNS` | `3` | Short context held in RAM; set `0` to disable |
| `HISTORY_CHAR_BUDGET` | `1600` | Approximate context budget in characters |
| `IDLE_THOUGHTS_ENABLED` | `True` | Speak occasional spontaneous AI thoughts |
| `IDLE_THOUGHT_MIN_SECONDS` / `MAX_SECONDS` | `60` / `300` | Random wait before a spontaneous thought |

`IDLE_THOUGHTS_ENABLED = False` disables spontaneous speech from the PC. It does not disable the ESP32's independent idle motor movements. Use firmware calibration mode to keep those movements disabled while adjusting axes.

A three-turn history means up to three user messages and three answers, subject to the character budget. Neither the conversation nor a personal profile is saved to disk by this application. Ollama and Whisper retain their downloaded model caches separately.

## Change the AI model

Install the alternative model with Ollama first. Then either edit `OLLAMA_MODEL` or set its environment override. It must expose Ollama's chat API and fit the PC's resources.

Linux:

```bash
export OLLAMA_MODEL=your-installed-model-tag
python glados.py
```

Windows Command Prompt:

```bat
set OLLAMA_MODEL=your-installed-model-tag
python glados.py
```

The script streams completed sentences to Piper, ignores separate model thinking fields and requests `think=False`. Different model families may handle that option differently.

## Change the character style

Edit `SYSTEM_PROMPT` and `IDLE_PROMPT` in `glados.py` for English. Edit their assigned values in `glados_fr.py` for French. These prompts contain no user-specific profile information.

## Firmware commands

Use the Arduino Serial Monitor or `tools/serial_console.py`; do not open the same port while GLaDOS runs.

| Command | Action |
| --- | --- |
| `PING` | Reply `ESP32_MOTORS_OK` |
| `STATUS` | Show queue, activity and estimated motor positions |
| `LIST` | List numbered animation commands |
| `1` through `6` | Queue a speaking animation |
| `0` | Finish the current animation, clear pending animations and return to initial targets |
| `CLEAR` | Clear queued animations; does not stop a movement already executing |
| `CAL ON` | Enable calibration mode and disable new idle motion |
| `EYE 56` | Example eye target in degrees, accepted only during calibration |
| `ROLL 30` | Example roll target in degrees, accepted only during calibration |
| `HEAD -350` | Example head pitch target in STEP pulses, accepted only during calibration |
| `CAL OFF` | Leave calibration mode after all axes settle |
| `SPEECH_START` | PC begins a speech session; idle motion yields |
| `SPEECH_KEEPALIVE` | PC maintains the session between spoken sentences |
| `SPEECH_END` | PC releases the session |

Calibration commands reject invalid, non-finite and out-of-range targets rather than silently taking a large excursion. `STATUS` and `PING` remain available during calibration. The firmware enforces the servo/stepper interlock: the two servos may move together, while stepper moves run separately.

For mechanical constants and animation-table fields, use the [calibration guide](CALIBRATION.md).
