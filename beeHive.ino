//#include <ESP8266WiFi.h>
#include <math.h>
#include <SoftwareSerial.h>

#include <GPRS_Shield_Arduino.h>
#include <sim900.h>
#include <HX711.h>
#include <Wire.h>
#include <DHT.h>

#include <WiFiEspClient.h>
#include <WiFiEsp.h>
#include <WiFiEspUdp.h>
#include <PubSubClient.h>

#include "secrets.h"


// ~~~ PIN declaration ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define PCBLED 13
#define ESPLED 13

// #define ANLG_IN A0
//#define DHTPIN 7

#define HX711_CLK 2
#define HX711_DAT 3

#define PIN_TX 7	// 5	// yellow cable
#define PIN_RX 8	// 6	// green cable


// ~~~ Variables - constants ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define BAUDRATE 9600	// 115200

const char* thingSpeakServer  = "api.thingspeak.com"; 	// 184.106.153.149
char apiKey[] = THINGSP_WR_APIKEY;						// API key w/ write access

char defaultSSID[] = WIFI_DEFAULT_SSID;
char defaultPASS[] = WIFI_DEFAULT_PASS;

float temperature = 0.0;
float humidity = 0.0;
float weight = 0.0;

unsigned int uploadInterval   = 0;
unsigned long currentMillis   = 0;
const unsigned int smsInterv  = 5000;
const unsigned int seconds30  = 30000;
const unsigned int seconds45  = 45000;
const unsigned int seconds90  = 90000;
const unsigned int seconds120 = 120000;

int SMS_command = 0;				// After incoming SMS message

char SMS_phone[16];
char SMS_datetime[24];
#define MESSAGE_LENGTH 160			// SMS charachter limit		// int SMS_messageLength = 160;
char SMS_message[MESSAGE_LENGTH];	// Incoming SMS
int messageIndex = 0;				// Defined in the readSMS() func

bool gprsMode 		= false;		// True if no WiFi connection
bool printInSerial 	= true;			// Printing in HW serial

const char* smsReport		= "report";					// --> reply back with SMS
const char* smsUpload 		= "upload";					// --> upload instantly - once
const char* smsUpload30 	= "auto30";					// --> upload every 30 seconds
const char* smsUpload45 	= "auto45";					// --> upload every 45 seconds
const char* smsUpload90 	= "auto90";					// --> upload every 90 seconds
const char* smsUpload120 	= "auto120";				// --> upload every 120 seconds
const char* smsUploadCancel = "autocancel";				// --> cancel auto upload

char beeHiveMessage; 									// Contents of outgoing SMS message


// ~~~ WiFi data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	// NOT NEEDED WITH WiFi Manager lib
// char ssid[]              = TOLIS_MOBILE_SSID;
// char password[]          = TOLIS_MOBILE_PASS;


// ~~~ Initialising ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// // DHT dht(DHTPIN, DHT11);
// DHT dht(DHTPIN, DHT11, 15);				// commented on 23/11/2019
// HX711 scale(HX711_DAT,HX711_CLK);
HX711 scale;

SoftwareSerial mySerial(PIN_TX,PIN_RX);

GPRS gprs(PIN_TX,PIN_RX,BAUDRATE);


// ~~~ Initializing ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup() {
	// pinMode(DHTPIN, INPUT);
	pinMode(PCBLED, OUTPUT);		// setting I/O
	pinMode(ESPLED, OUTPUT);

	digitalWrite(PCBLED, HIGH);		// turning LEDs OFF
  	digitalWrite(ESPLED, HIGH);

	randomSeed(analogRead(0));

	Serial.begin(BAUDRATE);			// starting serial
	delay(100);

	short gprsInitTimeout = 30; 						// 60 seconds timeout

	Serial.print("Initializing GPRS...");

	while((!gprs.init()) && (gprsInitTimeout > 0)) {
 		gprsInitTimeout--;
		Serial.print(".");
		delay(1000);
 	}
  	
	if (gprs.checkPowerUp()) {
		Serial.println(" done!\n\r");
	}
	delay(100);

	mySerial.begin(BAUDRATE);
	Serial.println("Software serial enabled.\n\r");
	delay(100);

	// DHT dht(DHTPIN, DHT11);
	// DHT dht(DHTPIN, DHT11,15);
	// dht.begin();							// commented on 23/11/2019
	Serial.println("DHT initiated.\n\r");
	delay(10);

	// HX711 scale (HX711_DAT, HX711_CLK);
	// HX711 scale;
	scale.begin(HX711_DAT, HX711_CLK);
	scale.set_scale(-101800);
	scale.tare();
	Serial.println("Scale initiated and calibrated.\n\r");
	delay(10);
}


// ~~~ Main loop ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop() {

  	currentMillis = millis();

	// // checking for SMS
	// if (currentMillis % smsInterv == 0) {
	// 	SMS_command = readSMS();
	// }
	SMS_command = -1;

	switch (SMS_command) {
		case -2:							// Invalid SMS content
			;
		break;
		case -1:							// No unread SMS
			;
		break;
		case 0:								// Auto upload canceled
			uploadInterval = 0;
		break;
		case 1:								// Upload data once
			uploadInterval = 1;
		break;
		case 30:							// Upload every 30 seconds
			uploadInterval = seconds30;
		break;
		case 45:							// Upload every 45 seconds
			uploadInterval = seconds45;
		break;
		case 90:							// Upload every 90 seconds
			uploadInterval = seconds90;
		break;
		case 120:							// Upload every 120 seconds
			uploadInterval = seconds120;
		break;
		case 1000:							// Reply with SMS
			getMeasurements();
			sendSMS();
		break;
		default:							// Invalid return code
			Serial.println("WARNING: unexpected readSMS() reply!");
		break;
	}

	if (currentMillis % 2500 == 0) {
		getMeasurements();
		Serial.println("");
	}

	if (
		(uploadInterval != 0) && 
		(currentMillis % uploadInterval == 0)
	) {

		// Reset uploadInterval if request was to upload once
		if (uploadInterval == 1) {
			uploadInterval = 0;
		}

		getMeasurements();
		if (!gprsMode)	// Sending data using WiFi
		{
			Send2ThingSpeakWiFi();
		} else			// Sending data using GPRS
		{
			Send2ThingSpeakGPRS();
		}
	}
	
	// delay(10);	// to check if needed or not!
}


// ~~~ Getting sensor data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void getMeasurements() {
	digitalWrite(PCBLED, LOW);
	// reset values
	temperature = 0;
	humidity = 0;
	weight = 0;
	beeHiveMessage = '\0';
	
	// read values
	// temperature = dht.readTemperature();
	// delay(50);
	// humidity = dht.readHumidity();
	// delay(50);
	temperature = random(18, 24);
	humidity = random(29, 36);
	weight = abs(scale.get_units(10));
	delay(50);


	// check values - build SMS text
	if (isnan(temperature))
	{
		Serial.println("Failed to read temperature sensor!");
		temperature = -100;
	}
	else
	{
		Serial.print("Temperature: ");
		Serial.print(temperature);
		Serial.println(" °C");
		beeHiveMessage += 'Temp:   ';
		beeHiveMessage += (char)temperature;
		beeHiveMessage += ' °C\r\n' ;
	}

	if (isnan(humidity))
	{
		Serial.println("Failed to read humidity sensor!");
		humidity = -100;
	}
	else
	{
		Serial.print("Humidity: ");
		Serial.print(humidity);
		Serial.println(" %");
		beeHiveMessage += 'Hum:   e ';
		beeHiveMessage += (char)humidity;
		beeHiveMessage += ' %\r\n';
	}

	if (isnan(weight))
	{
		Serial.println("Failed to read weight sensor!");
		weight = -100;
	}
	else
	{
		Serial.print("Weight: ");
		Serial.print(weight);
		Serial.println(" kg");
		beeHiveMessage += 'Weight: ';
		beeHiveMessage += (char)weight;
		beeHiveMessage += ' kg\r\n';
	}

	// When no sensor data
	if ((temperature = -100) && (humidity = -100) && (weight = -100)) {
		beeHiveMessage += 'Error reading sensors!';
	}

	beeHiveMessage += '\0';
	digitalWrite(PCBLED, HIGH);
}


// ~~~ Checking / Reading SMS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int readSMS() {
	digitalWrite(ESPLED, LOW);
	int returnValue = -2;
    short messageIndex = gprs.isSMSunread();

    // Serial.print("Checked for unread SMS: "); 
    // Serial.println(messageIndex);
    
	// At least one unread SMS
	if (messageIndex = 0) {
		// Serial.println("No unread SMS found.");
		returnValue = -1;
	}
	else {
		while (messageIndex > 0) {

			Serial.print("Analysing SMS with index: "); 
			Serial.println(messageIndex);

			// Reading SMS
			gprs.readSMS(messageIndex, SMS_message, MESSAGE_LENGTH, SMS_phone, SMS_datetime);

			// Print SMS data
			Serial.print("From number: ");
			Serial.println(String(SMS_phone));  
			Serial.print("Message Index: ");
			Serial.println(String(messageIndex));        
			Serial.print("Recieved Message: ");
			Serial.println(String(SMS_message));
			Serial.print("Timestamp: ");
			Serial.println(String(SMS_datetime));

			// If the substring is: smsReport OR smsUpload* [Cancel 30 45 90 120]
			if (strstr(SMS_message, smsReport) != NULL) {
				Serial.println("Requested SMS report ...");
				returnValue = 1000;
			}
			else if (strstr(SMS_message, smsUpload) != NULL) {
				Serial.println("Requested to upload once ...");
				returnValue = 1;
			}
			else if (strstr(SMS_message, smsUpload30) != NULL) {
				Serial.println("Requested to upload every 30 seconds ...");
				returnValue = 30;
			}
			else if (strstr(SMS_message, smsUpload45) != NULL) 
			{
				Serial.println("Requested to upload every 45 seconds ...");
				returnValue = 45;
			}
			else if (strstr(SMS_message, smsUpload90) != NULL) 
			{
				Serial.println("Requested to upload every 90 seconds ...");
				returnValue = 90;
			}
			else if (strstr(SMS_message, smsUpload120) != NULL) 
			{
				Serial.println("Requested to upload every 120 seconds ...");
				returnValue = 120;
			}
			else if (strstr(SMS_message, smsUploadCancel) != NULL) 
			{
				Serial.println("Requested to cancel auto upload.");
				returnValue = 0;
			}
			else
			{
				Serial.println("Invalid SMS text.");
				returnValue = -2;
			}

			// Memory of Vodafone SIM can store up to 30 SMS
			// Deleting SMS
			Serial.println("Deleting current SMS ...");
			gprs.deleteSMS(messageIndex);

			messageIndex = gprs.isSMSunread();
			// Serial.println("Just checked again for unread SMS"); 
		}
	}
	digitalWrite(ESPLED, HIGH);
	return returnValue;
}


// ~~~ Sending SMS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void sendSMS() {
	Serial.println("Sending SMS message ...");
	// getMeasurements();

	digitalWrite(ESPLED, LOW);
	gprs.sendSMS(SMS_phone, &beeHiveMessage);
	digitalWrite(ESPLED, HIGH);
}

// ~~~ Thingspeak WiFi ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Send2ThingSpeakWiFi() {
	digitalWrite(ESPLED, LOW);

	// if (client.connect(thingSpeakServer,80)) { 
	// 	Serial.println();
	// 	Serial.println("Sending data to Thingspeak...");
	// 	Serial.println();
		
	// 	String postStr = apiKey;
	// 	postStr +="&field1=";
	// 	postStr += String(temperature);
	// 	postStr +="&field2=";
	// 	postStr += String(humidity);
	// 	postStr +="&field3=";
	// 	postStr += String(weight);
	// 	postStr += "\r\n\r\n";

	// 	client.print("POST /update HTTP/1.1\n");
	// 	client.print("Host: api.thingspeak.com\n");
	// 	client.print("Connection: close\n");
	// 	client.print("X-THINGSPEAKAPIKEY: " + (String)apiKey + "\n");
	// 	client.print("Content-Type: application/x-www-form-urlencoded\n");
	// 	client.print("Content-Length: ");
	// 	client.print(postStr.length());
	// 	client.print("\n\n");
	// 	client.print(postStr);

	// 	Serial.println("Data sent to Thingspeak succesfully.");
		
	// 	// curl -v --request POST --header "X-THINGSPEAKAPIKEY: THINGSP_WR_APIKEY" --data "field1=23&field2=70&field3=70" "http://api.thingspeak.com/update")
	// }
	// client.stop();
	digitalWrite(ESPLED, HIGH);
}


// ~~~ Thingspeak GPRS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Send2ThingSpeakGPRS() {
	digitalWrite(ESPLED, LOW);

	mySerial.println("AT+CBAND=\"EGSM_DCS_MODE\"");
	if (printInSerial) { ShowSerialData(); }
	delay(200);

	//mySerial.println("AT+IPR=9600");
	//if (printInSerial) { ShowSerialData(); }
	//delay(100);
	
	mySerial.println("AT");
	if (printInSerial) { ShowSerialData(); }
	delay(200);

	mySerial.println("AT+CREG?");
	if (printInSerial) { ShowSerialData(); }
	delay(200);

	//Set the connection type to GPRS	
	mySerial.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
	if (printInSerial) { ShowSerialData(); }
	delay(500);

	//APN for Vodafone Greece --> 'internet.vodafone.gr'. APN For Cosmote Greece --> 'internet'
	mySerial.println("AT+SAPBR=3,1,\"APN\",\"internet.vodafone.gr\"");
	if (printInSerial) { ShowSerialData(); }
	delay(500);

	//Enable the GPRS
	mySerial.println("AT+SAPBR=1,1");
	if (printInSerial) { ShowSerialData(); }
	delay(3000);

	//Query if the connection is setup properly, if we get back a IP address then we can proceed
	mySerial.println("AT+SAPBR=2,1");
	if (printInSerial) { ShowSerialData(); }
	delay(500);

	//We were allocated a IP address and now we can proceed by enabling the HTTP mode
	mySerial.println("AT+HTTPINIT");
	if (printInSerial) { ShowSerialData(); }
	delay(500);
	
	//Start by setting up the HTTP bearer profile identifier
	mySerial.println("AT+HTTPPARA=\"CID\",1");
	if (printInSerial) { ShowSerialData(); }
	delay(500);
	
	//Setting up the url to the 'thingspeak.com' address 
	String tempCall;
	tempCall = "AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?api_key=";
	tempCall += String(apiKey);
	tempCall += "&field1=";
	tempCall += String(temperature);
	tempCall += "&field2=";
	tempCall += String(humidity);
	tempCall += "&field3=";
	tempCall += String(weight);
	tempCall += "\"";
	mySerial.println(tempCall);
	//mySerial.println("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?api_key=THINGSP_WR_APIKEY&field1=22&field2=15&field3=10\"");
	if (printInSerial) { ShowSerialData(); }
	delay(500);
	
	//Start the HTTP GET session
	mySerial.println("AT+HTTPACTION=0");
	if (printInSerial) { ShowSerialData(); }
	delay(500);
	
	//end of data sending
	mySerial.println("AT+HTTPREAD");
	if (printInSerial) { ShowSerialData(); }
	delay(100);
	digitalWrite(ESPLED, HIGH);
}


// // ~~~ Finding text possition ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// int find_text(String needle, String haystack) {
//   int foundpos = -1;
//   for (int i = 0; i <= haystack.length() - needle.length(); i++) {
//     if (haystack.substring(i,needle.length()+i) == needle) {
//       foundpos = i;
//     }
//   }
//   return foundpos;
// }

// Serial print data
void serialPrintAll() {
  Serial.print("Temperature: ");
  Serial.print(String(temperature));
  Serial.println("°C");
  Serial.print("Humidity: ");
  Serial.print(String(humidity));
  Serial.println("%");
  Serial.print("Weight: ");
  Serial.print(String(weight));
  Serial.println(" kg");
  Serial.println();
}

// ~~~ Print SW serial into HW ~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ShowSerialData() {
  while (mySerial.available() != 0)
    Serial.write(mySerial.read());
}