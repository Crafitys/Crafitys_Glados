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
| Suitable external DC supply | The map uses **24 V as an example**, feeding A4988 VMOT and the converter input; allow for both loads and set the actual motor's phase-current limit |
| DC/DC step-down converter rated for the external supply input | **Added for the map's 24 V → 5 V servo supply.** Set and measure 5.0 V before connection; confirm both servos support 5 V and size for their combined peak current |
| Electrolytic capacitor, at least 47 µF | Connected across VMOT and motor GND close to the driver; the 24 V map shows **100 µF / 50 V** |
| 330 Ω resistor, 1/4 W | Limits the red LED current in the shown 3.3 V circuit |
| USB data cable | Powers the C3 and connects it to the PC |
| Wires, headers and suitable connectors/terminal blocks | Signal wiring and motor power distribution |
| Multimeter | Identifies motor winding pairs and measures A4988 VREF |
| Driver heatsink/cooling as required | Keeps the A4988 within the module's thermal limits |

The [component wiring map](WIRING.md) shows **USB-C from the PC powering the ESP32-C3**, with a separate **24 V supply feeding the A4988 and a converter set to 5 V for both servos**. All negative terminals and component grounds share **0 V / GND**; this does not require negative-voltage supplies. Keep external +5 V separate from USB +5 V. A suitable 12 V motor supply with a compatible converter is an alternative; use capacitor voltage ratings with adequate margin for the selected rail (100 µF / 25 V is an example for 12 V).

See [the basic wiring guide](WIRING.md) for the pin-by-pin table and schematic, then [calibrate motor travel](CALIBRATION.md).
