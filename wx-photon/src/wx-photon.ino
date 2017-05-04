#include "application.h"
#include <SparkJson.h>
#include <Wire.h>
#include <limits.h>
#include <math.h>

#define BLUE_PIN D7
#define TRANSISTOR_PIN D2
#define SLAVE_ADDRESS 8
#define ZIP_LENGTH 5

const unsigned long I2C_BUFFER_SIZE = 32;

// Current WX:
// http://api.openweathermap.org/data/2.5/weather?APPID=<app-id>&units=metric&zip=94123,US
// Forecast WX:
// http://api.openweathermap.org/data/2.5/forecast/daily?APPID=<app-id>&units=metric&zip=94123,US

String wx = "";
String wxResult = "";
String zip = "";
String lastSuccessfulZip = "";
String lastSuccessfulWx = "";
String lastSuccessfulWxResult = "";

StaticJsonBuffer<12000> buffer;

bool done = false,
     needWx = true,
     isBluePinOn = false,
     isTransistorOn = false;

const unsigned int ONE_SECOND            = 1000,
                   INTERVAL_READ_ZIP     = 2 * ONE_SECOND,
                   INTERVAL_WEBHOOK_WAIT = 6 * ONE_SECOND,
                   INTERVAL_NORMAL_SLEEP = 8 * ONE_SECOND,
                   INTERVAL_FORCE_SLEEP  = 45 * ONE_SECOND,
                   NORMAL_SLEEP_SECONDS  = 4 * 3600, // 4 hours
                   FORCE_SLEEP_SECONDS   = 1800; // half hour

unsigned int timeRequest = 0,
             timeSend = 0,
             timeNormalSleep = ULONG_MAX,
             timeParse = ULONG_MAX,
             timeForceSleep = millis() + INTERVAL_FORCE_SLEEP,
             deepSleepSeconds = NORMAL_SLEEP_SECONDS;

void setup() {
    Wire.begin(); // I2C Master
    pinMode(BLUE_PIN, OUTPUT);
    pinMode(TRANSISTOR_PIN, OUTPUT);

    log("debug", "ready");
    Time.zone(-7); // TODO - account for DST

    Particle.subscribe("wx-data", dataHandler, MY_DEVICES);
    Particle.subscribe("wx-error", errorHandler, MY_DEVICES);

    setTransistorOn(true);
    delay(2000);
}

void loop() {
    loopRequest();
    loopParse();
    loopNormalSleep();
    loopForceSleep();
}

void setTransistorOn(bool value) {
  isTransistorOn = value;
  digitalWrite(TRANSISTOR_PIN, value ? HIGH : LOW);
  setBluePinOn(value);
}

void loopRequest() {
    const long now = millis();
    if (!needWx || (now < timeRequest)) {
        return;
    }
    timeRequest = now + INTERVAL_READ_ZIP;

    zip = getZipI2C();
    if (!zip.length()) { return; }

    log("zip/valid", zip);
    wx = "";

    Particle.publish("wx-request", zip, PRIVATE);

    timeParse = now + INTERVAL_WEBHOOK_WAIT;
    needWx = false;
}

void loopParse() {
    // Parser runs INTERVAL_WEBHOOK_WAIT milliseconds after API request is initiated.
    // We are guessing when the partial webhook responses come back,
    // that's why the need for this gate.
    // Parser runs only once.

    if (millis() < timeParse) {
        return;
    }
    timeParse = ULONG_MAX;

    wxResult = parseWX(wx);
    if (wxResult.length()) {
        sendI2C(wxResult);
        timeNormalSleep = millis() + INTERVAL_NORMAL_SLEEP;
    }
}

void loopNormalSleep() {
  if (millis() < timeNormalSleep) {
      return;
  }
  delay(500);
  log("debug", "TURNING OFF TRANSISTOR");
  delay(500);
  setTransistorOn(false); // Turn off Arduino and display power
  delay(500);
  log("debug", "DEEP SLEEP");
  delay(500);
  goToSleepNow();
}

void loopForceSleep() {
    if (millis() > timeForceSleep) {
        log("debug", "FORCE DEEP SLEEP");
        deepSleepSeconds = FORCE_SLEEP_SECONDS;
        goToSleepNow();
    }
}

void goToSleepNow() {
    timeForceSleep = ULONG_MAX;
    timeNormalSleep = ULONG_MAX;
    delay(500);
    System.sleep(SLEEP_MODE_DEEP, deepSleepSeconds);
}

String getZipI2C() {

    Wire.requestFrom(SLAVE_ADDRESS, ZIP_LENGTH);
    String newZip = "";
    bool valid = true;
    while (Wire.available()) {
        char c = Wire.read();
        if (c < '0' || c > '9') { valid = false; }
        newZip += c;
    }
    if (newZip.length() != ZIP_LENGTH) { valid = false; }

    log("i2c/zip", newZip);

    return (valid ? newZip : "");
}

void dataHandler(const char *event, const char *data) {
    wx += String(data);
}

void errorHandler(const char *event, const char *data) {
    log("wx-error", data);
}

String parseWX(String s) {

    JsonObject& json = buffer.parseObject((char*)s.c_str());

    if (!json.success()) {
        log("debug", "FAILED TO PARSE");
        return "";
    }

    const char* name = json["city"]["name"];

    // log("tempC", tempC);

    String weekdays[7] = {
        "Sun",
        "Mon",
        "Tue",
        "Wed",
        "Thu",
        "Fri",
        "Sat" };

    String months[12] = {
        "Jan",
        "Feb",
        "Mar",
        "Apr",
        "May",
        "Jun",
        "Jul",
        "Aug",
        "Sep",
        "Oct",
        "Nov",
        "Dec" };

    String result = String(name);

    // Time format options: http://strftime.org/
    String now = Time.format(Time.now(), "%b %d, %I:%M %p");
    result += "\n" + zip + ", " + now + "\n";

    int length = json["cnt"];
    for (int i = 0; i < length; i++) {
        JsonObject& obj = json["list"][i];
        String line = "";

        unsigned long timestamp = obj["dt"];
        int day = Time.day(timestamp);

        // 1 thru 7 for weekdays, 1 thru 12 for months;
        // see https://docs.particle.io/reference/firmware/photon/#weekday-
        String month = months[Time.month(timestamp) - 1];
        String weekday = weekdays[Time.weekday(timestamp) - 1];
        line += weekday + "," + month + " " + day;

        int tempMax = intFromJsonNumber(obj["temp"]["max"]);
        int tempMin = intFromJsonNumber(obj["temp"]["min"]);

        line += "," + String(tempMax) + "/" + String(tempMin);

        const char* icon = obj["weather"][0]["icon"];

        // Tue,Apr 4,20/12,02d
        line += "," + String(icon) + "\n";

        result += line;
    }

    // log("wx/result", result);
    return result;
}

int intFromJsonNumber(JsonVariant& number) {
    // If the value is a round number, like 23, treating it as double returns 0.
    double tempDouble = number.as<double>();
    return (tempDouble ? (int)round(tempDouble) : number.as<int>());
}

String sendI2C(String s) {

    log("i2c/message-length", s.length());

    int delayMillis = 100;

    Wire.beginTransmission(SLAVE_ADDRESS);
    Wire.write("START");
    Wire.endTransmission(false);
    delay(delayMillis);

    for (int i = 0; i < s.length(); i += I2C_BUFFER_SIZE) {
        String sub = s.substring(i, i + I2C_BUFFER_SIZE);
        Wire.beginTransmission(SLAVE_ADDRESS);
        Wire.write(sub.c_str());
        Wire.endTransmission(false);
        delay(delayMillis);
    }

    Wire.beginTransmission(SLAVE_ADDRESS);
    Wire.write("END");
    Wire.endTransmission(true);
}

void memory() {
    uint32_t mem = System.freeMemory();
    char memStr[16];
    sprintf(memStr, "%lu bytes", mem);
    log("free-memory", memStr);
}

void toggleBluePin() {
    setBluePinOn(!isBluePinOn);
}

void setBluePinOn(bool value) {
    isBluePinOn = value;
    digitalWrite(BLUE_PIN, isBluePinOn ? HIGH : LOW);
}

void log(String suffix, int number) {
    char str[16];
    sprintf(str, "%d", number);
    log(suffix, str);
}

void log(String suffix, String msg) {
    Particle.publish("wx/" + suffix, msg, PRIVATE);
}
