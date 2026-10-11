# Software package verification

This public package was prepared from the supplied complete Python script, the newer ESP32-C3 firmware ZIP, and the four supplied voice files. The older project sketch was not used because it assigned GPIO 23, which is not a valid ESP32-C3 GPIO.

## Release preparation changes

- Removed the persistent profile file, its loading/saving and confirmation functions, and hard-coded personal names.
- Added foreground terminal Enter control for starting a conversation.
- Provided English recognition/replies/voice and a French launcher with French recognition/replies/voice.
- Preserved streaming AI replies, sentence-by-sentence speech, USB animation sequencing and idle behavior.
- Made Piper use the CPU by default and added Windows terminal/serial compatibility.
- Added setup, voice and serial tools, detailed English guides and a signal diagram.
- Added firmware calibration commands, a named boot pulse count, a direction-inversion setting and compile-time initial-position checks.
- Fixed a negative random eye distance that could be converted into an invalid unsigned idle duration.

## Verification scope

The Python scripts are checked for syntax and tested for language selection, temporary conversation context, target paths and streamed reply handling. Both supplied ONNX files pass ONNX structure checks and produce non-empty 22,050 Hz speech using CPU Piper.

The public text files are scanned for the supplied private names, personal home paths, e-mail addresses, credentials and persistent profile code. The ONNX model metadata is also inspected; the supplied models have no custom metadata or model/graph documentation strings. No private profile or original personal test script is bundled.

The firmware compiles for `esp32:esp32:esp32c3:CDCOnBoot=cdc` with Arduino-ESP32 **3.3.0**, ESP32Servo **3.2.1**, Adafruit GFX **1.12.6**, Adafruit GC9A01A **1.1.1** and Adafruit BusIO **1.17.4**. The build uses **388,584 bytes** of program storage and **20,008 bytes** of global RAM. These figures depend on the selected core and library versions.

Six PC behavior tests pass. Run them from an installed, activated environment with `python -m unittest discover -s tests -v`. The tests use simulated Ollama responses and speech playback; they do not move hardware.

The available environment has no attached microphone, speakers, ESP32-C3 or assembled mechanism. Successful software checks do not establish physical travel, driver current, missed-step performance or Windows hardware operation. Always perform the wiring and calibration procedure on your build.

Validated direct dependency versions are recorded in `requirements-validated.txt`. That file records one check environment; `requirements.txt` specifies compatible release ranges for normal installation.
