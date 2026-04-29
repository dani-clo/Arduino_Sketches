/*
  APDS-9960 Diagnostic Sketch
  
  This sketch diagnoses why APDS-9960 initialization fails.
  It performs I2C scans and reads the sensor directly.
*/

#include <Wire.h>
#include <Arduino_APDS9960.h>

#define APDS9960_ADDR 0x39
#define APDS9960_ID_REG 0x92
#define PIN_APDS_INT_D26 26

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n=== APDS9960 Diagnostic ===\n");
   
  // 1. Initialize Wire1.
  Serial.println("\n1. Initializing Wire1...");
  Wire1.begin();
  Wire1.setClock(400000);  // Set to 400kHz
  delay(500);
  Serial.println("   ✓ Wire1.begin() + setClock(400000) done");

  // 2. Compare behavior when D26 is driven HIGH.
  Serial.println("\n2. Probing APDS with D26 driven HIGH...");
  pinMode(PIN_APDS_INT_D26, OUTPUT);
  digitalWrite(PIN_APDS_INT_D26, HIGH);
  delay(20);
  printProbeResult();

  // 3. Release D26 again before the regular scan.
  Serial.println("\n3. Releasing D26 again...");
  pinMode(PIN_APDS_INT_D26, INPUT_PULLUP);
  delay(20);
  printProbeResult();
  
  // 4. I2C Bus Scan
  Serial.println("\n4. I2C Bus Scan on Wire1:");
  byte error, address;
  int nDevices = 0;
  
  for(address = 1; address < 127; address++ ) {
    Wire1.beginTransmission(address);
    Wire1.write((uint8_t)0x00);
    error = Wire1.endTransmission();
    
    if (error == 0) {
      Serial.print("   Found I2C device at address 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      nDevices++;
    }
  }
  
  if (nDevices == 0) {
    Serial.println("   No I2C devices found!");
  } else {
    Serial.print("   Total devices found: ");
    Serial.println(nDevices);
  }
  
  // 5. Direct read of APDS9960 ID
  Serial.println("\n5. Attempting to read APDS9960 ID register (0x92)...");
  uint8_t id = readAPDSregister(APDS9960_ID_REG);
  Serial.print("   ID register value: 0x");
  Serial.println(id, HEX);
  Serial.print("   Expected: 0xAB");
  if (id == 0xAB) {
    Serial.println(" ✓ OK");
  } else {
    Serial.println(" ✗ FAIL");
  }
  
  // 6. Read other key registers
  Serial.println("\n6. Reading other registers...");
  uint8_t enable = readAPDSregister(0x80);  // ENABLE
  uint8_t status = readAPDSregister(0x93);  // STATUS
  uint8_t config1 = readAPDSregister(0x8D); // CONFIG1
  uint8_t config2 = readAPDSregister(0x90); // CONFIG2
  
  Serial.print("   ENABLE (0x80): 0x");   Serial.println(enable, HEX);
  Serial.print("   STATUS (0x93): 0x");   Serial.println(status, HEX);
  Serial.print("   CONFIG1 (0x8D): 0x");  Serial.println(config1, HEX);
  Serial.print("   CONFIG2 (0x90): 0x");  Serial.println(config2, HEX);
  
  // 7. Try APDS.begin() with detailed logging
  Serial.println("\n7. Attempting APDS.begin()...");
  if (!APDS.begin()) {
    Serial.println("   ✗ APDS.begin() FAILED");
    
    // Try to read ID again after begin attempt
    Serial.println("\n   Retrying ID read after failed begin...");
    id = readAPDSregister(APDS9960_ID_REG);
    Serial.print("   ID register value: 0x");
    Serial.println(id, HEX);
  } else {
    Serial.println("   ✓ APDS.begin() SUCCESS");
  }
  
  // 8. Check if it's a timing issue - add more delay and retry
  Serial.println("\n8. Testing with additional delays...");
  delay(1000);
  Serial.println("   After 1 second delay, retrying APDS.begin()...");
  if (!APDS.begin()) {
    Serial.println("   ✗ Still failed after delay");
  } else {
    Serial.println("   ✓ SUCCESS after delay!");
  }
  
  Serial.println("\n=== Diagnostic Complete ===\n");
}

void loop() {
  // Nothing here - diagnostics only run in setup
  delay(1000);
}

void printProbeResult() {
  uint8_t error = 0;

  Wire1.beginTransmission(APDS9960_ADDR);
  error = Wire1.endTransmission();

  Serial.print("   Address 0x39 probe result: ");
  if (error == 0) {
    Serial.println("ACK");
  } else {
    Serial.print("NACK/error ");
    Serial.println(error);
  }
}

// Helper function to read APDS register
uint8_t readAPDSregister(uint8_t reg) {
  Wire1.beginTransmission(APDS9960_ADDR);
  Wire1.write(reg);
  int error = Wire1.endTransmission();
  
  if (error != 0) {
    Serial.print("   Write error: ");
    Serial.println(error);
    return 0xFF;
  }
  
  Wire1.requestFrom(APDS9960_ADDR, 1);
  
  if (Wire1.available() == 0) {
    Serial.println("   No data available from sensor");
    return 0xFF;
  }
  
  uint8_t value = Wire1.read();
  return value;
}
