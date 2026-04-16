#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <SPI.h>
#include <EEPROM.h>
#include <SPIFFS.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <Adafruit_AHT10.h>
#include <Dusk2Dawn.h>
#include "time.h"

//  PIN DEFINITIONS
#define INPUT_SWITCH_1 34
#define INPUT_SWITCH_2 35
#define INPUT_SWITCH_3 36
#define INPUT_SWITCH_4 39

#define OUTPUT_SWITCH_1 18
#define OUTPUT_SWITCH_2 19
#define OUTPUT_SWITCH_3 23
#define OUTPUT_SWITCH_4 5

#define LED_PIN 16
#define BUZZER_PIN 17
#define BOOT_BUTTON 0

#define NUM_SWITCHES 4

const int outPin[NUM_SWITCHES] = {OUTPUT_SWITCH_1, OUTPUT_SWITCH_2, OUTPUT_SWITCH_3, OUTPUT_SWITCH_4};
const int inPin[NUM_SWITCHES] = {INPUT_SWITCH_1, INPUT_SWITCH_2, INPUT_SWITCH_3, INPUT_SWITCH_4};

//  GLOBAL OBJECTS
AsyncWebServer server(80);
Preferences prefs;
Adafruit_AHT10 aht;

//  STATE VARIABLES
// Switch state
bool swState[NUM_SWITCHES] = {false, false, false, false};
String swName[NUM_SWITCHES] = {"Living Room", "Bedroom", "Kitchen", "Bathroom"};
String swIcon[NUM_SWITCHES] = {"home", "bedroom", "kitchen", "bathroom"};
String relayMode[NUM_SWITCHES] = {"off", "off", "off", "off"};

// Timers Ã¢â‚¬â€ volatile (RAM only, lost on power cut)
struct SwTimer
{
  bool active = false;
  unsigned long endMs = 0;
  bool targetState = false;
};
SwTimer swTimers[NUM_SWITCHES];

// WiFi
bool dhcpOn = true;
String sIp, sMask = "255.255.255.0", sGw, sDns = "8.8.8.8";
bool portalFlag = false;

// Firebase
bool fbOn = false;
String fbUrl, fbToken;

// Admin / Time
String ntpSrv = "pool.ntp.org";
String tzStr = "+05:30";
long gmtOff = 19800;
float geoLat = 0.0, geoLon = 0.0;
bool timeSynced = false;
bool ahtOk = false;
bool locOk = false;

// Sunrise / Sunset
int srMin = 0, ssMin = 0, lastCalcDay = -1;

// Input debounce
bool lastInState[NUM_SWITCHES];
unsigned long lastDbMs[NUM_SWITCHES] = {0};
const unsigned long DB_DELAY = 50;

// Loop intervals
unsigned long lastSchedCheck = 0;
unsigned long lastSensorCheck = 0;
int lastCheckedMinute = -1;

// Restart flag
bool restartFlag = false;
unsigned long restartAt = 0;
bool forgetWifiFlag = false;
unsigned long forgetWifiAt = 0;
bool coreRoutesOnly = false;

const uint32_t MIN_FREE_HEAP_FOR_EXTENDED_ROUTES = 38000;
const int WIFI_CONNECT_MAX_ATTEMPTS = 5;
const unsigned long WIFI_CONNECT_RETRY_MS = 2000;
const unsigned long WIFI_PORTAL_RECOVERY_RESTART_MS = 200;

//  BEEP + LED ON NVS CHANGE
void notifyStorage()
{
  digitalWrite(LED_PIN, HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(LED_PIN, LOW);
}

//  TIMEZONE PARSING  "+05:30" Ã¢â€ â€™ seconds
void parseTZ()
{
  int sign = 1;
  if (tzStr.startsWith("-"))
    sign = -1;
  int c = tzStr.indexOf(':');
  int h = 5, m = 30;
  if (c > 0)
  {
    h = tzStr.substring(1, c).toInt();
    m = tzStr.substring(c + 1).toInt();
  }
  gmtOff = sign * (h * 3600L + m * 60L);
}

//  LOAD ALL SETTINGS FROM NVS
void loadSwitchSettings()
{
  prefs.begin("sw", true);
  for (int i = 0; i < NUM_SWITCHES; i++)
  {
    swName[i] = prefs.getString(("n" + String(i)).c_str(), swName[i]);
    swIcon[i] = prefs.getString(("i" + String(i)).c_str(), swIcon[i]);
    relayMode[i] = prefs.getString(("r" + String(i)).c_str(), "off");
    if (relayMode[i] == "on")
      swState[i] = true;
    else if (relayMode[i] == "remember")
      swState[i] = prefs.getBool(("l" + String(i)).c_str(), false);
    else
      swState[i] = false;
    digitalWrite(outPin[i], swState[i] ? HIGH : LOW);
  }
  prefs.end();
}

void loadWifiSettings()
{
  prefs.begin("wfcfg", true);
  dhcpOn = prefs.getBool("dhcp", true);
  sIp = prefs.getString("sip", "");
  sMask = prefs.getString("mask", "255.255.255.0");
  sGw = prefs.getString("gw", "");
  sDns = prefs.getString("dns", "8.8.8.8");
  prefs.end();
}

bool getSavedWifiCredentials(String &ssid, String &pass)
{
  prefs.begin("wfcfg", true);
  ssid = prefs.getString("ssid", "");
  pass = prefs.getString("pass", "");
  prefs.end();
  return !ssid.isEmpty();
}

bool hasValidStaticConfig(IPAddress &ip, IPAddress &gateway, IPAddress &subnet, IPAddress &dns)
{
  return ip.fromString(sIp) && gateway.fromString(sGw) && subnet.fromString(sMask) && dns.fromString(sDns);
}

void applyStationIpConfig()
{
  WiFi.mode(WIFI_STA);

  if (dhcpOn)
  {
    IPAddress zero(0, 0, 0, 0);
    if (WiFi.config(zero, zero, zero, zero))
    {
      Serial.println("[WIFI] DHCP mode enabled");
    }
    else
    {
      Serial.println("[WIFI] Failed to enable DHCP mode");
    }
    return;
  }

  IPAddress ip, gateway, subnet, dns;
  if (!hasValidStaticConfig(ip, gateway, subnet, dns))
  {
    Serial.println("[WIFI] Invalid static IP settings, falling back to DHCP");
    IPAddress zero(0, 0, 0, 0);
    WiFi.config(zero, zero, zero, zero);
    return;
  }

  if (WiFi.config(ip, gateway, subnet, dns))
  {
    Serial.printf("[WIFI] Static IP configured: %s\n", ip.toString().c_str());
  }
  else
  {
    Serial.println("[WIFI] Failed to apply static IP settings");
  }
}

void saveCurrentWifiCredentials(bool notifyChange = false)
{
  String currentSsid = WiFi.SSID();
  if (currentSsid.isEmpty())
  {
    return;
  }

  prefs.begin("wfcfg", false);
  prefs.putString("ssid", currentSsid);
  prefs.putString("pass", WiFi.psk());
  prefs.end();

  if (notifyChange)
  {
    notifyStorage();
  }
}

void prepareWifiForConfigPortal()
{
  WiFi.disconnect(false, true);
  delay(200);
  WiFi.softAPdisconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);
  delay(100);
}

bool runWifiConfigPortal(bool notifyChange = false, bool restartOnSuccess = true)
{
  WiFiManager wm;
  bool success = false;

  wm.setConfigPortalTimeout(180); // 3 minutes
  wm.setAPCallback([](WiFiManager *myWiFiManager)
                   { Serial.println("[WIFI] Config portal ready - connect to ESP HOME and open 192.168.4.1"); });
  prepareWifiForConfigPortal();
  Serial.println("[WIFI] Starting forced config portal...");
  Serial.println("[WIFI] SSID: 'ESP HOME' | IP: 192.168.4.1");
  success = wm.startConfigPortal("ESP HOME");

  if (success)
  {
    Serial.println("\n[SUCCESS] WiFi Connected via Config Portal");
    Serial.printf("SSID : %s\n", WiFi.SSID().c_str());
    Serial.printf("IP Address : %s\n", WiFi.localIP().toString().c_str());
    Serial.println("==============================\n");
    saveCurrentWifiCredentials(notifyChange);
    if (restartOnSuccess)
    {
      Serial.println("[WIFI] Restarting to bring the web server up on the saved network...");
      delay(500);
      ESP.restart();
    }
    return true;
  }

  Serial.println("\n[ERROR] Config Portal Timeout!");
  Serial.println("Device not connected to WiFi.");
  Serial.println("==============================\n");
  delay(1000);
  return false;
}

void loadFbSettings()
{
  prefs.begin("fb", true);
  fbOn = prefs.getBool("en", false);
  fbUrl = prefs.getString("url", "");
  fbToken = prefs.getString("tok", "");
  prefs.end();
}

void loadAdminSettings()
{
  prefs.begin("admin", true);
  ntpSrv = prefs.getString("ntp", "pool.ntp.org");
  tzStr = prefs.getString("tz", "+05:30");
  geoLat = prefs.getFloat("lat", 0.0);
  geoLon = prefs.getFloat("lon", 0.0);
  prefs.end();
  locOk = (geoLat != 0.0 || geoLon != 0.0);
  parseTZ();
}

//  USER MANAGEMENT HELPERS
void initDefaultUser()
{
  prefs.begin("users", false);
  if (prefs.getInt("cnt", 0) == 0)
  {
    DynamicJsonDocument d(1024);
    d["id"] = "mrinal";
    d["pass"] = "1234";
    d["role"] = "admin";
    String j;
    serializeJson(d, j);
    prefs.putString("u0", j);
    prefs.putInt("cnt", 1);
  }
  prefs.end();
}

bool verifyAdmin(const String &u, const String &p)
{
  prefs.begin("users", true);
  int n = prefs.getInt("cnt", 0);
  for (int i = 0; i < n; i++)
  {
    String js = prefs.getString(("u" + String(i)).c_str(), "");
    if (js.isEmpty())
      continue;
    DynamicJsonDocument d(1024);
    deserializeJson(d, js);
    if (d["id"].as<String>() == u && d["pass"].as<String>() == p && d["role"].as<String>() == "admin")
    {
      prefs.end();
      return true;
    }
  }
  prefs.end();
  return false;
}

bool verifyLogin(const String &u, const String &p, String &role)
{
  prefs.begin("users", true);
  int n = prefs.getInt("cnt", 0);
  for (int i = 0; i < n; i++)
  {
    String js = prefs.getString(("u" + String(i)).c_str(), "");
    if (js.isEmpty())
      continue;
    DynamicJsonDocument d(1024);
    deserializeJson(d, js);
    if (d["id"].as<String>() == u && d["pass"].as<String>() == p)
    {
      role = d["role"].as<String>();
      prefs.end();
      return true;
    }
  }
  prefs.end();
  return false;
}

bool hasAdmin()
{
  prefs.begin("users", true);
  int n = prefs.getInt("cnt", 0);
  for (int i = 0; i < n; i++)
  {
    String js = prefs.getString(("u" + String(i)).c_str(), "");
    if (js.isEmpty())
      continue;
    DynamicJsonDocument d(1024);
    deserializeJson(d, js);
    if (d["role"].as<String>() == "admin")
    {
      prefs.end();
      return true;
    }
  }
  prefs.end();
  return false;
}

int countAdmins()
{
  prefs.begin("users", true);
  int n = prefs.getInt("cnt", 0), a = 0;
  for (int i = 0; i < n; i++)
  {
    String js = prefs.getString(("u" + String(i)).c_str(), "");
    if (js.isEmpty())
      continue;
    DynamicJsonDocument d(1024);
    deserializeJson(d, js);
    if (d["role"].as<String>() == "admin")
      a++;
  }
  prefs.end();
  return a;
}

//  SWITCH CONTROL
void setSwitch(int i, bool st)
{
  if (i < 0 || i >= NUM_SWITCHES)
    return;
  swState[i] = st;
  digitalWrite(outPin[i], st ? HIGH : LOW);
  if (relayMode[i] == "remember")
  {
    prefs.begin("sw", false);
    prefs.putBool(("l" + String(i)).c_str(), st);
    prefs.end();
  }
}

//  TIME SYNC
bool syncTime()
{
  if (WiFi.status() != WL_CONNECTED)
    return false;
  configTime(gmtOff, 0, ntpSrv.c_str());
  struct tm ti;
  for (int i = 0; i < 10; i++)
  {
    if (getLocalTime(&ti))
    {
      timeSynced = true;
      Serial.printf("[TIME] Synced: %02d:%02d:%02d %02d/%02d/%04d\n",
                    ti.tm_hour, ti.tm_min, ti.tm_sec, ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
      return true;
    }
    delay(500);
  }
  return false;
}

//  SUNRISE & SUNSET
void calcSunriseSunset()
{
  if (!locOk || !timeSynced)
    return;
  struct tm ti;
  if (!getLocalTime(&ti))
    return;
  int day = ti.tm_mday;
  if (day == lastCalcDay)
    return;

  int tzH = gmtOff / 3600;
  int tzM = (abs(gmtOff) % 3600) / 60;
  Dusk2Dawn loc(geoLat, geoLon, tzH);
  srMin = loc.sunrise(ti.tm_year + 1900, ti.tm_mon + 1, day, false) + tzM;
  ssMin = loc.sunset(ti.tm_year + 1900, ti.tm_mon + 1, day, false) + tzM;
  lastCalcDay = day;
  Serial.printf("[SUN] Rise=%02d:%02d  Set=%02d:%02d\n", srMin / 60, srMin % 60, ssMin / 60, ssMin % 60);
}

//  TIMER CHECK (loop)
void checkTimers()
{
  unsigned long now = millis();
  for (int i = 0; i < NUM_SWITCHES; i++)
  {
    if (swTimers[i].active && now >= swTimers[i].endMs)
    {
      setSwitch(i, swTimers[i].targetState);
      swTimers[i].active = false;
      Serial.printf("[TIMER] SW%d -> %s\n", i, swTimers[i].targetState ? "ON" : "OFF");
    }
  }
}

int parseClockMinutes(const String &timeText)
{
  if (timeText.length() < 5 || timeText.charAt(2) != ':')
    return -1;

  int hours = timeText.substring(0, 2).toInt();
  int mins = timeText.substring(3, 5).toInt();
  if (hours < 0 || hours > 23 || mins < 0 || mins > 59)
    return -1;

  return hours * 60 + mins;
}

String getScheduleActionValue(JsonObject obj, const char *defaultValue = "on")
{
  String action = obj["action"].as<String>();
  if (action != "on" && action != "off")
    action = defaultValue;
  return action;
}

String getRecurringStartTime(JsonObject obj)
{
  String timeText = obj["fromTime"].as<String>();
  if (timeText.isEmpty())
    timeText = obj["onTime"].as<String>();
  return timeText;
}

String getRecurringEndTime(JsonObject obj)
{
  String timeText = obj["toTime"].as<String>();
  if (timeText.isEmpty())
    timeText = obj["offTime"].as<String>();
  return timeText;
}

String getFutureStartTime(JsonObject obj)
{
  String timeText = obj["fromTime"].as<String>();
  if (timeText.isEmpty())
    timeText = obj["time"].as<String>();
  return timeText;
}

String getFutureEndTime(JsonObject obj)
{
  return obj["toTime"].as<String>();
}

bool rangesOverlapMinutes(int startA, int endA, int startB, int endB)
{
  return startA < endB && endA > startB;
}

bool scheduleDaysOverlap(JsonArray daysA, JsonArray daysB)
{
  for (JsonVariant dayA : daysA)
  {
    String dayText = dayA.as<String>();
    for (JsonVariant dayB : daysB)
    {
      if (dayText == dayB.as<String>())
        return true;
    }
  }
  return false;
}

bool validateRecurringSchedulesData(const String &data, String &error)
{
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, data))
  {
    error = "Invalid schedule data";
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull())
  {
    error = "Schedule data must be an array";
    return false;
  }

  for (int i = 0; i < arr.size(); i++)
  {
    JsonObject entry = arr[i].as<JsonObject>();
    if (!entry["enabled"].as<bool>())
      continue;

    String fromTime = getRecurringStartTime(entry);
    String toTime = getRecurringEndTime(entry);
    int fromMin = parseClockMinutes(fromTime);
    int toMin = parseClockMinutes(toTime);
    JsonArray days = entry["days"].as<JsonArray>();

    if (fromMin < 0 || toMin < 0)
    {
      error = "Each enabled schedule needs valid From and To times";
      return false;
    }
    if (toMin <= fromMin)
    {
      error = "Each enabled schedule needs a Time Range where To is after From";
      return false;
    }
    if (days.isNull() || days.size() == 0)
    {
      error = "Each enabled schedule needs at least one repeat day";
      return false;
    }
  }

  for (int i = 0; i < arr.size(); i++)
  {
    JsonObject entryA = arr[i].as<JsonObject>();
    if (!entryA["enabled"].as<bool>())
      continue;

    int startA = parseClockMinutes(getRecurringStartTime(entryA));
    int endA = parseClockMinutes(getRecurringEndTime(entryA));
    JsonArray daysA = entryA["days"].as<JsonArray>();

    for (int j = i + 1; j < arr.size(); j++)
    {
      JsonObject entryB = arr[j].as<JsonObject>();
      if (!entryB["enabled"].as<bool>())
        continue;

      int startB = parseClockMinutes(getRecurringStartTime(entryB));
      int endB = parseClockMinutes(getRecurringEndTime(entryB));
      JsonArray daysB = entryB["days"].as<JsonArray>();

      if (getScheduleActionValue(entryA, "on") == getScheduleActionValue(entryB, "on") && scheduleDaysOverlap(daysA, daysB) && rangesOverlapMinutes(startA, endA, startB, endB))
      {
        error = "Schedules with the same action cannot overlap on the same repeat days";
        return false;
      }
    }
  }

  return true;
}

bool validateFutureSchedulesData(const String &data, String &error)
{
  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, data))
  {
    error = "Invalid future schedule data";
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.isNull())
  {
    error = "Future schedule data must be an array";
    return false;
  }

  for (int i = 0; i < arr.size(); i++)
  {
    JsonObject entry = arr[i].as<JsonObject>();
    if (!entry["enabled"].as<bool>())
      continue;

    String date = entry["date"].as<String>();
    String fromTime = getFutureStartTime(entry);
    String toTime = getFutureEndTime(entry);
    bool legacyOneShot = !fromTime.isEmpty() && toTime.isEmpty() && entry["fromTime"].as<String>().isEmpty();
    int fromMin = parseClockMinutes(fromTime);

    if (date.length() < 10)
    {
      error = "Each enabled future schedule needs a valid date";
      return false;
    }
    if (fromMin < 0)
    {
      error = "Each enabled future schedule needs a valid From time";
      return false;
    }
    if (!legacyOneShot)
    {
      int toMin = parseClockMinutes(toTime);
      if (toMin < 0)
      {
        error = "Each enabled future schedule needs a valid To time";
        return false;
      }
      if (toMin <= fromMin)
      {
        error = "Each enabled future schedule needs a Time Range where To is after From";
        return false;
      }
    }
  }

  for (int i = 0; i < arr.size(); i++)
  {
    JsonObject entryA = arr[i].as<JsonObject>();
    if (!entryA["enabled"].as<bool>())
      continue;

    String dateA = entryA["date"].as<String>();
    int startA = parseClockMinutes(getFutureStartTime(entryA));
    String endTimeA = getFutureEndTime(entryA);
    int endA = endTimeA.isEmpty() ? startA + 1 : parseClockMinutes(endTimeA);

    for (int j = i + 1; j < arr.size(); j++)
    {
      JsonObject entryB = arr[j].as<JsonObject>();
      if (!entryB["enabled"].as<bool>())
        continue;

      if (dateA != entryB["date"].as<String>())
        continue;

      int startB = parseClockMinutes(getFutureStartTime(entryB));
      String endTimeB = getFutureEndTime(entryB);
      int endB = endTimeB.isEmpty() ? startB + 1 : parseClockMinutes(endTimeB);

      if (getScheduleActionValue(entryA, "on") == getScheduleActionValue(entryB, "on") && rangesOverlapMinutes(startA, endA, startB, endB))
      {
        error = "Future schedules with the same action cannot overlap on the same date";
        return false;
      }
    }
  }

  return true;
}

//  SCHEDULE CHECK (loop)
void checkSchedules()
{
  if (!timeSynced)
    return;
  struct tm ti;
  if (!getLocalTime(&ti))
    return;

  int curMin = ti.tm_hour * 60 + ti.tm_min;
  if (curMin == lastCheckedMinute)
    return;
  lastCheckedMinute = curMin;

  const char *dn[] = {"sun", "mon", "tue", "wed", "thu", "fri", "sat"};
  String today = dn[ti.tm_wday];

  for (int sw = 0; sw < NUM_SWITCHES; sw++)
  {
    // Regular schedules
    prefs.begin("sched", true);
    String sj = prefs.getString(("s" + String(sw)).c_str(), "[]");
    prefs.end();

    DynamicJsonDocument sd(2048);
    if (deserializeJson(sd, sj))
      continue;
    for (JsonObject o : sd.as<JsonArray>())
    {
      if (!o["enabled"].as<bool>())
        continue;
      bool dayOk = false;
      for (JsonVariant dv : o["days"].as<JsonArray>())
      {
        if (dv.as<String>() == today)
        {
          dayOk = true;
          break;
        }
      }
      if (!dayOk)
        continue;

      String action = getScheduleActionValue(o, "on");
      String fromT = getRecurringStartTime(o);
      int fromMin = parseClockMinutes(fromT);
      if (fromMin >= 0 && curMin == fromMin)
      {
        setSwitch(sw, action == "on");
      }

      String toT = getRecurringEndTime(o);
      int toMin = parseClockMinutes(toT);
      if (toMin >= 0 && curMin == toMin)
      {
        setSwitch(sw, action != "on");
      }
    }

    // Future schedules
    prefs.begin("fsched", false);
    String fj = prefs.getString(("f" + String(sw)).c_str(), "[]");
    DynamicJsonDocument fd(2048);
    if (deserializeJson(fd, fj))
    {
      prefs.end();
      continue;
    }

    char ds[11];
    sprintf(ds, "%04d-%02d-%02d", ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);
    char ts[6];
    sprintf(ts, "%02d:%02d", ti.tm_hour, ti.tm_min);
    JsonArray fa = fd.as<JsonArray>();
    bool mod = false;
    for (int x = fa.size() - 1; x >= 0; x--)
    {
      JsonObject fo = fa[x];
      if (!fo["enabled"].as<bool>())
        continue;

      if (fo["date"].as<String>() != String(ds))
        continue;

      String action = getScheduleActionValue(fo, "on");
      String fromT = getFutureStartTime(fo);
      String toT = getFutureEndTime(fo);
      int fromMin = parseClockMinutes(fromT);
      int toMin = parseClockMinutes(toT);
      bool removeEntry = false;

      if (fromMin >= 0 && curMin == fromMin)
      {
        setSwitch(sw, action == "on");
        if (toMin < 0)
        {
          removeEntry = true;
        }
      }

      if (toMin >= 0 && curMin == toMin)
      {
        setSwitch(sw, action != "on");
        removeEntry = true;
      }

      if (removeEntry)
      {
        fa.remove(x);
        mod = true;
      }
    }
    if (mod)
    {
      String nj;
      serializeJson(fd, nj);
      prefs.putString(("f" + String(sw)).c_str(), nj);
      notifyStorage();
    }
    prefs.end();
  }
}

//  SENSOR AUTOMATION CHECK (loop)
void checkSensors()
{
  if (!ahtOk)
    return;
  sensors_event_t hev, tev;
  aht.getEvent(&hev, &tev);
  float cTemp = tev.temperature;
  float cHum = hev.relative_humidity;

  for (int sw = 0; sw < NUM_SWITCHES; sw++)
  {
    prefs.begin("sensor", true);
    String tj = prefs.getString(("t" + String(sw)).c_str(), "");
    String hj = prefs.getString(("h" + String(sw)).c_str(), "");
    String sj = prefs.getString(("x" + String(sw)).c_str(), "");
    prefs.end();

    // Temperature
    if (!tj.isEmpty())
    {
      DynamicJsonDocument d(1024);
      if (!deserializeJson(d, tj) && d["enabled"].as<bool>())
      {
        String c = d["condition"].as<String>();
        bool act = false;
        if (c == "below")
          act = cTemp < d["value"].as<float>();
        else if (c == "above")
          act = cTemp > d["value"].as<float>();
        else if (c == "equal")
          act = abs(cTemp - d["value"].as<float>()) < 0.5;
        else if (c == "between")
          act = cTemp >= d["min"].as<float>() && cTemp <= d["max"].as<float>();
        if (act)
          setSwitch(sw, d["action"].as<String>() == "on");
      }
    }
    // Humidity
    if (!hj.isEmpty())
    {
      DynamicJsonDocument d(1024);
      if (!deserializeJson(d, hj) && d["enabled"].as<bool>())
      {
        String c = d["condition"].as<String>();
        bool act = false;
        if (c == "below")
          act = cHum < d["value"].as<float>();
        else if (c == "above")
          act = cHum > d["value"].as<float>();
        else if (c == "equal")
          act = abs(cHum - d["value"].as<float>()) < 0.5;
        else if (c == "between")
          act = cHum >= d["min"].as<float>() && cHum <= d["max"].as<float>();
        if (act)
          setSwitch(sw, d["action"].as<String>() == "on");
      }
    }
    // Sunrise/Sunset
    if (!sj.isEmpty() && locOk && timeSynced)
    {
      DynamicJsonDocument d(1024);
      if (!deserializeJson(d, sj) && d["enabled"].as<bool>())
      {
        struct tm ti;
        if (getLocalTime(&ti))
        {
          int now = ti.tm_hour * 60 + ti.tm_min;
          int off = d["offset"].as<int>();
          String c = d["condition"].as<String>();
          bool act = false;
          if (c == "after_sunset")
            act = now >= ssMin + off;
          else if (c == "before_sunset")
            act = now <= ssMin + off;
          else if (c == "after_sunrise")
            act = now >= srMin + off;
          else if (c == "before_sunrise")
            act = now <= srMin + off;
          else if (c == "between_sunset_sunrise")
            act = now >= ssMin + off || now <= srMin + off;
          else if (c == "between_sunrise_sunset")
            act = now >= srMin + off && now <= ssMin + off;
          if (act)
            setSwitch(sw, d["action"].as<String>() == "on");
        }
      }
    }
  }
}

//  WiFi CONNECTION
// void connectWiFi() {
//   WiFi.mode(WIFI_STA);
//   if (!dhcpOn && sIp.length() > 0) {
//     IPAddress ip, gw, sn, dns;
//     ip.fromString(sIp);
//     gw.fromString(sGw);
//     sn.fromString(sMask);
//     dns.fromString(sDns);
//     WiFi.config(ip, gw, sn, dns);
//   }

//   // Try saved credentials first
//   prefs.begin("wfcfg", true);
//   String savedSsid = prefs.getString("ssid", "");
//   String savedPass = prefs.getString("pass", "");
//   prefs.end();

//   if (savedSsid.length() > 0) {
//     WiFi.begin(savedSsid.c_str(), savedPass.c_str());
//     Serial.print("[WIFI] Connecting to " + savedSsid);
//   } else {
//     WiFi.begin();
//     Serial.print("[WIFI] Connecting");
//   }

//   for (int i = 0; i < 20 && WiFi.status() != WL_CONNECTED; i++) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println();

//   if (WiFi.status() == WL_CONNECTED) {
//     Serial.printf("[WIFI] Connected: %s  IP: %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
//   } else {
//     Serial.println("[WIFI] Failed -> starting config portal...");
//     WiFiManager wm;
//     wm.setConfigPortalTimeout(180);
//     wm.setCaptivePortalEnable(true);
//     wm.setAPCallback([](WiFiManager *myWiFiManager) {
//       Serial.println("[WIFI] AP Started - connect to ESP HOME and go to 192.168.4.1");
//     });
//     if (!wm.autoConnect("ESP HOME")) {
//       Serial.println("[WIFI] Portal timeout - restarting...");
//       ESP.restart();
//     }
//     if (WiFi.status() == WL_CONNECTED) {
//       Serial.printf("[WIFI] Portal OK: %s\n", WiFi.localIP().toString().c_str());
//       // Save the credentials WiFiManager just connected with
//       prefs.begin("wfcfg", false);
//       prefs.putString("ssid", WiFi.SSID());
//       prefs.putString("pass", WiFi.psk());
//       prefs.end();
//     }
//   }
// }

bool connectToSavedWiFi()
{
  String savedSsid;
  String savedPass;

  Serial.println("\n==============================");
  Serial.println("WiFi Connection Started");
  Serial.println("==============================");

  if (!getSavedWifiCredentials(savedSsid, savedPass))
  {
    Serial.println("\n[WARNING] No saved WiFi found!");
    Serial.println("[INFO] Starting Config Portal...");
    return runWifiConfigPortal();
  }

  applyStationIpConfig();
  WiFi.disconnect(false, false);
  delay(200);
  WiFi.begin(savedSsid.c_str(), savedPass.c_str());

  int attempts = 0;

  while (attempts < WIFI_CONNECT_MAX_ATTEMPTS)
  {
    char attemptStr[16];
    snprintf(attemptStr, sizeof(attemptStr), "Attempt: %d/%d", attempts + 1, WIFI_CONNECT_MAX_ATTEMPTS);
    Serial.printf("[INFO] %s\n", attemptStr);

    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("\n[SUCCESS] Connected to Saved WiFi");
      Serial.printf("SSID       : %s\n", WiFi.SSID().c_str());
      Serial.printf("IP Address : %s\n", WiFi.localIP().toString().c_str());
      Serial.println("==============================\n");
      delay(2000);
      return true;
    }

    delay(WIFI_CONNECT_RETRY_MS);
    attempts++;
  }

  Serial.println("\n[WARNING] Saved WiFi connection failed!");
  Serial.println("[INFO] Starting Config Portal...");
  Serial.println("AP SSID    : ESP HOME");
  Serial.println("AP IP      : 192.168.4.1");
  Serial.println("Timeout    : 180 seconds");
  Serial.println("------------------------------");
  return runWifiConfigPortal();
}

//  PHYSICAL INPUT SWITCH HANDLING
void checkPhysicalSwitches()
{
  for (int i = 0; i < NUM_SWITCHES; i++)
  {
    bool reading = digitalRead(inPin[i]);
    if (reading != lastInState[i])
      lastDbMs[i] = millis();
    if ((millis() - lastDbMs[i]) > DB_DELAY && reading != lastInState[i])
    {
      lastInState[i] = reading;
      if (reading == LOW)
      {
        setSwitch(i, !swState[i]);
        Serial.printf("[SW] Physical toggle SW%d -> %s\n", i, swState[i] ? "ON" : "OFF");
      }
    }
    lastInState[i] = reading;
  }
}

void sendWebFile(AsyncWebServerRequest *request, const char *path, const char *contentType)
{
  String gzPath = String(path) + ".gz";

  if (SPIFFS.exists(gzPath))
  {
    Serial.printf("[HTTP] %s -> %s\n", request->url().c_str(), gzPath.c_str());
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, gzPath, contentType);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    return;
  }

  if (SPIFFS.exists(path))
  {
    Serial.printf("[HTTP] %s -> %s\n", request->url().c_str(), path);
    request->send(SPIFFS, path, contentType);
    return;
  }

  Serial.printf("[HTTP] Missing file for %s (expected %s or %s)\n",
                request->url().c_str(), path, gzPath.c_str());
  request->send(404, "text/plain", "File not found");
}

//  WEB SERVER: ALL API ENDPOINTS
void setupWebServer()
{

  // CORS
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  server.on("/ping", HTTP_GET, [](AsyncWebServerRequest *req)
            { req->send(200, "text/plain", "pong"); });

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *req)
            { sendWebFile(req, "/index.html", "text/html"); });

  server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *req)
            { sendWebFile(req, "/index.html", "text/html"); });

  server.on("/config.html", HTTP_GET, [](AsyncWebServerRequest *req)
            { sendWebFile(req, "/config.html", "text/html"); });

  server.on("/firebase.html", HTTP_GET, [](AsyncWebServerRequest *req)
            { sendWebFile(req, "/firebase.html", "text/html"); });

  server.on("/index.svg", HTTP_GET, [](AsyncWebServerRequest *req)
            { sendWebFile(req, "/index.svg", "image/svg+xml"); });

  server.on("/settings.svg", HTTP_GET, [](AsyncWebServerRequest *req)
            { sendWebFile(req, "/settings.svg", "image/svg+xml"); });

  server.on("/firebase.svg", HTTP_GET, [](AsyncWebServerRequest *req)
            { sendWebFile(req, "/firebase.svg", "image/svg+xml"); });

  // ──── STATUS ────
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *req)
            {
    DynamicJsonDocument d(1024);
    d["timeSynced"] = timeSynced;
    d["ahtOk"] = ahtOk;
    d["locOk"] = locOk;
    d["wifiOk"] = (WiFi.status() == WL_CONNECTED);
    d["fbOn"] = fbOn;
    d["mac"] = WiFi.macAddress();
    String r;
    serializeJson(d, r);
    req->send(200, "application/json", r); });

  // ──── LOGIN ────
  server.on("/api/login", HTTP_POST, [](AsyncWebServerRequest *req)
            {
    if (!req->hasParam("user", true) || !req->hasParam("pass", true)) {
      req->send(400, "application/json", "{\"error\":\"Missing credentials\"}");
      return;
    }
    String u = req->getParam("user", true)->value();
    String p = req->getParam("pass", true)->value();
    String role;
    if (verifyLogin(u, p, role))
      req->send(200, "application/json", "{\"ok\":true,\"role\":\"" + role + "\"}");
    else
      req->send(401, "application/json", "{\"error\":\"Invalid credentials\"}"); });

  // ──── SWITCHES ────
  server.on("/api/switches", HTTP_GET, [](AsyncWebServerRequest *req)
            {
    DynamicJsonDocument d(1024);
    JsonArray na = d["names"].to<JsonArray>();
    JsonArray ic = d["icons"].to<JsonArray>();
    JsonArray st = d["states"].to<JsonArray>();
    JsonArray rl = d["relays"].to<JsonArray>();
    for (int i = 0; i < NUM_SWITCHES; i++) {
      na.add(swName[i]);
      ic.add(swIcon[i]);
      st.add(swState[i]);
      rl.add(relayMode[i]);
    }
    String r;
    serializeJson(d, r);
    req->send(200, "application/json", r); });

  server.on("/api/switch/toggle", HTTP_POST, [](AsyncWebServerRequest *req)
            {
    if (!req->hasParam("index", true) || !req->hasParam("state", true)) {
      req->send(400, "application/json", "{\"error\":\"Missing params\"}");
      return;
    }
    int idx = req->getParam("index", true)->value().toInt();
    bool st = req->getParam("state", true)->value() == "true";
    setSwitch(idx, st);
    req->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/api/switches/names", HTTP_POST, [](AsyncWebServerRequest *req)
            {
    prefs.begin("sw", false);
    for (int i = 0; i < NUM_SWITCHES; i++) {
      String k = "name" + String(i);
      if (req->hasParam(k, true)) {
        swName[i] = req->getParam(k, true)->value();
        prefs.putString(("n" + String(i)).c_str(), swName[i]);
      }
    }
    prefs.end();
    notifyStorage();
    req->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/api/switches/icons", HTTP_POST, [](AsyncWebServerRequest *req)
            {
    prefs.begin("sw", false);
    for (int i = 0; i < NUM_SWITCHES; i++) {
      String k = "icon" + String(i);
      if (req->hasParam(k, true)) {
        swIcon[i] = req->getParam(k, true)->value();
        prefs.putString(("i" + String(i)).c_str(), swIcon[i]);
      }
    }
    prefs.end();
    notifyStorage();
    req->send(200, "application/json", "{\"ok\":true}"); });

  server.on("/api/switches/relay", HTTP_POST, [](AsyncWebServerRequest *req)
            {
    prefs.begin("sw", false);
    for (int i = 0; i < NUM_SWITCHES; i++) {
      String k = "relay" + String(i);
      if (req->hasParam(k, true)) {
        relayMode[i] = req->getParam(k, true)->value();
        prefs.putString(("r" + String(i)).c_str(), relayMode[i]);
      }
    }
    prefs.end();
    notifyStorage();
    req->send(200, "application/json", "{\"ok\":true}"); });

  uint32_t heapAfterCoreRoutes = ESP.getFreeHeap();
  if (heapAfterCoreRoutes < MIN_FREE_HEAP_FOR_EXTENDED_ROUTES)
  {
    coreRoutesOnly = true;
    Serial.printf("[SERVER] Low heap after core routes: %u bytes\n", heapAfterCoreRoutes);
    Serial.println("[SERVER] Starting in core-routes-only mode. Extended config APIs are skipped.");
  }
  else
  {
    coreRoutesOnly = false;

    // ──── TIMERS (volatile) ────
    server.on("/api/timer/set", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("sw", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing sw\"}");
        return;
      }
      int sw = req->getParam("sw", true)->value().toInt();
      int h = req->hasParam("h", true) ? req->getParam("h", true)->value().toInt() : 0;
      int m = req->hasParam("m", true) ? req->getParam("m", true)->value().toInt() : 0;
      int s = req->hasParam("s", true) ? req->getParam("s", true)->value().toInt() : 0;
      String act = req->hasParam("action", true) ? req->getParam("action", true)->value() : "off";
      unsigned long dur = (h * 3600UL + m * 60UL + s) * 1000UL;
      if (dur == 0 || sw < 0 || sw >= NUM_SWITCHES) {
        req->send(400, "application/json", "{\"error\":\"Invalid\"}");
        return;
      }
      swTimers[sw].active = true;
      swTimers[sw].endMs = millis() + dur;
      swTimers[sw].targetState = (act == "on");
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/timers", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      DynamicJsonDocument d(1024);
      JsonArray timers = d.to<JsonArray>();
      unsigned long nowMs = millis();

      for (int i = 0; i < NUM_SWITCHES; i++) {
        if (!swTimers[i].active) {
          continue;
        }

        unsigned long remainingMs =
          swTimers[i].endMs > nowMs ? swTimers[i].endMs - nowMs : 0;

        if (remainingMs == 0) {
          continue;
        }

        JsonObject timer = timers.createNestedObject();
        timer["sw"] = i;
        timer["action"] = swTimers[i].targetState ? "on" : "off";
        timer["remainingMs"] = remainingMs;
      }

      String r;
      serializeJson(d, r);
      req->send(200, "application/json", r); });

    server.on("/api/timer/clear", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (req->hasParam("sw", true)) {
        int sw = req->getParam("sw", true)->value().toInt();
        if (sw >= 0 && sw < NUM_SWITCHES) swTimers[sw].active = false;
      }
      req->send(200, "application/json", "{\"ok\":true}"); });

    // ──── SCHEDULES (persistent) ────
    server.on("/api/schedules", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      int sw = req->hasParam("sw") ? req->getParam("sw")->value().toInt() : 0;
      prefs.begin("sched", true);
      String j = prefs.getString(("s" + String(sw)).c_str(), "[]");
      prefs.end();
      req->send(200, "application/json", j); });

    server.on("/api/schedules", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("sw", true) || !req->hasParam("data", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing params\"}");
        return;
      }
      if (!timeSynced) {
        req->send(400, "application/json", "{\"error\":\"Update time first\"}");
        return;
      }
      int sw = req->getParam("sw", true)->value().toInt();
      String data = req->getParam("data", true)->value();
      String error;
      if (sw < 0 || sw >= NUM_SWITCHES) {
        req->send(400, "application/json", "{\"error\":\"Invalid switch\"}");
        return;
      }
      if (!validateRecurringSchedulesData(data, error)) {
        req->send(400, "application/json", String("{\"error\":\"") + error + "\"}");
        return;
      }
      prefs.begin("sched", false);
      prefs.putString(("s" + String(sw)).c_str(), data);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    // ──── FUTURE SCHEDULES (persistent) ────
    server.on("/api/fschedules", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      int sw = req->hasParam("sw") ? req->getParam("sw")->value().toInt() : 0;
      prefs.begin("fsched", true);
      String j = prefs.getString(("f" + String(sw)).c_str(), "[]");
      prefs.end();
      req->send(200, "application/json", j); });

    server.on("/api/fschedules", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("sw", true) || !req->hasParam("data", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing params\"}");
        return;
      }
      if (!timeSynced) {
        req->send(400, "application/json", "{\"error\":\"Update time first\"}");
        return;
      }
      int sw = req->getParam("sw", true)->value().toInt();
      String data = req->getParam("data", true)->value();
      String error;
      if (sw < 0 || sw >= NUM_SWITCHES) {
        req->send(400, "application/json", "{\"error\":\"Invalid switch\"}");
        return;
      }
      if (!validateFutureSchedulesData(data, error)) {
        req->send(400, "application/json", String("{\"error\":\"") + error + "\"}");
        return;
      }
      prefs.begin("fsched", false);
      prefs.putString(("f" + String(sw)).c_str(), data);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    // ──── SENSOR CONTROL (persistent) ────
    server.on("/api/sensor/temp", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      int sw = req->hasParam("sw") ? req->getParam("sw")->value().toInt() : 0;
      prefs.begin("sensor", true);
      String j = prefs.getString(("t" + String(sw)).c_str(), "{}");
      prefs.end();
      req->send(200, "application/json", j); });

    server.on("/api/sensor/temp", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("data", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing data\"}");
        return;
      }
      if (!ahtOk) {
        req->send(400, "application/json", "{\"error\":\"AHT10 sensor not initialized\"}");
        return;
      }
      String data = req->getParam("data", true)->value();
      DynamicJsonDocument d(1024);
      if (deserializeJson(d, data)) {
        req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      int sw = d["switchId"].as<int>();
      prefs.begin("sensor", false);
      prefs.putString(("t" + String(sw)).c_str(), data);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/sensor/humid", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      int sw = req->hasParam("sw") ? req->getParam("sw")->value().toInt() : 0;
      prefs.begin("sensor", true);
      String j = prefs.getString(("h" + String(sw)).c_str(), "{}");
      prefs.end();
      req->send(200, "application/json", j); });

    server.on("/api/sensor/humid", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("data", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing data\"}");
        return;
      }
      if (!ahtOk) {
        req->send(400, "application/json", "{\"error\":\"AHT10 sensor not initialized\"}");
        return;
      }
      String data = req->getParam("data", true)->value();
      DynamicJsonDocument d(1024);
      if (deserializeJson(d, data)) {
        req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      int sw = d["switchId"].as<int>();
      prefs.begin("sensor", false);
      prefs.putString(("h" + String(sw)).c_str(), data);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/sensor/sun", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      int sw = req->hasParam("sw") ? req->getParam("sw")->value().toInt() : 0;
      prefs.begin("sensor", true);
      String j = prefs.getString(("x" + String(sw)).c_str(), "{}");
      prefs.end();
      req->send(200, "application/json", j); });

    server.on("/api/sensor/sun", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("data", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing data\"}");
        return;
      }
      if (!ahtOk) {
        req->send(400, "application/json", "{\"error\":\"AHT10 sensor not initialized\"}");
        return;
      }
      if (!locOk) {
        req->send(400, "application/json", "{\"error\":\"Latitude/Longitude not configured\"}");
        return;
      }
      if (!timeSynced) {
        req->send(400, "application/json", "{\"error\":\"Update time first\"}");
        return;
      }
      String data = req->getParam("data", true)->value();
      DynamicJsonDocument d(1024);
      if (deserializeJson(d, data)) {
        req->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
      }
      int sw = d["switchId"].as<int>();
      prefs.begin("sensor", false);
      prefs.putString(("x" + String(sw)).c_str(), data);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    // ──── WIFI ────
    server.on("/api/wifi/status", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      DynamicJsonDocument d(1024);
      bool connected = (WiFi.status() == WL_CONNECTED);
      String savedSsid;
      String savedPass;
      getSavedWifiCredentials(savedSsid, savedPass);
      d["connected"] = connected;
      d["ssid"] = connected ? WiFi.SSID() : savedSsid;
      d["savedSsid"] = savedSsid;
      d["ip"] = WiFi.localIP().toString();
      d["subnet"] = WiFi.subnetMask().toString();
      d["gateway"] = WiFi.gatewayIP().toString();
      d["dns"] = WiFi.dnsIP().toString();
      d["mac"] = WiFi.macAddress();
      d["rssi"] = connected ? WiFi.RSSI() : 0;
      d["dhcp"] = dhcpOn;
      d["staticIp"] = sIp;
      d["staticMask"] = sMask;
      d["staticGw"] = sGw;
      d["staticDns"] = sDns;
      String r;
      serializeJson(d, r);
      req->send(200, "application/json", r); });

    server.on("/api/wifi/connect", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("ssid", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing SSID\"}");
        return;
      }
      if (!req->hasParam("adminUser", true) || !req->hasParam("adminPass", true)) {
        req->send(400, "application/json", "{\"error\":\"Admin credentials required\"}");
        return;
      }
      if (!verifyAdmin(req->getParam("adminUser", true)->value(), req->getParam("adminPass", true)->value())) {
        req->send(403, "application/json", "{\"error\":\"Admin verification failed\"}");
        return;
      }
      String ssid = req->getParam("ssid", true)->value();
      String pass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : "";
      if (ssid.isEmpty()) {
        req->send(400, "application/json", "{\"error\":\"SSID is required\"}");
        return;
      }
      prefs.begin("wfcfg", false);
      prefs.putString("ssid", ssid);
      prefs.putString("pass", pass);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"WiFi credentials saved. Restarting to connect.\"}");
      restartFlag = true;
      restartAt = millis() + 1200; });

    server.on("/api/wifi/disconnect", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      WiFi.disconnect();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/wifi/scan", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"WiFi portal starting. Connect to ESP HOME at 192.168.4.1. The current web session will disconnect until setup finishes.\"}");
      portalFlag = true; });

    server.on("/api/wifi/saved", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      DynamicJsonDocument d(256);
      String savedSsid;
      String savedPass;
      bool connected = (WiFi.status() == WL_CONNECTED);
      getSavedWifiCredentials(savedSsid, savedPass);
      d["connected"] = connected;
      d["ssid"] = connected ? WiFi.SSID() : savedSsid;
      d["savedSsid"] = savedSsid;
      d["ip"] = connected ? WiFi.localIP().toString() : "";
      String r;
      serializeJson(d, r);
      req->send(200, "application/json", r); });

    server.on("/api/wifi/forget", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("adminUser", true) || !req->hasParam("adminPass", true)) {
        req->send(400, "application/json", "{\"error\":\"Admin credentials required\"}");
        return;
      }
      if (!verifyAdmin(req->getParam("adminUser", true)->value(), req->getParam("adminPass", true)->value())) {
        req->send(403, "application/json", "{\"error\":\"Admin verification failed\"}");
        return;
      }
      prefs.begin("wfcfg", false);
      prefs.remove("ssid");
      prefs.remove("pass");
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"WiFi credentials forgotten. Device will disconnect and restart.\"}");
      forgetWifiFlag = true;
      forgetWifiAt = millis() + 1000; });

    server.on("/api/wifi/ip", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("dhcp", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing\"}");
        return;
      }
      dhcpOn = req->getParam("dhcp", true)->value() == "true";
      prefs.begin("wfcfg", false);
      prefs.putBool("dhcp", dhcpOn);
      if (!dhcpOn) {
        if (!req->hasParam("ip", true) || !req->hasParam("mask", true) || !req->hasParam("gw", true) || !req->hasParam("dns", true)) {
          prefs.end();
          req->send(400, "application/json", "{\"error\":\"Static IP, subnet mask, gateway and DNS are required\"}");
          return;
        }

        sIp = req->getParam("ip", true)->value();
        sMask = req->getParam("mask", true)->value();
        sGw = req->getParam("gw", true)->value();
        sDns = req->getParam("dns", true)->value();

        IPAddress ip, gw, sn, dns;
        if (!ip.fromString(sIp) || !gw.fromString(sGw) || !sn.fromString(sMask) || !dns.fromString(sDns)) {
          prefs.end();
          req->send(400, "application/json", "{\"error\":\"Invalid static IP settings\"}");
          return;
        }

        prefs.putString("sip", sIp);
        prefs.putString("mask", sMask);
        prefs.putString("gw", sGw);
        prefs.putString("dns", sDns);
      }
      prefs.end();
      notifyStorage();
      if (dhcpOn) {
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"DHCP enabled. Device will reconnect using DHCP.\"}");
      } else {
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Static IP settings saved. Device will reconnect using the configured IP.\",\"ip\":\"" + sIp + "\"}");
      }
      restartFlag = true;
      restartAt = millis() + 1200; });

    // ──── FIREBASE ────
    server.on("/api/fb", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      DynamicJsonDocument d(1024);
      d["enabled"] = fbOn;
      d["url"] = fbUrl;
      d["token"] = fbToken;
      prefs.begin("fb", true);
      d["rules"] = prefs.getString("rules", "");
      prefs.end();
      String r;
      serializeJson(d, r);
      req->send(200, "application/json", r); });

    server.on("/api/fb/toggle", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("enabled", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing\"}");
        return;
      }
      fbOn = req->getParam("enabled", true)->value() == "true";
      prefs.begin("fb", false);
      prefs.putBool("en", fbOn);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/fb/url", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("url", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing\"}");
        return;
      }
      fbUrl = req->getParam("url", true)->value();
      prefs.begin("fb", false);
      prefs.putString("url", fbUrl);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/fb/token", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("token", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing\"}");
        return;
      }
      fbToken = req->getParam("token", true)->value();
      prefs.begin("fb", false);
      prefs.putString("tok", fbToken);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/fb/test", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (fbUrl.isEmpty() || fbToken.isEmpty()) {
        req->send(400, "application/json", "{\"error\":\"URL or Token not configured\"}");
        return;
      }
      if (WiFi.status() != WL_CONNECTED) {
        req->send(400, "application/json", "{\"error\":\"WiFi not connected\"}");
        return;
      }
      HTTPClient http;
      String testUrl = fbUrl + "/.json?auth=" + fbToken + "&shallow=true";
      http.begin(testUrl);
      int code = http.GET();
      String body = http.getString();
      http.end();
      if (code == 200)
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Connection successful\"}");
      else
        req->send(400, "application/json", "{\"error\":\"Connection failed: HTTP " + String(code) + "\"}"); });

    server.on("/api/fb/rules", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("rules", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing rules\"}");
        return;
      }
      String rules = req->getParam("rules", true)->value();
      prefs.begin("fb", false);
      prefs.putString("rules", rules);
      prefs.end();
      notifyStorage();
      if (fbUrl.isEmpty() || fbToken.isEmpty()) {
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Saved locally (no Firebase credentials)\"}");
        return;
      }
      if (WiFi.status() != WL_CONNECTED) {
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Saved locally (WiFi not connected)\"}");
        return;
      }
      HTTPClient http;
      http.begin(fbUrl + "/.settings/rules.json?auth=" + fbToken);
      http.addHeader("Content-Type", "application/json");
      int code = http.PUT(rules);
      http.end();
      if (code == 200)
        req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Rules saved and uploaded to Firebase\"}");
      else
        req->send(400, "application/json", "{\"error\":\"Saved locally but upload failed: HTTP " + String(code) + "\"}"); });

    // ──── USERS ────
    server.on("/api/users", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      prefs.begin("users", true);
      int n = prefs.getInt("cnt", 0);
      DynamicJsonDocument d(1024);
      JsonArray a = d.to<JsonArray>();
      for (int i = 0; i < n; i++) {
        String js = prefs.getString(("u" + String(i)).c_str(), "");
        if (js.isEmpty()) continue;
        DynamicJsonDocument ud(512);
        deserializeJson(ud, js);
        JsonObject o = a.createNestedObject();
        o["id"] = ud["id"];
        o["role"] = ud["role"];
      }
      prefs.end();
      String r;
      serializeJson(d, r);
      req->send(200, "application/json", r); });

    server.on("/api/users/add", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("id", true) || !req->hasParam("pass", true) || !req->hasParam("role", true) || !req->hasParam("adminUser", true) || !req->hasParam("adminPass", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing params\"}");
        return;
      }
      if (!hasAdmin()) {
        req->send(403, "application/json", "{\"error\":\"No admin user exists\"}");
        return;
      }
      if (!verifyAdmin(req->getParam("adminUser", true)->value(), req->getParam("adminPass", true)->value())) {
        req->send(403, "application/json", "{\"error\":\"Admin verification failed\"}");
        return;
      }
      DynamicJsonDocument nd(512);
      nd["id"] = req->getParam("id", true)->value();
      nd["pass"] = req->getParam("pass", true)->value();
      nd["role"] = req->getParam("role", true)->value();
      String nj;
      serializeJson(nd, nj);
      prefs.begin("users", false);
      int cnt = prefs.getInt("cnt", 0);
      prefs.putString(("u" + String(cnt)).c_str(), nj);
      prefs.putInt("cnt", cnt + 1);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/users/remove", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("id", true) || !req->hasParam("adminUser", true) || !req->hasParam("adminPass", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing params\"}");
        return;
      }
      if (!hasAdmin()) {
        req->send(403, "application/json", "{\"error\":\"No admin user exists\"}");
        return;
      }
      if (!verifyAdmin(req->getParam("adminUser", true)->value(), req->getParam("adminPass", true)->value())) {
        req->send(403, "application/json", "{\"error\":\"Admin verification failed\"}");
        return;
      }
      String uid = req->getParam("id", true)->value();
      prefs.begin("users", false);
      int cnt = prefs.getInt("cnt", 0);
      int targetIdx = -1;
      bool targetIsAdmin = false;
      for (int i = 0; i < cnt; i++) {
        String js = prefs.getString(("u" + String(i)).c_str(), "");
        DynamicJsonDocument d(1024);
        deserializeJson(d, js);
        if (d["id"].as<String>() == uid) {
          targetIdx = i;
          targetIsAdmin = (d["role"].as<String>() == "admin");
          break;
        }
      }
      if (targetIdx < 0) {
        prefs.end();
        req->send(404, "application/json", "{\"error\":\"User not found\"}");
        return;
      }
      if (targetIsAdmin && countAdmins() <= 1) {
        prefs.end();
        req->send(403, "application/json", "{\"error\":\"Cannot remove the last admin user\"}");
        return;
      }
      for (int i = targetIdx; i < cnt - 1; i++) {
        String next = prefs.getString(("u" + String(i + 1)).c_str(), "");
        prefs.putString(("u" + String(i)).c_str(), next);
      }
      prefs.remove(("u" + String(cnt - 1)).c_str());
      prefs.putInt("cnt", cnt - 1);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    // ──── ADMINISTRATOR ────
    server.on("/api/admin/time", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      DynamicJsonDocument d(1024);
      d["ntp"] = ntpSrv;
      d["tz"] = tzStr;
      d["synced"] = timeSynced;
      String r;
      serializeJson(d, r);
      req->send(200, "application/json", r); });

    server.on("/api/admin/time", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (req->hasParam("ntp", true)) ntpSrv = req->getParam("ntp", true)->value();
      if (req->hasParam("tz", true)) {
        tzStr = req->getParam("tz", true)->value();
        parseTZ();
      }
      prefs.begin("admin", false);
      prefs.putString("ntp", ntpSrv);
      prefs.putString("tz", tzStr);
      prefs.end();
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/admin/time/sync", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (syncTime())
        req->send(200, "application/json", "{\"ok\":true}");
      else
        req->send(400, "application/json", "{\"error\":\"Time sync failed. Check WiFi and NTP server.\"}"); });

    server.on("/api/admin/time/current", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      struct tm ti;
      if (getLocalTime(&ti)) {
        char buf[32];
        sprintf(buf, "%02d:%02d:%02d %02d/%02d/%04d", ti.tm_hour, ti.tm_min, ti.tm_sec,
                ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
        req->send(200, "application/json", "{\"time\":\"" + String(buf) + "\",\"synced\":" + (timeSynced ? "true" : "false") + "}");
      } else {
        req->send(200, "application/json", "{\"time\":\"--:--:-- --/--/----\",\"synced\":false}");
      } });

    server.on("/api/admin/location", HTTP_GET, [](AsyncWebServerRequest *req)
              {
      DynamicJsonDocument d(1024);
      d["lat"] = geoLat;
      d["lon"] = geoLon;
      d["configured"] = locOk;
      String r;
      serializeJson(d, r);
      req->send(200, "application/json", r); });

    server.on("/api/admin/location", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("lat", true) || !req->hasParam("lon", true)) {
        req->send(400, "application/json", "{\"error\":\"Missing params\"}");
        return;
      }
      geoLat = req->getParam("lat", true)->value().toFloat();
      geoLon = req->getParam("lon", true)->value().toFloat();
      locOk = true;
      lastCalcDay = -1;
      prefs.begin("admin", false);
      prefs.putFloat("lat", geoLat);
      prefs.putFloat("lon", geoLon);
      prefs.end();
      notifyStorage();
      calcSunriseSunset();
      req->send(200, "application/json", "{\"ok\":true}"); });

    server.on("/api/admin/restart", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("adminUser", true) || !req->hasParam("adminPass", true)) {
        req->send(400, "application/json", "{\"error\":\"Admin credentials required\"}");
        return;
      }
      if (!verifyAdmin(req->getParam("adminUser", true)->value(), req->getParam("adminPass", true)->value())) {
        req->send(403, "application/json", "{\"error\":\"Admin verification failed\"}");
        return;
      }
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Restarting...\"}");
      restartFlag = true;
      restartAt = millis() + 500; });

    server.on("/api/admin/reset/storage", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("adminUser", true) || !req->hasParam("adminPass", true)) {
        req->send(400, "application/json", "{\"error\":\"Admin credentials required\"}");
        return;
      }
      if (!verifyAdmin(req->getParam("adminUser", true)->value(), req->getParam("adminPass", true)->value())) {
        req->send(403, "application/json", "{\"error\":\"Admin verification failed\"}");
        return;
      }
      // Clear all NVS namespaces
      const char *ns[] = { "sw", "bt", "wfcfg", "fb", "admin", "users", "sched", "fsched", "sensor" };
      for (int i = 0; i < 9; i++) {
        prefs.begin(ns[i], false);
        prefs.clear();
        prefs.end();
      }
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"All storage cleared. Restart recommended.\"}"); });

    server.on("/api/admin/reset/settings", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("adminUser", true) || !req->hasParam("adminPass", true)) {
        req->send(400, "application/json", "{\"error\":\"Admin credentials required\"}");
        return;
      }
      if (!verifyAdmin(req->getParam("adminUser", true)->value(), req->getParam("adminPass", true)->value())) {
        req->send(403, "application/json", "{\"error\":\"Admin verification failed\"}");
        return;
      }
      // Clear settings but keep users
      const char *ns[] = { "sw", "bt", "wfcfg", "fb", "admin", "sched", "fsched", "sensor" };
      for (int i = 0; i < 8; i++) {
        prefs.begin(ns[i], false);
        prefs.clear();
        prefs.end();
      }
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Settings cleared (users preserved). Restart recommended.\"}"); });

    server.on("/api/admin/reset/factory", HTTP_POST, [](AsyncWebServerRequest *req)
              {
      if (!req->hasParam("adminUser", true) || !req->hasParam("adminPass", true)) {
        req->send(400, "application/json", "{\"error\":\"Admin credentials required\"}");
        return;
      }
      if (!verifyAdmin(req->getParam("adminUser", true)->value(), req->getParam("adminPass", true)->value())) {
        req->send(403, "application/json", "{\"error\":\"Admin verification failed\"}");
        return;
      }
      // Full factory reset
      const char *ns[] = { "sw", "bt", "wfcfg", "fb", "admin", "users", "sched", "fsched", "sensor" };
      for (int i = 0; i < 9; i++) {
        prefs.begin(ns[i], false);
        prefs.clear();
        prefs.end();
      }
      WiFi.disconnect(true, true);
      notifyStorage();
      req->send(200, "application/json", "{\"ok\":true,\"msg\":\"Factory reset complete. Restarting...\"}");
      restartFlag = true;
      restartAt = millis() + 500; });
  }

  server.onNotFound([](AsyncWebServerRequest *req)
                    {
    if (req->method() == HTTP_OPTIONS) {
      req->send(204);
      return;
    }

    if (req->url().startsWith("/api/")) {
      if (coreRoutesOnly) {
        req->send(503, "application/json", "{\"error\":\"Extended API disabled in low-memory mode\"}");
      } else {
        req->send(404, "application/json", "{\"error\":\"API route not found\"}");
      }
      Serial.printf("[HTTP] API miss %s\n", req->url().c_str());
      return;
    }

    Serial.printf("[HTTP] 404 %s\n", req->url().c_str());
    req->send(404, "text/plain", "Not found"); });

  server.begin();
  Serial.println("[SERVER] Web server started");
}

//  SETUP
void setup()
{
  Serial.begin(115200);
  Serial.println("\n==============================");
  Serial.println("  ESP32 Smart Home Starting");
  Serial.println("==============================");

  // Pin modes
  for (int i = 0; i < NUM_SWITCHES; i++)
  {
    pinMode(outPin[i], OUTPUT);
    pinMode(inPin[i], INPUT);
    lastInState[i] = digitalRead(inPin[i]);
  }
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BOOT_BUTTON, INPUT_PULLUP);

  // Startup beep
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);
  delay(2000);

  // On demand WiFi setup
  if (digitalRead(BOOT_BUTTON) == LOW)
  {
    Serial.println("[BOOT] Boot button pressed - entering WiFi setup mode...");
    // Double beep to indicate WiFi setup mode
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);

    runWifiConfigPortal(true);
  }

  // --- Storage Init ---
  Serial.println("\n==============================");
  Serial.println("Settings Initialization");
  Serial.println("==============================");
  Serial.println("[INFO] Initializing EEPROM...");
  delay(1000);
  EEPROM.begin(512);
  // loadSettings();
  Serial.println("[SUCCESS] EEPROM Initialized.");
  Serial.printf("[INFO] EEPROM Size: %d bytes\n", 512);
  Serial.println("==============================\n");

  delay(1000);

  // --- SPIFFS ---
  Serial.println("\n==============================");
  Serial.println("SPIFFS Initialization");
  Serial.println("==============================");
  if (!SPIFFS.begin(true))
  {
    // u8g2.drawStr(0, 18, "SPIFFS: ERROR");
    Serial.println("[ERROR] SPIFFS Mount Failed!");
    Serial.println("[INFO] Filesystem not available.");
    Serial.println("==============================\n");
  }
  else
  {
    // u8g2.drawStr(0, 18, "SPIFFS: OK");
    Serial.println("[SUCCESS] SPIFFS Mounted Successfully.");
    Serial.println("[INFO] Listing Files:");
    Serial.println("------------------------------");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    int fileCount = 0;
    while (file)
    {
      Serial.printf("File %02d : %s  |  Size: %d bytes\n",
                    fileCount + 1,
                    file.name(),
                    file.size());
      fileCount++;
      file = root.openNextFile();
    }
    Serial.println("------------------------------");
    Serial.printf("Total Files: %d\n", fileCount);
    Serial.println("==============================\n");
    root.close();
  }

  // Load all settings from NVS
  loadSwitchSettings();
  loadWifiSettings();
  loadFbSettings();
  loadAdminSettings();
  initDefaultUser();
  Serial.println("[OK] Settings loaded from NVS");

  // AHT10 sensor
  Wire.begin();
  if (aht.begin())
  {
    ahtOk = true;
    Serial.println("[OK] AHT10 sensor detected");
  }
  else
  {
    Serial.println("[WARN] AHT10 not detected");
  }

  // WiFi
  connectToSavedWiFi();
  delay(1000);
  // connectWiFi();

  if (WiFi.status() == WL_CONNECTED)
  {
    WiFi.setSleep(false);
    Serial.println("[WIFI] Power save disabled for stable web server");
  }

  // Time sync
  if (WiFi.status() == WL_CONNECTED)
  {
    syncTime();
  }

  // Sunrise/Sunset
  calcSunriseSunset();

  // Web server
  if (WiFi.status() == WL_CONNECTED)
  {
    setupWebServer();
  }

  Serial.println("==============================");
  Serial.println("  System Ready");
  if (WiFi.status() == WL_CONNECTED)
    Serial.printf("  IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.println("==============================\n");
}

//  LOOP
void loop()
{
  // Physical switch input
  checkPhysicalSwitches();

  // Timer execution (volatile)
  checkTimers();

  // Schedule checks (every 15s)
  unsigned long now = millis();
  if (now - lastSchedCheck >= 15000)
  {
    lastSchedCheck = now;
    checkSchedules();
    calcSunriseSunset();
  }

  // Sensor checks (every 30s)
  if (now - lastSensorCheck >= 30000)
  {
    lastSensorCheck = now;
    checkSensors();
  }

  // WiFi portal request
  if (portalFlag)
  {
    portalFlag = false;
    server.end();
    delay(100);
    if (!runWifiConfigPortal(true))
    {
      restartFlag = true;
      restartAt = millis() + WIFI_PORTAL_RECOVERY_RESTART_MS;
    }
  }

  if (forgetWifiFlag && millis() >= forgetWifiAt)
  {
    forgetWifiFlag = false;
    WiFi.disconnect(true, true);
    ESP.restart();
  }

  // Restart request
  if (restartFlag && millis() >= restartAt)
  {
    ESP.restart();
  }
}