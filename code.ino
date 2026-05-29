/*
============================================================================
AIR MOUSE PRO V7 HYBRID
GYRO PRECISION + TILT ASSIST + REAL CORNER REACH
============================================================================

THIS VERSION FIXES:

✓ Real diagonal movement
✓ Easy corner reaching
✓ Tiny icon precision
✓ Smooth floating cursor
✓ Stable idle
✓ Continuous movement when tilted
✓ Fast traversal across screen
✓ Gyro precision retained
✓ Commercial air mouse feel
✓ Android + PC usability improved

ARCHITECTURE:

SMALL MOVEMENTS:
  Gyroscope controls cursor precisely

LARGE TILT:
  Accelerometer adds continuous directional force

RESULT:
  Precision + easy traversal together

BOARD:
ESP32-C3 Super Mini

MPU6050:
SDA -> GPIO6
SCL -> GPIO7

BUTTONS:
GPIO4 -> LEFT CLICK
GPIO5 -> RIGHT CLICK

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
// HYBRID TUNING
// ============================================================================

// ----------------------------
// GYRO CONTROL
// ----------------------------

// LOWER = FASTER
float gyroScale = 24.0;

// ----------------------------
// TILT ASSIST
// IMPORTANT PART
// ----------------------------

// Strength of continuous movement
float tiltAssist = 2.4;

// Nonlinear curve
float tiltCurve = 1.8;

// Minimum tilt before assist starts
float tiltDeadzone = 0.12;

// ----------------------------
// STABILITY
// ----------------------------

float alphaStill = 0.96;
float alphaMove  = 0.22;

float damping = 0.86;

float deadzone = 0.08;

float stillThreshold = 1.5;

// ----------------------------
// MICRO MOVEMENTS
// ----------------------------

float microBoost = 1.15;

// ----------------------------
// DYNAMIC ACCELERATION
// ----------------------------

float accelMultiplier = 0.22;
float accelExponent   = 1.35;

// ----------------------------
// CURSOR LIMIT
// ----------------------------

int maxSpeed = 90;

// ----------------------------
// RESIDUAL CLEANUP
// ----------------------------

float residualDecay = 0.90;

// ============================================================================
// VARIABLES
// ============================================================================

float gyroOffsetX = 0;
float gyroOffsetY = 0;

float accelOffsetX = 0;
float accelOffsetY = 0;

float smoothX = 0;
float smoothY = 0;

float accumX = 0;
float accumY = 0;

bool prevLeft  = HIGH;
bool prevRight = HIGH;

unsigned long lastLeftClick  = 0;
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

  // Wake MPU
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // Gyro ±250 deg/s
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // Accel ±2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // DLPF
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A);
  Wire.write(0x04);
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

  long sumGX = 0;
  long sumGY = 0;

  long sumAX = 0;
  long sumAY = 0;

  for (int i = 0; i < 1000; i++) {

    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    readMPU(ax, ay, az, gx, gy, gz);

    sumGX += gx;
    sumGY += gy;

    sumAX += ax;
    sumAY += ay;

    delay(2);
  }

  gyroOffsetX = sumGX / 1000.0;
  gyroOffsetY = sumGY / 1000.0;

  accelOffsetX = sumAX / 1000.0;
  accelOffsetY = sumAY / 1000.0;

  Serial.println("CALIBRATION DONE");

  Serial.print("GX OFFSET: ");
  Serial.println(gyroOffsetX);

  Serial.print("GY OFFSET: ");
  Serial.println(gyroOffsetY);
}

// ============================================================================
// CURVE FUNCTION
// ============================================================================

float curvedTilt(float value) {

  float sign = (value >= 0) ? 1.0 : -1.0;

  value = fabs(value);

  if (value < tiltDeadzone)
    return 0;

  value -= tiltDeadzone;

  return sign * pow(value, tiltCurve);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println("\nAIR MOUSE PRO V7 HYBRID");

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  Wire.begin(SDA_PIN, SCL_PIN);

  Wire.setClock(400000);

  initMPU();

  calibrateMPU();

  bleMouse.begin();

  Serial.println("BLE READY");
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
    // GYRO
    // ============================================================

    float gyroX =
      (gx - gyroOffsetX) / 131.0;

    float gyroY =
      (gy - gyroOffsetY) / 131.0;

    // ============================================================
    // ACCEL
    // ============================================================

    float accelX =
      (ax - accelOffsetX) / 16384.0;

    float accelY =
      (ay - accelOffsetY) / 16384.0;

    // ============================================================
    // GYRO MOVEMENT
    // ============================================================

    float rawX = gyroY / gyroScale;
    float rawY = -gyroX / gyroScale;

    // ============================================================
    // TILT ASSIST
    // CONTINUOUS MOVEMENT TOWARD CORNERS
    // ============================================================

    float tiltX =
      curvedTilt(accelY) * tiltAssist;

    float tiltY =
      curvedTilt(accelX) * tiltAssist;

    rawX += tiltX;
    rawY += tiltY;

    // ============================================================
    // MAGNITUDE
    // ============================================================

    float magnitude =
      sqrt((rawX * rawX) +
           (rawY * rawY));

    // ============================================================
    // DEADZONE
    // ============================================================

    if (fabs(rawX) < deadzone)
      rawX = 0;

    if (fabs(rawY) < deadzone)
      rawY = 0;

    // ============================================================
    // DYNAMIC ACCELERATION
    // ============================================================

    float accel =
      1.0 +
      pow(magnitude * accelMultiplier,
          accelExponent);

    rawX *= accel;
    rawY *= accel;

    // ============================================================
    // ADAPTIVE SMOOTHING
    // ============================================================

    float alpha;

    if (magnitude < stillThreshold) {
      alpha = alphaStill;
    } else {
      alpha = alphaMove;
    }

    smoothX =
      (alpha * smoothX) +
      ((1.0 - alpha) * rawX);

    smoothY =
      (alpha * smoothY) +
      ((1.0 - alpha) * rawY);

    // ============================================================
    // DAMPING
    // ============================================================

    smoothX *= damping;
    smoothY *= damping;

    // ============================================================
    // MICRO BOOST
    // ============================================================

    if (fabs(smoothX) < 0.35 &&
        smoothX != 0) {

      smoothX *= microBoost;
    }

    if (fabs(smoothY) < 0.35 &&
        smoothY != 0) {

      smoothY *= microBoost;
    }

    // ============================================================
    // IDLE LOCK
    // ============================================================

    if (magnitude < 0.25) {

      smoothX = 0;
      smoothY = 0;

      accumX *= residualDecay;
      accumY *= residualDecay;

      if (fabs(accumX) < 0.05)
        accumX = 0;

      if (fabs(accumY) < 0.05)
        accumY = 0;
    }

    // ============================================================
    // SUB-PIXEL ACCUMULATION
    // ============================================================

    accumX += smoothX;
    accumY += smoothY;

    int moveX = (int)accumX;
    int moveY = (int)accumY;

    accumX -= moveX;
    accumY -= moveY;

    // ============================================================
    // LIMIT
    // ============================================================

    moveX = constrain(moveX, -maxSpeed, maxSpeed);
    moveY = constrain(moveY, -maxSpeed, maxSpeed);

    // ============================================================
    // SEND MOVEMENT
    // ============================================================

    if (moveX != 0 || moveY != 0) {

      bleMouse.move(moveX, moveY);
    }

    // ============================================================
    // BUTTONS
    // ============================================================

    bool leftNow  = digitalRead(BTN_LEFT);
    bool rightNow = digitalRead(BTN_RIGHT);

    // LEFT CLICK
    if (prevLeft == HIGH && leftNow == LOW) {

      if (millis() - lastLeftClick > 80) {

        bleMouse.click(MOUSE_LEFT);

        Serial.println("LEFT CLICK");

        lastLeftClick = millis();
      }
    }

    // RIGHT CLICK
    if (prevRight == HIGH && rightNow == LOW) {

      if (millis() - lastRightClick > 80) {

        bleMouse.click(MOUSE_RIGHT);

        Serial.println("RIGHT CLICK");

        lastRightClick = millis();
      }
    }

    prevLeft  = leftNow;
    prevRight = rightNow;

    // ============================================================
    // DEBUG
    // ============================================================

    static unsigned long lastPrint = 0;

    if (millis() - lastPrint > 150) {

      Serial.print("X:");
      Serial.print(moveX);

      Serial.print(" Y:");
      Serial.print(moveY);

      Serial.print(" MAG:");
      Serial.print(magnitude, 2);

      Serial.print(" TILT X:");
      Serial.print(tiltX, 2);

      Serial.print(" TILT Y:");
      Serial.println(tiltY, 2);

      lastPrint = millis();
    }
  }

  delay(2);
}

/*
============================================================================
BEST TUNING
============================================================================

FASTER OVERALL:
gyroScale = 20

SLOWER:
gyroScale = 30

MORE CORNER PULL:
tiltAssist = 3.2

LESS CORNER PULL:
tiltAssist = 1.6

MORE PRECISE:
tiltCurve = 2.2

MORE RESPONSIVE:
tiltCurve = 1.4

MORE STABLE:
alphaStill = 0.97

LESS LAG:
alphaMove = 0.18

============================================================================
*/
