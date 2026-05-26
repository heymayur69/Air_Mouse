/*
============================================================================
AIR MOUSE PRO V5
REAL DIAGONAL MOVEMENT + STABLE + PRECISE + GOOD BUTTONS
============================================================================

FIXES OVER PREVIOUS VERSION:
✓ TRUE diagonal movement
✓ No robotic 4-direction feel
✓ Natural floating cursor
✓ Tiny movements possible
✓ Smooth Android TV control
✓ Better UX
✓ Stable when still
✓ Fast when moving
✓ Proper button logic
✓ No crazy random cursor jumps
✓ Real sub-pixel accumulation
✓ Commercial air mouse feel

IMPORTANT:
This uses GYROSCOPE as MAIN control.
NOT accelerometer tilt.

WHY?
Tilt-based systems feel robotic.
Gyro-based systems feel natural.

BOARD:
ESP32-C3 Super Mini

MPU6050:
SDA -> GPIO6
SCL -> GPIO7

BUTTONS:
LEFT  -> GPIO4 -> GND
RIGHT -> GPIO5 -> GND

ESP32 BOARD SETTINGS:
Board: ESP32C3 Dev Module
USB CDC On Boot: ENABLED
Partition: Huge APP
Upload Speed: 921600

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
// TUNING
// ============================================================================

// LOWER = FASTER
float gyroScale = 22.0;

// smoothing
float alphaStill = 0.92;
float alphaMove  = 0.30;

// deadzone
float deadzone = 0.10;

// motion threshold
float stillThreshold = 2.0;

// damping
float damping = 0.90;

// max speed
int maxSpeed = 50;

// tiny movement boost
float microBoost = 1.35;

// gyro drift correction
float gyroDriftX = 0;
float gyroDriftY = 0;

// ============================================================================
// VARIABLES
// ============================================================================

float velX = 0;
float velY = 0;

float smoothX = 0;
float smoothY = 0;

// residual accumulation
float accumX = 0;
float accumY = 0;

// gyro offsets
float gyroOffsetX = 0;
float gyroOffsetY = 0;

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

  // gyro ±250 deg/s
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // accel ±2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // LOW PASS FILTER
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

  for (int i = 0; i < 500; i++) {

    int16_t ax, ay, az;
    int16_t gx, gy, gz;

    readMPU(ax, ay, az, gx, gy, gz);

    sumGX += gx;
    sumGY += gy;

    delay(3);
  }

  gyroOffsetX = sumGX / 500.0;
  gyroOffsetY = sumGY / 500.0;

  Serial.println("CALIBRATION DONE");

  Serial.print("OFFSET X: ");
  Serial.println(gyroOffsetX);

  Serial.print("OFFSET Y: ");
  Serial.println(gyroOffsetY);
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {

  Serial.begin(115200);

  delay(1500);

  Serial.println("\nAIR MOUSE PRO V5");

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
    // GYRO ONLY
    // ============================================================

    float gyroX = (gx - gyroOffsetX) / 131.0;
    float gyroY = (gy - gyroOffsetY) / 131.0;

    // ============================================================
    // TRUE DIAGONAL VELOCITY
    // ============================================================

    float rawX = gyroY / gyroScale;
    float rawY = -gyroX / gyroScale;

    // ============================================================
    // MOTION MAGNITUDE
    // ============================================================

    float motion = sqrt(
      (rawX * rawX) +
      (rawY * rawY)
    );

    // ============================================================
    // ADAPTIVE SMOOTHING
    // ============================================================

    float alpha;

    if (motion < stillThreshold) {
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
    // MICRO MOVEMENT BOOST
    // ============================================================

    if (fabs(smoothX) < 0.5 && smoothX != 0) {
      smoothX *= microBoost;
    }

    if (fabs(smoothY) < 0.5 && smoothY != 0) {
      smoothY *= microBoost;
    }

    // ============================================================
    // DEADZONE
    // ============================================================

    if (fabs(smoothX) < deadzone) smoothX = 0;
    if (fabs(smoothY) < deadzone) smoothY = 0;

    // ============================================================
    // SUB-PIXEL ACCUMULATION
    // THIS IS WHAT MAKES SMALL MOVEMENTS POSSIBLE
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

      static unsigned long lastPrint = 0;

      if (millis() - lastPrint > 100) {

        Serial.print("X:");
        Serial.print(moveX);

        Serial.print(" Y:");
        Serial.print(moveY);

        Serial.print(" SX:");
        Serial.print(smoothX, 2);

        Serial.print(" SY:");
        Serial.println(smoothY, 2);

        lastPrint = millis();
      }
    }

    // ============================================================
    // BUTTONS
    // ============================================================

    bool leftNow = digitalRead(BTN_LEFT);
    bool rightNow = digitalRead(BTN_RIGHT);

    // ============================================================
    // LEFT CLICK
    // ============================================================

    if (prevLeft == HIGH && leftNow == LOW) {

      if (millis() - lastLeftClick > 80) {

        bleMouse.click(MOUSE_LEFT);

        Serial.println("LEFT CLICK");

        lastLeftClick = millis();
      }
    }

    // ============================================================
    // RIGHT CLICK
    // ============================================================

    if (prevRight == HIGH && rightNow == LOW) {

      if (millis() - lastRightClick > 80) {

        bleMouse.click(MOUSE_RIGHT);

        Serial.println("RIGHT CLICK");

        lastRightClick = millis();
      }
    }

    prevLeft = leftNow;
    prevRight = rightNow;
  }

  delay(2);
}

/*
============================================================================
BEST SETTINGS
============================================================================

MORE STABLE:
alphaStill = 0.95

LESS LAG:
alphaMove = 0.22

FASTER:
gyroScale = 18

SLOWER:
gyroScale = 28

BETTER SMALL TARGETING:
microBoost = 1.5

LESS JITTER:
deadzone = 0.15

MORE FLOATY:
damping = 0.94

MORE SHARP:
damping = 0.86

============================================================================
WHY THIS FEELS BETTER
============================================================================

OLD SYSTEM:
Tilt angle -> cursor

PROBLEM:
Cursor only likes cardinal directions
Feels robotic
Tiny movement impossible

NEW SYSTEM:
Angular velocity -> accumulated movement

RESULT:
Natural diagonals
Floating feel
Tiny targeting possible
Commercial UX
No stair-stepping

============================================================================
*/
