/*
  Web client

 This sketch connects to a website (http://example.com) using the WiFi module.

 This example is written for a network using WPA encryption. For
 WEP or WPA, change the Wifi.begin() call accordingly.

 created 13 July 2010
 by dlf (Metodo2 srl)
 modified 31 May 2012
 by Tom Igoe
 */

#include <ZephyrClient.h>
#include <WiFi.h>

#include "arduino_secrets.h"
///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;           // your network key Index number (needed only for WEP)

int status = WL_IDLE_STATUS;
// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
// IPAddress server(93,184,216,34);  // IP address for example.com (no DNS)
const char *server = "example.com";  // host name for example.com (using DNS)
const int concurrentConnections = 6;  // number of sockets opened in parallel
const int totalRounds = 4;           // number of bursts to run
const unsigned long roundDurationMs = 1200;
const unsigned long interRoundDelayMs = 500;
const int headerPaddingLength = 256;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ;  // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_SHIELD) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true)
      ;
  }

  // attempt to connect to Wifi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 3 seconds for connection:
    delay(3000);
  }
  Serial.println("Connected to wifi");
  printWifiStatus();

  Serial.println("\nStarting aggressive burst of concurrent network connections...");
  for (int round = 0; round < totalRounds; ++round) {
    Serial.print("Round ");
    Serial.print(round + 1);
    Serial.print("/");
    Serial.println(totalRounds);

    print_thread_stack_usage("before round");

    ZephyrClient clients[concurrentConnections];
    bool connected[concurrentConnections] = {false};

    for (int i = 0; i < concurrentConnections; ++i) {
      Serial.print("Connecting client ");
      Serial.print(i + 1);
      Serial.print("/");
      Serial.println(concurrentConnections);

      if (clients[i].connect(server, 80)) {
        connected[i] = true;
        Serial.print("client ");
        Serial.print(i + 1);
        Serial.println(" connected");

        clients[i].println("GET / HTTP/1.1");
        clients[i].print("Host: ");
        clients[i].println(server);
        clients[i].println("Connection: keep-alive");
        clients[i].print("X-Test-Load: ");
        for (int j = 0; j < headerPaddingLength; ++j) {
          clients[i].print('A');
        }
        clients[i].println();
        clients[i].println();
      } else {
        Serial.print("client ");
        Serial.print(i + 1);
        Serial.println(" connection failed");
      }
    }

    print_thread_stack_usage("after connect burst");

    unsigned long deadline = millis() + roundDurationMs;
    while (millis() < deadline) {
      for (int i = 0; i < concurrentConnections; ++i) {
        if (!connected[i]) {
          continue;
        }

        while (clients[i].available()) {
          char c = clients[i].read();
          (void)c;
        }

        if (!clients[i].connected()) {
          clients[i].stop();
          connected[i] = false;
        }
      }
      delay(10);
    }

    for (int i = 0; i < concurrentConnections; ++i) {
      if (connected[i]) {
        clients[i].stop();
      }
    }

    Serial.println("disconnecting clients.");
    print_thread_stack_usage("after disconnect burst");

    if (round < totalRounds - 1) {
      delay(interRoundDelayMs);
    }
  }

  Serial.println("Aggressive burst complete. The sketch will now stop.");
  while (true)
    ;
}

void loop() {
  // Intentionally left empty: the setup() function performs the network stress test.
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}
