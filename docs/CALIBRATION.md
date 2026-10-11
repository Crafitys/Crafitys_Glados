# Set the travel of all three motors

All motor travel constants live near the top of **`firmware/glados/glados.ino`**. Edit the Arduino file and upload it to apply a permanent setting. Python does not define the mechanical endpoints.

## 1. Identify the motor and its variables

| Movement | Axis / command | Variables | Supplied values | Units |
| --- | --- | --- | --- | --- |
| Eye up/down | `servo1` / `EYE` | `SERVO1_MIN`, `SERVO1_MAX`, `SERVO1_INIT` | **36**, **70**, **56** | Commanded servo degrees |
| Head left/right lean | `servo2` / `ROLL` | `SERVO2_MIN`, `SERVO2_MAX`, `SERVO2_INIT` | **9**, **121**, **30** | Commanded servo degrees |
| Head up/down nod | NEMA / `HEAD` | `NEMA_MIN_POS`, `NEMA_MAX_POS`, `NEMA_CENTER_POSITION` | **−760**, **−50**, **−350** | STEP pulses |

`MIN` means the lower numerical coordinate and `MAX` the upper coordinate. Which one physically points up or left depends on the mounted horn, linkage and stepper direction. Identify the direction by observing small test movements.

`INIT` is the startup servo angle; `NEMA_CENTER_POSITION` is the estimated stepper coordinate assigned after boot travel, and the target for command `0`. None of these values measure actual motor position.

## 2. Begin with automatic motion disabled

Before first power-up of an uncalibrated mechanism, set:

```cpp
const int NEMA_BOOT_TRAVEL_STEPS = 0;
const bool CALIBRATION_ON_BOOT = true;
```

Upload with motor power disconnected. This prevents the 301-pulse boot move and idle motor movements. It does not prevent the servos from being commanded to their `INIT` angles when powered.

For initial fitting, power the servos with horns/linkages detached so they reach the intended initial command. Then disconnect power and mount the horns and mechanism in a corresponding comfortable reference pose. Support the head when its drive is disconnected.

Begin with narrow software limits around each initial angle, expanding only after small movements demonstrate available clearance. For example, a known 56° eye center could start at 54–58°, and a known 30° roll center at 28–32°. These are illustrative test ranges, not guaranteed suitable physical poses.

For the head stepper, manually place the mechanism in a known centered pose **with power disconnected**, and set `NEMA_CENTER_POSITION` to the coordinate you want to assign to that pose. With zero boot travel, the firmware assigns that coordinate without moving the stepper.

The firmware checks at compilation that each initial/center value lies within its corresponding limits.

## 3. Open the calibration console

Stop `glados.py` and close Arduino's Serial Monitor. Activate the PC virtual environment, then:

```bash
python tools/serial_console.py
```

If needed, pass the correct port with `--port`. Opening a port may reset the board; use the zero-boot-travel setting during calibration.

Start with:

```text
PING
CAL ON
STATUS
```

`CALIBRATION_ON` confirms calibration mode. While it is active, idle motor movements and numbered animations are disabled. A move already in progress settles first. Wait until `STATUS` shows **S1=IDLE**, **S2=IDLE**, **N=IDLE** before sending a new target.

If an animation or initial-pose return is running, `CAL ON` returns `BUSY`; wait for it to finish and send `CAL ON` again.

## 4. Set the eye servo travel

The eye is **servo1 on GPIO 6**. Start near the current initial position. With the supplied eye center, a small sequence is:

```text
EYE 56
```

Wait for the move to finish, then:

```text
EYE 57
```

Then try `EYE 55` after it finishes. A command is rejected if it exceeds the currently compiled limits. Increase the test range in small steps, staying clear of a mechanical stop or a binding linkage. Do not use servo buzzing/stalling as a method of finding an endpoint.

When you have found a comfortable range with clearance, write it into:

```cpp
constexpr float SERVO1_MIN = 36.0f;
constexpr float SERVO1_MAX = 70.0f;
constexpr float SERVO1_INIT = 56.0f;
```

These are the supplied example values; replace them with your measured safe values. `SERVO1_INIT` need not be the arithmetic midpoint if a different neutral pose looks better.

## 5. Set the head roll servo travel

The left/right head lean is **servo2 on GPIO 7**. Use the same method:

```text
ROLL 30
```

Then test one or two degrees on each side, waiting for each move to finish. Observe which numerical direction leans left and which leans right. The supplied wide range must not be assumed suitable for every build.

Record the endpoints and preferred initial lean:

```cpp
constexpr float SERVO2_MIN = 9.0f;
constexpr float SERVO2_MAX = 121.0f;
constexpr float SERVO2_INIT = 30.0f;
```

Servo startup/command `0` uses **30°** here. Some speaking animations finish at other values, such as **56°**. Changing `SERVO2_INIT` alone does not change those animation table entries; see step 8.

## 6. Set the head pitch stepper travel

The nodding axis uses **STEP on GPIO 3** and **DIR on GPIO 10**. Its software position is the estimated count of step pulses, not an angle or encoder reading.

After placing the unpowered head at the chosen reference pose and powering up with zero boot travel, the supplied center is **−350**. Test ten pulses at a time:

```text
HEAD -340
```

Wait until idle. Return to the center, then try the other direction:

```text
HEAD -350
```

```text
HEAD -360
```

Only send the next line after the previous move finishes. Work out the safe coordinate range by gradually increasing the excursion while retaining clearance from physical stops. Keep the head supported and the driver current appropriate for the motor.

Write the resulting numerical bounds and neutral target into:

```cpp
const long NEMA_MIN_POS = -760;
const long NEMA_MAX_POS = -50;
const long NEMA_CENTER_POSITION = -350;
```

Negative positions are normal. For example, −760 is numerically below −50. Firmware motion commands stay within the compiled bounds, but software limits cannot compensate for an incorrect starting pose or missed steps.

### Microsteps and direction

One `HEAD` unit is one **STEP pulse sent to the driver**. Changing the driver's microstep selection, motor step angle, pulley ratio or belt arrangement changes the physical movement associated with these coordinates. Recalibrate after changing any of them.

If you need to invert the direction, change:

```cpp
const bool NEMA_INVERT_DIR = true;
```

This reverses the electrical DIR polarity for both normal movements and boot travel. Re-establish the reference pose and recheck the endpoints after changing it. It does not invert the meaning of the numerical min/max values.

## 7. Understand the boot movement and command 0

The original reference firmware sends **301 STEP pulses in the decreasing-coordinate direction** at boot, then assigns `NEMA_CENTER_POSITION` to the final physical pose. The public firmware exposes that count as `NEMA_BOOT_TRAVEL_STEPS`.

This boot travel runs **outside normal software endpoint checking** because it happens before a position reference has been established. The initial physical pose must leave room for the entire travel. A numerical `NEMA_MIN_POS` cannot protect an incorrectly placed mechanism during this boot move.

There are two supported setup choices:

| Choice | Boot setting | Your responsibility |
| --- | --- | --- |
| Manual center reference | `NEMA_BOOT_TRAVEL_STEPS = 0` | Place the unpowered mechanism at the defined center before each fresh start/reset |
| Original reference boot travel | `NEMA_BOOT_TRAVEL_STEPS = 301`, or your calibrated count | Start from the repeatable pre-travel pose that ends at your defined center |

There is no limit switch or encoder in the supplied firmware. After a reset, skipped steps or manually moving the motor, re-establish the reference. **Command `0` does not find a physical home**: it finishes the current animation, clears queued animations and returns to the configured targets using estimated positions.

## 8. Update the animation targets if needed

The `ANIM_1_STEPS` through `ANIM_6_STEPS` tables contain fixed target angles and pulse positions. Limits clamp their targets at run time. If you substantially narrow travel or change the preferred neutral pose, edit the tables so the expressions still look intentional rather than being repeatedly clamped.

Each `AnimStep` row has these fields, in order:

| Field | Meaning |
| --- | --- |
| `useServo1`, `servo1Pos`, `servo1TimeMs` | Enable eye movement, eye degrees, requested duration in ms |
| `useServo2`, `servo2Pos`, `servo2TimeMs` | Enable head roll, roll degrees, requested duration in ms |
| `useNema`, `nemaPos`, `nemaTimeMs` | Enable head pitch, pulse coordinate, requested duration in ms |
| `waitMode` | Completion rule for the row |
| `eyeTimeMs` | Screen eye effect duration; not the eye servo angle |

The supplied animations use eye targets around **40–68°**, roll targets around **12–90°**, and stepper targets around **−600 to −200**. Several final rows contain **58° eye**, **56° roll**, and **−350 head**. Check each enabled axis; fields belonging to an axis marked `false` are ignored.

The firmware sequences mixed rows as **servos first, then the stepper**. This preserves the original movement interlock. Servo motion can take longer than a table's requested duration because the smooth trajectory enforces a maximum commanded speed.

## 9. Adjust speed and idle motion

| Variable | Supplied value | Effect |
| --- | --- | --- |
| `EYE_MAX_SPEED` | `60.0f` | Eye commanded speed cap, degrees/second |
| `HEAD_MAX_SPEED` | `90.0f` | **Roll servo** commanded speed cap, degrees/second |
| `NEMA_TIME_SCALE` | `2.0f` | Multiplies requested pitch move duration; larger is slower |
| `NEMA_MIN_STEP_INTERVAL_US` | `2500` | Minimum time between STEP pulses; larger is slower |
| `NEMA_INIT_STEP_INTERVAL_US` | `5000` | Boot travel pulse interval only |
| `IDLE_DELAY_MIN_MS` / `IDLE_DELAY_MAX_MS` | `4000` / `9000` | Delay before a new idle movement |
| `SERVO_MIN_US` / `SERVO_MAX_US` | `500` / `2400` | Pulse-width mapping for 0–180° commands |

`HEAD_MAX_SPEED` refers to the head **roll servo**, despite its general name. It does not set the NEMA pitch speed. The stepper uses a fixed pulse interval for each requested move; it has no acceleration ramp in this firmware.

Leave the pulse-width mapping unchanged unless your servo specification and tests justify a different range. Changing it changes the physical shaft angle produced by every servo angle and requires recalibration.

Idle movements are generated in `updateIdleMovements()`. The eye excursion is **2–5°**, roll **20–49.9°**, and pitch **20–49 pulses** in this public build. These values come from the code, not legacy comments. Targets are still clamped to the compiled limits. Narrow the roll idle amplitude if you want subtler motion by changing its `random(200, 500) / 10.0f` expression.

## 10. Finish and test the normal animations

1. Save your measured limits and chosen boot-reference settings in the Arduino file.
2. Set `CALIBRATION_ON_BOOT = false` when you are ready for idle movement at startup.
3. Disconnect motor power, upload the revised sketch and re-establish the physical reference pose.
4. Power the mechanism and check the startup pose.
5. Send `CAL OFF` if calibration mode is active, then test numbered animations **one at a time**.
6. Send `0` to check the initial-pose return.
7. Close the serial console and launch the Python edition you want.

`CAL_TARGET_ACCEPTED` means a calibration move was scheduled, not that it has completed. `ERROR:TARGET_OUT_OF_RANGE` means no target was accepted. `BUSY` means wait for the current movement to finish. Calibration commands are temporary; only edited and re-uploaded firmware constants persist after restart.
