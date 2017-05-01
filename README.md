# Overview

This project uses a Wi-Fi module to request weather data from a public API and display it on an E-Paper display.

# Components

* 4.3 Inch E-Paper display ([link](www.waveshare.com/wiki/4.3inch_e-Paper))
* Particle Photon
* Arduino Uno
* 

# How it works
## Logic
* Photon and Arduino establish communication via I2C. Photon is Master, Arduino is Slave.
* Photon requests and receives ZIP code from Arduino (this is hard.
* Photon publishes an event called **wx-request**, which is tied to a Particle [Webhook](https://docs.particle.io/guide/tools-and-features/webhooks/):
```C
Particle.subscribe("wx-data", dataHandler, MY_DEVICES);
...
Particle.publish("wx-request", zip, PRIVATE);
```
* The webhook creates a URL in the format:
```http://api.openweathermap.org/data/2.5/forecast/daily?APPID=<app-id>&zip=<ZIP>,US&units=metric```
  * *app-id* is supplied in the Webhook config; *zip* comes from **wx-request**.
* *dataHandler* collects partial responses (the full response is larger than Particle's payload limit, which I think is 256 bytes).
* Photon parses the response using the ```SparkJson``` library:
```C
StaticJsonBuffer<12000> buffer;
...
JsonObject& json = buffer.parseObject((char*)apiResponse.c_str());
```
The necessary fields are extracted. This includes a code for the weather icon.
* The result is sent to Arduino via I2C. It looks like this:
```
San Francisco
04-09-2017 12:00 PM
Mon,Apr 9,20/15,01n
Tue,Apr 10,10/8,03d
Wed,Apr 11,0/-4,09d
Thu,Apr 12,-2/-5,10d
Fri,Apr 13,30/19,50d
```
* Arduino parses the message, and renders the data on the screen. The last item is converted to an image name; for example, ```09d``` becomes ```09D.BMP```.
## Hardware

* Sleep; force sleep
* E-Paper
* Icons
* SD card

## Libraries

* SparkJson
* Wire (I2C)
* EPD
  * The EPD library is modified to include a function called ```epd_import_sd()```. This allows copying images from the SD card to the display's built-in NandFlash memory. This removes the need to have an SD card always plugged in, which is good because it seems to corrupt SD cards once in a while.
  
# Future
* ZIP code entry plug-in
* Low Power (Arduino Pro Mini)
  
## Sample openweathermap output
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
