/*
  Ethernet TCP Diagnostic Server

  Standalone TCP diagnostic server to validate connectivity over Ethernet,
  using Zephyr's socket APIs.
*/

#include <SPI.h>
#if !defined(ARDUINO_ARCH_ZEPHYR)
#error "This example is only for ARDUINO_ARCH_ZEPHYR"
#endif

#include "ZephyrServer.h"
#include "ZephyrClient.h"
#include "ZephyrEthernet.h"

int status = 0;
const uint16_t tcpPort = 1502;

// Static fallback if DHCP is unavailable.
IPAddress localIp(192, 168, 1, 200);
IPAddress dnsIp(192, 168, 1, 1);

ZephyrServer tcpServer(tcpPort);

void printEthernetStatus() {
  Serial.print("Link: ");
  Serial.println(Ethernet.linkStatus() == LinkON ? "ON" : "OFF");

  Serial.print("IP Address: ");
  Serial.println(Ethernet.localIP());

  Serial.print("Gateway: ");
  Serial.println(Ethernet.gatewayIP());

  Serial.print("Subnet: ");
  Serial.println(Ethernet.subnetMask());
}

bool initEthernet() {
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    Serial.println("[ERR] Ethernet hardware not found");
    return false;
  }

  while (Ethernet.linkStatus() != LinkON) {
    Serial.println("Waiting for Ethernet link...");
    delay(250);
  }

  Serial.println("Starting Ethernet with DHCP...");
  status = Ethernet.begin();
  if (status == 0) {
    Serial.println("DHCP failed, using static fallback IP");
    Ethernet.begin(localIp, dnsIp);
  }

  delay(1000);
  return true;
}

bool localTcpSelfProbe(uint16_t port) {
  ZephyrClient probe;
  IPAddress self = Ethernet.localIP();

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
  ZephyrClient probe;
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

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ;
  }

  Serial.println("Ethernet TCP Diagnostic Server");

  if (!initEthernet()) {
    while (1) {
      delay(1000);
    }
  }

  printEthernetStatus();

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

  Serial.print("Self probes: local-ip=");
  Serial.print(selfOk ? "OK" : "FAIL");
  Serial.print(", loopback=");
  Serial.println(loopOk ? "OK" : "FAIL");

  Serial.println("Server ready");
}

void loop() {
  ZephyrClient client = tcpServer.accept();

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
