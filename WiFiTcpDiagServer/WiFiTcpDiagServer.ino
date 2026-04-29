/*
  WiFi TCP Diagnostic Server

  Standalone TCP diagnostic server to validate network reachability between
  boards without involving Modbus.
*/

#include <SPI.h>
#if !defined(ARDUINO_ARCH_ZEPHYR)
#error "This example is only for ARDUINO_ARCH_ZEPHYR"
#endif
#include <WiFi.h>
#include <ZephyrServer.h>
#include <ZephyrClient.h>

#include "arduino_secrets.h"

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

int status = WL_IDLE_STATUS;
const uint16_t tcpPort = 1502;

ZephyrServer tcpServer(tcpPort);

bool localTcpSelfProbe(uint16_t port) {
  WiFiClient probe;
  IPAddress self = WiFi.localIP();

  Serial.print("[SELF] probing ");
  Serial.print(self);
  Serial.print(":");
  Serial.println(port);

  if (!probe.connect(self, port)) {
    Serial.println("[SELF] failed");
    return false;
  }

  Serial.println("[SELF] connected");
  probe.stop();
  return true;
}

bool localTcpLoopbackProbe(uint16_t port) {
  WiFiClient probe;
  IPAddress loopback(127, 0, 0, 1);

  Serial.print("[LOOPBACK] probing ");
  Serial.print(loopback);
  Serial.print(":");
  Serial.println(port);

  if (!probe.connect(loopback, port)) {
    Serial.println("[LOOPBACK] failed");
    return false;
  }

  Serial.println("[LOOPBACK] connected");
  probe.stop();
  return true;
}

void printWifiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());

  Serial.print("Subnet: ");
  Serial.println(WiFi.subnetMask());

  Serial.print("RSSI: ");
  Serial.print(WiFi.RSSI());
  Serial.println(" dBm");
}

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println("WiFi TCP Diagnostic Server");

  while (status != WL_CONNECTED) {
    Serial.print("Connecting to SSID: ");
    Serial.println(ssid);
    status = WiFi.begin(ssid, pass);
    delay(5000);
  }

  printWifiStatus();

  Serial.print("Starting TCP server on port ");
  Serial.println(tcpPort);
  tcpServer.begin();

  if (!tcpServer) {
    Serial.println("[ERR] tcpServer begin failed (bind/listen)");
    while (1) {
      delay(1000);
    }
  }

  delay(200);
  bool selfOk = localTcpSelfProbe(tcpPort);
  bool loopOk = localTcpLoopbackProbe(tcpPort);

  Serial.print("Self probes: wifi-ip=");
  Serial.print(selfOk ? "OK" : "FAIL");
  Serial.print(", loopback=");
  Serial.println(loopOk ? "OK" : "FAIL");

  Serial.println("Server ready");
}

void loop() {
  WiFiClient client = tcpServer.available();

  if (!client) {
    delay(10);
    return;
  }

  Serial.println("[CONN] client connected");
  Serial.print("[CONN] remote IP: ");
  Serial.println(client.remoteIP());

  unsigned long lastRx = millis();

  while (client.connected()) {
    while (client.available()) {
      int c = client.read();
      if (c < 0) {
        break;
      }

      lastRx = millis();

      // Echo back payload so client can verify bidirectional traffic.
      client.write((uint8_t)c);

      if (c == '\n') {
        Serial.println("[RX] newline received and echoed");
      }
    }

    if (millis() - lastRx > 10000) {
      Serial.println("[CONN] idle timeout, closing client");
      client.stop();
      break;
    }

    delay(5);
  }

  Serial.println("[CONN] client disconnected");
}
