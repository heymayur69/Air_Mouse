/*
============================================================================
ESP32-C3 AIR MOUSE (FIXED VERSION)
VISIBLE CURSOR + WINDOWS + ANDROID + BUTTON FIX
============================================================================

FIXES:
✓ Uses BleMouse library (proper HID mouse)
✓ Visible cursor on Android/Windows
✓ Faster cursor
✓ Correct axis mapping
✓ Edge-detection buttons (single click only)
✓ No cursor disappearing
✓ Safe GPIO pins
✓ Smoother movement
✓ Deadzone added
✓ MPU6050 raw I2C stable reading

BOARD:
ESP32-C3 Super Mini

WIRING:
MPU6050 SDA -> GPIO6
MPU6050 SCL -> GPIO7

LEFT BUTTON  -> GPIO4
RIGHT BUTTON -> GPIO5

Buttons:
GPIO ---- BUTTON ---- GND

============================================================================
*/

#include <Wire.h>
#include <BleMouse.h>

// ========================= PINS =========================
#define SDA_PIN 6
#define SCL_PIN 7

// SAFE GPIOs
#define BTN_LEFT  4
#define BTN_RIGHT 5

#define MPU_ADDR 0x68

// ========================= BLE =========================
BleMouse bleMouse("AirMouse-C3", "ESP32", 100);

// ========================= SETTINGS =========================

// LOWER = FASTER
float sensitivity = 1800.0;

// movement smoothing
float smoothX = 0;
float smoothY = 0;

// smoothing factor
float alpha = 0.7;

// deadzone
int deadzone = 2;

// ========================= BUTTON EDGE DETECTION =========================

// previous states
bool prevLeftState = HIGH;
bool prevRightState = HIGH;

// ========================================================
// MPU6050 INIT
// ========================================================

void initMPU() {

  Wire.beginTransmission(MPU_ADDR);

  if (Wire.endTransmission() != 0) {
    Serial.println("MPU6050 NOT FOUND!");

    while (1) {
      delay(100);
    }
  }

  // wake up MPU6050
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0x00);
  Wire.endTransmission(true);

  Serial.println("MPU6050 READY");
}

// ========================================================
// READ MPU6050
// ========================================================

void readMPU(int16_t &ax, int16_t &ay, int16_t &az) {

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);

  Wire.requestFrom(MPU_ADDR, 6, true);

  ax = (Wire.read() << 8) | Wire.read();
  ay = (Wire.read() << 8) | Wire.read();
  az = (Wire.read() << 8) | Wire.read();
}

// ========================================================
// SETUP
// ========================================================

void setup() {

  Serial.begin(115200);

  delay(2000);

  Serial.println("\nESP32-C3 AIR MOUSE");

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // buttons
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  // MPU
  initMPU();

  // BLE Mouse
  bleMouse.begin();

  Serial.println("BLE MOUSE STARTED");
  Serial.println("SEARCH: AirMouse-C3");
}

// ========================================================
// LOOP
// ========================================================

void loop() {

  if (bleMouse.isConnected()) {

    int16_t ax, ay, az;

    readMPU(ax, ay, az);

    // ====================================================
    // AXIS MAPPING
    // ====================================================

    // FIXED:
    // Tilt LEFT/RIGHT -> cursor LEFT/RIGHT
    // Tilt UP/DOWN    -> cursor UP/DOWN

    float rawX = -ax / sensitivity;
    float rawY = -ay / sensitivity;

    // ====================================================
    // SMOOTHING FILTER
    // ====================================================

    smoothX = (alpha * smoothX) + ((1.0 - alpha) * rawX);
    smoothY = (alpha * smoothY) + ((1.0 - alpha) * rawY);

    int moveX = (int)smoothX;
    int moveY = (int)smoothY;

    // ====================================================
    // DEADZONE
    // ====================================================

    if (abs(moveX) < deadzone) moveX = 0;
    if (abs(moveY) < deadzone) moveY = 0;

    // ====================================================
    // SEND MOUSE MOVEMENT
    // ====================================================

    bleMouse.move(moveX, moveY);

    // ====================================================
    // BUTTONS (EDGE DETECTION)
    // ====================================================

    bool leftState = digitalRead(BTN_LEFT);
    bool rightState = digitalRead(BTN_RIGHT);

    // LEFT CLICK
    // only trigger ONCE when pressed

    if (prevLeftState == HIGH && leftState == LOW) {

      bleMouse.click(MOUSE_LEFT);

      Serial.println("LEFT CLICK");
    }

    // RIGHT CLICK
    // only trigger ONCE when pressed

    if (prevRightState == HIGH && rightState == LOW) {

      bleMouse.click(MOUSE_RIGHT);

      Serial.println("RIGHT CLICK");
    }

    // save states
    prevLeftState = leftState;
    prevRightState = rightState;

    // ====================================================
    // DEBUG
    // ====================================================

    Serial.print("X: ");
    Serial.print(moveX);

    Serial.print("  Y: ");
    Serial.println(moveY);
  }

  delay(10);
}
