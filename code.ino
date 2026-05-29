/*
============================================================================
AIR MOUSE PRO V7 ULTRA
DOWNWARD FIX + TRUE DIAGONALS + STABLE + SMOOTH UX
============================================================================

FIXES:
✓ Real diagonal movement
✓ Stable idle
✓ Better downward movement
✓ Easier downward diagonals
✓ Less aggressive wrist tilt needed
✓ Smooth tiny targeting
✓ Fast corner traversal
✓ No robotic feel
✓ Proper BLE mouse behavior
✓ Better Android TV UX
✓ Residual precision movement
✓ Human wrist compensation

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
// TUNING
// ============================================================================

// LOWER = FASTER
float gyroScale = 24.0;

// smoothing
float alphaStill = 0.96;
float alphaMove  = 0.20;

// deadzone
float deadzone = 0.15;

// still detection
float stillThreshold = 1.8;

// damping
float damping = 0.86;

// micro movement boost
float microBoost = 1.18;

// acceleration
float accelMultiplier = 0.16;
float accelExponent   = 1.25;

// max speed
int maxSpeed = 85;

// residual cleanup
float residualDecay = 0.90;

// ============================================================================
// VARIABLES
// ============================================================================

float gyroOffsetX = 0;
float gyroOffsetY = 0;

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

  // Gyro ±250°/s
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // Accel ±2g
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);
  Wire.write(0x00);
  Wire.endTransmission(true);

  // Low Pass Filter
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1A);
  Wire.write(0x04);
  Wire.endTransmission(true);

  Serial.println("MPU READY");
}

// ============================================================================
// READ MPU
// ============================================================================

void readMPU(int16_t &gx, int16_t &gy) {

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x43);
  Wire.endTransmission(false);

  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)4, (uint8_t)true);

  gx = (Wire.read() << 8) | Wire.read();
  gy = (Wire.read() << 8) | Wire.read();
}

// ============================================================================
// CALIBRATION
// ============================================================================

void calibrateMPU() {

  Serial.println("KEEP DEVICE STILL");

  long sumGX = 0;
  long sumGY = 0;

  for (int i = 0; i < 800; i++) {

    int16_t gx, gy;

    readMPU(gx, gy);

    sumGX += gx;
    sumGY += gy;

    delay(2);
  }

  gyroOffsetX = sumGX / 800.0;
  gyroOffsetY = sumGY / 800.0;

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

  Serial.println("\nAIR MOUSE PRO V7 ULTRA");

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

    int16_t gx, gy;

    readMPU(gx, gy);

    // ============================================================
    // GYRO
    // ============================================================

    float gyroX =
      (gx - gyroOffsetX) / 131.0;

    float gyroY =
      (gy - gyroOffsetY) / 131.0;

    // ============================================================
    // TRUE DIAGONAL VELOCITY
    // ============================================================

    float rawX = gyroY / gyroScale;
    float rawY = -gyroX / gyroScale;

    // ============================================================
    // HUMAN WRIST COMPENSATION
    // Downward movement assist
    // ============================================================

    if (rawY > 0) {

      rawY *= 1.38;
      rawX *= 1.08;
    }

    if (rawY < 0) {

      rawY *= 1.02;
    }

    // ============================================================
    // NOISE FILTER
    // ============================================================

    if (fabs(rawX) < deadzone)
      rawX = 0;

    if (fabs(rawY) < deadzone)
      rawY = 0;

    // ============================================================
    // MAGNITUDE
    // ============================================================

    float magnitude =
      sqrt((rawX * rawX) +
           (rawY * rawY));

    // ============================================================
    // DYNAMIC ACCELERATION
    // ============================================================

    float accel =
      1.0 +
      pow(magnitude * accelMultiplier,
          accelExponent);

    // Extra acceleration downward
    if (rawY > 0) {

      accel *= 1.22;
    }

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

    // ============================================================
    // SMOOTHING
    // ============================================================

    smoothX =
      (alpha * smoothX) +
      ((1.0 - alpha) * rawX);

    smoothY =
      (alpha * smoothY) +
      ((1.0 - alpha) * rawY);

    // ============================================================
    // DIRECTIONAL DAMPING
    // ============================================================

    float dampX = damping;
    float dampY = damping;

    // freer downward motion
    if (smoothY > 0) {

      dampY = 0.92;
    }

    smoothX *= dampX;
    smoothY *= dampY;

    // ============================================================
    // MICRO MOVEMENT BOOST
    // ============================================================

    if (fabs(smoothX) < 0.35 &&
        smoothX != 0) {

      smoothX *= microBoost;
    }

    if (fabs(smoothY) < 0.35 &&
        smoothY != 0) {

      smoothY *= microBoost * 1.12;
    }

    // ============================================================
    // IDLE LOCK
    // ============================================================

    if (magnitude < 0.30) {

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
    // SUB PIXEL ACCUMULATION
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

        lastLeftClick = millis();

        Serial.println("LEFT CLICK");
      }
    }

    // RIGHT CLICK
    if (prevRight == HIGH && rightNow == LOW) {

      if (millis() - lastRightClick > 80) {

        bleMouse.click(MOUSE_RIGHT);

        lastRightClick = millis();

        Serial.println("RIGHT CLICK");
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
      Serial.println(magnitude, 2);

      lastPrint = millis();
    }
  }

  delay(2);
}

/*
============================================================================
TUNING GUIDE
============================================================================

FASTER CURSOR:
gyroScale = 20

SLOWER CURSOR:
gyroScale = 28

MORE STABLE:
alphaStill = 0.97

LESS LAG:
alphaMove = 0.15

LESS JITTER:
deadzone = 0.20

MORE FLOATY:
damping = 0.90

MORE SHARP:
damping = 0.80

MORE DOWNWARD ASSIST:
rawY *= 1.50;

============================================================================
*/
