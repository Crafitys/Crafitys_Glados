# Electrical parts from the Crafitys build list

These are the electrical items in the supplied shopping list. Quantities below are the number **used in one robot**, not the number of packs to buy. Mechanical items such as screws, inserts, bearings, belts, pulleys and aluminium profiles are covered by the assembly manual on [Cults3D](https://cults3d.com/fr/utilisateurs/Crafitys/fichiers-3d).

| Component | Used | Role | Listed product |
| --- | ---: | --- | --- |
| ESP32-C3 Super Mini | 1 | Runs the Arduino firmware | [ESP32-C3 board](https://www.amazon.fr/dp/B0DMNBWTFD) |
| 15 kg·cm class servo | 2 | One moves the eye; the other tilts the head left/right | [Pack of two servos](https://www.amazon.fr/dp/B09KZ8VTNB) |
| GC9A01 round SPI display | 1 | Animated eye image | [Round screen](https://www.amazon.fr/dp/B0CFXVD9HX) |
| NEMA 17 bipolar stepper | 1 | Tilts the head up/down | [Stepper motor](https://www.amazon.fr/dp/B0B38GX54H) |
| A4988 stepper driver module | 1 | Drives the NEMA motor from STEP/DIR signals | [A4988 driver](https://www.amazon.fr/dp/B08R64Y7V7) |
| Red LED | 1 | Firmware-controlled light effect | [LED pack](https://www.amazon.fr/dp/B0C3VGHGR1) |

The product links identify the supplied list; they do not establish the electrical ratings of every module revision or variant. The Amazon pages could not be inspected during guide preparation. Read the actual labels and component documentation before selecting power supplies. The screen's 3.3 V VCC compatibility, exact servo model/voltage and motor phase current remain to be confirmed.

## Add these electrical items

| Item absent from the list | Purpose |
| --- | --- |
| Regulated servo supply, at the servo's rated voltage | Powers both servos independently of the C3 regulator; allow for both servos' peak current |
| Suitable DC motor supply | Powers A4988 VMOT; the wiring guide uses **12 V as an example**, with the current limit set for the actual motor |
| Electrolytic capacitor, at least 47 µF | Connected across VMOT and motor GND close to the driver; **100 µF / 25 V** is an example for a 12 V supply |
| 330 Ω resistor, 1/4 W | Limits the red LED current in the shown 3.3 V circuit |
| USB data cable | Powers the C3 and connects it to the PC |
| Wires, headers and suitable connectors/terminal blocks | Signal wiring and motor power distribution |
| Multimeter | Identifies motor winding pairs and measures A4988 VREF |
| Driver heatsink/cooling as required | Keeps the A4988 within the module's thermal limits |

A single higher-voltage supply with a correctly rated adjustable step-down converter can provide a separate servo rail. Set and measure that rail before connecting the servos. The simpler diagram shows two external supply outputs; their **negative terminals share GND**, while their positive outputs remain separate.

See [the basic wiring guide](WIRING.md) for the pin-by-pin table and schematic, then [calibrate motor travel](CALIBRATION.md).
