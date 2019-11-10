#include <Arduino.h>
#include <ESP8266WiFi.h>
// #include <DNSServer.h>
// #include <ESP8266WebServer.h>
#include <WiFiManager.h>
// #include <WiFiUdp.h>

#include <DHT.h>
#include <HX711.h>
#include <GPRS_Shield_Arduino.h>
#include <sim900.h>

#include <SoftwareSerial.h>
#include <Wire.h>

#include "secrets.h"


// ~~~ PIN declaration ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define PCBLED 16		// D0 / LED_BUILTIN
#define ESPLED 2 		// D4
#define GREEN_LED 4		// D2	old: 6 / CLK
#define BLUE_LED 0		// D3 	old: 4 D2
#define RED_LED 14		// D5	old: 5 D1

// #define ANLG_IN A0
#define DHTPIN 5 		// D1	old: 2
#define GSM_TX 7		// SD0	
#define GSM_RX 8		// SD1

#define HX711_CLK 12	// D6
#define HX711_DAT 13	// D7


// ~~~ Variables - constants ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const char* thingSpeakServer  = "api.thingspeak.com"; 	// 184.106.153.149
char apiKey[] = THINGSP_WR_APIKEY;						// API key w/ write access

char defaultSSID[] = WIFI_DEFAULT_SSID;
char defaultPASS[] = WIFI_DEFAULT_PASS;

float temperature = 0;
float humidity = 0;
float weight = 0;

unsigned int uploadInterval   = 0;
unsigned long currentMillis   = 0;
const unsigned int smsInterv  = 5000;
const unsigned int seconds30  = 30000;
const unsigned int seconds45  = 45000;
const unsigned int seconds90  = 90000;
const unsigned int seconds120 = 120000;

int SMS_command 		= 0;
int SMS_phone 			= 0;
int SMS_messageLength 	= 0;
String SMS_message 		= "";
String SMS_datetime 	= "";

bool gprsMode = false;  								// true: GPRS , false: WiFi

const char* smsReport		= "report";					// --> reply back with SMS
const char* smsUpload 		= "upload";					// --> upload instantly - once
const char* smsUpload30 	= "auto30";					// --> upload every 30 seconds
const char* smsUpload45 	= "auto45";					// --> upload every 45 seconds
const char* smsUpload90 	= "auto90";					// --> upload every 90 seconds
const char* smsUpload120 	= "auto120";				// --> upload every 120 seconds
const char* smsUploadCancel = "autocancel";				// --> cancel auto upload

string beeHiveMessage; 									// The variable beeHiveMessage will inform the apiarist what
														// is going on with the weight, temperature and humidity


// ~~~ WiFi data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~	// NOT NEEDED WITH WiFi Manager lib
// char ssid[]              = TOLIS_MOBILE_SSID;
// char password[]          = TOLIS_MOBILE_PASS;


// ~~~ Initialising ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// DHT dht(DHTPIN, DHT11);
DHT dht(DHTPIN, DHT11,15);
HX711 scale(HX711_DAT,HX711_CLK);
// ESP8266WebServer server(80);
WiFiClient client;


// ~~~ Initializing ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup() {
	// pinMode(DHTPIN, INPUT);
	pinMode(PCBLED, OUTPUT);		// setting I/O
	pinMode(ESPLED, OUTPUT);
	pinMode(BLUE_LED, OUTPUT)
	pinMode(RED_LED, OUTPUT);
	pinMode(GREEN_LED, OUTPUT);

	digitalWrite(PCBLED, HIGH);		// turning LEDs OFF
  	digitalWrite(ESPLED, HIGH);
	digitalWrite(BLUE_LED, LOW);
	digitalWrite(RED_LED, LOW);
	digitalWrite(GREEN_LED, LOW);

	Serial.begin(115200);			// starting serial
	delay(100);

	WiFiManager wifiManager;
	//wifiManager.resetSettings();
	wifiManager.setConfigPortalTimeout(120);  // 120 sec timeout for WiFi configuration
	wifiManager.autoConnect(defaultSSID, defaultPASS);

	Serial.println("Connected to WiFi.");
	Serial.print("IP: ");
	Serial.println(WiFi.localIP());
	Serial.println("\n\r");

	// server.on("/", handle_OnConnect);
	// server.on("/about", handle_OnConnectAbout);
	// server.onNotFound(handle_NotFound);
	
	// server.begin();
	// Serial.println("HTTP server starter on port 80.");

	delay(5000);
	if (WiFi.status() != WL_CONNECTED) {
		Serial.println("No WiFi. Enabling GPRS mode ...");
		gprsMode = true;
	}

	delay(400);

	dht.begin();					// starting DHT sensor
	delay(100);
	
	scale.set_scale(-101800);		// starting HX711 chip
	scale.tare();
	delay(100);
}


// ~~~ Main loop ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop() {

  	currentMillis = millis();

	// checking for SMS
	if (currentMillis % smsInterv == 0) {
		SMS_command = readSMS();
	}

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
	
	delay(10);	// to check if needed or not!
}


// ~~~ Getting sensor data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void getMeasurements() {
	digitalWrite(ESPLED, LOW);
	// reset values
	temperature = 0;
	humidity = 0;
	weight = 0;
	beeHiveMessage = "";
	
	// read values
	temperature = dht.readTemperature();
	delay(50);
	humidity = dht.readHumidity();
	delay(50);
	weight = scale.get_units(10);
	delay(50);


	// check values - build SMS text
	if (isnan(temperature))
	{
		Serial.println("Failed to read temperature sensor!");
		temperature = -100;
	}
	else
	{
		Serial.println("Temperature: ");
		Serial.print(temperature);
		Serial.print(" °C");
		beeHiveMessage += "Temp: ";
		beeHiveMessage += String(temperature);
		beeHiveMessage += " °C\r\n" ;
	}

	if (isnan(humidity))
	{
		Serial.println("Failed to read humidity sensor!");
		humidity = -100;
	}
	else
	{
		Serial.println("Humidity: ");
		Serial.print(humidity);
		Serial.print(" %");
		beeHiveMessage += "Hum: ";
		beeHiveMessage += String(humidity);
		beeHiveMessage += " %\r\n";
	}

	if (isnan(weight))
	{
		Serial.println("Failed to read weight sensor!");
		weight = -100;
	}
	else
	{
		Serial.println("Weight: ");
		Serial.print(weight);
		Serial.print(" kg");
		beeHiveMessage += "Weight: ";
		beeHiveMessage += String(weight);
		beeHiveMessage += " kg\r\n";
	}
	digitalWrite(ESPLED, HIGH);
}


// ~~~ Checking / Reading SMS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int readSMS() {
	digitalWrite(ESPLED, LOW);
	int returnValue = -2;
    short messageIndex = gprs.isSMSunread();

    // Serial.print("Checked for unread SMS: "); 
    // Serial.println(messageIndex);
    
	// At least one unread SMS
	while (messageIndex > 0) {

		Serial.print("Analysing SMS with index: "); 
		Serial.println(messageIndex);

		// Reading SMS
		gprs.readSMS(messageIndex, SMS_message, SMS_messageLength, SMS_phone, SMS_datetime);

		// Print SMS data
		Serial.print("From number: ");
		Serial.println(phone);  
		Serial.print("Message Index: ");
		Serial.println(messageIndex);        
		Serial.print("Recieved Message: ");
    	Serial.println(message);
		Serial.print("Timestamp: ");
    	Serial.println(datetime);

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

		// Deleting SMS
		Serial.println("Deleting current SMS ...");
		gprs.deleteSMS(messageIndex);

    	// messageIndex = gprs.isSMSunread();					// <?><?><?><?><?><?><?<>?><?><?> DO I NEED TO CHECK AGAIN?
    	// Serial.println("Just checked again for unread SMS"); 
	}
	else {
		// Serial.println("No unread SMS found.");
		returnValue = -1;
	}

	digitalWrite(ESPLED, HIGH);
	return returnValue;
}


// ~~~ Sending SMS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void sendSMS() {
	Serial.println("Sending SMS message ...");
	getMeasurements();
	gprs.sendSMS(SMS_phone,beeHiveMessage);
}

// ~~~ Thingspeak WiFi ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Send2ThingSpeakWiFi() {
	digitalWrite(ESPLED, LOW);
	bool printInSerial = false;			// true: printing in HW serial // <?><?><?><?><?><?><?><?><?><?><?>
	
	if (client.connect(thingSpeakServer,80)) { 
		Serial.println();
		Serial.println("Sending data to Thingspeak...")
		Serial.println();
		
		String postStr = apiKey;
		postStr +="&field1=";
		postStr += String(temperature);
		postStr +="&field2=";
		postStr += String(humidity);
		postStr +="&field3=";
		postStr += String(weight);
		postStr += "\r\n\r\n";

		client.print("POST /update HTTP/1.1\n");
		client.print("Host: api.thingspeak.com\n");
		client.print("Connection: close\n");
		client.print("X-THINGSPEAKAPIKEY: " + (String)apiKey + "\n");
		client.print("Content-Type: application/x-www-form-urlencoded\n");
		client.print("Content-Length: ");
		client.print(postStr.length());
		client.print("\n\n");
		client.print(postStr);

		Serial.println("Data sent to Thingspeak succesfully.");
		
		// curl -v --request POST --header "X-THINGSPEAKAPIKEY: THINGSP_WR_APIKEY" --data "field1=23&field2=70&field3=70" "http://api.thingspeak.com/update")
	}
	client.stop();
	digitalWrite(ESPLED, HIGH);
}


// ~~~ Thingspeak GPRS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Send2ThingSpeakGPRS() {
	digitalWrite(ESPLED, LOW);
	bool printInSerial = false;			// true: printing in HW serial

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
	mySerial.println("AT+SAPBR=3,1,\"APN\",\"internet\"");
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
	string tempCall;
	tempCall = "AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?api_key=";
	tempCall += (String)apiKey;
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


// ~~~ Print SW serial into HW ~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ShowSerialData() {
  while (mySerial.available() != 0)
    Serial.write(mySerial.read());
}