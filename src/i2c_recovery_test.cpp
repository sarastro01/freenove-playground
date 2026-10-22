/**
 * @file i2c_recovery_test.cpp
 * @brief I2C Bus Recovery Diagnostic Firmware
 * @date 2026-10-22
 * 
 * USAGE: Rename to main.cpp and compile to test I2C bus recovery
 * 
 * This firmware attempts to recover a stuck I2C bus by:
 * 1. Measuring SDA/SCL voltage levels
 * 2. Toggling SCL clock line 9 times
 * 3. Sending I2C STOP condition
 * 4. Reinitializing I2C module
 * 5. Scanning for devices
 */

#include <Arduino.h>
#include <Wire.h>

#define SDA_PIN 21
#define SCL_PIN 22

void measureBusVoltage() {
  int sda_raw = analogRead(SDA_PIN);
  int scl_raw = analogRead(SCL_PIN);
  
  float sda_v = (sda_raw / 4095.0) * 3.3;
  float scl_v = (scl_raw / 4095.0) * 3.3;
  
  Serial.printf("[BUS] SDA: %.2fV (raw: %d), SCL: %.2fV (raw: %d)\n", sda_v, sda_raw, scl_v, scl_raw);
  
  if (sda_v < 0.5 || scl_v < 0.5) {
    Serial.println("[BUS] ⚠️  WARNING: I2C lines appear stuck LOW");
  }
  if (sda_v > 3.0 && scl_v > 3.0) {
    Serial.println("[BUS] ✓ OK: I2C lines pulled high");
  }
}

void recoverI2CBus() {
  Wire.end();
  delay(100);
  
  pinMode(SCL_PIN, OUTPUT);
  pinMode(SDA_PIN, INPUT_PULLUP);
  
  Serial.println("[RECOVERY] Toggling SCL 9 times...");
  for (int i = 0; i < 9; i++) {
    digitalWrite(SCL_PIN, LOW);
    delayMicroseconds(10);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(10);
    Serial.print(".");
  }
  Serial.println(" Done");
  
  pinMode(SDA_PIN, OUTPUT);
  digitalWrite(SDA_PIN, LOW);
  delayMicroseconds(10);
  digitalWrite(SCL_PIN, LOW);
  delayMicroseconds(10);
  digitalWrite(SCL_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SDA_PIN, HIGH);
  delayMicroseconds(10);
  Serial.println("[RECOVERY] Sent STOP condition");
  
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, INPUT_PULLUP);
  delay(100);
  
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setTimeOut(100);
  Serial.println("[RECOVERY] I2C module reinitialized");
}

void testI2CBus() {
  uint8_t found = 0;
  
  Serial.println("[SCAN] Testing I2C addresses 0x01-0x7F:");
  
  for (uint8_t addr = 0x01; addr < 0x80; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
      Serial.printf("[SCAN] ✓ Device found at 0x%02X\n", addr);
      found++;
    }
  }
  
  Serial.printf("[SCAN] Total found: %d device(s)\n", found);
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=====================================");
  Serial.println("I2C BUS RECOVERY DIAGNOSTIC");
  Serial.println("=====================================\n");
  
  Serial.println("[1] Measuring initial bus state...");
  measureBusVoltage();
  
  Serial.println("\n[2] Attempting I2C bus recovery...");
  recoverI2CBus();
  
  Serial.println("\n[3] Measuring bus state after recovery...");
  measureBusVoltage();
  
  Serial.println("\n[4] Scanning for I2C devices...");
  testI2CBus();
  
  Serial.println("\n[5] Diagnostic complete!");
  Serial.println("=====================================\n");
}

void loop() {
  delay(1000);
}
