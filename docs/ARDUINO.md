# Upload the firmware to the ESP32-C3 Super Mini

Use **an ESP32-C3 Super Mini**, with native USB and 4 MB flash, for the documented pinout. A classic ESP32, an S3 or a different board is not a direct substitute: GPIO availability, header labels and USB behavior differ.

## 1. Prepare for the first flash

Keep motor power disconnected while installing or uploading firmware. The USB connection powers the C3 for flashing. Connecting USB, resetting the board or opening a serial connection may restart the firmware and run its boot sequence.

Open `firmware/glados/glados.ino` in [Arduino IDE](https://www.arduino.cc/en/software). Its containing folder must be called `glados`, matching `glados.ino`; the repository already provides that structure.

For an uncalibrated build, change these two constants before the first upload:

```cpp
const int NEMA_BOOT_TRAVEL_STEPS = 0;
const bool CALIBRATION_ON_BOOT = true;
```

This removes the automatic stepper boot travel and disables idle motor movements from startup. Servos still receive their initial angle commands; fit their horns/linkages at the initial angles as described in the [calibration guide](CALIBRATION.md).

## 2. Install the Espressif board package

1. Open **File → Preferences**.
2. Find **Additional Boards Manager URLs**.
3. Add the stable Espressif package URL below. If other URLs are already present, add it as a separate entry rather than deleting them.

```text
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```

4. Open **Tools → Board → Boards Manager**, or use the Boards Manager sidebar.
5. Search for **esp32**.
6. Install **esp32 by Espressif Systems**, version **3.x**. Use a matching current ESP32Servo release; current ESP32Servo targets Arduino-ESP32 3.x.

## 3. Select the C3 board and USB options

In **Tools → Board → esp32**, select **ESP32C3 Dev Module**.

| Option | Setting |
| --- | --- |
| Board | **ESP32C3 Dev Module** |
| USB CDC On Boot | **Enabled** — required |
| Flash Size | **4 MB**, matching the Super Mini |
| CPU Frequency | **160 MHz**, if shown |
| Partition Scheme | Default for the selected 4 MB board |
| Upload Speed | Start at **115200** if uploads are unreliable |
| Port | The USB port that appears when you connect the board |

Leave other options at their board defaults unless your specific module requires otherwise. A flash-mode mismatch is a board-specific issue; check the supplier's specifications if a flashed board fails to boot.

The sketch includes a compile-time check for **USB CDC On Boot**. If it stops with `Enable USB CDC On Boot in the Arduino board options`, change the setting and compile again. The C3 uses USB CDC here; you do not need a USB DFU setting.

## 4. Install the libraries

Open **Sketch → Include Library → Manage Libraries**. Install:

| Library | Why it is needed |
| --- | --- |
| **ESP32Servo** | Two servo PWM signals and smooth commanded trajectories |
| **Adafruit GFX Library** | Display graphics |
| **Adafruit GC9A01A** | The round GC9A01A display driver |
| **Adafruit BusIO** | Adafruit dependency; accept dependency installation if prompted |

`SPI.h`, `Arduino.h` and standard math functions come with the ESP32 board package; they are not separate downloaded project files.

## 5. Verify and upload

1. Connect the ESP32-C3 to the PC with a USB data cable.
2. Select its port.
3. Click **Verify** (checkmark). Wait for compilation to finish.
4. Close Python GLaDOS and any serial console using the port.
5. Click **Upload** (arrow).
6. After the first upload, press **RESET** if necessary to run the firmware.

If no usable port appears, or uploading stays at “Connecting”:

1. Hold **BOOT**.
2. Press and release **RESET** while holding BOOT.
3. Release BOOT.
4. Select the newly appearing port and upload again.

GPIO 9 is the C3's BOOT pin and is also used for the LED output. Avoid an external circuit that holds it low during startup.

## 6. Check communication before enabling motion

Open **Tools → Serial Monitor** at **115200 baud**. Set the line ending to **Newline** or **Both NL & CR**.

Send:

```text
PING
```

Expected reply:

```text
ESP32_MOTORS_OK
```

`STATUS` reports estimated positions. `LIST` lists the animation commands. If you missed the initial `ESP32_MOTORS_BOOT_OK` message, `PING` still confirms the firmware is responding.

Do not send an animation until you have calibrated the mechanism. Follow [motor calibration](CALIBRATION.md) next.

## 7. Use the Python serial console instead

With the PC venv activated:

```bash
python tools/serial_console.py
```

Or specify the identified port:

```bash
python tools/serial_console.py --port /dev/ttyACM0
```

On Windows, substitute a port such as `COM5`. The console and Arduino Serial Monitor do the same serial-command job; use one at a time. Close them before launching `glados.py`.

## Updating the firmware later

Stop GLaDOS, disconnect motor power, edit the firmware and upload it. Every upload/reset invalidates your knowledge of the stepper's physical pose unless you re-establish the configured starting reference. Preserve your calibrated constants when updating.

Official references: [Espressif installation](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html), [C3 USB CDC setup](https://docs.espressif.com/projects/arduino-esp32/en/latest/tutorials/cdc_dfu_flash.html), [ESP32Servo](https://github.com/madhephaestus/ESP32Servo).
