#include <Arduino.h>
#include <ESP32Servo.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <math.h>

#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "Enable USB CDC On Boot in the Arduino board options."
#endif

enum WaitMode : uint8_t {
  WAIT_NONE,
  WAIT_ALL,
  WAIT_SERVOS,
  WAIT_SERVO1_NEMA,
  WAIT_SERVO1,
  WAIT_SERVO2,
  WAIT_NEMA
};

struct AnimStep {
  bool useServo1;
  float servo1Pos;
  uint16_t servo1TimeMs;

  bool useServo2;
  float servo2Pos;
  uint16_t servo2TimeMs;

  bool useNema;
  long nemaPos;
  uint16_t nemaTimeMs;

  WaitMode waitMode;
  uint16_t eyeTimeMs;
};

struct AnimationDef {
  const char* name;
  const AnimStep* steps;
  uint8_t stepCount;
};

bool isWaitSatisfied(WaitMode mode);
bool startAnimStep(const AnimStep& step);

void updateNema();
void updateServo1();
void updateServo2();
void renderEye(bool forceRedraw);
void processSerialLine(String line);
void cancelIdleForSpeech();
void beginSpeechSession();
void endSpeechSession();

// Eye tilt servo (up/down). GPIO numbers, not header positions.
const int PIN_SERVO_1 = 6;  
// Head roll servo (left/right tilt).
const int PIN_SERVO_2 = 7;  

// Head pitch stepper driver: STEP and DIR. EN is handled by the wiring.
const int PIN_STEP = 3;
const int PIN_DIR = 10;

// Active-low LED output; GPIO 9 is also the C3 BOOT pin.
const int PIN_LED = 9;

#define TFT_CS   5
#define TFT_DC   1
#define TFT_RST  0
#define TFT_SCLK 2
#define TFT_MOSI 4

// Travel limits: degrees for servos, STEP pulses for the stepper.
// These values fit the reference build; calibrate your linkage before use.
constexpr float SERVO1_MIN = 36.0f;
constexpr float SERVO1_MAX = 70.0f;
constexpr float SERVO1_INIT = 56.0f;

constexpr float SERVO2_MIN = 9.0f;
constexpr float SERVO2_MAX = 121.0f;
constexpr float SERVO2_INIT = 30.0f;

const long NEMA_MIN_POS = -760;
const long NEMA_MAX_POS = -50;
const long NEMA_CENTER_POSITION = -350;

static_assert(SERVO1_MIN <= SERVO1_INIT && SERVO1_INIT <= SERVO1_MAX,
              "Eye initial angle must be within its travel limits.");
static_assert(SERVO2_MIN <= SERVO2_INIT && SERVO2_INIT <= SERVO2_MAX,
              "Roll initial angle must be within its travel limits.");
static_assert(NEMA_MIN_POS <= NEMA_CENTER_POSITION && NEMA_CENTER_POSITION <= NEMA_MAX_POS,
              "Head center position must be within its travel limits.");

const int SERVO_MIN_US = 500;
const int SERVO_MAX_US = 2400;

const int SERVO_PWM_HZ = 50;

const unsigned long SERVO_UPDATE_MS = 10;

// Maximum commanded speed in degrees/second.
const float EYE_MAX_SPEED = 60.0f;
const float HEAD_MAX_SPEED = 90.0f;

// Larger time scale or pulse interval slows head pitch movement.
const float NEMA_TIME_SCALE = 2.0f;

const unsigned long NEMA_MIN_STEP_INTERVAL_US = 2500;
const unsigned long NEMA_INIT_STEP_INTERVAL_US = 5000;

// Boot movement is open-loop, not sensor-based homing.
// During first calibration, use NEMA_BOOT_TRAVEL_STEPS = 0 and CALIBRATION_ON_BOOT = true.
const int NEMA_BOOT_TRAVEL_STEPS = 301;
const bool NEMA_INVERT_DIR = false;
const bool CALIBRATION_ON_BOOT = false;

const unsigned long IDLE_DELAY_MIN_MS = 4000;
const unsigned long IDLE_DELAY_MAX_MS = 9000;

float clampf(float value, float low, float high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

long clampl(long value, long low, long high) {
  if (value < low) return low;
  if (value > high) return high;
  return value;
}

float lerpf(float a, float b, float t) {
  return a + (b - a) * t;
}

bool timeReached(unsigned long now, unsigned long deadline) {
  return (int32_t)(now - deadline) >= 0;
}

unsigned long slowNemaDuration(unsigned long originalMs) {
  return (unsigned long)(
    originalMs * NEMA_TIME_SCALE + 0.5f
  );
}

int servoAngleToUs(float angle) {
  angle = clampf(angle, 0.0f, 180.0f);

  return (int)lroundf(
    SERVO_MIN_US +
    angle * (SERVO_MAX_US - SERVO_MIN_US) / 180.0f
  );
}

float servoEase(float t) {
  t = clampf(t, 0.0f, 1.0f);

  return t * t * (3.0f - 2.0f * t);
}

unsigned long safeServoDuration(
  float distance,
  unsigned long requestedMs,
  float maxSpeed
) {

  unsigned long minimumMs = (unsigned long)ceilf(
    1500.0f * fabsf(distance) / maxSpeed
  );

  if (requestedMs < 200) requestedMs = 200;
  if (requestedMs < minimumMs) requestedMs = minimumMs;

  return requestedMs;
}

Servo servo1;
Servo servo2;

float servo1Pos = SERVO1_INIT;
float servo2Pos = SERVO2_INIT;

float servo1Target = SERVO1_INIT;
float servo2Target = SERVO2_INIT;

float servo1StartPos = SERVO1_INIT;
float servo2StartPos = SERVO2_INIT;

bool servo1Active = false;
bool servo2Active = false;

bool nemaActive = false;

unsigned long servo1StartMs = 0;
unsigned long servo2StartMs = 0;

unsigned long servo1DurationMs = 1;
unsigned long servo2DurationMs = 1;

unsigned long lastServo1Move = 0;
unsigned long lastServo2Move = 0;

int lastServo1Us = -1;
int lastServo2Us = -1;

bool servo1To(float target, unsigned long durationMs) {
  
  if (servo1Active || nemaActive) return false;

  target = clampf(target, SERVO1_MIN, SERVO1_MAX);

  servo1StartPos = servo1Pos;
  servo1Target = target;

  if (fabsf(target - servo1Pos) < 0.001f) {
    servo1Pos = target;
    lastServo1Us = servoAngleToUs(target);
    servo1.writeMicroseconds(lastServo1Us);
    return true;
  }

  servo1DurationMs = safeServoDuration(
    target - servo1Pos,
    durationMs,
    EYE_MAX_SPEED
  );

  servo1StartMs = millis();
  lastServo1Move = servo1StartMs;
  servo1Active = true;

  return true;
}

bool servo2To(float target, unsigned long durationMs) {
  
  if (servo2Active || nemaActive) return false;

  target = clampf(target, SERVO2_MIN, SERVO2_MAX);

  servo2StartPos = servo2Pos;
  servo2Target = target;

  if (fabsf(target - servo2Pos) < 0.001f) {
    servo2Pos = target;
    lastServo2Us = servoAngleToUs(target);
    servo2.writeMicroseconds(lastServo2Us);
    return true;
  }

  servo2DurationMs = safeServoDuration(
    target - servo2Pos,
    durationMs,
    HEAD_MAX_SPEED
  );

  servo2StartMs = millis();
  lastServo2Move = servo2StartMs;
  servo2Active = true;

  return true;
}

void updateServo1() {
  if (!servo1Active) return;

  unsigned long now = millis();
  unsigned long elapsed = now - servo1StartMs;

  bool finished = elapsed >= servo1DurationMs;

  if (!finished && now - lastServo1Move < SERVO_UPDATE_MS) {
    return;
  }

  lastServo1Move = now;

  if (finished) {
    servo1Pos = servo1Target;
  } else {
    float t = (float)elapsed / servo1DurationMs;

    servo1Pos = lerpf(
      servo1StartPos,
      servo1Target,
      servoEase(t)
    );
  }

  int pulse = servoAngleToUs(servo1Pos);

  if (pulse != lastServo1Us) {
    servo1.writeMicroseconds(pulse);
    lastServo1Us = pulse;
  }

  if (finished) servo1Active = false;
}

void updateServo2() {
  if (!servo2Active) return;

  unsigned long now = millis();
  unsigned long elapsed = now - servo2StartMs;

  bool finished = elapsed >= servo2DurationMs;

  if (!finished && now - lastServo2Move < SERVO_UPDATE_MS) {
    return;
  }

  lastServo2Move = now;

  if (finished) {
    servo2Pos = servo2Target;
  } else {
    float t = (float)elapsed / servo2DurationMs;

    servo2Pos = lerpf(
      servo2StartPos,
      servo2Target,
      servoEase(t)
    );
  }

  int pulse = servoAngleToUs(servo2Pos);

  if (pulse != lastServo2Us) {
    servo2.writeMicroseconds(pulse);
    lastServo2Us = pulse;
  }

  if (finished) servo2Active = false;
}

long nemaPos = 0;
long nemaTarget = 0;

bool currentStepNemaPending = false;

unsigned long nemaStepDelayUs = NEMA_MIN_STEP_INTERVAL_US;
unsigned long lastNemaStepUs = 0;

void emitNemaPulse() {
  digitalWrite(PIN_STEP, HIGH);
  delayMicroseconds(3);
  digitalWrite(PIN_STEP, LOW);
}

bool nemaTo(long target, unsigned long durationMs) {

  if (nemaActive || servo1Active || servo2Active) return false;

  target = clampl(target, NEMA_MIN_POS, NEMA_MAX_POS);
  nemaTarget = target;

  long distance = labs(target - nemaPos);

  if (distance == 0) {
    nemaActive = false;
    return true;
  }

  durationMs = slowNemaDuration(durationMs);
  if (durationMs < 1) durationMs = 1;

  nemaStepDelayUs =
    durationMs * 1000UL / (unsigned long)distance;

  if (nemaStepDelayUs < NEMA_MIN_STEP_INTERVAL_US) {
    nemaStepDelayUs = NEMA_MIN_STEP_INTERVAL_US;
  }

  digitalWrite(PIN_DIR, (target > nemaPos) != NEMA_INVERT_DIR ? HIGH : LOW);
  delayMicroseconds(3);

  lastNemaStepUs = micros();
  nemaActive = true;

  return true;
}

// Software position is estimated; missed steps are not detected.
void updateNema() {
  if (!nemaActive) return;

  unsigned long now = micros();

  if (now - lastNemaStepUs < nemaStepDelayUs) return;
  lastNemaStepUs = now;

  if (nemaPos == nemaTarget) {
    nemaActive = false;
    return;
  }

  int direction = nemaTarget > nemaPos ? 1 : -1;
  long nextPosition = nemaPos + direction;

  if (nextPosition < NEMA_MIN_POS ||
      nextPosition > NEMA_MAX_POS) {
    nemaActive = false;
    return;
  }

  emitNemaPulse();
  nemaPos = nextPosition;

  if (nemaPos == nemaTarget) {
    nemaActive = false;
  }
}

void initNemaBlockingToCenter() {
  
  

  delay(300);

  digitalWrite(PIN_DIR, NEMA_INVERT_DIR ? HIGH : LOW);
  delayMicroseconds(3);

  for (int i = 0; i < NEMA_BOOT_TRAVEL_STEPS; ++i) {
    emitNemaPulse();
    delayMicroseconds(NEMA_INIT_STEP_INTERVAL_US);
    yield();
  }

  nemaPos = NEMA_CENTER_POSITION;
  nemaTarget = nemaPos;
  nemaActive = false;
}

bool allMotorsIdle() {
  return !servo1Active && !servo2Active &&
         !nemaActive && !currentStepNemaPending;
}

bool isWaitSatisfied(WaitMode mode) {
  switch (mode) {
    case WAIT_NONE:
      return true;

    case WAIT_ALL:
      return allMotorsIdle();

    case WAIT_SERVOS:
      return !servo1Active && !servo2Active;

    case WAIT_SERVO1_NEMA:
      return !servo1Active &&
             !nemaActive && !currentStepNemaPending;

    case WAIT_SERVO1:
      return !servo1Active;

    case WAIT_SERVO2:
      return !servo2Active;

    case WAIT_NEMA:
      return !nemaActive && !currentStepNemaPending;

    default:
      return true;
  }
}

Adafruit_GC9A01A tft(TFT_CS, TFT_DC, TFT_RST);

const int PIXEL_SIZE = 12;
const int PIXEL_GAP = 1;
const int GRID_W = 20;
const int GRID_H = 20;

uint16_t colors[5];

bool visibleMap[GRID_H][GRID_W];
float baseMap[GRID_H][GRID_W];
float centerMap[GRID_H][GRID_W];
float ringMap[GRID_H][GRID_W];
uint8_t shownLevel[GRID_H][GRID_W];

bool eyeActive = false;
unsigned long eyeEndMs = 0;
unsigned long nextPulseAt = 0;

float talkAmount = 0.0f;
float pulseCurrent = 0.0f;
float pulseTarget = 0.0f;

void eyePlay(unsigned long durationMs) {
  eyeActive = true;

  eyeEndMs = millis() + durationMs * 2UL;
}

void buildEyeMaps() {
  for (int gy = 0; gy < GRID_H; ++gy) {
    for (int gx = 0; gx < GRID_W; ++gx) {
      float px = (gx + 0.5f) * PIXEL_SIZE - 120.0f;
      float py = (gy + 0.5f) * PIXEL_SIZE - 120.0f;

      visibleMap[gy][gx] =
        px * px + py * py < 118.0f * 118.0f;

      shownLevel[gy][gx] = 255;

      float dx = gx + 0.5f - GRID_W * 0.5f;
      float dy = gy + 0.5f - GRID_H * 0.5f;
      float radius = sqrtf(dx * dx + dy * dy);

      float radial = radius / 8.5f;
      float core = 1.0f - clampf(radial, 0.0f, 1.0f);

      float ring = clampf(
        1.0f - fabsf(radial - 0.45f) * 2.4f,
        0.0f,
        1.0f
      );

      baseMap[gy][gx] = clampf(
        0.40f + (0.75f * core + 0.25f * ring) * 0.60f,
        0.0f,
        1.0f
      );

      float c =
        1.0f - clampf(radius / 12.0f, 0.0f, 1.0f);

      centerMap[gy][gx] = c * c * c * c;

      ringMap[gy][gx] = clampf(
        1.0f - fabsf(radius / 1.8f - 2.4f) / 1.2f,
        0.0f,
        1.0f
      );
    }
  }
}

void renderEye(bool forceRedraw) {
  tft.startWrite();

  for (int gy = 0; gy < GRID_H; ++gy) {
    for (int gx = 0; gx < GRID_W; ++gx) {
      
      updateNema();

      if (!visibleMap[gy][gx]) continue;

      float value = baseMap[gy][gx];
      value += centerMap[gy][gx] * talkAmount * 0.12f;
      value += centerMap[gy][gx] * pulseCurrent * 0.34f;
      value += ringMap[gy][gx] * pulseCurrent * 0.16f;

      value = clampf(value, 0.0f, 1.0f);

      uint8_t level =
        value < 0.30f ? 1 :
        value < 0.55f ? 2 :
        value < 0.80f ? 3 : 4;

      if (forceRedraw || level != shownLevel[gy][gx]) {
        tft.writeFillRect(
          gx * PIXEL_SIZE,
          gy * PIXEL_SIZE,
          PIXEL_SIZE - PIXEL_GAP,
          PIXEL_SIZE - PIXEL_GAP,
          colors[level]
        );

        shownLevel[gy][gx] = level;
      }
    }

    updateServo1();
    updateServo2();
  }

  tft.endWrite();
}

void updateEye() {
  unsigned long now = millis();

  if (eyeActive && timeReached(now, eyeEndMs)) {
    eyeActive = false;
  }

  if (eyeActive) {
    talkAmount = lerpf(talkAmount, 1.0f, 0.16f);

    if (timeReached(now, nextPulseAt)) {
      int r = random(100);

      if (r < 20) {
        pulseTarget = 0.15f + random(20) / 100.0f;
      } else if (r < 80) {
        pulseTarget = 0.35f + random(35) / 100.0f;
      } else {
        pulseTarget = 0.70f + random(25) / 100.0f;
      }

      nextPulseAt = now + random(70, 160);
    }

    pulseCurrent = lerpf(pulseCurrent, pulseTarget, 0.28f);
    pulseTarget *= 0.90f;
  } else {
    talkAmount = lerpf(talkAmount, 0.0f, 0.10f);
    pulseTarget = 0.0f;
    pulseCurrent = lerpf(pulseCurrent, 0.0f, 0.18f);
  }

  talkAmount = clampf(talkAmount, 0.0f, 1.0f);
  pulseCurrent = clampf(pulseCurrent, 0.0f, 1.0f);
  pulseTarget = clampf(pulseTarget, 0.0f, 1.0f);
}

bool blinking = false;
bool ledState = true;

int blinkCount = 0;
int blinkTarget = 0;

unsigned long lastBlinkChange = 0;
unsigned long currentBlinkInterval = 0;
unsigned long nextBlinkTime = 0;

void updateLedBlink() {
  unsigned long now = millis();

  if (!blinking && timeReached(now, nextBlinkTime)) {
    blinking = true;
    blinkTarget = random(1, 4);
    blinkCount = 0;
    ledState = false;

    digitalWrite(PIN_LED, LOW);

    lastBlinkChange = now;
    currentBlinkInterval = random(80, 301);
  }

  if (blinking &&
      now - lastBlinkChange >= currentBlinkInterval) {
    lastBlinkChange = now;
    ledState = !ledState;

    digitalWrite(PIN_LED, ledState ? HIGH : LOW);
    currentBlinkInterval = random(80, 301);

    if (ledState && ++blinkCount >= blinkTarget) {
      blinking = false;
      ledState = true;

      digitalWrite(PIN_LED, HIGH);
      nextBlinkTime = now + random(10000, 30001);
    }
  }
}

// AnimStep fields:
// useEye, eyeDegrees, eyeMs, useRoll, rollDegrees, rollMs,
// useHead, headSteps, headMs, waitMode, screenEffectMs.
// Mixed servo/stepper steps run servos first, then the stepper.
const AnimStep ANIM_1_STEPS[] = {
  {true,45,600, true,90,600, true,-500,800, WAIT_ALL,900},
  {true,68,500, true,20,500, false,0,0, WAIT_SERVOS,0},
  {true,58,400, true,56,400, true,-350,500, WAIT_ALL,0}
};

const AnimStep ANIM_2_STEPS[] = {
  {true,40,700, false,0,0, true,-500,500, WAIT_SERVO1_NEMA,800},
  {true,65,700, false,0,0, true,-200,800, WAIT_SERVO1_NEMA,0},
  {true,65,700, false,0,0, true,-350,400, WAIT_ALL,0}
};

const AnimStep ANIM_3_STEPS[] = {
  {false,0,0, true,90,800, true,-600,400, WAIT_ALL,450},
  {true,68,500, false,0,0, false,0,0, WAIT_SERVO1,0},
  {true,58,400, true,56,400, true,-350,400, WAIT_ALL,0}
};

const AnimStep ANIM_4_STEPS[] = {
  {true,60,100, false,90,800, true,-400,400, WAIT_ALL,400},
  {true,40,200, false,0,0, true,-300,400, WAIT_ALL,400},
  {true,58,100, false,56,400, true,-350,400, WAIT_ALL,0}
};

const AnimStep ANIM_5_STEPS[] = {
  {false,60,100, true,90,400, true,-450,400, WAIT_ALL,450},
  {false,40,200, true,20,800, false,0,0, WAIT_ALL,855},
  {false,58,100, true,56,400, true,-350,250, WAIT_ALL,0}
};

const AnimStep ANIM_6_STEPS[] = {
  {false,60,100, true,12,400, true,-550,400, WAIT_ALL,450},
  {true,40,500, false,20,800, true,-350,500, WAIT_ALL,855},
  {true,58,100, true,56,400, true,-350,250, WAIT_ALL,0}
};

#define ANIM_ENTRY(label, array) \
  {label, array, (uint8_t)(sizeof(array) / sizeof(array[0]))}

const AnimationDef ANIMATIONS[] = {
  ANIM_ENTRY("speak1", ANIM_1_STEPS),
  ANIM_ENTRY("speak2", ANIM_2_STEPS),
  ANIM_ENTRY("speak3", ANIM_3_STEPS),
  ANIM_ENTRY("speak4", ANIM_4_STEPS),
  ANIM_ENTRY("speak5", ANIM_5_STEPS),
  ANIM_ENTRY("speak6", ANIM_6_STEPS)
};

#undef ANIM_ENTRY

const uint8_t ANIMATION_COUNT =
  sizeof(ANIMATIONS) / sizeof(ANIMATIONS[0]);

const uint8_t ANIM_QUEUE_SIZE = 10;

uint8_t animQueue[ANIM_QUEUE_SIZE];
uint8_t animQueueHead = 0;
uint8_t animQueueTail = 0;
uint8_t animQueueCount = 0;

bool animRunning = false;
uint8_t currentAnimId = 255;
uint8_t currentStepIndex = 0;
bool currentStepStarted = false;

bool calibrationMode = CALIBRATION_ON_BOOT;

bool homeRequested = false;
bool homeActive = false;

enum HomePhase : uint8_t {
  HOME_PHASE_IDLE,
  HOME_PHASE_SERVOS,
  HOME_PHASE_NEMA
};

HomePhase homePhase = HOME_PHASE_IDLE;

uint8_t idlePhase = 0;
uint8_t idleAxis = 0;
unsigned long nextIdleAt = 0;

float idleInitialServo1 = SERVO1_INIT;
float idleInitialServo2 = SERVO2_INIT;
long idleInitialNema = NEMA_CENTER_POSITION;

bool speechSessionActive = false;
unsigned long speechHeartbeatAt = 0;
const unsigned long SPEECH_WATCHDOG_MS = 6500;

void cancelIdleForSpeech() {
  
  if (idlePhase != 0 && !animRunning && !homeActive) {
    updateServo1();
    updateServo2();
    
    servo1Active = false;
    servo2Active = false;
    nemaActive = false;
    servo1Target = servo1Pos;
    servo2Target = servo2Pos;
    nemaTarget = nemaPos;
    digitalWrite(PIN_STEP, LOW);
  }
  idlePhase = 0;
  nextIdleAt = 0;
}

void beginSpeechSession() {
  speechSessionActive = true;
  speechHeartbeatAt = millis();
  cancelIdleForSpeech();
}

void endSpeechSession() {
  speechSessionActive = false;
  
  nextIdleAt = 0;
}

bool enqueueAnimation(uint8_t id) {
  if (id >= ANIMATION_COUNT ||
      animQueueCount >= ANIM_QUEUE_SIZE) {
    return false;
  }

  animQueue[animQueueTail] = id;
  animQueueTail = (animQueueTail + 1) % ANIM_QUEUE_SIZE;
  ++animQueueCount;

  return true;
}

void clearAnimationQueue() {
  animQueueHead = 0;
  animQueueTail = 0;
  animQueueCount = 0;
}

float variedServoTarget(
  float current,
  float requested,
  float minimum,
  float maximum
) {
  
  float factor = random(85, 116) / 100.0f;

  return clampf(
    current + (requested - current) * factor,
    minimum,
    maximum
  );
}

unsigned long variedServoDuration(unsigned long originalMs) {

  return (unsigned long)(
    originalMs * (random(90, 126) / 100.0f)
  );
}

bool startAnimStep(const AnimStep& step) {
  bool usesServo = step.useServo1 || step.useServo2;

  if (step.useServo1 && servo1Active) return false;
  if (step.useServo2 && servo2Active) return false;
  if (step.useNema && nemaActive) return false;

  if (step.useNema && (servo1Active || servo2Active)) {
    return false;
  }

  if (usesServo && nemaActive) return false;

  currentStepNemaPending = false;

  bool finalStep =
    currentStepIndex + 1 >= ANIMATIONS[currentAnimId].stepCount;

  if (step.eyeTimeMs) {
    eyePlay(step.eyeTimeMs);
  }

  if (step.useServo1) {
    float target = step.servo1Pos;

    if (!finalStep) {
      target = variedServoTarget(
        servo1Pos,
        target,
        SERVO1_MIN,
        SERVO1_MAX
      );
    }

    servo1To(
      target,
      variedServoDuration(step.servo1TimeMs)
    );
  }

  if (step.useServo2) {
    float target = step.servo2Pos;

    if (!finalStep) {
      target = variedServoTarget(
        servo2Pos,
        target,
        SERVO2_MIN,
        SERVO2_MAX
      );
    }

    servo2To(
      target,
      variedServoDuration(step.servo2TimeMs)
    );
  }

  if (step.useNema) {
    if (usesServo) {
      currentStepNemaPending = true;
    } else if (!nemaTo(step.nemaPos, step.nemaTimeMs)) {
      return false;
    }
  }

  return true;
}

void updateAnimationEngine() {
  if (homeActive) return;

  if (!animRunning) {
    if (homeRequested ||
        !animQueueCount ||
        !allMotorsIdle()) {
      return;
    }

    currentAnimId = animQueue[animQueueHead];
    animQueueHead = (animQueueHead + 1) % ANIM_QUEUE_SIZE;
    --animQueueCount;

    currentStepIndex = 0;
    currentStepStarted = false;
    currentStepNemaPending = false;
    animRunning = true;

    Serial.println("ANIM_START");
  }

  const AnimationDef& animation = ANIMATIONS[currentAnimId];

  if (currentStepIndex < animation.stepCount) {
    const AnimStep& step = animation.steps[currentStepIndex];

    if (!currentStepStarted) {
      if (!startAnimStep(step)) return;
      currentStepStarted = true;
    }

    if (currentStepNemaPending) {
      if (servo1Active || servo2Active) return;

      if (!nemaTo(step.nemaPos, step.nemaTimeMs)) return;
      currentStepNemaPending = false;
    }

    if (step.useNema && nemaActive) return;

    if (!isWaitSatisfied(step.waitMode)) return;

    ++currentStepIndex;
    currentStepStarted = false;
    currentStepNemaPending = false;
  }

  if (currentStepIndex >= animation.stepCount &&
      allMotorsIdle()) {
    animRunning = false;
    currentAnimId = 255;
    currentStepIndex = 0;
    currentStepStarted = false;
    currentStepNemaPending = false;

    Serial.println("ANIM_DONE");
  }
}

void requestHome() {
  clearAnimationQueue();

  if (homeActive) {
    Serial.println("INIT_RUNNING");
    return;
  }

  homeRequested = true;
  Serial.println("INIT_QUEUED");
}

void updateHome() {
  if (homeActive) {

    if (homePhase == HOME_PHASE_SERVOS) {
      if (servo1Active || servo2Active) return;

      if (!nemaTo(NEMA_CENTER_POSITION, 1000)) return;
      homePhase = HOME_PHASE_NEMA;
      return;
    }

    if (homePhase == HOME_PHASE_NEMA) {
      if (nemaActive) return;

      homePhase = HOME_PHASE_IDLE;
      homeActive = false;
      Serial.println("INIT_DONE");
    }

    return;
  }

  if (!homeRequested ||
      animRunning ||
      !allMotorsIdle()) {
    return;
  }

  homeRequested = false;
  homeActive = true;
  homePhase = HOME_PHASE_SERVOS;
  eyeActive = false;

  servo1To(SERVO1_INIT, 1000);
  servo2To(SERVO2_INIT, 1000);

  Serial.println("INIT_START");
}

void updateIdleMovements() {
  if (calibrationMode) return;
  static unsigned long idleDurationMs = 600;
  static unsigned long pauseEndMs = 0;

  if (speechSessionActive &&
      (unsigned long)(millis() - speechHeartbeatAt) >= SPEECH_WATCHDOG_MS) {
    endSpeechSession();
    Serial.println("SPEECH_TIMEOUT");
  }

  if (speechSessionActive || animRunning || animQueueCount > 0 ||
      homeRequested || homeActive) {
    cancelIdleForSpeech();
    pauseEndMs = 0;
    return;
  }

  if (!allMotorsIdle()) return;

  unsigned long now = millis();

  if (idlePhase == 1) {
    pauseEndMs = now + (unsigned long)random(500, 3001);
    idlePhase = 3;
    return;
  }

  if (idlePhase == 3) {
    if (!timeReached(now, pauseEndMs)) return;

    if (idleAxis == 0) {
      servo1To(idleInitialServo1, idleDurationMs);
    } else if (idleAxis == 1) {
      servo2To(idleInitialServo2, idleDurationMs);
    } else {
      nemaTo(idleInitialNema, idleDurationMs);
    }

    idlePhase = 2;
    return;
  }

  if (idlePhase == 2) {
    idlePhase = 0;
    nextIdleAt = 0;
  }

  if (nextIdleAt == 0) {
    nextIdleAt = now + (unsigned long)random(
      IDLE_DELAY_MIN_MS,
      IDLE_DELAY_MAX_MS + 1
    );
    return;
  }

  if (!timeReached(now, nextIdleAt)) return;

  idleInitialServo1 = servo1Pos;
  idleInitialServo2 = servo2Pos;
  idleInitialNema = nemaPos;

  idleAxis = random(0, 3);
  int direction = random(0, 2) ? 1 : -1;

  if (idleAxis == 0) {
    
    // Always use a positive distance when deriving an unsigned duration.
    float amplitude = random(20, 51) / 10.0f;
    float target = servo1Pos + direction * amplitude;

    if (target < SERVO1_MIN || target > SERVO1_MAX) {
      target = servo1Pos - direction * amplitude;
    }

    float speed = random(18, 36);

    idleDurationMs = (unsigned long)ceilf(
      1500.0f * amplitude / speed
    );

    unsigned long minimumDuration = random(350, 651);

    if (idleDurationMs < minimumDuration) {
      idleDurationMs = minimumDuration;
    }

    servo1To(target, idleDurationMs);

  } else if (idleAxis == 1) {
    
    float amplitude = random(200, 500) / 10.0f;
    float target = servo2Pos + direction * amplitude;

    if (target < SERVO2_MIN || target > SERVO2_MAX) {
      target = servo2Pos - direction * amplitude;
    }

    float speed = random(20, 46);

    idleDurationMs = (unsigned long)ceilf(
      1500.0f * amplitude / speed
    );

    unsigned long minimumDuration = random(420, 801);

    if (idleDurationMs < minimumDuration) {
      idleDurationMs = minimumDuration;
    }

    servo2To(target, idleDurationMs);

  } else {
    
    long amplitude = random(20, 50);
    long target = nemaPos + direction * amplitude;

    if (target < NEMA_MIN_POS || target > NEMA_MAX_POS) {
      target = nemaPos - direction * amplitude;
    }

    idleDurationMs = random(600, 901);

    nemaTo(target, idleDurationMs);
  }

  idlePhase = 1;
}

String serialLine;
bool serialOverflow = false;

void printStatus() {
  Serial.print("RUNNING=");
  Serial.print(animRunning ? "YES" : "NO");

  Serial.print(" QUEUE=");
  Serial.print(animQueueCount);

  Serial.print(" INIT=");
  Serial.println(
    homeActive ? "RUNNING" :
    homeRequested ? "QUEUED" : "NO"
  );

  Serial.print("ESTIMATED S1=");
  Serial.print(servo1Pos, 2);
  Serial.print(" TARGET=");
  Serial.print(servo1Target, 2);
  Serial.print(" MOVING=");
  Serial.println(servo1Active ? "YES" : "NO");

  Serial.print("ESTIMATED S2=");
  Serial.print(servo2Pos, 2);
  Serial.print(" TARGET=");
  Serial.print(servo2Target, 2);
  Serial.print(" MOVING=");
  Serial.println(servo2Active ? "YES" : "NO");

  Serial.print("ESTIMATED NEMA=");
  Serial.print(nemaPos);
  Serial.print(" TARGET=");
  Serial.print(nemaTarget);
  Serial.print(" MOVING=");
  Serial.println(nemaActive ? "YES" : "NO");
}

void processNumber(int number) {
  if (calibrationMode) {
    Serial.println("ERROR:CALIBRATION_ACTIVE");
    return;
  }
  if (number == 0) {
    requestHome();
    return;
  }

  if (number < 1 || number > ANIMATION_COUNT) {
    Serial.print("UNKNOWN_ANIM:");
    Serial.println(number);
    return;
  }

  if (homeRequested || homeActive) {
    Serial.println("BUSY");
    return;
  }

  uint8_t id = number - 1;

  if (enqueueAnimation(id)) {
    cancelIdleForSpeech();
    Serial.print("QUEUED:");
    Serial.println(ANIMATIONS[id].name);
  } else {
    Serial.println("BUSY");
    Serial.println("QUEUE_FULL");
  }
}

// Calibration requests preserve the servo/stepper movement interlock.
// CAL ON stops idle activity after the current commanded move settles.
// Direct target requests are accepted only in calibration mode with every axis idle.
void processCalibrationLine(String line) {
  if (line == "CAL ON") {
    if (animRunning || homeActive || homeRequested) {
      Serial.println("BUSY");
      return;
    }
    clearAnimationQueue();
    calibrationMode = true;
    idlePhase = 0;
    Serial.println("CALIBRATION_ON");
    return;
  }
  if (line == "CAL OFF") {
    if (!allMotorsIdle()) {
      Serial.println("BUSY");
      return;
    }
    calibrationMode = false;
    nextIdleAt = millis() + IDLE_DELAY_MAX_MS;
    Serial.println("CALIBRATION_OFF");
    return;
  }
  if (!calibrationMode) {
    Serial.println("ERROR:ENABLE_CALIBRATION_FIRST");
    return;
  }
  if (!allMotorsIdle() || animRunning || homeActive || homeRequested) {
    Serial.println("BUSY");
    return;
  }
  int separator = line.indexOf(' ');
  if (separator < 0) {
    Serial.println("ERROR:TARGET_REQUIRED");
    return;
  }
  String axis = line.substring(0, separator);
  String text = line.substring(separator + 1);
  text.trim();
  char* end = nullptr;
  double value = strtod(text.c_str(), &end);
  if (!text.length() || end == text.c_str() || *end != '\0' || !isfinite(value)) {
    Serial.println("ERROR:INVALID_TARGET");
    return;
  }
  bool accepted = false;
  if (axis == "EYE" && value >= SERVO1_MIN && value <= SERVO1_MAX) {
    accepted = servo1To((float)value, 1200);
  } else if (axis == "ROLL" && value >= SERVO2_MIN && value <= SERVO2_MAX) {
    accepted = servo2To((float)value, 1200);
  } else if (axis == "HEAD" && value >= NEMA_MIN_POS && value <= NEMA_MAX_POS && floor(value) == value) {
    accepted = nemaTo((long)value, 1500);
  }
  Serial.println(accepted ? "CAL_TARGET_ACCEPTED" : "ERROR:TARGET_OUT_OF_RANGE");
}

void processSerialLine(String line) {
  line.trim();

  if (!line.length()) return;
  if (line == "CAL ON" || line == "CAL OFF" ||
      line.startsWith("EYE") || line.startsWith("ROLL") || line.startsWith("HEAD")) {
    processCalibrationLine(line);
    return;
  }

  if (line == "SPEECH_START" || line == "SPEECH_KEEPALIVE") {
    beginSpeechSession();
    return;
  }
  if (line == "SPEECH_END") {
    endSpeechSession();
    return;
  }

  if (line == "PING") {
    Serial.println("ESP32_MOTORS_OK");
    return;
  }

  if (line == "STATUS") {
    printStatus();
    return;
  }

  if (line == "CLEAR") {
    clearAnimationQueue();
    Serial.println("QUEUE_CLEARED");
    return;
  }

  if (line == "LIST") {
    Serial.println("0 -> return to initial pose");

    for (uint8_t i = 0; i < ANIMATION_COUNT; ++i) {
      Serial.print(i + 1);
      Serial.print(" -> ");
      Serial.println(ANIMATIONS[i].name);
    }

    return;
  }

  bool numeric = true;

  for (unsigned int i = 0; i < line.length(); ++i) {
    if (!isDigit(line[i])) {
      numeric = false;
      break;
    }
  }

  if (numeric) {
    processNumber(line.toInt());
  } else {
    Serial.print("UNKNOWN:");
    Serial.println(line);
  }
}

void handleSerialCommands() {
  
  for (uint8_t n = 0;
       n < 32 && Serial.available() > 0;
       ++n) {
    updateNema();

    char c = (char)Serial.read();

    if (c == '\r' || c == '\n') {
      if (!serialOverflow) {
        processSerialLine(serialLine);
      }

      serialLine = "";
      serialOverflow = false;
      continue;
    }

    if (serialOverflow) continue;

    if (!serialLine.length() && c >= '0' && c <= '9') {
      processNumber(c - '0');
      continue;
    }

    if (serialLine.length() >= 64) {
      serialLine = "";
      serialOverflow = true;
      Serial.println("ERROR:SERIAL_LINE_TOO_LONG");
      continue;
    }

    serialLine += c;
  }
}

void setup() {
  Serial.begin(115200);

  delay(300);

  serialLine.reserve(65);
  randomSeed(micros());

  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);
  nextBlinkTime = millis() + random(10000, 30001);

  pinMode(PIN_STEP, OUTPUT);
  pinMode(PIN_DIR, OUTPUT);

  digitalWrite(PIN_STEP, LOW);
  digitalWrite(PIN_DIR, LOW);

  servo1.setPeriodHertz(SERVO_PWM_HZ);
  servo2.setPeriodHertz(SERVO_PWM_HZ);

  servo1.attach(PIN_SERVO_1, SERVO_MIN_US, SERVO_MAX_US);
  servo2.attach(PIN_SERVO_2, SERVO_MIN_US, SERVO_MAX_US);

  servo1Pos = servo1Target = SERVO1_INIT;
  servo2Pos = servo2Target = SERVO2_INIT;

  servo1StartPos = servo1Pos;
  servo2StartPos = servo2Pos;

  lastServo1Us = servoAngleToUs(servo1Pos);
  lastServo2Us = servoAngleToUs(servo2Pos);

  servo1.writeMicroseconds(lastServo1Us);
  servo2.writeMicroseconds(lastServo2Us);

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.begin();
  tft.setRotation(0);

  colors[0] = GC9A01A_BLACK;
  colors[1] = tft.color565(180, 150, 0);
  colors[2] = tft.color565(235, 205, 0);
  colors[3] = tft.color565(255, 230, 30);
  colors[4] = tft.color565(255, 255, 170);

  tft.fillScreen(colors[0]);

  buildEyeMaps();
  renderEye(true);

  // Count pulses from a known physical start position; there is no limit switch.
  initNemaBlockingToCenter();

  Serial.println("ESP32_MOTORS_BOOT_OK");
  Serial.println("CAL ON / EYE <degrees> / ROLL <degrees> / HEAD <steps> / CAL OFF");
  Serial.println("Ready.");
  Serial.println("0 : return to initial pose");
  Serial.println("1..6 : animations");
  Serial.println("PING / LIST / STATUS / CLEAR");
  Serial.println("SPEECH_START / SPEECH_KEEPALIVE / SPEECH_END");
}

void loop() {
  updateNema();
  handleSerialCommands();

  updateServo1();
  updateServo2();
  updateNema();

  updateIdleMovements();

  updateAnimationEngine();
  updateHome();

  updateLedBlink();

  static unsigned long lastFrame = 0;
  unsigned long now = millis();

  if (now - lastFrame >= 28) {
    lastFrame = now;
    updateEye();
    renderEye(false);
  }

  updateNema();
  yield();
}
