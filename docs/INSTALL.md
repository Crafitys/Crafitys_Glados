# Install GLaDOS on your PC

This guide starts from a fresh project download. Keep a terminal open: all Python commands below must run from the project folder, using its virtual environment.

## What the PC does

The microphone feeds faster-whisper. Your transcribed question goes to a local Ollama model. Piper turns the answer into speech. While the audio plays, the Python script sends animation commands to the ESP32-C3 over USB.

Use a PC with a working microphone and speakers. **8 GB RAM is a practical starting point; 16 GB gives more room.** The 4B Ollama download is roughly 3.3 GB, and Python dependencies and Whisper need additional disk space. Allow several gigabytes beyond the project itself. A compatible GPU can accelerate Ollama; CPU operation is possible but slower. This is a planning estimate, not a guaranteed minimum or response time.

The supplied configuration keeps Whisper and Piper on the CPU. It does not require CUDA packages for speech recognition or synthesis.

## 1. Download the project

Use [Download the complete package](https://github.com/Crafitys/Crafitys_Glados/releases/latest/download/Crafitys_Glados.zip), or open **Releases** on the repository and download the **Crafitys_Glados.zip** asset. Extract it. This package already contains both voice models.

Alternatively, with Git installed:

```bash
git clone https://github.com/Crafitys/Crafitys_Glados.git
cd Crafitys_Glados
```

Check that you can see `glados.py`, `requirements.txt`, `firmware/`, and `voices/`. The Git source tree and **Code → Download ZIP** contain the voice JSON files but keep the large ONNX weights in the release. If you used either source download method, after installing Python run `python tools/download_voices.py` from the project folder. The installer uses only the Python standard library and verifies the downloaded models. On Linux before activating a venv, use `python3`; on Windows use `py`.

Before launching GLaDOS, both ONNX models must be real files of about 63 MB each, with a matching `.onnx.json` beside each one.

## 2. Install Python and system audio support

### Ubuntu / Debian

Install Python, its virtual-environment support and the native audio libraries:

```bash
sudo apt update
sudo apt install python3 python3-venv python3-pip libportaudio2 libsndfile1 git
```

Check Python:

```bash
python3 --version
```

Use Python **3.10–3.12** for the documented setup. Package support may differ on newer Python versions.

Open a terminal inside the extracted project folder. Most file managers offer **Open in Terminal**. If you cloned with Git, stay in the folder reached by `cd Crafitys_Glados`.

### Windows

Install 64-bit Python 3.11 or 3.12 from [python.org](https://www.python.org/downloads/windows/). Enable **Add Python to PATH** during installation. Open **Command Prompt** in the project folder; the commands in the Windows sections below are Command Prompt commands.

```bat
py --version
```

Sounddevice wheels provide PortAudio on Windows. You do not use the Linux `apt` commands.

## 3. Create the virtual environment

A **virtual environment**, or **venv**, is a private set of Python packages for this project. It keeps GLaDOS dependencies separate from other Python projects. Create it once; activate it whenever you open a new terminal.

### Linux

From the project folder:

```bash
python3 -m venv .venv
source .venv/bin/activate
```

### Windows — Command Prompt

```bat
py -m venv .venv
.venv\Scripts\activate.bat
```

You should see `(.venv)` at the beginning of the prompt. Now `python` refers to the project's interpreter. Confirm it:

```bash
python -c "import sys; print(sys.executable)"
```

The displayed path should contain `.venv`. Do not run the GLaDOS script with `sudo`.

## 4. Install the Python packages

Keep the venv active:

```bash
python -m pip install --upgrade pip
python -m pip install -r requirements.txt
```

This installs faster-whisper, Piper, audio support, USB serial support and the other dependencies. It does **not** install Ollama or the Arduino firmware.

If installation fails, copy the first error message and check [troubleshooting](TROUBLESHOOTING.md). Do not install packages globally to work around a missing venv.

## 5. Install Ollama and download the AI model

Install Ollama from the official [download page](https://ollama.com/download), using the installer or instructions for your OS.

Open a terminal and run:

```bash
ollama --version
ollama pull huihui_ai/qwen3.5-abliterated:4B
```

Use the explicit **4B** tag. The untagged default can select a substantially larger model.

Test it once:

```bash
ollama run huihui_ai/qwen3.5-abliterated:4B
```

Ask a short question. Type `/bye` to leave the Ollama chat; keep the Ollama service running.

```bash
ollama list
```

The installed model should appear. The GLaDOS script contacts `http://localhost:11434/api/chat` on the same PC. If Ollama is not running, start the app or run `ollama serve` in a separate terminal. If it reports that the address is already in use, Ollama may already be running.

## 6. Verify the included voices

Do not rename these files:

| English | French |
| --- | --- |
| `voices/en_US-glados-medium.onnx` | `voices/fr_FR-glados-medium.onnx` |
| `voices/en_US-glados-medium.onnx.json` | `voices/fr_FR-glados-medium.onnx.json` |

Test English, then French if needed:

```bash
python tools/test_voice.py --language en
python tools/test_voice.py --language fr
```

Each command should play a short test phrase through the default PC output. `--no-play` checks synthesis without playing audio.

## 7. Set microphone and speaker devices

Grant your terminal/Python access to the microphone if your OS requests it. Select the microphone and speakers you want in the OS audio settings.

List devices:

```bash
python -m sounddevice
```

In `glados.py`, `INPUT_DEVICE = None` and `OUTPUT_DEVICE = None` mean the system defaults. If needed, change them to the numerical IDs reported by the list. Device IDs can change when you reconnect USB audio hardware.

The recording uses a 16 kHz mono stream. If your selected audio device does not accept that rate, select a compatible device or host audio backend. See troubleshooting rather than changing only `SAMPLE_RATE`: frame sizes and speech processing also depend on it.

## 8. Flash and connect the ESP32-C3

Follow [Arduino upload](ARDUINO.md), [wiring](WIRING.md) and [calibration](CALIBRATION.md) before enabling motor power on an assembled build.

Connect the C3 to the PC with a USB **data** cable. Close Arduino's Serial Monitor and any serial console before starting GLaDOS. Only one application should own the serial port.

On Ubuntu/Debian, if the USB port reports permission denied:

```bash
sudo usermod -aG dialout "$USER"
```

Log out and log back in for the group change to apply. This command uses the current user's account rather than a hard-coded username.

List ports:

```bash
python -m serial.tools.list_ports -v
```

The script detects matching USB serial ports automatically. If you have several serial devices, set the correct port explicitly before running it:

Linux example — replace the device with the one you just identified:

```bash
export ESP32_PORT=/dev/ttyACM0
```

Windows Command Prompt example — replace `COM5`:

```bat
set ESP32_PORT=COM5
```

## 9. Check the complete PC setup

With the venv active and Ollama running:

```bash
python tools/check_setup.py
```

This checks dependencies, both voice files, available audio devices, serial ports and the expected Ollama model. It does not open the ESP32 port or move motors. No USB port is expected if you are only trying the PC conversation first.

## Run GLaDOS

From the project folder, with the venv active:

```bash
python glados.py
```

Or use the French edition:

```bash
python glados_fr.py
```

The first launch downloads the Whisper `small` recognition model. Keep internet access available for that first download. After models are cached, subsequent launches reuse them.

1. Wait for model loading and the **Press Enter, then speak** message.
2. Press Enter with the terminal focused.
3. Speak your question within six seconds.
4. Pause; recording stops after one second of silence.
5. Whisper transcribes your question, Ollama generates a reply, and Piper speaks it.
6. The ESP32 performs speaking animations while speech plays.

The microphone is closed while the reply plays. Press Enter again after the script returns to its waiting message. Stop with **Ctrl+C**.

## Launch again tomorrow

You do **not** reinstall packages or recreate the venv each time.

Linux: open a terminal in the project folder, then:

```bash
source .venv/bin/activate
python glados.py
```

Windows Command Prompt: open it in the project folder, then:

```bat
.venv\Scripts\activate.bat
python glados.py
```

Ollama must also be running. Use `python glados_fr.py` for French. To leave the virtual environment after stopping the script, type `deactivate`.

## Tune microphone sensitivity

The script shows microphone RMS levels while recording. `SILENCE_THRESHOLD = 4000` is the supplied build's starting value, not a universal microphone setting.

- If your voice is ignored, lower the threshold below your spoken RMS level.
- If it never stops recording, raise it above the room's background RMS level.
- Keep it between the background noise and your normal speech levels.

Change this in `glados.py`; the French launcher shares those settings. [Configuration reference](CONFIGURATION.md) explains the other controls.

## Official software references

- [Python virtual environments](https://docs.python.org/3/library/venv.html)
- [Ollama download](https://ollama.com/download)
- [Exact Ollama model](https://ollama.com/huihui_ai/qwen3.5-abliterated:4B)
- [faster-whisper](https://github.com/SYSTRAN/faster-whisper)
- [Piper Python API](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/API_PYTHON.md)
