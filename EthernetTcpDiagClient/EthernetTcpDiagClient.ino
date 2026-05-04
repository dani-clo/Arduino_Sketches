/*
  Ethernet TCP Diagnostic Client

  Standalone TCP diagnostic client to validate connectivity over Ethernet,
  using Zephyr's socket APIs.
*/

#include <SPI.h>
#if !defined(ARDUINO_ARCH_ZEPHYR)
#error "This example is only for ARDUINO_ARCH_ZEPHYR"
#endif

#include <ZephyrClient.h>
#include "ZephyrEthernet.h"

int status = 0;
const uint16_t tcpPort = 1502;
IPAddress serverIp(192, 168, 1, 13);

// Static fallback if DHCP is unavailable.
IPAddress localIp(192, 168, 1, 201);
IPAddress dnsIp(192, 168, 1, 1);

void printEthernetStatus() {
  Serial.print("Link: ");
  Serial.println(Ethernet.linkStatus() == LinkON ? "ON" : "OFF");

  Serial.print("Client IP: ");
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

bool tcpConnectProbe(IPAddress ip, uint16_t port) {
  ZephyrClient probe;

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
  ZephyrClient client;
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

  Serial.println("Ethernet TCP Diagnostic Client");

  if (!initEthernet()) {
    while (1) {
      delay(1000);
    }
  }

  printEthernetStatus();
}

void loop() {
  Serial.println("------------------------------");
  printEthernetStatus();

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
