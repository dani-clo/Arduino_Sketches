/*
  APDS-9960 Manual Init Test
  
  This sketch tests if the APDS-9960 can be initialized with manual reset and delay.
  Useful if the library's begin() fails.
*/

#include <Wire.h>

#define APDS9960_ADDR 0x39
#define APDS9960_ID_REG 0x92
#define APDS9960_ENABLE_REG 0x80
#define APDS9960_STATUS_REG 0x93

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=== APDS9960 Manual Init Test ===\n");
  
  // Reset INT pin
  Serial.println("1. Resetting INT pin (D26)...");
  pinMode(26, OUTPUT);
  digitalWrite(26, LOW);
  delayMicroseconds(10);
  digitalWrite(26, HIGH);
  pinMode(26, INPUT_PULLUP);
  delay(100);
  
  // Initialize I2C
  Serial.println("2. Initializing Wire1...");
  Wire1.begin();
  Wire1.setClock(400000);
  delay(500);
  
  // Attempt to read ID with multiple retries
  Serial.println("\n3. Attempting to read APDS9960 ID (with retries)...");
  uint8_t id = 0xFF;
  int attempts = 0;
  
  for(int i = 0; i < 5; i++) {
    Wire1.beginTransmission(APDS9960_ADDR);
    Wire1.write(APDS9960_ID_REG);
    int txErr = Wire1.endTransmission(false);  // Repeated START
    
    if(txErr != 0) {
      Serial.print("   Attempt ");
      Serial.print(i+1);
      Serial.print(": Write error ");
      Serial.println(txErr);
      delay(100);
      continue;
    }
    
    Wire1.requestFrom(APDS9960_ADDR, 1);
    if(Wire1.available()) {
      id = Wire1.read();
      Serial.print("   ✓ Attempt ");
      Serial.print(i+1);
      Serial.print(": Read ID = 0x");
      Serial.println(id, HEX);
      break;
    } else {
      Serial.print("   Attempt ");
      Serial.print(i+1);
      Serial.println(": No data available");
      delay(100);
    }
  }
  
  if(id == 0xAB) {
    Serial.println("\n   ✓ SENSOR FOUND! ID is correct (0xAB)");
    
    // Try to enable power
    Serial.println("\n4. Attempting to enable sensor power...");
    Wire1.beginTransmission(APDS9960_ADDR);
    Wire1.write(APDS9960_ENABLE_REG);
    Wire1.write(0x01);  // PON bit
    int err = Wire1.endTransmission();
    Serial.print("   Enable result: ");
    Serial.println(err == 0 ? "OK" : "FAILED");
    
    delay(100);
    
    // Read status
    Serial.println("\n5. Reading status register...");
    Wire1.beginTransmission(APDS9960_ADDR);
    Wire1.write(APDS9960_STATUS_REG);
    Wire1.endTransmission(false);
    Wire1.requestFrom(APDS9960_ADDR, 1);
    if(Wire1.available()) {
      uint8_t status = Wire1.read();
      Serial.print("   Status = 0x");
      Serial.println(status, HEX);
    }
    
  } else {
    Serial.println("\n   ✗ SENSOR NOT FOUND!");
    Serial.print("   ID read was: 0x");
    Serial.println(id, HEX);
    Serial.println("\n   Possible causes:");
    Serial.println("   - Sensor not powered");
    Serial.println("   - Sensor physically disconnected");
    Serial.println("   - I2C bus issue (but BMI270 works?)");
    Serial.println("   - Wrong I2C address");
  }
  
  Serial.println("\n=== Test Complete ===\n");
}

void loop() {
  delay(1000);
}
