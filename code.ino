#include <Wire.h>
#include <BleMouse.h>

#define SDA_PIN 6
#define SCL_PIN 7

#define BTN_LEFT 1
#define BTN_RIGHT 2

BleMouse bleMouse("AirMouse-C3", "ESP32", 100);

float sensitivity = 7000.0;

void setup() {

  Serial.begin(115200);
  delay(2000);

  Serial.println("ESP32-C3 AIR MOUSE");

  Wire.begin(SDA_PIN, SCL_PIN);

  // Wake MPU6050
  Wire.beginTransmission(0x68);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  bleMouse.begin();

  Serial.println("BLE Mouse Started");
  Serial.println("Search: AirMouse-C3");
}

void loop() {

  if (bleMouse.isConnected()) {

    // Read accelerometer
    Wire.beginTransmission(0x68);
    Wire.write(0x3B);
    Wire.endTransmission(false);

    Wire.requestFrom(0x68, 6, true);

    int16_t ax =
      (Wire.read() << 8) | Wire.read();

    int16_t ay =
      (Wire.read() << 8) | Wire.read();

    int16_t az =
      (Wire.read() << 8) | Wire.read();

    int moveX = ay / sensitivity;
    int moveY = -ax / sensitivity;

    // Deadzone
    if (abs(moveX) < 1) moveX = 0;
    if (abs(moveY) < 1) moveY = 0;

    bleMouse.move(moveX, moveY);

    // Left click
    if (digitalRead(BTN_LEFT) == LOW) {
      bleMouse.click(MOUSE_LEFT);
      delay(200);
    }

    // Right click
    if (digitalRead(BTN_RIGHT) == LOW) {
      bleMouse.click(MOUSE_RIGHT);
      delay(200);
    }

    Serial.print("X: ");
    Serial.print(moveX);

    Serial.print(" Y: ");
    Serial.println(moveY);
  }

  delay(10);
}
