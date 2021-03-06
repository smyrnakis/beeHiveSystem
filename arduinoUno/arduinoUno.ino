// http://beehivesystem.ddns.net

#include <SoftwareSerial.h>
#include <GPRS_Shield_Arduino.h>
#include <sim900.h>
#include <HX711.h>
#include <Wire.h>
#include <DHT.h>

#include "secrets.h"


// ~~~ PIN declaration ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define PCBLED 13
#define ESPLED 13

// #define ANLG_IN A0
#define DHTPIN 5

#define HX711_CLK 2
#define HX711_DAT 3

#define PIN_TX_GSM 7	// 5	// yellow cable
#define PIN_RX_GSM 8	// 6	// green cable
#define PIN_TX_ESP 11
#define PIN_RX_ESP 12


// ~~~ Variables - constants ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
#define BAUDRATE 9600

// SIM connection info
// 0,1	--> connected
// 0,2	--> not connected, searching
// 0,4	--> unknown connection state
// 0,5	--> connected, roaming
#define SIM900checkNetReg "AT+CREG?"

// Signal quality in dB: [0-31] (higher: better)
#define SIM900checkSignal "AT+CSQ"

// SIM card number
#define SIM900simInfo "AT+CCID"

// Check if modem is ready
#define SIM900isReady "AT+CPIN?"

// Board info
#define SIM900boardInfo "ATI"

// Check if internet is connected
#define SIM900internet "AT+COPS?"

// Operators available in the network
#define SIM900operators "AT+COPS=?"

// Check battery level (2nd num: bat % , 3rd num: voltage in mV)
#define SIM900battery "AT+CBC"

// List UNREAD SMS
#define SIM900unreadSMS "AT+CMGL=\"REC UNREAD\""

// List READ SMS
#define SIM900readSMS "AT+CMGL=\"REC READ\""

// Delete ALL READ SMSs
#define SIM900delRead "AT+CMGD=1,1"

// Delete ALL SMSs
#define SIM900delAll "AT+CMGD=1,4"


const char* thingSpeakServer  = "api.thingspeak.com"; 	// 184.106.153.149
char apiKey[] = THINGSP_WR_APIKEY;						// API key w/ write access

int temperature = 0;
int humidity = 0;
float weight = 0.0;

unsigned int uploadInterval   = 0;
unsigned long currentMillis   = 0;
unsigned long startMillisTEMP = 0;
unsigned long startMillisDeb  = 0;
unsigned long startMillisInte = 0;
const unsigned int smsInterv  = 10000;
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

String phone_tolis = TOLIS_MOBILE;
String phone_nick = NICK_MOBILE;

bool allowSMS		= false;		// Debounce for SMS send
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
String dataToESP; 										// Content of data sent to ESP
String dataToSMS;										// String with outgoing SMS text

String cmd;
String inboundSerialESP;								// Used for communication across devices
String inboundSerialGSM;
String outboundSerialESP;
String outboundSerialGSM;


// ~~~ Initialising ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
DHT dht(DHTPIN, DHT11);
HX711 scale;

SoftwareSerial mySerialGSM(PIN_TX_GSM,PIN_RX_GSM);
SoftwareSerial mySerialESP(PIN_TX_ESP,PIN_RX_ESP);

GPRS gprs(PIN_TX_GSM,PIN_RX_GSM,BAUDRATE);


// ~~~ Initializing ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void setup() {
	pinMode(DHTPIN, INPUT);
	pinMode(PCBLED, OUTPUT);		// setting I/O
	pinMode(ESPLED, OUTPUT);

	digitalWrite(PCBLED, HIGH);		// turning LEDs OFF
	digitalWrite(ESPLED, HIGH);

	Serial.begin(BAUDRATE);			// starting serial
	Serial.println("Serial enabled.\n\r");
	delay(100);

	short gprsInitTimeout = 30;
	Serial.print("Initialising GPRS...");
	gprs.init();

	while((!gprs.checkPowerUp()) && (gprsInitTimeout > 0)) {
 		delay(1000);
		gprsInitTimeout--;
		Serial.print(".");
		gprs.init();
 	}
	if (gprs.checkPowerUp()) {
		Serial.println(" done\n\r");
	}
	else {
		Serial.println(" failed\n\r");
	}
	delay(100);

	Serial.print("Initialising DHT ...");
	dht.begin();
	Serial.println(" done\n\r");
	delay(100);

	Serial.print("Initialising scale ...");
	scale.begin(HX711_DAT, HX711_CLK);
	scale.set_scale(-101800);
	scale.tare();
	Serial.println(" done\n\r");
	delay(100);

	Serial.print("Configuring SIM900 ...");
	mySerialGSM.begin(BAUDRATE);
	delay(5);
	// Inform for new SMS w/ index number (default)
	// mySerialGSM.println("AT+CNMI=2,1,0,0,0");
	// Forward new SMS to Serial monitor
	mySerialGSM.println("AT+CNMI=2,2,0,0,0");
	mySerialGSM.end();
	Serial.println(" done\n\r");
	delay(100);

	mySerialGSM.flush();
	Serial.flush();
}


// ~~~ Main loop ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void loop() {
  	currentMillis = millis();

	// Send to serial whatever SIM900 says
	mySerialGSM.begin(BAUDRATE);
	delay(10);
	mySerialGSM.listen();
	if (mySerialGSM.available()) {
		// String readString;
		while (mySerialGSM.available()) {
			inboundSerialGSM = mySerialGSM.readString();
			// readString = mySerialGSM.readString();
			// inboundSerialGSM = mySerialGSM.readStringUntil('\r\n');
		}
		Serial.println("GSM >>>> ");
		Serial.println(inboundSerialGSM);
		// Serial.println(readString);
		// inboundSerialGSM = readString;			// <?<>?<?><?><?><?><?><?><?>
	}
	else {
		inboundSerialGSM = "\r\n\0";
	}
	mySerialGSM.end();

	// Send to serial whatever ESP8266 says
	mySerialESP.begin(BAUDRATE);
	delay(10);
	mySerialESP.listen();
	if (mySerialESP.available()) {
		// String readString;
		while (mySerialESP.available()) {
			inboundSerialESP = mySerialESP.readString();
			// readString = mySerialESP.readString();
			// inboundSerialESP = mySerialESP.readStringUntil('\r\n');
		}
		Serial.println("ESP >>>> ");
		Serial.println(inboundSerialESP);
		// Serial.println(readString);
		// if ((String(inboundSerialESP)).indexOf('report') > 0) {
		// 	Serial.println("Report requested!\r\n");
		// }
	}
	else {
		inboundSerialESP = "\r\n\0";
	}
	mySerialESP.end();

	// Send to devices whatever we send in serial
	// delay(10);
	if (Serial.available()) {
		delay(10);
		// String cmd = "";
		cmd = "";
		while (Serial.available()) {
			cmd += (char)Serial.read();
		}
		Serial.println();
    	Serial.print(">>>> ");
    	Serial.println(cmd);

		if (cmd.indexOf('AT') > 0) {
			mySerialGSM.begin(BAUDRATE);
			mySerialGSM.print(cmd);
			mySerialGSM.end();
		}
		else {
			mySerialESP.begin(BAUDRATE);
			mySerialESP.print(cmd);
			mySerialESP.end();
		}
	}


	if ((inboundSerialGSM.indexOf(phone_tolis) >= 0) || 
	    (inboundSerialGSM.indexOf(phone_nick) >= 0)) {
		Serial.println("User requested a report by SMS!\r\n");
		SMS_command = readSMS();
	}
	else {
		SMS_command = -1;
	}

	if (cmd.indexOf(smsUpload) >= 0) {
		getMeasurements();

		dataToESP = String(temperature);
		dataToESP += "&";
		dataToESP += String(humidity);
		dataToESP += "&";
		dataToESP += String(weight);
		dataToESP += "\r\n";

		mySerialESP.print(dataToESP);

		cmd = "";
	}

	if (cmd.indexOf(smsReport) >= 0) {

		SMS_command = 1000;

		cmd = "";
	}

	if (cmd.indexOf(smsUploadCancel) >= 0) {

		SMS_command = 0;

		cmd = "";
	}

	if (cmd.indexOf(smsUpload30) >= 0) {

		SMS_command = 30;

		cmd = "";
	}

	if (cmd.indexOf(smsUpload45) >= 0) {

		SMS_command = 45;

		cmd = "";
	}

	if (cmd.indexOf(smsUpload90) >= 0) {

		SMS_command = 90;

		cmd = "";
	}

	if (cmd.indexOf(smsUpload120) >= 0) {

		SMS_command = 120;

		cmd = "";
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
			if (allowSMS) {
				allowSMS = false;
				getMeasurements();
				sendSMS();
			}
		break;
		default:							// Invalid return code
			Serial.println("WARNING: unexpected readSMS() reply!");
		break;
	}


	if (uploadInterval == 1) {
		// Reset uploadInterval if request was to upload once
		uploadInterval = 0;

		Serial.println("Uploading once (WiFi)...\r\n");

		getMeasurements();
		if (!gprsMode)	// Sending data using WiFi
		{
			dataToESP = String(temperature);
			dataToESP += "&";
			dataToESP += String(humidity);
			dataToESP += "&";
			dataToESP += String(weight);
			dataToESP += "\r\n";

			mySerialESP.print(dataToESP);
			// Serial.println(dataToESP);
		} else
		{
			// Send2ThingSpeakGPRS();
			Serial.println("Uploading once (GPRS) ...\r\n");
		}

	}
	else if (
		(uploadInterval != 0) && 
		(currentMillis - startMillisInte >= uploadInterval)
	) {

		Serial.println("Recurring upload (WiFi) ...\r\n");

		getMeasurements();
		if (!gprsMode)	// Sending data using WiFi
		{
			dataToESP = String(temperature);
			dataToESP += "&";
			dataToESP += String(humidity);
			dataToESP += "&";
			dataToESP += String(weight);
			dataToESP += "\r\n";

			mySerialESP.print(dataToESP);
			// Serial.println(dataToESP);
		} else
		{
			// Send2ThingSpeakGPRS();
			Serial.println("Recurring upload (GPRS) ...\r\n");
		}

		startMillisInte = currentMillis;
	}


	// Debounce every 10 sec
	if (currentMillis - startMillisDeb >= 10000) {
		allowSMS = true;
		// Serial.println("Debounce reset");
		startMillisDeb = currentMillis;
	}

	// delay(1);
}


// ~~~ Getting sensor data ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void getMeasurements() {
	digitalWrite(PCBLED, HIGH);

	// read values
	temperature = dht.readTemperature();
	delay(50);
	humidity = dht.readHumidity();
	delay(50);
	weight = fabs(scale.get_units(10));
	// weight = abs(scale.get_units(10));
	delay(50);


	// check values - build SMS text
	if (isnan(temperature)) {
		Serial.println("Failed to read temperature sensor!");
		temperature = -100;
	}
	else {
		Serial.print("Temperature: ");
		Serial.print(temperature);
		Serial.println(" °C");
	}

	if (isnan(humidity)) {
		Serial.println("Failed to read humidity sensor!");
		humidity = -100;
	}
	else {
		Serial.print("Humidity: ");
		Serial.print(humidity);
		Serial.println(" %");
	}

	if (isnan(weight)) {
		Serial.println("Failed to read weight sensor!");
		weight = -100;
	}
	else {
		Serial.print("Weight: ");
		Serial.print(weight);
		Serial.println(" kg");
	}

	// When no sensor data
	if ((temperature == -100) && (humidity == -100) && (weight == -100)) {
		Serial.println("Error reading sensors!");
	}

	digitalWrite(PCBLED, LOW);
}


// ~~~ Checking / Reading SMS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
int readSMS() {
	digitalWrite(ESPLED, HIGH);
	int returnValue = -2;

	if (inboundSerialGSM.indexOf(smsReport) >= 0) {
		Serial.println("Requested SMS report ...");
		returnValue = 1000;
	}
	else if (inboundSerialGSM.indexOf(smsUpload) >= 0) {
		Serial.println("Requested to upload once ...");
		returnValue = 1;
	}
	else if (inboundSerialGSM.indexOf(smsUpload30) >= 0) {
		Serial.println("Requested to upload every 30 seconds ...");
		returnValue = 30;
	}
	else if (inboundSerialGSM.indexOf(smsUpload45) >= 0) {
		Serial.println("Requested to upload every 45 seconds ...");
		returnValue = 45;
	}
	else if (inboundSerialGSM.indexOf(smsUpload90) >= 0) {
		Serial.println("Requested to upload every 90 seconds ...");
		returnValue = 90;
	}
	else if (inboundSerialGSM.indexOf(smsUpload120) >= 0) {
		Serial.println("Requested to upload every 120 seconds ...");
		returnValue = 120;
	}
	else if (inboundSerialGSM.indexOf(smsUploadCancel) >= 0) {
		Serial.println("Requested to cancel auto upload.");
		returnValue = 0;
	}
	else {
		Serial.println("Invalid SMS text.");
		returnValue = -2;
	}

	// delay(1000);
	mySerialGSM.println(SIM900delAll);

	digitalWrite(ESPLED, LOW);
	return returnValue;
}


// ~~~ Sending SMS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void sendSMS() {
	Serial.println("Sending SMS message ...");

	digitalWrite(ESPLED, HIGH);

	dataToSMS = "Temp: ";
	dataToSMS += String(temperature);
	dataToSMS += "C\r\n";
	dataToSMS += "Hum: ";
	dataToSMS += String(humidity);
	dataToSMS += "%\r\n";
	dataToSMS += "Wei: ";
	dataToSMS += String(weight);
	dataToSMS += "kg\r\n";
	// dataToSMS += "\0";

	// Configuring TEXT mode
	mySerialGSM.print("AT+CMGF=1\r");
	delay(100);
	// while(mySerialGSM.available()) {
	// 	Serial.write(mySerialGSM.read());
	// }

	// mySerialGSM.println("AT + CMGS = \"+3069XXXXXXXX\"");
	// mySerialGSM.println("AT + CMGS = \"+3069XXXXXXXX\"");
	// mySerialGSM.println("AT + CMGS = \"+3069XXXXXXXX\"");

	mySerialGSM.print("AT+CMGS=\"");
	// mySerialGSM.print(String(SMS_phone));
	// mySerialGSM.print(phone_tolis);
	mySerialGSM.print(phone_tolis);
	mySerialGSM.println("\"");
	// mySerialGSM.println("AT+CMGS=\"+3069XXXXXXXX\"");
	delay(100);
	
	// while(mySerialGSM.available()) {
	// 	Serial.write(mySerialGSM.read());
	// }

	// SMS content
	// mySerialGSM.print(beeHiveMessage);
	mySerialGSM.print(dataToSMS);
	delay(100);
	// while(mySerialGSM.available()) {
	// 	Serial.write(mySerialGSM.read());
	// }

	// Ctrl+Z character
	// mySerialGSM.println((char)26);
	mySerialGSM.write("26");
	// mySerialGSM.print("\x1A");
	// mySerialGSM.println( 0x1a );
	delay(100);
	mySerialGSM.println();
	delay(5000);

	// delay(1000);
	mySerialGSM.println(SIM900delAll);

	Serial.println("SMS sent.");

	digitalWrite(ESPLED, LOW);
}


// ~~~ Thingspeak GPRS ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
void Send2ThingSpeakGPRS() {
	digitalWrite(ESPLED, LOW);

	mySerialGSM.println("AT+CBAND=\"EGSM_DCS_MODE\"");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(200);

	//mySerialGSM.println("AT+IPR=9600");
	//if (printInSerial) { ShowSerialDataGSM(); }
	//delay(100);
	
	mySerialGSM.println("AT");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(200);

	mySerialGSM.println("AT+CREG?");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(200);

	//Set the connection type to GPRS	
	mySerialGSM.println("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(500);

	//APN Vodafone: 'internet.vodafone.gr'. APN Cosmote: 'internet', APN Q: myq
	// https://wiki.apnchanger.org/Greece
	mySerialGSM.println("AT+SAPBR=3,1,\"APN\",\"myq\"");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(500);

	//Enable the GPRS
	mySerialGSM.println("AT+SAPBR=1,1");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(3000);

	//Query if the connection is setup properly, if we get back a IP address then we can proceed
	mySerialGSM.println("AT+SAPBR=2,1");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(500);

	//We were allocated a IP address and now we can proceed by enabling the HTTP mode
	mySerialGSM.println("AT+HTTPINIT");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(500);
	
	//Start by setting up the HTTP bearer profile identifier
	mySerialGSM.println("AT+HTTPPARA=\"CID\",1");
	// if (printInSerial) { ShowSerialDataGSM(); }
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
	mySerialGSM.println(tempCall);
	//mySerialGSM.println("AT+HTTPPARA=\"URL\",\"http://api.thingspeak.com/update?api_key=THINGSP_WR_APIKEY&field1=22&field2=15&field3=10\"");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(500);
	
	//Start the HTTP GET session
	mySerialGSM.println("AT+HTTPACTION=0");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(500);
	
	//end of data sending
	mySerialGSM.println("AT+HTTPREAD");
	// if (printInSerial) { ShowSerialDataGSM(); }
	delay(100);
	digitalWrite(ESPLED, HIGH);
}