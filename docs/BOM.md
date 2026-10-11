# Bill of materials

Everything to plan the printed build, its wiring and the PC connection. This page combines Crafitys' supplied shopping list with the additional items needed by the [power and signal map](WIRING.md) and the [assembly manual](assembly/Glados_Assembly_Instructions.pdf).

[Assembly guide](ASSEMBLY.md) · [Wiring map](WIRING.md) · [STL files on Cults3D](https://cults3d.com/fr/utilisateurs/Crafitys/fichiers-3d) · [Original shopping-list PDF](assembly/Original_Shopping_List.pdf)

## Reference shopping links

**“Buy” means the number of packs in the supplied list, not the number of parts installed in one robot.** Product links below retain the same Amazon product IDs, with search/tracking parameters removed. Prices, availability and selected variants can change; confirm dimensions and electrical ratings before ordering. The listed variants' ratings have not been independently verified.

### Electronics and motors

| Item | Buy | Parts used / selection notes | Supplied link |
| --- | ---: | --- | --- |
| ESP32-C3 Super Mini | 1 board | Use the **C3** pinout, with USB-C and 4 MB flash | [Board](https://www.amazon.fr/dp/B0DMNBWTFD) |
| 15 kg·cm class servos, pack of 2 | 1 pack | **2 servos:** eye and head roll; confirm 5 V compatibility for the shown converter setup | [Servo pack](https://www.amazon.fr/dp/B09KZ8VTNB) |
| GC9A01 round SPI screen | 1 screen | 240 × 240 eye display; confirm VCC and 3.3 V signal compatibility | [Screen](https://www.amazon.fr/dp/B0CFXVD9HX) |
| NEMA 17 bipolar stepper | 1 motor | Head pitch; establish rated phase current and identify both coil pairs | [Motor](https://www.amazon.fr/dp/B0B38GX54H) |
| A4988 driver module | 1 module | STEP/DIR driver; check sense resistors, pin labels and supplied cooling | [Driver](https://www.amazon.fr/dp/B08R64Y7V7) |
| Red LED | 1 listed pack/item | **1 bare red LED** used; add the 330 Ω resistor below | [LEDs](https://www.amazon.fr/dp/B0C3VGHGR1) |

### Frame, transmission and fasteners

| Item | Buy / stock | Use and selection notes | Supplied link |
| --- | ---: | --- | --- |
| Socket-head screw assortment | 1 kit | Check that the exact sizes in the fastener table below are included | [Screw kit](https://www.amazon.fr/dp/B0DGGG3LJ4) |
| M4 heat-set inserts, pack of 50 | 1 pack | Printed-part threads; confirm insert dimensions against the STL pockets | [M4 inserts](https://www.amazon.fr/dp/B07YSV66Y5) |
| M5 heat-set inserts, pack of 50 | 1 pack | Head plates and supports; confirm insert dimensions against the STL pockets | [M5 inserts](https://www.amazon.fr/dp/B07YSVXWS8) |
| F688ZZ flanged bearings, pack of 10 | 1 pack | Head roll and pivot assemblies; follow the manual's bearing positions | [Bearings](https://www.amazon.fr/dp/B094NL7MDS) |
| GT2 pulleys, pack of 5 | 1 pack | Motor pulley and printed transmission: match bore, belt width and tooth geometry | [Pulleys](https://www.amazon.fr/dp/B0H3HK3JTX) |
| 2020 aluminium extrusion | **750 mm total in the supplied list** | Manual explicitly names 47 mm, 60 mm and 150 mm pieces; also allow for the base and saw kerf | [Extrusion](https://www.amazon.fr/dp/B0G3WY1KL1) |
| 2020 corner brackets, pack of 20 | 1 pack | Base and motor support; check that matching T-slot nuts and fixing screws are included | [Brackets](https://www.amazon.fr/dp/B0GY8RY9CW) |
| Ø2 × 6 mm plastic screws, pack of 250 | 1 pack | **2 screws** explicitly specified for the display | [Plastic screws](https://www.amazon.fr/dp/B0CH9ZXMSK) |
| M8 × 40 mm bolts with nuts, pack of 20 | 1 pack | **1 bolt and matching nut** explicitly specified for the pivot | [Bolts and nuts](https://www.amazon.fr/dp/B09V4XTDTB) |
| 300 mm GT2 belts, pack of 5 | 1 pack | **1 belt** for the head pitch transmission; match belt width to the pulleys | [Belts](https://www.amazon.fr/dp/B0G712L3TR) |

### Tool already included in the supplied list

| Tool | Buy | Use | Supplied link |
| --- | ---: | --- | --- |
| M5 taps, pack of 2 | 1 pack | Tap the extrusion ends specified by the manual | [M5 taps](https://www.amazon.fr/dp/B0F6MLD36Y) |

## Additional electrical items

These items were **absent from the supplied shopping-list PDF**. They complete the USB-powered C3 / external motor-power arrangement. No specific supplier link was provided for them; choose to these specifications rather than assuming a particular product variant.

| Add | Quantity | Specification / purpose |
| --- | ---: | --- |
| External regulated DC supply | 1 | **24 V example** for A4988 VMOT and converter input; size for both loads. A suitable 12 V supply with a compatible converter is an alternative |
| DC/DC step-down converter | 1 | Input rated for the chosen supply; adjust and measure **5.0 V** before attaching compatible servos. Output current must cover **both servos' combined peak current** |
| Electrolytic capacitor | 1 | **100 µF / 50 V** across VMOT and motor GND, close to the A4988, for the 24 V example; observe polarity. At least 47 µF is required by the referenced carrier guide |
| LED resistor | 1 | **330 Ω, 1/4 W**, in series with the bare red LED |
| USB data cable to USB-C | 1 | PC → ESP32-C3; carries 5 V board power and serial data |
| Signal wires / headers | As needed | C3, screen, LED and driver signal connections |
| Motor-power wires and distribution connectors | As needed | Supply, converter, driver and both servos; size for the actual currents. Join all GND returns through suitable distribution wiring |
| Insulation / heat-shrink tubing | As needed | Insulate soldered connections and exposed joints |
| A4988 heatsink / cooling | As required | Check what comes with the module and select cooling for the current limit used |

**Keep external servo +5 V separate from USB +5 V.** Supply negative terminals are **0 V / GND**, not negative-voltage rails. Do not send 24 V directly to the servos, display or C3. See the [pin-by-pin wiring guide](WIRING.md) for exact connections and the driver current-limit procedure.

The servo model/voltage, screen supply compatibility, stepper phase current and carrier sense-resistor values still need confirmation on the actual hardware. Consequently this list does not invent a PSU current rating, converter current rating or universal A4988 VREF.

## Additional build materials and tools

| Add / have available | Quantity | Why it is needed |
| --- | ---: | --- |
| Printed GLaDOS parts and suitable filament | Complete STL set | Download the mechanical files from [Crafitys on Cults3D](https://cults3d.com/fr/utilisateurs/Crafitys/fichiers-3d); filament quantity depends on the sliced parts |
| Counterweight filling | Approximately **1 kg** | Nuts, sand or pellets, as specified on manual page 17 |
| Matching 2020 T-slot nuts and bracket bolts | As needed if absent from bracket kit | Frame joints and motor support |
| Servo mounting / horn screws | As needed if absent from servo packs | Use the screws that fit the actual servos and horns |
| Soldering iron and solder | 1 set | LED wiring and heat-set inserts |
| Hot glue gun and glue sticks | 1 set | LED retention, as specified on manual page 2 |
| Saw, deburring tool and measuring tools | 1 set | Cut and finish the aluminium extrusion lengths |
| Tap holder and drill/tools appropriate to the extrusion | 1 set | Prepare and tap the required M5 end holes |
| Hex keys and screwdrivers | 1 set | Assembly fasteners and driver adjustment |
| Multimeter | 1 | Measure converter output and VREF; identify winding pairs |
| PC, microphone and speakers | 1 setup | Speech recognition, local AI and GLaDOS voice; see [PC requirements](INSTALL.md) |

## Check the screw kit before ordering

These are the **explicit quantities in the supplied manual**, not a complete count of every mounting screw. Extrusion bracket screws and unspecified servo fixings are additional. If the assortment lacks a size or quantity, buy those fasteners separately.

| Fastener | Explicit quantity | Manual pages / use |
| --- | ---: | --- |
| **M5 × 12 mm** | **18** | pp. 5–6: 10; p. 10: 2; p. 11: 1; p. 12: 1; p. 17: 4 |
| **M5 × 20 mm** | **1** | p. 9: bearing-spacer assembly |
| **M4 × 10 mm** | **7** | p. 13: 3 locking-plate screws; p. 17: 4 counterweight lid screws |
| **M3 × 16 mm** | **4** | p. 14: NEMA 17 mounting |
| **M8 × 40 mm + nut** | **1 set** | p. 13: pivot axis |
| **Ø2 × 6 mm plastic screw** | **2** | p. 2: display attachment |

Page 10 describes inserting three M4 inserts, while page 17 calls for four lid screws in previously fitted M4 inserts. Check all insert pockets in the printed parts, including the counterweight lid mounting locations; the three-insert instruction is not a total insert count for the robot. The manual does not give a dimensioned base cut list, so verify the base arrangement before cutting all 750 mm of extrusion.

The original PDFs are retained as supplied. This expanded Markdown BOM adds purchasing checks and missing build items without rewriting the source documents.
