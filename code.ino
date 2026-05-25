/*
============================================================================
ESP32-C3 AIR MOUSE PRO V2 (LOW LATENCY + STABLE + FAST BUTTONS)
============================================================================

IMPROVEMENTS:
✓ Much lower lag
✓ More stable when still
✓ Faster cursor
✓ Better air-mouse feel
✓ Responsive buttons
✓ Button debounce fixed
✓ Better smoothing logic
✓ Reduced delay()
✓ Faster update loop
✓ Motion acceleration
✓ Idle stabilization
✓ Serial debug prints

BOARD:
ESP32-C3 Super Mini

MPU6050:
SDA -> GPIO6
SCL -> GPIO7

BUTTONS:
LEFT  -> GPIO4 -> GND
RIGHT -> GPIO5 -> GND

============================================================================
*/

#include <Wire.h>
#include <BleMouse.h>
#include <math.h>

// ============================================================================
// PINS
// ============================================================================

#define SDA_PIN      6
#define SCL_PIN      7

#define BTN_LEFT     4
#define BTN_RIGHT    5

#define MPU_ADDR     0x68

// ============================================================================
// BLE
// ============================================================================

BleMouse bleMouse("AirMouse-C3", "ESP32", 100);

// ============================================================================
/* TUNING
FASTER:
sensitivity = 900

MORE STABLE:
alphaStill = 0.95

LESS LAG:
alphaMove = 0.25

MORE SPEED:
speedBoost = 2.2

LESS JITTER:
deadzone = 2.0

*/// ============================================================================

// LOWER = FASTER
float sensitivity = 900;

// stability
float alphaStill = 0.92;
float alphaMove  = 0.45;

// deadzone
float deadzone = 2.5;

// gyro assist
float gyroAssist = 0.18;

// speed multiplier
float speedBoost = 1.5;

// max cursor speed
int maxSpeed = 45;

// idle stabilization
float stillThreshold = 4.0;

// ============================================================================
// VARIABLES
// ============================================================================

float smoothX = 0;
float smoothY = 0;

float centerAX = 0;
float centerAY = 0;

bool prevLeft = HIGH;
bool prevRight = HIGH;

unsigned long lastLeftClick = 0;
unsigned long lastRightClick = 0;

// ============================================================================
// MPU INIT
// ============================================================================

void initMPU() {

  Wire.beginTransmission(MPU_ADDR);

  if (Wire.endTransmission() != 0) {

    Serial.println("MPU6050 NOT FOUND");

    while (1) {
      delay(100);
    }
  }

  // wake
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // gyro ±250
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // accel ±2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // DLPF
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A);
  Wire.write(0x03);
  Wire.endTransmission(true);

  Serial.println("MPU READY");
}

// ============================================================================
// READ MPU
// ============================================================================

void readMPU(
  int16_t &ax,
  int16_t &ay,
  int16_t &az,
  int16_t &gx,
  int16_t &gy,
  int16_t &gz
) {

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);

  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14, (uint8_t)true);

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();

  Wire.read();
  Wire.read();

  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
  gz = (Wire.read() << 8) | Wire.read();
}

// ============================================================================
// CALIBRATION
// ============================================================================

void calibrateMPU() {

  Serial.println("KEEP DEVICE STILL");

  long sumAX = 0;
  long sumAY = 0;

  for (int i = 0; i < 300; i++) {

    int16_t ax, ay, az, gx, gy, gz;

    readMPU(ax, ay, az, gx, gy, gz);

    sumAX += ax;
    sumAY += ay;

    delay(4);
  }

  centerAX = sumAX / 300.0;
  centerAY = sumAY / 300.0;

  Serial.println("CALIBRATION DONE");
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println("\nAIR MOUSE PRO V2");

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  // FAST I2C
  Wire.setClock(400000);

  initMPU();

  calibrateMPU();

  bleMouse.begin();

  Serial.println("BLE STARTED");
  Serial.println("SEARCH: AirMouse-C3");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {

  if (bleMouse.isConnected()) {

    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    readMPU(ax, ay, az, gx, gy, gz);

    // ============================================================
    // ACCEL
    // ============================================================

    float accelX = (ax - centerAX) / sensitivity;
    float accelY = (ay - centerAY) / sensitivity;

    // ============================================================
    // GYRO
    // ============================================================

    float gyroX = gx / 131.0;
    float gyroY = gy / 131.0;

    // ============================================================
    // HYBRID MOTION
    // ============================================================

    float rawX =
      (-accelX) +
      ((gyroY * gyroAssist) / 35.0);

    float rawY =
      (-accelY) +
      ((-gyroX * gyroAssist) / 35.0);

    // ============================================================
    // MOTION MAGNITUDE
    // ============================================================

    float motion =
      sqrt((gyroX * gyroX) + (gyroY * gyroY));

    // ============================================================
    // ADAPTIVE SMOOTHING
    // ============================================================

    float alpha;

    if (motion < stillThreshold) {

      // VERY STABLE WHEN STILL
      alpha = alphaStill;

    } else {

      // VERY FAST WHEN MOVING
      alpha = alphaMove;
    }

    smoothX =
      (alpha * smoothX) +
      ((1.0 - alpha) * rawX);

    smoothY =
      (alpha * smoothY) +
      ((1.0 - alpha) * rawY);

    // ============================================================
    // OUTPUT
    // ============================================================

    int moveX = smoothX * speedBoost;
    int moveY = smoothY * speedBoost;

    // deadzone
    if (abs(moveX) < deadzone) moveX = 0;
    if (abs(moveY) < deadzone) moveY = 0;

    // limit
    moveX = constrain(moveX, -maxSpeed, maxSpeed);
    moveY = constrain(moveY, -maxSpeed, maxSpeed);

    // ============================================================
    // SEND MOVEMENT
    // ============================================================

    if (moveX != 0 || moveY != 0) {

      bleMouse.move(moveX, moveY);

      Serial.print("MOVE X:");
      Serial.print(moveX);

      Serial.print(" Y:");
      Serial.print(moveY);

      Serial.print(" G:");
      Serial.println(motion);
    }

    // ============================================================
// BUTTONS
// ============================================================

bool leftNow = digitalRead(BTN_LEFT);
bool rightNow = digitalRead(BTN_RIGHT);

// ============================================================
// LEFT BUTTON
// SINGLE LEFT CLICK
// ============================================================

if (prevLeft == HIGH && leftNow == LOW) {

  if (millis() - lastLeftClick > 120) {

    bleMouse.press(MOUSE_LEFT);

    delay(25);

    bleMouse.release(MOUSE_LEFT);

    Serial.println("LEFT CLICK OK");

    lastLeftClick = millis();
  }
}

// ============================================================
// RIGHT BUTTON
// ALSO SEND LEFT CLICK FOR TV/APPS
// ============================================================

if (prevRight == HIGH && rightNow == LOW) {

  if (millis() - lastRightClick > 120) {

    // REAL RIGHT CLICK
    bleMouse.press(MOUSE_RIGHT);

    delay(25);

    bleMouse.release(MOUSE_RIGHT);

    // EXTRA LEFT CLICK FOR TV/ANDROID
    delay(10);

    bleMouse.press(MOUSE_LEFT);

    delay(20);

    bleMouse.release(MOUSE_LEFT);

    Serial.println("RIGHT BUTTON ACTION");

    lastRightClick = millis();
  }
}

prevLeft = leftNow;
prevRight = rightNow;
  }

  // VERY LOW LATENCY LOOP
  delay(2);
}

/*
============================================================================
BEST SETTINGS
============================================================================

FASTER:
sensitivity = 900

MORE STABLE:
alphaStill = 0.95

LESS LAG:
alphaMove = 0.25

MORE SPEED:
speedBoost = 2.2

LESS JITTER:
deadzone = 2.0

============================================================================
*/
