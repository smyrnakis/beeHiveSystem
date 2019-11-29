// Uses serial monitor for communication with ESP8266
// Connect GND from the Arduiono to GND on the ESP8266
#include <Arduino.h>
#include <SoftwareSerial.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

#include "secrets.h"


#define TX_ESP D2
#define RX_ESP D3

String localIPaddress;

char defaultSSID[] = WIFI_DEFAULT_SSID;
char defaultPASS[] = WIFI_DEFAULT_PASS;

const char * thinkSpeakAPIurl = "api.thingspeak.com"; // "184.106.153.149" or api.thingspeak.com

WiFiClient client;

SoftwareSerial UNOSerial(TX_ESP, RX_ESP);

char sz[] = "temp;humidity;weight";
char * sensors[3];
int iterator;

unsigned long startMillis; //some global variables available anywhere in the program
unsigned long currentMillis;
const unsigned long blinkPeriod = 1000; //the value is a number of milliseconds

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600); // communication with the host computer
  delay(1000);

  // Start the software serial for communication with the Arduino Uno
  UNOSerial.begin(9600);

  WiFiManager wifiManager;
  //wifiManager.resetSettings();
  wifiManager.setConfigPortalTimeout(600);
  wifiManager.autoConnect(defaultSSID, defaultPASS);

  Serial.println("Connected to WiFi.");
  Serial.print("IP: ");
  localIPaddress = (WiFi.localIP()).toString();
  Serial.println(localIPaddress);

  startMillis = millis(); //initial start time

}

void loop() {
  currentMillis = millis();

  if (currentMillis - startMillis >= blinkPeriod) {
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
    startMillis = currentMillis;
  }

  if (UNOSerial.available()) {
    String readString;

    while (UNOSerial.available()) {
      readString = UNOSerial.readStringUntil('\r\n');

      // If there is at least one '&' symbol
      if ((String(readString)).indexOf('&') > 0) {
        // Convert from String Object to String.
        char buf[sizeof(sz)];
        readString.toCharArray(buf, sizeof(buf));
        char * p = buf;
        char * str;
        iterator = 0;
        while ((str = strtok_r(p, "&", & p)) != NULL) { // delimiter is the semicolon
          sensors[iterator] = str;
          ++iterator;
        }
        UNOSerial.println("Received");
        thingSpeakRequestBeeHive();
      }
    }
  }

  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  // listen for user input and send it to the Arduino Uno
  //if ( Serial.available() ){  UNOSerial.write( Serial.read() );  }
}
void thingSpeakRequestBeeHive() {
  client.stop();
  if (client.connect(thinkSpeakAPIurl, 80)) {
    char apiKeyBeehive[] = BEEHIVE_WR_APIKEY;

    String postStr = apiKeyBeehive;
    postStr += "&field1=";
    postStr += sensors[0];
    postStr += "&field2=";
    postStr += sensors[1];
    postStr += "&field3=";
    postStr += sensors[2];
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + (String) apiKeyBeehive + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
    client.stop();
    UNOSerial.println("Sent");
  } else {
    UNOSerial.println("ERROR: could not upload data to thingspeak (beehive)!");
  }
}