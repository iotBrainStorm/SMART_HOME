#include <Arduino.h>
#include <math.h>  // To calculate NTC
#include <WiFi.h>
#include <WiFiManager.h>        // To save wifi password, instead of hard coding
#include <ESPAsyncWebServer.h>  // For web server
#include <HTTPClient.h>         // Node-Red
#include <Wire.h>               // I2C communication
#include <SPI.h>
#include <EEPROM.h>          // Settings storage
#include <SPIFFS.h>          // File system
#include <HardwareSerial.h>  // Serial communication
#include "time.h"            // Time management
#include <U8g2lib.h>         // OLED display
#include <ArduinoJson.h>     // make json format
#include <Preferences.h>     // To store users' settings
#include <Adafruit_AHT10.h>  // For temperature, humidity
#include <Dusk2Dawn.h>       // For Sunset & Sunrise



#define INPUT_SWITCH_1 34
#define INPUT_SWITCH_2 35
#define INPUT_SWITCH_3 36
#define INPUT_SWITCH_4 39

#define OUTPUT_SWITCH_1 18
#define OUTPUT_SWITCH_2 19
#define OUTPUT_SWITCH_3 23
#define OUTPUT_SWITCH_4 5

#define LED 16
#define BUZZER 17






// //////////////////////   SUNRISE & SUNSET CALCULATION   //////////////////////

// void calculateSunriseSunset() {
//   struct tm timeinfo;
//   if (!getLocalTime(&timeinfo)) {
//     Serial.println("[SUN] Cannot calculate - time not available");
//     return;
//   }

//   int tzHours = (int)settings.tzOffset;
//   int tzRemainder = (int)((settings.tzOffset - tzHours) * 60);

//   Dusk2Dawn location(settings.latitude, settings.longitude, tzHours);

//   int year = timeinfo.tm_year + 1900;
//   int month = timeinfo.tm_mon + 1;
//   int day = timeinfo.tm_mday;

//   sunriseMinutes = location.sunrise(year, month, day, false) + tzRemainder;
//   sunsetMinutes = location.sunset(year, month, day, false) + tzRemainder;

//   Dusk2Dawn::min2str(sunriseStr, sunriseMinutes);
//   Dusk2Dawn::min2str(sunsetStr, sunsetMinutes);

//   lastCalcDay = day;

//   Serial.println("\n==============================");
//   Serial.println("  Sunrise & Sunset Calculated");
//   Serial.println("==============================");
//   Serial.printf("Date       : %02d/%02d/%04d\n", day, month, year);
//   Serial.printf("Latitude   : %.6f\n", settings.latitude);
//   Serial.printf("Longitude  : %.6f\n", settings.longitude);
//   Serial.printf("Timezone   : %.1f hrs\n", settings.tzOffset);
//   Serial.printf("Sunrise    : %s\n", sunriseStr);
//   Serial.printf("Sunset     : %s\n", sunsetStr);
//   Serial.println("==============================\n");
// }


// //////////////////////   WIFI SETUP   //////////////////////

// bool connectToSavedWiFi() {

//   Serial.println("\n==============================");
//   Serial.println("WiFi Connection Started");
//   Serial.println("==============================");

//   u8g2.clearBuffer();
//   u8g2.setFont(u8g2_font_6x12_tf);
//   u8g2.drawStr(0, 12, "Connecting WiFi...");
//   u8g2.sendBuffer();

//   WiFiManager wm;
//   bool success = false;

//   WiFi.mode(WIFI_STA);
//   WiFi.begin();

//   int attempts = 0;
//   const int MAX_ATTEMPTS = 5;

//   while (attempts < MAX_ATTEMPTS) {

//     char attemptStr[16];
//     snprintf(attemptStr, sizeof(attemptStr), "Attempt: %d/5", attempts + 1);

//     u8g2.drawStr(0, 24, attemptStr);
//     u8g2.sendBuffer();

//     Serial.printf("[INFO] %s\n", attemptStr);

//     if (WiFi.status() == WL_CONNECTED) {

//       Serial.println("\n[SUCCESS] Connected to Saved WiFi");
//       Serial.printf("SSID       : %s\n", WiFi.SSID().c_str());
//       Serial.printf("IP Address : %s\n", WiFi.localIP().toString().c_str());
//       Serial.println("==============================\n");

//       u8g2.clearBuffer();
//       u8g2.drawStr(0, 12, "WiFi Connected!");
//       u8g2.drawStr(0, 24, "SSID:");
//       u8g2.drawStr(0, 36, WiFi.SSID().c_str());
//       u8g2.drawStr(0, 48, "IP:");
//       u8g2.drawStr(0, 60, WiFi.localIP().toString().c_str());
//       u8g2.sendBuffer();

//       delay(2000);
//       return true;
//     }

//     delay(2000);
//     attempts++;
//   }

//   // Failed to connect
//   Serial.println("\n[WARNING] No saved WiFi found!");
//   Serial.println("[INFO] Starting Config Portal...");
//   Serial.println("AP SSID    : ESP HOME");
//   Serial.println("AP IP      : 192.168.4.1");
//   Serial.println("Timeout    : 60 seconds");
//   Serial.println("------------------------------");

//   u8g2.clearBuffer();
//   u8g2.drawStr(0, 12, "No Saved WiFi!");
//   u8g2.drawStr(0, 24, "Starting AP...");
//   u8g2.drawStr(0, 36, "AP IP:");
//   u8g2.drawStr(0, 48, "192.168.4.1");
//   u8g2.drawStr(0, 60, "Connect & Setup");
//   u8g2.sendBuffer();

//   delay(1500);

//   wm.setConfigPortalTimeout(60);
//   success = wm.autoConnect("ESP HOME");

//   if (success) {

//     Serial.println("\n[SUCCESS] WiFi Connected via Config Portal");
//     Serial.printf("SSID       : %s\n", WiFi.SSID().c_str());
//     Serial.printf("IP Address : %s\n", WiFi.localIP().toString().c_str());
//     Serial.println("==============================\n");

//     u8g2.clearBuffer();
//     u8g2.drawStr(0, 12, "WiFi Connected!");
//     u8g2.drawStr(0, 24, "SSID:");
//     u8g2.drawStr(0, 36, WiFi.SSID().c_str());
//     u8g2.drawStr(0, 48, "IP:");
//     u8g2.drawStr(0, 60, WiFi.localIP().toString().c_str());
//     u8g2.sendBuffer();

//     delay(2000);
//     return true;
//   } else {

//     Serial.println("\n[ERROR] Config Portal Timeout!");
//     Serial.println("Device not connected to WiFi.");
//     Serial.println("==============================\n");

//     u8g2.clearBuffer();
//     u8g2.drawStr(0, 12, "Time over!");
//     u8g2.sendBuffer();

//     delay(1000);
//     return false;  // Better logic than returning true
//   }
// }

// //////////////////////   TIME SETUP   //////////////////////

// void configDateTime() {

//   Serial.println("\n==============================");
//   Serial.println("Date & Time Configuration");
//   Serial.println("==============================");

//   if (WiFi.status() != WL_CONNECTED) {

//     Serial.println("[WARNING] WiFi not connected!");
//     Serial.println("[INFO] Running in offline mode.");
//     Serial.println("[INFO] Setting default time: 01/01/2025 12:00:00");

//     u8g2.clearBuffer();
//     u8g2.setFont(u8g2_font_6x12_tf);
//     u8g2.drawStr(0, 24, "No WiFi!");
//     u8g2.drawStr(0, 36, "Time not synced!");
//     u8g2.drawStr(0, 48, "Starting offline...");
//     u8g2.sendBuffer();

//     unsigned long start = millis();
//     while (millis() - start < 2000)
//       yield();

//     struct tm tm;
//     tm.tm_year = 2025 - 1900;
//     tm.tm_mon = 0;
//     tm.tm_mday = 1;
//     tm.tm_hour = 12;
//     tm.tm_min = 0;
//     tm.tm_sec = 0;

//     time_t t = mktime(&tm);
//     struct timeval now = { .tv_sec = t };
//     settimeofday(&now, nullptr);

//     Serial.println("[SUCCESS] Default time applied.");
//     Serial.println("==============================\n");
//     return;
//   }

//   Serial.println("[INFO] WiFi connected.");
//   Serial.println("[INFO] Starting NTP sync...");

//   u8g2.clearBuffer();
//   u8g2.setFont(u8g2_font_6x12_tf);
//   u8g2.drawStr(0, 12, "Syncing Time...");
//   u8g2.sendBuffer();

//   unsigned long start = millis();
//   while (millis() - start < 1000)
//     yield();

//   int attempts = 0;
//   const int MAX_ATTEMPTS = 5;
//   struct tm timeinfo;

//   while (attempts < MAX_ATTEMPTS) {

//     Serial.printf("[INFO] NTP Attempt %d/%d\n", attempts + 1, MAX_ATTEMPTS);

//     char attemptStr[16];
//     snprintf(attemptStr, sizeof(attemptStr), "Attempt: %d/5", attempts + 1);

//     u8g2.clearBuffer();
//     u8g2.drawStr(0, 12, "Syncing Time...");
//     u8g2.drawStr(0, 24, attemptStr);
//     u8g2.sendBuffer();

//     configTime(settings.gmtOffset, daylightOffset_sec, ntpServer);

//     start = millis();
//     while (millis() - start < 1000)
//       yield();

//     if (getLocalTime(&timeinfo)) {
//       Serial.println("[SUCCESS] Time synced from NTP server.");
//       break;
//     }

//     attempts++;
//   }

//   if (getLocalTime(&timeinfo)) {

//     char timeStr[16];
//     char dateStr[18];
//     char gmtStr[30];

//     strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
//     strftime(dateStr, sizeof(dateStr), "%d.%m.%Y", &timeinfo);
//     strftime(gmtStr, sizeof(gmtStr), "%z %Z", &timeinfo);

//     Serial.println("-------- Current Time --------");
//     Serial.printf("Time : %s\n", timeStr);
//     Serial.printf("Date : %s\n", dateStr);
//     Serial.printf("Zone : %s\n", gmtStr);
//     Serial.println("------------------------------");
//     Serial.println("==============================\n");

//     strftime(timeStr, sizeof(timeStr), "Time: %H:%M:%S", &timeinfo);
//     strftime(dateStr, sizeof(dateStr), "Date: %d.%m.%Y", &timeinfo);
//     strftime(gmtStr, sizeof(gmtStr), "GMT: %z %Z", &timeinfo);

//     u8g2.clearBuffer();
//     u8g2.setFont(u8g2_font_t0_14_tr);
//     u8g2.drawStr(0, 16, timeStr);
//     u8g2.drawStr(0, 32, dateStr);
//     u8g2.drawStr(0, 62, gmtStr);
//     u8g2.sendBuffer();

//     start = millis();
//     while (millis() - start < 2000)
//       yield();
//   } else {

//     Serial.println("[ERROR] NTP sync failed!");
//     Serial.println("[INFO] Applying default offline time.");
//     Serial.println("[INFO] Default: 01/01/2025 12:00:00");

//     struct tm tm;
//     tm.tm_year = 2025 - 1900;
//     tm.tm_mon = 0;
//     tm.tm_mday = 1;
//     tm.tm_hour = 12;
//     tm.tm_min = 0;
//     tm.tm_sec = 0;

//     time_t t = mktime(&tm);
//     struct timeval now = { .tv_sec = t };
//     settimeofday(&now, nullptr);

//     Serial.println("[SUCCESS] Default time applied.");
//     Serial.println("==============================\n");

//     u8g2.clearBuffer();
//     u8g2.drawStr(0, 12, "Time not synced!");
//     u8g2.drawStr(0, 24, "Check internet!");
//     u8g2.drawStr(0, 48, "Starting offline...");
//     u8g2.sendBuffer();

//     start = millis();
//     while (millis() - start < 2000)
//       yield();
//   }
// }




void setup() {
  Serial.begin(115200);

  pinMode(INPUT_SWITCH_1, INPUT);
  pinMode(INPUT_SWITCH_2, INPUT);
  pinMode(INPUT_SWITCH_3, INPUT);
  pinMode(INPUT_SWITCH_4, INPUT);

  pinMode(OUTPUT_SWITCH_1, OUTPUT);
  pinMode(OUTPUT_SWITCH_2, OUTPUT);
  pinMode(OUTPUT_SWITCH_3, OUTPUT);
  pinMode(OUTPUT_SWITCH_4, OUTPUT);

  digitalWrite(BUZZER, HIGH);
  delay(100);
  digitalWrite(BUZZER, LOW);
  delay(100);

  // // --- Storage Init ---
  // u8g2.clearBuffer();
  // u8g2.setFont(u8g2_font_t0_14_tr);
  // u8g2.drawStr(0, 18, "Settings Init:");
  // u8g2.sendBuffer();
  // Serial.println("\n==============================");
  // Serial.println("Settings Initialization");
  // Serial.println("==============================");
  // Serial.println("[INFO] Initializing EEPROM...");
  // delay(1000);
  // EEPROM.begin(512);
  // loadSettings();
  // Serial.println("[SUCCESS] EEPROM Initialized.");
  // Serial.printf("[INFO] EEPROM Size: %d bytes\n", 512);
  // Serial.println("==============================\n");
  // u8g2.clearBuffer(); // Recommended for clean update
  // u8g2.drawStr(0, 18, "Settings Init: OK");
  // u8g2.sendBuffer();
  // delay(1000);

  // // --- SPIFFS ---
  // u8g2.clearBuffer();
  // Serial.println("\n==============================");
  // Serial.println("SPIFFS Initialization");
  // Serial.println("==============================");
  // if (!SPIFFS.begin(true))
  // {
  //   u8g2.drawStr(0, 18, "SPIFFS: ERROR");
  //   Serial.println("[ERROR] SPIFFS Mount Failed!");
  //   Serial.println("[INFO] Filesystem not available.");
  //   Serial.println("==============================\n");
  // }
  // else
  // {
  //   u8g2.drawStr(0, 18, "SPIFFS: OK");
  //   Serial.println("[SUCCESS] SPIFFS Mounted Successfully.");
  //   Serial.println("[INFO] Listing Files:");
  //   Serial.println("------------------------------");
  //   File root = SPIFFS.open("/");
  //   File file = root.openNextFile();
  //   int fileCount = 0;
  //   while (file)
  //   {
  //     Serial.printf("File %02d : %s  |  Size: %d bytes\n",
  //                   fileCount + 1,
  //                   file.name(),
  //                   file.size());
  //     fileCount++;
  //     file = root.openNextFile();
  //   }
  //   Serial.println("------------------------------");
  //   Serial.printf("Total Files: %d\n", fileCount);
  //   Serial.println("==============================\n");
  //   root.close();
  // }
  // u8g2.sendBuffer();
  // delay(1000);

  //   // --- AHT Sensor Init ---
  // u8g2.clearBuffer();
  // Serial.println("\n==============================");
  // Serial.println("AHT10 Sensor Initialization");
  // Serial.println("==============================");
  // Serial.println("[INFO] Checking AHT10 sensor...");
  // Wire.begin(); // SDA, SCL default for ESP32
  // if (!aht.begin())
  // {
  //   u8g2.drawStr(0, 18, "AHT10: ERROR");
  //   Serial.println("[ERROR] AHT10 not detected!");
  //   Serial.println("[INFO] Check wiring (SDA/SCL) and power supply.");
  //   Serial.println("==============================\n");
  // }
  // else
  // {
  //   u8g2.drawStr(0, 18, "AHT10: OK");
  //   Serial.println("[SUCCESS] AHT10 detected successfully.");
  //   Serial.println("[INFO] Sensor ready for reading");
  //   Serial.println("==============================\n");
  // }
  // u8g2.sendBuffer();
  // delay(1000);

  // // --- WiFi Setup ---
  // connectToSavedWiFi();
  // delay(1000);

  // // --- Server Setup ---
  // if (WiFi.status() == WL_CONNECTED)
  // {
  //   Serial.println("\n==============================");
  //   Serial.println("Web Server Initialization");
  //   Serial.println("==============================");
  //   Serial.println("[INFO] WiFi Connected.");
  //   Serial.println("[INFO] Starting Web Server...");
  //   u8g2.clearBuffer();
  //   u8g2.setFont(u8g2_font_6x12_tf);
  //   u8g2.drawStr(0, 12, "Starting Server...");
  //   u8g2.sendBuffer();
  //   delay(300);

  //   setupWebServer();

  //   Serial.println("[SUCCESS] Web Server Started!");
  //   Serial.printf("SSID       : %s\n", WiFi.SSID().c_str());
  //   Serial.printf("Server IP  : %s\n", WiFi.localIP().toString().c_str());
  //   Serial.println("==============================\n");
  //   u8g2.clearBuffer();
  //   u8g2.drawStr(0, 12, "Server Started!");
  //   u8g2.drawStr(0, 24, "IP Address:");
  //   u8g2.drawStr(0, 36, WiFi.localIP().toString().c_str());
  //   u8g2.sendBuffer();
  //   delay(2000);
  // }
  // else
  // {
  //   Serial.println("\n==============================");
  //   Serial.println("Web Server Initialization");
  //   Serial.println("==============================");
  //   Serial.println("[ERROR] WiFi not connected!");
  //   Serial.println("[INFO] Server will auto-start once WiFi reconnects.");
  //   Serial.println("==============================\n");
  //   u8g2.clearBuffer();
  //   u8g2.drawStr(0, 12, "WiFi Failed!");
  //   u8g2.drawStr(0, 24, "Will auto-start");
  //   u8g2.drawStr(0, 36, "when connected");
  //   u8g2.sendBuffer();
  //   delay(2000);
  // }

  // // --- Time Setup ---
  // configDateTime();
  // delay(1000);

  // // --- Sunrise & Sunset Calculation ---
  // calculateSunriseSunset();
}



void loop() {
}