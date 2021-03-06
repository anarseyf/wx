# Overview

This project uses a Wi-Fi module to request weather data from a public API and display it on an E-Paper display. Weather is only fetched every 4 hours, and power consumption is on the order of microamps between updates. The display requires no power when not updating, which is perfect for this application. The widget is powered by two batteries serving distinct purposes. Photos below.

# Components

* [4.3 Inch E-Paper display](http://www.waveshare.com/wiki/4.3inch_e-Paper)
* [Particle Photon](https://store.particle.io/collections/photon) (WiFi module)
* [Arduino Uno](http://www.arduino.org/products/boards/arduino-uno) (controls the display)
* 9V battery (powers the Arduino and the display)
* Small [3.7V rechargeable battery](https://www.sparkfun.com/products/13851) (powers the Photon only)
* [TIP-120 Transistor](https://www.adafruit.com/product/976) (turns off power to Arduino and display when not updating)

# How it works
## Logic
* Photon connects to the web, then turns on the transistor which powers on the Arduino.
* Photon and Arduino establish communication via I<sup>2</sup>C. Photon is Master, Arduino is Slave.
* Photon requests and receives ZIP code from Arduino (this is hard-coded to `94123`, but I plan to add a plug-in module to select any ZIP).
* Photon publishes an event called `wx-request`, which is tied to a Particle [Webhook](https://docs.particle.io/guide/tools-and-features/webhooks/):
```C
Particle.subscribe("wx-data", dataHandler, MY_DEVICES);
...
Particle.publish("wx-request", zip, PRIVATE);
```
* The webhook creates an HTTP request to OpenWeatherMap in this format:
```http://api.openweathermap.org/data/2.5/forecast/daily?APPID=<app-id>&zip=<zip>,US&units=metric```
  * *app-id* is my unique OpenWeatherMap API key; it's configured as a parameter in the Webhook
  * *zip* comes from `wx-request`.
* *dataHandler* collects partial responses (the full response is larger than Particle's payload limit, which I think is 256 bytes).
* Photon parses the response using the ```SparkJson``` library:
```C
StaticJsonBuffer<12000> buffer;
...
JsonObject& json = buffer.parseObject((char*)apiResponse.c_str());
```
The necessary fields are extracted. This includes a code for the weather icon.
* The result is sent to Arduino via I<sup>2</sup>C. It looks like this:
```
San Francisco
04-09-2017 12:00 PM
Mon,Apr 9,20/15,01n
Tue,Apr 10,10/8,03d
Wed,Apr 11,0/-4,09d
Thu,Apr 12,-2/-5,10d
Fri,Apr 13,30/19,50d
```
* Arduino parses the message, and renders the data on the screen. The last item is converted to an image name; for example, `09d` becomes `09D.BMP`.
* Photon turns off the transistor, then enters deep sleep mode.
* 4 hours later the process repeats.
## Hardware notes

* Sleep and Force Sleep:
  * The Photon has an excellent low-power mode. In Deep Sleep it only consumes a handful of microamps of current. At the end of the specified sleep period the chip restarts.
  * There is a separate force-sleep cycle that shuts down the Photon after 45 seconds. This is in case there was a connectivity or other problem that prevented a proper update. After a forced sleep, the restart occurs in 30 minutes rather than 4 hours.
```C
System.sleep(SLEEP_MODE_DEEP, deepSleepSeconds);
```
* E-Paper: this display consumes no power except when updating.
* SD Card: 
  * the `epd_disp_bitmap()` function can be used to load BMP images from the display's memory. These are initially loaded from a micro-SD card, then copied to internal flash memory.
* Acrylic case:
  * I sent AutoCAD files with laser cutting specs for top and bottom cover to [Pagoda Arts](pagodaarts.com), located in San Francisco. They cut an 1/8" sheet of acrylic to these specs. See photo below.
* Current:
  * Photon: normal 50mA, deep sleep 50µA (very rough numbers)
  * Arduino Uno: 50mA
  * E-Paper display: 150-200mA when updating
* Voltage:
  * Photon: Accepts 3.3-5V (bad idea to plug in a 9V battery directly, as I've found out)
  * Arduino Uno: Accepts 9V from battery
  * E-Paper display: accepts both 3.3V and 5V.

## Libraries used

* SparkJson
* Wire (I<sup>2</sup>C)
* [EPD](https://github.com/sabas1080/LibraryEPD/)
  * The EPD library is modified to include a function called [epd_import_sd()](/libraries/epd-modified/epd.cpp#L782). This allows copying images from the SD card to the display's built-in NandFlash memory. This removes the need to have an SD card always plugged in, which is good because it seems to corrupt SD cards once in a while.
  
## Code
* [wx-photon.ino](wx-photon/src/wx-photon.ino) (runs on the Wi-Fi module)
* [wx-arduino.ino](/wx-arduino/wx-arduino.ino) (runs on the Arduino)
  
# Various
* Why even use an Arduino Uno - can't the Photon handle everything?
  * The EPD library wouldn't run on the Photon.

# Future plans (version 2)
* ZIP code entry plug-in
* Even less power (Arduino Pro Mini)
* Button for immediate refresh
* Easier access to Photon's Safe Mode
* Better jumper wires that don't fall out so easily
* Self-report battery level
* Post diagnostic info or heartbeat to a web service

# Photos
## Display
<img src="/screenshots/display.jpg" width="75%">

## Acrylic Case
<img src="/screenshots/case.jpg" width="75%">

## Main components 
<img src="/screenshots/bottom.jpg" width="75%">

## Transistor
<img src="/screenshots/transistor.jpg" width="75%">
  
# Sample openweathermap response
```javascript
{
    city: {
        id: 5391959,
        name: "San Francisco",
        coord: {
            lon: -122.4195,
            lat: 37.7749
        },
        country: "US",
        population: 805235
    },
    cod: "200",
    message: 0.502746,
    cnt: 7,
    list: [{
        dt: 1493582400,
        temp: {
           day: 23.47,
           min: 13.29,
           max: 23.47,
           night: 13.29,
           eve: 20.17,
           morn: 23.47
        },
        pressure: 1024.2,
        humidity: 51,
        weather: [{
            id: 800,
            main: "Clear",
            description: "sky is clear",
            icon: "01n"
        }],
        speed: 2.86,
        deg: 274,
        clouds: 0
    }, ...
]}
```
