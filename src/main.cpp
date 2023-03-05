#include <Arduino.h>
#include "WiFi.h"
#include "time.h"
#include "string.h"
#include <WebServer.h>
// #include <WiFiClient.h>
#include <ESPmDNS.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>

// WiFi information
String WiFi_SSID = "RadantWireless";
String WiFi_PWD = "122PutnamRoad";
/*
// Or
String WiFi_SSID = "guest";
String WiFi_PWD = "";
*/

// Webserver and page(s) constants
WebServer server(80);
const int PageRefreshInterval = 15; // seconds
const String NodeName = "Prototype ESP32 DHT22 Program"; // identified of the board and code
// const String NodeID = "0.2"; // Number and version ID, may not be useful
const String HostName = "Node01"; // Hostname used on network

// Date & Time placeholders
// char* DateTime;
String timeStamp;
String timeStamp2;
String dateStamp;
// char timeStamp[12];
//char dateStamp[11];

// Eastern time, NYC
const char *timezone = "EST5EDT,M3.2.0,M11.1.0";

// Sleep Mode settings
const int uS2S = 1000000; /* Conversion factor for micro seconds to seconds */
// const int* SleepTimer = 15; /* Time ESP32 will go to sleep, seconds */

//	DHT Configuration
/*	Must insert lines
	#include <Adafruit_Sensor.h>
	#include <DHT.h>
	call void readDHT22();
	in Setup, add 'Sensor1.begin(100)'
*/
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTPIN 22
DHT Sensor1(DHTPIN, DHTTYPE);
float TempValue1F, TempValue1C, HumidityValue1;

// Function Declarations
void WiFi_init();
void WiFi_connect();
void DeepSleep(int Duration);
boolean initTime();
boolean GetDateTime();
void readDHT22();
void initWebServer();
void htmlServeMain();
void htmlServeHostName();
void htmlServeTime();
void htmlServeRSSI();
void htmlServeTemp1F();
void htmlServeTemp1C();
void htmlServeRH1();
void htmlHandleNotFound();

void setup() {
	Serial.begin(115200);
	Serial.println("Starting Setup");
	WiFi_init();
	if (WiFi.status() != WL_CONNECTED) {
		WiFi_connect();
	}
	if (initTime() != true){
		Serial.println("Failed to sync to time server");
		DeepSleep(60);
	}
	initWebServer();
	Sensor1.begin(100); // 100 is just for testing, can be left blank .begin()
	Serial.println("Setup complete");
}

void loop() {
	server.handleClient();
	delay(500);
	// ESP.restart();
}

/*	This initializes the WiFi interface and attempts to make first
	connection. If unsuccessful, it puts the chip into Deep Sleep mode.
	This is also used if the WiFi_connect() function fails to reconnect */
void WiFi_init() {
	int i = 0;
	WiFi.mode(WIFI_STA);
	WiFi.begin(WiFi_SSID.c_str(), WiFi_PWD.c_str());
	delay(1000);
	while ((WiFi.status() != WL_CONNECTED) && (i < 30)) {
		// waits 60 seconds checking for connection every 2 seconds
		i++;
		Serial.print(".");
		delay(1000);
	}
	if (WiFi.status() != WL_CONNECTED) {
		// Sleep code here
		Serial.println("");
		Serial.println("No WiFi connection made");
		Serial.println("Entering Deep Sleep");
		DeepSleep(60); // Sleep for 60 seconds
	}
	else
	{
		Serial.println("WiFi Connected");
		Serial.print("Local IP: ");
		Serial.println(WiFi.localIP().toString().c_str());
		Serial.print("Signal Strength: ");
		Serial.println(WiFi.RSSI());
	}
}

/*	This checks for connectivity. If not connected it will
	attempt to reconnect. If that fails, it will go to DeepSleep */
void WiFi_connect() {
	int i = 0;
	// Try simple reconnect function
	if (WiFi.status() != WL_CONNECTED){
		WiFi.reconnect();
		delay(1000);
	}
	// If reconnect failed, disconnect
	if (WiFi.status() != WL_CONNECTED) {
		WiFi.disconnect();
		delay(1000);
	}
	// Now make connection again by calling WiFi_init()
	WiFi_init();
}

boolean GetDateTime() {
	struct tm timeinfo;
	if (!getLocalTime(&timeinfo)) {
		// Failed to get current time
		Serial.println("Failed to getLocalTime");
		return false;
	}
	char tmpTD[30];
	strftime(tmpTD,30, "%r", &timeinfo);
	timeStamp = tmpTD;
	strftime(tmpTD,30, "%F %T", &timeinfo);
	timeStamp2 = tmpTD;
	strftime(tmpTD,30, "%F", &timeinfo);
	dateStamp = tmpTD;
	// Serial.println("Currently: " + timeStamp + " " + dateStamp);
	// Serial.println("Acquired local Date and Time");
	return true;
}

void DeepSleep(int Duration) { //Duration in seconds
	if (Duration <= 0) { // If Duration is not greater than 0, default to 15
		Duration = 15;
	}
	Duration = Duration * uS2S;
	esp_sleep_enable_timer_wakeup(Duration);
	esp_deep_sleep_start();
}

/*	Initializes the time server and sets the local timezone
	to Eastern time */
boolean initTime() {
	struct tm timeinfo;
	configTime(0, 0, "pool.ntp.org");    // First connect to NTP server, with 0 TZ offset
	if (!getLocalTime(&timeinfo)) {
		// Failed to obtain time
		return false;
	}
	// Set timezone
	setenv("TZ", timezone, 1);
	tzset();
	return true;
}

/*	Initializes the Webserver, starts mDNS, and makes
	web page assignmentss based on call */
void initWebServer(){
	 // WebServer server(80);
	if (MDNS.begin(HostName.c_str())) {
		Serial.println("mDNS UDP Service has started.");
	}
	// Set HTML response pages below
	server.on("/", htmlServeMain);
	server.on("/hostname", htmlServeHostName);
	server.on("/time", htmlServeTime);
	server.on("/rssi", htmlServeRSSI);
	server.on("/temp1f", htmlServeTemp1F);
	server.on("/temp1c", htmlServeTemp1C);
	server.on("/rh1",htmlServeRH1);
	server.onNotFound(htmlHandleNotFound);
	server.begin();
}

void htmlServeMain() {
	readDHT22();
	String localIP = "http://";
	String page = "<!DOCTYPE html>\n";
	page +=" <meta http-equiv='refresh' content='";
	page += String(PageRefreshInterval) +"'/>\n";// how often page is refreshed
	if (GetDateTime() != true){
		page += "Error retreiving system time and date.";
	}
	else {
		page +="<html>\n";
		page +="<head>\n";
		page +="<title>";
		page += HostName + " Main Page";
		page +="</title>\n";
		page +="<head>\n";  
		page +="<body>\n";
		page +="<h3>" + NodeName + "</h3>";
		page +="</head>\n";
		// Show Host name, IP address, and MAC address
		localIP += String(WiFi.localIP().toString().c_str()) + "/";
		page += "<a href=\"" + localIP + "hostname" + "\">mDNS Hostname</a>" + ":  " + HostName + "<br/>";
		page += "Local IP: " + String(WiFi.localIP().toString().c_str()) + "<br/>";   // WiFi.localIP().toString() + "<br/>"; //  + String(WiFi.localIP().toString().c_str()) + "<br/>";
		page += "MAC: " + String(WiFi.macAddress()) + "<br/>";
		page += "******************************<br/>";
		page += "Current Date: " + dateStamp + "<br/>";
		page += "Current Time: " + timeStamp + "<br/>";
		page += "******************************<br/>";
		page += "Data Points:<br/>";
		page += "<a href=\"" + localIP + "rssi" + "\">Signal Strength</a>" + ":  " + String(WiFi.RSSI()) + "<br/>";
		page += "<a href=\"" + localIP + "time" + "\">Time Stamp</a>" + ":  " + timeStamp2 + "<br/>";
		page += "<a href=\"" + localIP + "temp1f" + "\">Temperature 1</a>" + ":  " + String(TempValue1F, 1) + "&degF" + "<br/>";
		page += "<a href=\"" + localIP + "temp1c" + "\">Temperature 1</a>" + ":  " + String(TempValue1C, 1) + "&degC" + "<br/>";
		page += "<a href=\"" + localIP + "rh1" + "\">Relative Humidity</a>" + ":  " + String(HumidityValue1, 1) + "%" + "<br/>";
		page +="</p>\n</body>";  
		page +="</html>"; // \n";  
	}
	server.send(200, "text/html", page);
}

void htmlServeHostName(){
	String page = "<!DOCTYPE html>\n";
	page += "<title>" + HostName + " Host Name Page</title>\n";
	page += HostName;
	server.send(200, "text/html", page);
}

void htmlServeTime(){
	String page = "<!DOCTYPE html>\n";
	page +=" <meta http-equiv='refresh' content='";
	page += String(PageRefreshInterval) +"'/>\n";// how often page is refreshed
	page += "<title>" + HostName + " Time Page</title>\n";
	if (GetDateTime() != true){
		page += "Error retreiving system time and date.";
	}
	else {
		// page += timeStamp + "<br/>";
		page += timeStamp2;
		page += "</h2></body>\n";
	}
	server.send(200, "text/html", page);
}

void htmlServeRSSI(){
	String page = "<!DOCTYPE html>\n";
	page +=" <meta http-equiv='refresh' content='";
	page += String(PageRefreshInterval) +"'/>\n";// how often page is refreshed
	page += "<title>" + HostName + " RSSI Page</title>\n";
	if (WiFi.status() != WL_CONNECTED){
		page += "Error retreiving WiFi RSSI signal strength.";
	}
	else
	{
		page += String(WiFi.RSSI()); // + "<br/>";
	}
	server.send(200, "text/html", page);
}

void htmlServeTemp1F(){
	String page = "<!DOCTYPE html>\n";
	page +=" <meta http-equiv='refresh' content='";
	page += String(PageRefreshInterval) +"'/>\n";// how often page is refreshed
	page += "<title>" + HostName + " Temperature &degF Page</title>\n";
	page += String(TempValue1F, 1); // + "<br/>";
	server.send(200, "text/html", page);
}

void htmlServeTemp1C(){
	String page = "<!DOCTYPE html>\n";
	page +=" <meta http-equiv='refresh' content='";
	page += String(PageRefreshInterval) +"'/>\n";// how often page is refreshed
	page += "<title>" + HostName + " Temperature &degC Page</title>\n";
	page += String(TempValue1C, 1); // + "<br/>";
	server.send(200, "text/html", page);
}

void htmlServeRH1(){
	String page = "<!DOCTYPE html>\n";
	page +=" <meta http-equiv='refresh' content='";
	page += String(PageRefreshInterval) +"'/>\n";// how often page is refreshed
	page += "<title>" + HostName + " Relative Humidity Page</title>\n";
	page += String(HumidityValue1, 1); // + "<br/>";
	server.send(200, "text/html", page);
}

void htmlHandleNotFound() {
	String page = "<title>" + HostName + " RSSI Page</title>\n";
	page += "File Not Found\n\n";
	page += "URI: ";
	page += server.uri();
	/*
	page += "\nMethod: ";
	page += (server.method() == HTTP_GET) ? "GET" : "POST";
	page += "\nArguments: ";
	page += server.args();
	messpageage += "\n";
	for (uint8_t i = 0; i < server.args(); i++) {
		page += " " + server.argName(i) + ": " + server.arg(i) + "\n";
		}
	*/
	server.send(404, "text/html", page);
}

void readDHT22(){
	HumidityValue1 = Sensor1.readHumidity(); // Reading humidity 
	TempValue1F = Sensor1.readTemperature(true); // Read temperature as Fahrenheit (isFahrenheit = true)
	TempValue1C = Sensor1.readTemperature(false); // Read temperature as Fahrenheit (isFahrenheit = false)
}