#include <SPI.h>
#include <Wire.h>

static const uint8_t UART2_TX_PIN = 18;
static const uint8_t SPI_PWM_PIN = 11;

// ADC test pins: A0=PC4 (pin 76), A3=PB1 (pin 79)
static const uint8_t ADC_TEST_PIN_A0 = A0;
static const uint8_t ADC_TEST_PIN_A3 = A3;

static void print_banner() {
  Serial.println("\n=== GIGA Dynamic PINCTRL test ===");
  Serial.println("Setup:");
  Serial.println("  - Serial/Logic Analyzer CH1: D18 (UART->GPIO transition)");
  Serial.println("  - Logic Analyzer CH2: D11 (SPI/PWM/GPIO transitions)");
  Serial.println("  - Serial/Logic Analyzer CH3: A0->GND, A3->3.3V (ADC/GPIO/ADC transitions)");
}

static void test_uart_gpio() {
  Serial.println("\n[1] UART2 -> GPIO on D18");
  Serial.println("  Expectation: D18 active during Serial1, then manual GPIO toggle");

  Serial2.begin(115200);
  Serial2.println("UART2 active before GPIO");
  delay(40);
  Serial2.end();

  pinMode(UART2_TX_PIN, OUTPUT);
  digitalWrite(UART2_TX_PIN, HIGH);
  delay(80);
  digitalWrite(UART2_TX_PIN, LOW);

  Serial2.begin(115200);
  Serial2.println("UART2 active after GPIO");
  delay(40);
  Serial2.end();
}

static void test_spi_pwm_gpio() {
  Serial.println("\n[2] SPI -> PWM -> GPIO on D11");
  Serial.println("  Expectation on D11: visible SPI transfer, then PWM waveform, then static GPIO");

  SPI1.begin();
  SPI1.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  for (uint16_t i = 0; i < 10; i++) {
    (void)SPI1.transfer((uint8_t)i);
  }
  SPI1.endTransaction();
  SPI1.end();

  delay(1000);

  analogWrite(SPI_PWM_PIN, 128);
  delay(250);
  analogWrite(SPI_PWM_PIN, 32);
  delay(250);
  analogWrite(SPI_PWM_PIN, 0);

  delay(1000);

  pinMode(SPI_PWM_PIN, OUTPUT);
  digitalWrite(SPI_PWM_PIN, HIGH);
  delay(100);
  digitalWrite(SPI_PWM_PIN, LOW);
  delay(100);

  SPI1.begin();
  SPI1.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  for (uint16_t i = 0; i < 10; i++) {
    (void)SPI1.transfer((uint8_t)i);
  }
  SPI1.endTransaction();
  SPI1.end();
}

static void test_adc_gpio_adc_manual() {
  Serial.println("\n[2] ADC -> GPIO -> ADC on A0/A3 (manual test)");
  Serial.println("  Expectation: Read A0->GND and A3->3.3V then GPIO toggle, then read A0/A3 again with same expected values");
  delay(1500);

  int a0_before = analogRead(ADC_TEST_PIN_A0);
  int a3_before = analogRead(ADC_TEST_PIN_A3);
  Serial.print("  Before switch: A0=");
  Serial.print(a0_before);
  Serial.print(" A3=");
  Serial.println(a3_before);

  Serial.println("  [MANUAL] DISCONNECT A0/A3 now and connect Logic Analyzer (10s)");
  delay(10000);

  pinMode(ADC_TEST_PIN_A0, OUTPUT);
  pinMode(ADC_TEST_PIN_A3, OUTPUT);
  for (int i = 0; i < 4; i++) {
    digitalWrite(ADC_TEST_PIN_A0, HIGH);
    digitalWrite(ADC_TEST_PIN_A3, LOW);
    delay(60);
    digitalWrite(ADC_TEST_PIN_A0, LOW);
    digitalWrite(ADC_TEST_PIN_A3, HIGH);
    delay(60);
  }

  Serial.println("  [MANUAL] RECONNECT A0/A3 now (10s)");
  delay(10000);

  int a0_after = analogRead(ADC_TEST_PIN_A0);
  int a3_after = analogRead(ADC_TEST_PIN_A3);
  Serial.print("  After switch:  A0=");
  Serial.print(a0_after);
  Serial.print(" A3=");
  Serial.println(a3_after);

  bool a0_ok = (a0_after < 50);
  bool a3_ok = (a3_after > 950);
  Serial.println((a0_ok && a3_ok) ? "  PASS: ADC re-acquired correctly" :
                                  "  FAIL: ADC value mismatch after pinctrl switch");
}

static void test_spi_gpio_spi() {
  Serial.println("\n[4] SPI -> GPIO -> SPI re-acquire on D11 path");

  SPI1.begin();
  SPI1.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  for (uint16_t i = 0; i < 10; i++) {
    (void)SPI1.transfer((uint8_t)(0x30 + (i & 0x0F)));
  }
  SPI1.endTransaction();
  SPI1.end();

  pinMode(SPI_PWM_PIN, OUTPUT);
  digitalWrite(SPI_PWM_PIN, HIGH);
  delay(100);
  digitalWrite(SPI_PWM_PIN, LOW);

  SPI1.begin();
  SPI1.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  for (uint16_t i = 0; i < 10; i++) {
    (void)SPI1.transfer((uint8_t)(0xC0 + (i & 0x0F)));
  }
  SPI1.endTransaction();
  SPI1.end();
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  print_banner();
}

void loop() {
  test_uart_gpio();
  delay(2500);

  //test_spi_pwm_gpio();
  //delay(250);

  test_adc_gpio_adc_manual();
  delay(250);

  test_spi_gpio_spi();
  delay(250);

  Serial.println("\n=== Test cycle complete ===");

  while (1) {};
}
