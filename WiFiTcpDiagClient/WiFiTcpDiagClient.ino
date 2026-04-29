/*
  WiFi TCP Diagnostic Client

  Standalone TCP diagnostic client to validate connectivity and payload exchange
  without involving Modbus.
*/

#include <SPI.h>
#if !defined(ARDUINO_ARCH_ZEPHYR)
#error "This example is only for ARDUINO_ARCH_ZEPHYR"
#endif
#include <WiFi.h>

#include "arduino_secrets.h"

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

int status = WL_IDLE_STATUS;
const uint16_t tcpPort = 1502;
IPAddress serverIp(10, 130, 22, 157);

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("Client IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("Subnet: ");
  Serial.println(WiFi.subnetMask());

  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

bool tcpConnectProbe(IPAddress ip, uint16_t port) {
  WiFiClient probe;

  Serial.print("[PROBE] connect to ");
  Serial.print(ip);
  Serial.print(":");
  Serial.println(port);

  if (!probe.connect(ip, port)) {
    Serial.println("[PROBE] connect failed");
    return false;
  }

  Serial.println("[PROBE] connected");
  probe.stop();
  return true;
}

bool tcpEchoProbe(IPAddress ip, uint16_t port) {
  WiFiClient client;
  const char *msg = "PING_FROM_CLIENT\n";
  char rx[32];
  int idx = 0;

  Serial.print("[ECHO] connect to ");
  Serial.print(ip);
  Serial.print(":");
  Serial.println(port);

  if (!client.connect(ip, port)) {
    Serial.println("[ECHO] connect failed");
    return false;
  }

  client.print(msg);
  Serial.println("[ECHO] sent: PING_FROM_CLIENT");

  unsigned long start = millis();
  while ((millis() - start) < 2000) {
    while (client.available()) {
      int c = client.read();
      if (c < 0) {
        continue;
      }

      if (idx < (int)(sizeof(rx) - 1)) {
        rx[idx++] = (char)c;
      }

      if (c == '\n') {
        rx[idx] = '\0';
        Serial.print("[ECHO] received: ");
        Serial.print(rx);
        client.stop();
        return true;
      }
    }
    delay(5);
  }

  Serial.println("[ECHO] timeout waiting response");
  client.stop();
  return false;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println("WiFi TCP Diagnostic Client");

  while (status != WL_CONNECTED) {
    Serial.print("Connecting to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000);
  }

  printWifiStatus();
}

void loop() {
  Serial.println("------------------------------");
  printWifiStatus();

  bool connectOk = tcpConnectProbe(serverIp, tcpPort);
  bool echoOk = false;

  if (connectOk) {
    echoOk = tcpEchoProbe(serverIp, tcpPort);
  }

  Serial.print("[RESULT] connect=");
  Serial.print(connectOk ? "OK" : "FAIL");
  Serial.print(", echo=");
  Serial.println(echoOk ? "OK" : "FAIL");

  delay(3000);
}
