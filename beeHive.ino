// ~ INPUTS ~
//  P? --> SWITCH gprsMode
// ~~~~~~~~~~~

// ~ OUTPUTS ~
//  P4 --> LED yellow
//  P5 --> LED red
//  P6 --> LED green
// ~~~~~~~~~~~

// ~ VARIOUS ~
//  P2 --> DHT sensor
//  P7 --> GSM Tx
//  P8 --> GSM Rx

//  P12 --> HX711 clk
//  P13 --> HX711 dat 
// ~~~~~~~~~~~

// http://beehivesystem.ddns.net

#include <DHT.h>
#include <ESP8266WiFi.h>

#include <HX711.h>

#include <GPRS_Shield_Arduino.h>
#include <sim900.h>

#include <SoftwareSerial.h>
#include <Wire.h>

#include "secrets.h"


// ~~~ PIN declaration ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define YELLOWLED 4
#define REDLED 5
#define GREENLED 6

#define DHTPIN 2

#define GSM_TX 7
#define GSM_RX 8

#define HX711_CLK 12
#define HX711_DAT 13


// ~~~ Variables - constants ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
const char* thingSpeakServer  = "api.thingspeak.com"; 	// "184.106.153.149"
char apiKey[] = THINGSP_WR_APIKEY;						// API key w/ Write access

float temperature;
float humidity;
float weight;

boolean gprsMode;  										// true: GPRS , false: WiFi - to be set using a switch

const char* smsReport = "report";						// --> reply back with SMS
const char* smsUpload = "upload";						// --> upload instantly - once
const char* smsUpload15 = "auto15";						// --> upload every 15 minutes
const char* smsUpload45 = "auto45";						// --> upload every 45 minutes
const char* smsUpload90 = "auto90";						// --> upload every 90 minutes
const char* smsUpload120 = "auto120";					// --> upload every 120 minutes
const char* smsUploadCancel = "autocancel";				// --> cancel auto upload

string beeHiveMessage; 									// The variable beeHiveMessage will inform the apiarist what
														// is going on with the weight, temperature and humidity


// ~~~ WiFi data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
char ssid[]              = NICK_HOME_SSID;
char password[]          = NICK_HOME_PASS;


// ~~~ Initialising ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
DHT dht(DHTPIN, DHT11,15);
HX711 scale(HX711_DAT,HX711_CLK);
WiFiClient client;


// ~~~ Initializing ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup() {
	gprsMode = false;				// true: GPRS , false: WiFi

	pinMode(YELLOWLED, OUTPUT)		// setting LED pins
	pinMode(REDLED, OUTPUT);
	pinMode(GREENLED, OUTPUT);

	digitalWrite(YELLOWLED, LOW);	// turning LEDs OFF
	digitalWrite(REDLED, LOW);
	digitalWrite(GREENLED, LOW);

	Serial.begin(115200);			// starting serial
	delay(100);
	
	dht.begin();					// starting DHT sensor
	delay(100);
	
	scale.set_scale(-101800);		// starting DHT chip
	scale.tare();
	delay(100);

	temperature = 0;
	humidity = 0;
	weight = 0;

	numOf1Kdelay = 0;				// timmer for minutes delay
	
	if (!gprsMode)
	{
		Serial.println();
		Serial.println();
		Serial.print("Connecting to ");
		Serial.print(ssid);
		Serial.print(" ");

		WiFi.begin(ssid, password);

		while (WiFi.status() != WL_CONNECTED) 
		{
			delay(500);
			Serial.print(".");
		}
		Serial.println();
		Serial.println("WiFi connected.");
	} else
	{
		// GPRS mode
		// Actually, no need to start anything! The connection is 
		// starting just before transmitting data (inside the function)
	}
}


// ~~~ Main loop ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop() {

	// checking if WiFi OK (when in WiFi mode)
	if (!gprsMode)
	{
		if (WiFi.status() != WL_CONNECTED)
		{
			digitalWrite(REDLED, HIGH);
			Serial.println();
			Serial.println();
			Serial.print("WiFi disconnected! Reconnecting ");
			WiFi.begin(ssid, password);

			while (WiFi.status() != WL_CONNECTED) 
			{
				delay(500);
				Serial.print(".");
			}
			Serial.println();
			Serial.println("WiFi connected.");
			digitalWrite(REDLED, LOW);
		}
	}
	// checking if Mobile signal OK (when in GPRS mode)
	else
	{
		while (FALSE)				// <?><?><?><?><?><?><?><?><?><?><?><?><?><?> TO FIX
		{
			digitalWrite(REDLED, HIGH);
			Serial.println();
			Serial.println();
			Serial.print("No mobile signal!");
			Serial.println();
			Serial.println();
			digitalWrite(REDLED, LOW);
		}
	}

	// checking for SMS
	readSMS();


	/*  numOf1Kdelay :
		~~~~~~~~~~~~~~ update frequency can be 15 / 45 / 90 / 120 minutes ~~~~~~~~~~~~~~
		15'  :	0M	900K ms -->  15 x 60K |  60 x 15K | 180  x 5K | 900  x 1K 	ms_delay
		45'  :	2M	700K ms -->  45 x 60K | 180 x 15K | 540  x 5K | 2700 x 1K 	ms_delay
		90'	 : 	5M	400K ms -->  90 x 60K | 360 x 15K | 1080 x 5K | 5400 x 1K 	ms_delay
		120' :	7M	200K ms --> 120 x 60K | 480 x 15K | 1440 x 5K | 7200 x 1K 	ms_delay
		~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	*/

	// Counting auto-update interval time
	for (short i = 0; i < numOf1Kdelay; i++)
	{
		digitalWrite(GREENLED, HIGH);
		delay(1000);
		readSMS();		// in case of new SMS, auto-update can be updated
		digitalWrite(GREENLED, LOW);
	}


	// Auto-update set
	if (numOf1Kdelay != 0)
	{
		// get sensor data
		getMeasurements();
		
		digitalWrite(YELLOWLED, HIGH);
		if (!gprsMode)	// Sending data using WiFi
		{
			Send2ThingSpeakWiFi();
		} else			// Sending data using GPRS
		{
			Send2ThingSpeakGPRS();
		}
		digitalWrite(YELLOWLED, LOW);

		// Resetting interval when upload int was 1
		if (numOf1Kdelay == 1)
		{
			numOf1Kdelay = 0;
		}
	}
	// No auto-update - check for SMS every 30 sec
	else
	{
		digitalWrite(GREENLED, HIGH);
		delay(1000);
		digitalWrite(GREENLED, LOW);
		delay(29000);
		readSMS();
	}
}


// ~~~ Getting measurements ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void getMeasurements() {
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
		temperature = -10;
	}
	else
	{
		Serial.println("Temperature: ");
		Serial.print(temperature);
		Serial.print(" °C");
		beeHiveMessage += "Temp: " ; beeHiveMessage += String(temperature) ; beeHiveMessage += " °C\r\n" ;
	}

	if (isnan(humidity))
	{
		Serial.println("Failed to read humidity sensor!");
		humidity = -10;
	}
	else
	{
		Serial.println("Humidity: ");
		Serial.print(humidity);
		Serial.print(" %");
		beeHiveMessage += "Hum: " ; beeHiveMessage += String(humidity) ; beeHiveMessage += " %\r\n" ;
	}

	if (isnan(weight))
	{
		Serial.println("Failed to read weight sensor!");
		weight = -10;
	}
	else
	{
		Serial.println("Weight: ");
		Serial.print(weight);
		Serial.print(" kg");
		beeHiveMessage += "Weight: " ; beeHiveMessage += String(weight) ; beeHiveMessage += " kg\r\n" ;
	}
}


// ~~~ Checking / Reading SMS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void readSMS() {
    messageIndex = gprs.isSMSunread();

    Serial.print("Just checked for unread SMS "); 
    Serial.println(messageIndex);
    
	// While there is at least one UNREAD SMS, we will read it and delete it immediatly
	while (messageIndex > 0)
	{
		//Lets read the SMS
		gprs.readSMS(messageIndex, message, MESSAGE_LENGTH, phone, datetime);

		// If the substring is: smsReport OR smsUpload* 8 = 0 15 45 90 120
		if (strstr(message, smsReport) != NULL)
		{
			Serial.println("Reading sensors data ...");
			getMeasurements()
			Serial.println("Sending SMS message ...");
			digitalWrite(YELLOWLED, HIGH);
			gprs.sendSMS(phone,beeHiveMessage);
			digitalWrite(YELLOWLED, LOW);
		}
		else if (strstr(message, smsUpload) != NULL) 
		{
			numOf1Kdelay = 1;		// Upload data just once
			Serial.println("Uploading to ThingSpeak (one time) ...");
		}
		else if (strstr(message, smsUpload15) != NULL) 
		{
			numOf1Kdelay = 900; 	// 900 x 1K ms_delay = 15'
			Serial.println("Uploading to ThingSpeak every 15 minutes ...");
		}
		else if (strstr(message, smsUpload45) != NULL) 
		{
			numOf1Kdelay = 2700; 	// 2700 x 1K ms_delay = 45'
			Serial.println("Uploading to ThingSpeak every 45 minutes ...");
		}
		else if (strstr(message, smsUpload90) != NULL) 
		{
			numOf1Kdelay = 5400; 	// 5400 x 1K ms_delay = 90'
			Serial.println("Uploading to ThingSpeak every 90 minutes ...");
		}
		else if (strstr(message, smsUpload120) != NULL) 
		{
			numOf1Kdelay = 7200; 	// 7200 x 1K ms_delay = 120'
			Serial.println("Uploading to ThingSpeak every 120 minutes ...");
		}
		else if (strstr(message, smsUploadCancel) != NULL) 
		{
			numOf1Kdelay = 0; 		// canceling auto-update
			Serial.println("Auto upload canceled.");
		}
		else
		{
			//If there isn't any known word (or even a known phone number) in the received message, we won't answer!        
			Serial.print("Invalid SMS content \r\n");
			numOf1Kdelay = 0;		// Resetting / stopping upload interval
		}

		//In order not to full SIM Memory, it's better to delete it
		gprs.deleteSMS(messageIndex);
		Serial.print("From number: ");
		Serial.println(phone);  
		Serial.print("Message Index: ");
		Serial.println(messageIndex);        
		Serial.print("Recieved Message: ");
    	Serial.println(message);

    	// messageIndex = gprs.isSMSunread();					// <?><?><?><?><?><?><?<>?><?><?> DO I NEED TO CHECK AGAIN?
    	// Serial.println("Just checked again for unread SMS"); 
	}
	else {
		Serial.println("No unread SMS."); 
		numOf1Kdelay = 0;		// Resetting / stopping upload interval
	}
}


// ~~~ Thingspeak WiFi ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Send2ThingSpeakWiFi() {
	boolean printInSerial = false;			// true: printing in HW serial // <?><?><?><?><?><?><?><?><?><?><?>
	
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
}


// ~~~ Thingspeak GPRS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Send2ThingSpeakGPRS() {
  boolean printInSerial = false;			// true: printing in HW serial

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
}


// ~~~ Finding text possition ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int find_text(String needle, String haystack) {
  int foundpos = -1;
  for (int i = 0; i <= haystack.length() - needle.length(); i++) {
    if (haystack.substring(i,needle.length()+i) == needle) {
      foundpos = i;
    }
  }
  return foundpos;
}


// ~~~ Print SW serial into HW ~~~~~~~~~~~~~~~~~~~~~~~~~~~
void ShowSerialData() {
  while (mySerial.available() != 0)
    Serial.write(mySerial.read());
}
