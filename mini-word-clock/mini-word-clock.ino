/*
This software was made out of complete spite for the maker of https://www.etsy.com/uk/listing/853880992/miniature-word-clock-set-in-turned-irish
who refused to give me the software for a corupt miniture word clock. So here is a complete and better firmware for a mini word clock.

This software is completely open-source under CC licence and does not require the above product to use.

All that is needed is an ESP8266 and a MAX7219 for this to work.

This code uses the Arduino Compiler to build.
*/

/*-------- Libraries ----------*/
#include <LedControl.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <FS.h>
#include <EEPROM.h>

const char *project = "Mini_WordClock";

// Define MAX7219 Pins
#define DIN 2
#define CLK 4
#define CS 0

/*-------- Timezone definitions ----------*/
// Each timezone value increments GMT for example 0 is GMT+0, 1 is GMT+1
#define EUROPE_LONDON 0
#define EUROPE_CENTRAL 1

/*-------- Misc definitions ----------*/
int currentBrightness = 0;
uint addr = 0;

struct
{
	int firstRun = -1;
	int brightness = -1;
	int timezone = -1;
} eeprom_data;

LedControl matrix = LedControl(DIN, CLK, CS, 1);

/*-------- NTP Server Details ----------*/
const unsigned int localPort = 2390;
IPAddress timeServerIP;
const char *ntpServerName = "uk.pool.ntp.org";
WiFiUDP udp;
WiFiManager wifiManager;
char timeZone[2] = "0"; // GMT+Val
time_t prevDisplay = 0;

/*-------- Web Server Code ----------*/
// Web Server Port
ESP8266WebServer server(80);

// Root Web Interface
void handleRoot()
{
	char temp[50000];

	snprintf(temp, 50000, "\
<!DOCTYPE html>\
<html lang=\"en\">\
	<head>\
		<meta charset=\"UTF-8\" />\
		<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\" />\
		<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\" />\
		<title>Mini Word Clock</title>\
		<style>\
			html,\
			body {\
				height: 100vh;\
				margin: 0;\
			}\
\
			body {\
				display: flex;\
				flex-direction: column;\
				align-content: center;\
				align-items: center;\
				justify-content: center;\
				font-family: \"Segoe UI\", Tahoma, Geneva, Verdana, sans-serif;\
			}\
\
			main {\
				display: block;\
				flex-direction: column;\
				padding: 1em;\
			}\
\
			header {\
				text-align: center;\
				font-family: \"Segoe UI\", Tahoma, Geneva, Verdana, sans-serif;\
				font-size: 2rem;\
			}\
\
			h2 {\
				position: absolute;\
				transform: translate(-0.2em, -2.25em);\
				background: white;\
				display: inline-block;\
				padding: 0 0.2em;\
			}\
\
			section {\
				position: relative;\
				margin-top: 3em;\
				padding: 1em 2em;\
				border-radius: 1em;\
				border: solid grey;\
			}\
\
			.submit {\
				display: flex;\
				justify-content: right;\
			}\
		</style>\
	</head>\
	<body>\
		<header>\
			<h1>Mini Word Clock</h1>\
		</header>\
		<main>\
			<section>\
				<h2>Time and Date</h2>\
				<form action=\"/time\">\
					<h3>Time Zone</h3>\
					<p>\
						Select the time zone that you want the clock to synchronise with.\
					</p>\
					<select name=\"timezone\">\
						<option value=\"0\">Europe/London</option>\
						<option value=\"1\">Europe/Central</option>\
					</select>\
					<div class=\"submit\">\
						<button type=\"submit\">Save Time and Date options</button>\
					</div>\
				</form>\
			</section>\
			<section>\
				<h2>System Options</h2>\
				<form action=\"/system\">\
					<h3>Brightness</h3>\
					<p>Select the intensity level of the screen, from (darkest) 0 to 15 (brightest!)</p>\
					<select name=\"brightness\">\
						<option value=\"\" disabled selected>No change (Currently %d)</option>\
						<option value=\"0\">0 (darkest)</option>\
						<option value=\"1\">1</option>\
						<option value=\"2\">2</option>\
						<option value=\"3\">3</option>\
						<option value=\"4\">4</option>\
						<option value=\"5\">5</option>\
						<option value=\"6\">6</option>\
						<option value=\"7\">7</option>\
						<option value=\"8\">8</option>\
						<option value=\"9\">9</option>\
						<option value=\"10\">10</option>\
						<option value=\"11\">11</option>\
						<option value=\"12\">12</option>\
						<option value=\"13\">13</option>\
						<option value=\"14\">14</option>\
						<option value=\"15\">15 (Brightest!)</option>\
					</select>\
					<div class=\"submit\">\
						<button type=\"submit\">Save System options</button>\
					</div>\
				</form>\
			</section>\
			<section>\
				<h2>Power & Reset</h2>\
				<form action=\"/reboot\">\
					<h3>Reboot</h3>\
					<p>Perform a system reboot</p>\
					<div class=\"submit\">\
						<button type=\"submit\">System Reboot</button>\
					</div>\
				</form>\
				<details>\
					<summary>Advanced Options</summary>\
					<form action=\"/reset\">\
						<h3>Reset</h3>\
						<p>Perform a system factory reset</p>\
						<div class=\"submit\">\
							<button type=\"submit\">System Reset</button>\
						</div>\
					</form>\
				</details>\
			</section>\
		</main>\
	</body>\
</html>\
",
			 currentBrightness);
	server.send(200, "text/html", temp);
}

/*-------- General Setup ----------*/
void setup()
{
	Serial.begin(115200);
	delay(500);
	Serial.println();
	Serial.println();
	Serial.println("Bootloader Handover Complete");
	Serial.println("Booting...");

	// EEPROM Setup
	Serial.println("Fetching Configuration...");
	EEPROM.begin(512);

	// Get current EEPROM values and set enviroment variables from EEPROM
	EEPROM.get(addr, eeprom_data);
	Serial.println("Current brightness value is: " + String(eeprom_data.brightness) + " Current Timezone is: " + String(eeprom_data.timezone));
	if (isFirstRun() == true)
	{
		Serial.println("First run detected!");
		Serial.println("Initial settings have been set");
		Serial.println("You can change the current settings using the Web Interface");
	}
	else
	{
		currentBrightness = eeprom_data.brightness;
		timeZone[2] = char(eeprom_data.timezone);
	}

	matrix.shutdown(0, false);
	matrix.setIntensity(0, currentBrightness);
	matrix.clearDisplay(0);
	SetWords("Boot");

	wifiManager.setMinimumSignalQuality(1);

	if (!wifiManager.autoConnect(project))
	{
		Serial.println("Failed to connect to WiFi");
		delay(3000);
		ESP.reset();
		delay(5000);
	}

	Serial.println("WiFi Connection Established");

	Serial.println("IP Details:");
	Serial.println(WiFi.localIP());
	Serial.println(WiFi.gatewayIP());
	Serial.println(WiFi.subnetMask());

	Serial.println("Starting UDP");
	udp.begin(localPort);
	Serial.print("Local port: ");
	Serial.println(udp.localPort());

	delay(100);
	server.on("/", handleRoot);
	server.on("/time", handleTimezone);
	server.on("/system", handleSystem);
	server.on("/reboot", handleReboot);
	server.on("/reset", handleReset);
	server.onNotFound(handleNotFound);
	server.begin();
	Serial.print("HTTP server started @ ");
	Serial.println(WiFi.localIP());
	Serial.println("");
}

bool isFirstRun()
{
	EEPROM.get(addr, eeprom_data);
	if (eeprom_data.firstRun == -1)
	{
		eeprom_data.brightness = currentBrightness;
		Serial.print("Default EEPROM Brightness Set: ");
		Serial.println(currentBrightness);
		
		int tempA;
		tempA = timeZone[2];
		eeprom_data.timezone = tempA;
		Serial.print("Default EEPROM TimeZone Set: ");
		Serial.println(tempA);

		eeprom_data.firstRun = 1;
		EEPROM.put(addr, eeprom_data);
		EEPROM.commit();
		return true;
	}
	else
	{
		return false;
	}
}


/*-------- WebUI Handles ----------*/
void handleTimezone()
{
	// TODO Add current timezone to WebUI
	String timezone = server.arg("timezone");
	int timezone_num = timezone.toInt();
	if (timezone_num == EUROPE_LONDON)
	{
		timeZone[2] = '0';
		Serial.println("EUROPE/LONDON TIMEZONE SET");
		handleEEPROMTimeZone();
	}
	else if (timezone_num == EUROPE_CENTRAL)
	{
		timeZone[2] = '1';
		Serial.println("EUROPE/CENTRAL TIMEZONE SET");
		handleEEPROMTimeZone();
	}
	server.sendHeader("Location", String("/"), true);
	server.send(302, "text/plain", "");
}

void handleEEPROMTimeZone(){
	int tempA;
	tempA = timeZone[2];
	eeprom_data.timezone = tempA;
	EEPROM.put(addr, eeprom_data);
	EEPROM.commit();
}

void handleSystem()
{
	String brightness = server.arg("brightness");
	int brightness_num = brightness.toInt();

	if (brightness_num >= 0 && brightness_num <= 15)
	{
		currentBrightness = brightness_num;
		eeprom_data.brightness = brightness_num;
		EEPROM.put(addr, eeprom_data);
		EEPROM.commit();
		matrix.setIntensity(0, currentBrightness);
	}

	server.sendHeader("Location", String("/"), true);
	server.send(302, "text/plain", "");
}

void handleReboot()
{
	server.send(200, "text/html", "\
	<html>\
		<body>\
			<h1>Reload in progress...</h1>\
			<script>\
				const tryReboot = () => {\
					fetch(window.location.origin)\
						.then(res => {\
							if (res.ok) {\
								window.location.href = \"/\"\
							} else {\
								setTimeout(() => {\
									tryReboot()\
								}, 1000)\
							}\
						})\
				};\
				tryReboot();\
			</script>\
		</body>\
	</html>\
	");
	delay(500);
	ESP.reset();
}

void handleReset()
{
	server.send(200, "text/plain", "Resetting system. Reconnect to the Mini Word Clock Wifi Hotspot to continue.");
	eeprom_data.firstRun = -1;
	EEPROM.put(addr, eeprom_data);
	EEPROM.commit();
	wifiManager.resetSettings();
	ESP.reset();
}

// 404 Handle
void handleNotFound()
{
	String message = "404 Not Found\n\n";
	message += "URI: ";
	message += server.uri();
	message += "\nMethod: ";
	message += (server.method() == HTTP_GET) ? "GET" : "POST";
	message += "\nArguments: ";
	message += server.args();
	message += "\n";

	for (uint8_t i = 0; i < server.args(); i++)
	{
		message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
	}

	server.send(404, "text/plain", message);
}

/*-------- Main Loop ----------*/
void loop()
{
	// Checks for NTP sync and updates clock
	server.handleClient();
	if (timeStatus() != timeNotSet)
	{
		if (now() != prevDisplay)
		{
			prevDisplay = now();

			SetDisplayTime();
		}
	}
	// Get NTP sync
	else
	{
		WiFi.hostByName(ntpServerName, timeServerIP);
		Serial.println("Waiting for NTP sync...");
		setSyncProvider(getNtpTime);
	}
}

/*-------- Checks for DST ----------*/
int GetSummOrWinterHour()
{
	bool summerTime;

	if (month() < 3 || month() > 10)
	{
		summerTime = false;
	}
	else if (month() > 3 && month() < 10)
	{
		summerTime = true;
	}
	else if (month() == 3 && (hour() + 24 * day()) >= (1 + String(timeZone).toInt() + 24 * (31 - (5 * year() / 4 + 4) % 7)) || month() == 10 && (hour() + 24 * day()) < (1 + String(timeZone).toInt() + 24 * (31 - (5 * year() / 4 + 1) % 7)))
	{
		summerTime = true;
	}
	else
	{
		summerTime = false;
	}

	if (summerTime)
	{
		int temp = hourFormat12() + 1;
		if (temp == 13)
		{
			return 1;
		}
		else
		{
			return temp;
		}
	}
	else
	{
		return hourFormat12();
	}
}

/*-------- Converts Time to Words ----------*/
void SetDisplayTime()
{
	matrix.clearDisplay(0);

	if (minute() >= 30 && minute() < 35)
	{
		SetWords("Half");
		SetWords("Past");
		SetWords(String(GetSummOrWinterHour()));
	}
	else if (minute() >= 35 && minute() < 40)
	{
		SetWords("25");
		SetWords("To");
		SetWords(GetTheHour());
	}
	else if (minute() >= 40 && minute() < 45)
	{
		SetWords("20");
		SetWords("To");
		SetWords(GetTheHour());
	}
	else if (minute() >= 45 && minute() < 50)
	{
		SetWords("15");
		SetWords("To");
		SetWords(GetTheHour());
	}
	else if (minute() >= 50 && minute() < 55)
	{
		SetWords("Top_10");
		SetWords("To");
		SetWords(GetTheHour());
	}

	else if (minute() >= 55)
	{
		SetWords("Top_5");
		SetWords("To");
		SetWords(GetTheHour());
	}
	else if (minute() >= 5 && minute() < 10)
	{
		SetWords("Top_5");
		SetWords("Past");
		SetWords(String(GetSummOrWinterHour()));
	}

	else if (minute() >= 10 && minute() < 15)
	{
		SetWords("Top_10");
		SetWords("Past");
		SetWords(String(GetSummOrWinterHour()));
	}

	else if (minute() >= 15 && minute() < 20)
	{
		SetWords("15");
		SetWords("Past");
		SetWords(String(GetSummOrWinterHour()));
	}
	else if (minute() >= 20 && minute() < 25)
	{
		SetWords("20");
		SetWords("Past");
		SetWords(String(GetSummOrWinterHour()));
	}
	else if (minute() >= 25 && minute() < 30)
	{
		SetWords("25");
		SetWords("Past");
		SetWords(String(GetSummOrWinterHour()));
	}
	else
	{
		SetWords(String(GetSummOrWinterHour()));
	}
}

/*-------- Hour Calculator ----------*/
String GetTheHour()
{
	int h = GetSummOrWinterHour() + 1;

	if (h > 12)
	{
		h = 1;
	}
	return String(h);
}

/*-------- Word Printer ----------*/
void SetWords(String word)
{
	if (word == "1")
	{
		matrix.setColumn(0, 0, B01001001);
	}
	else if (word == "2")
	{
		matrix.setColumn(0, 1, B11000000);
		matrix.setColumn(0, 0, B01000000);
	}
	else if (word == "3")
	{
		matrix.setColumn(0, 2, B00011111);
	}
	else if (word == "4")
	{
		matrix.setColumn(0, 0, B11110000);
	}
	else if (word == "5")
	{
		matrix.setColumn(0, 3, B11110000);
	}
	else if (word == "6")
	{
		matrix.setColumn(0, 2, B11100000);
	}
	else if (word == "7")
	{
		matrix.setColumn(0, 2, B10000000);
		matrix.setColumn(0, 1, B00001111);
	}
	else if (word == "8")
	{
		matrix.setColumn(0, 3, B00011111);
	}
	else if (word == "9")
	{
		matrix.setColumn(0, 0, B00001111);
	}
	else if (word == "10")
	{
		matrix.setColumn(0, 1, B10001001);
	}
	else if (word == "11")
	{
		matrix.setColumn(0, 1, B00111111);
	}
	else if (word == "12")
	{
		matrix.setColumn(0, 1, B11110110);
	}
	else if (word == "To")
	{
		matrix.setColumn(0, 4, B00001100);
	}
	else if (word == "Past")
	{
		matrix.setColumn(0, 4, B01111000);
	}
	else if (word == "Half")
	{
		matrix.setColumn(0, 5, B00001111);
	}
	else if (word == "15")
	{
		matrix.setColumn(0, 6, B11111110);
	}
	else if (word == "Top_5")
	{
		matrix.setColumn(0, 5, B11110000);
	}
	else if (word == "Top_10")
	{
		matrix.setColumn(0, 7, B01011000);
	}
	else if (word == "20")
	{
		matrix.setColumn(0, 7, B01111110);
	}
	else if (word == "25")
	{
		matrix.setColumn(0, 7, B01111110);
		matrix.setColumn(0, 5, B11110000);
	}
	else if (word == "Halb_4")
	{
		matrix.setColumn(0, 5, B11111111);
	}
	else if (word == "Boot")
	{
		matrix.setColumn(0, 0, B10000001);
		matrix.setColumn(0, 7, B10000001);
	}
}

/*-------- NTP Handler ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
	while (udp.parsePacket() > 0)
		;
	Serial.println("Sending NTP Request...");
	sendNTPpacket(timeServerIP);
	uint32_t beginWait = millis();
	while (millis() - beginWait < 1500)
	{
		int size = udp.parsePacket();
		if (size >= NTP_PACKET_SIZE)
		{
			Serial.println("Received NTP Response!");
			udp.read(packetBuffer, NTP_PACKET_SIZE);
			unsigned long secsSince1900;

			secsSince1900 = (unsigned long)packetBuffer[40] << 24;
			secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
			secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
			secsSince1900 |= (unsigned long)packetBuffer[43];
			return secsSince1900 - 2208988800UL + String(timeZone).toInt() * SECS_PER_HOUR;
		}
	}
	Serial.println("The NTP server failed to respond in a timely fassion");
	Serial.println("The NTP server could just be slow, or, check WiFi connection");
	return 0;
}

void sendNTPpacket(IPAddress &address)
{
	memset(packetBuffer, 0, NTP_PACKET_SIZE);

	packetBuffer[0] = 0b11100011;
	packetBuffer[1] = 0;
	packetBuffer[2] = 6;
	packetBuffer[3] = 0xEC;

	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;

	udp.beginPacket(address, 123);
	udp.write(packetBuffer, NTP_PACKET_SIZE);
	udp.endPacket();
}