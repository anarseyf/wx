#include <epd.h>
#include <limits.h>
#include <Wire.h>
#include <LowPower.h>

const int SLAVE_ADDRESS = 8; // We are I2C Slave
const int LED_PIN = 13;
    //const int RED_LED_PIN = 8; // TODO
const int WAKEUP_PIN = 2; // yellow
const int RESET_PIN = 3; // blue

const int TRANSISTOR_PIN = 10;

const int IMAGE_COUNT = 18,
  SCREEN_WIDTH = 800,
  SCREEN_HEIGHT = 600,
  NUM_COLUMNS = 5,
  COLUMN_SIZE = (SCREEN_WIDTH / NUM_COLUMNS) - 5,
  IMAGE_SIZE = 125,
  OFFSET = (COLUMN_SIZE - IMAGE_SIZE) / 2;

const char SEPARATOR = ',';

unsigned int nextTime = 0;
const int SLEEP_INTERVAL = 5000;

String zip = "94123";
String wx = "";

    // 4.3 Inch E-Paper:
    //       VCC = red
    //       GND = black
    //       D-OUT = white
    //       D-IN = green
    //       WAKEUP = yellow
    //       RESET = blue

void setup() {
    Serial.begin(115200);
    Serial.println("Ready!");
    
    pinMode(LED_PIN, OUTPUT);
    
    Wire.begin(SLAVE_ADDRESS);
    Wire.onRequest(requested);
    Wire.onReceive(received);
    
    epd_init(WAKEUP_PIN, RESET_PIN);
    epd_wakeup(WAKEUP_PIN);
    epd_set_memory(MEM_NAND); // MEM_TF, MEM_NAND
    
    // test();
    // epd_import_sd(); // Uncomment this to import images from SD card

    //lowPower();
}

void loop() {
    
}

void lowPower() {
    digitalWrite(LED_PIN, LOW); // TODO
    delay(100);
    LowPower.powerDown(SLEEP_FOREVER, ADC_OFF, BOD_OFF);
}

void test() {
    String testWx = "Our City (TEST)\n01-01-2001 12:00 PM\n";
    testWx += "Mon,Apr 9,20/15,01d\n";
    testWx += "Tue,Apr 10,10/8,03d\n";
    testWx += "Wed,Apr 11,0/-4,09d\n";
    testWx += "Thu,Apr 12,-2/-3,10n\n";
    testWx += "Fri,Apr 13,100/99,50n\n";
    
    showWx(testWx);
}

void showIcons() {
    
    char * imageNames[] = {
        "01D",
        "01N",
        "02D",
        "02N",
        "03D",
        "03N",
        "04D",
        "04N",
        "09D",
        "09N",
        "10D",
        "10N",
        "11D",
        "11N",
        "13D",
        "13N",
        "50D",
        "50N"
    };
    
    epd_set_color(BLACK, WHITE);
    epd_clear();
    
    delay(500);
    
    epd_set_en_font(ASCII32);
    
    for (int i = 0; i < IMAGE_COUNT; i++) {
        char * imageId = imageNames[i];
        String imageName = String(imageId);
        imageName += ".BMP";
        
        int row = i / NUM_COLUMNS,
        col = i % NUM_COLUMNS,
        x = col * COLUMN_SIZE,
        y = row * COLUMN_SIZE;
        
        epd_disp_bitmap(imageName.c_str(), x, y);
        epd_disp_string(imageId, x, y + IMAGE_SIZE);
            //Serial.println(imageName);
    }
    
    epd_udpate();
}

void requested() {
    Serial.println("Requested! Sending " + zip);
    Wire.write(zip.c_str());
}

void received(int howMany) {
    
    digitalWrite(LED_PIN, HIGH);
    
    String s;
    while (Wire.available()) {
        char c = Wire.read();
        s += c;
    }
    
    if (s.equals("START")) {
        wx = "";
    }
    else if (s.equals("END")) {
        Serial.println(wx);
        showWx(wx);
        
            //lowPower();
    }
    else {
        wx += s;
    }
}

// http://stackoverflow.com/questions/29671455/how-to-split-a-string-using-a-specific-delimiter-in-arduino
String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = {0, -1};
    int maxIndex = data.length()-1;
    
    for(int i=0; i<=maxIndex && found<=index; i++){
        if(data.charAt(i)==separator || i==maxIndex){
            found++;
            strIndex[0] = strIndex[1]+1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    
    return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void showWx(String s) {
    
    epd_set_color(BLACK, WHITE);
    epd_clear();
    
    String line = "";
    int lineCount = 0, wxCount = 0;
    for (int i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '\n') {
            if (lineCount == 0) {
                Serial.println("\n\nLocation: " + line + "\n\n");
                showLocation(line);
            }
            else if (lineCount == 1) {
                showTimestamp(line);
            }
            else {
                showWxDay(line, wxCount++);
            }
            lineCount++;
            line = "";
        }
        else {
            line += c;
        }
    }
    
    delay(500);
    
    epd_udpate();
}

void showWxDay(String line, int index) {
    
        //Serial.println(line);
    if (index >= NUM_COLUMNS) { return; }
    
    epd_set_en_font(ASCII48);
    
        // Tue,Apr 4,20/12,02d
    const int LEN = line.length();
    const int CODE_LEN = 3;
    
    String code = line.substring(LEN - CODE_LEN);
    code.toUpperCase();
    String imageName = code + ".BMP";
    
    String weekday, date, temperature, current = "";
    int dataIndex = 0;
    for (int i = 0; i < line.length(); i++) {
        char c = line.charAt(i);
        if (c == SEPARATOR) {
            if (dataIndex == 0) { weekday = current; }
            else if (dataIndex == 1) { date = current; }
            else if (dataIndex == 2) { temperature = current; }
            current = "";
            dataIndex++;
        }
        else {
            current += c;
        }
    }
    
    int x = 15 + index * COLUMN_SIZE,
        y = 200;
    epd_disp_string(weekday.c_str(), x + OFFSET, y);
    
    y += 50;
    epd_set_en_font(ASCII32);
    epd_disp_string(date.c_str(), x + OFFSET, y);
    
    y += 60;
    epd_set_en_font(ASCII48);
    epd_disp_bitmap(imageName.c_str(), x, y);
    
    y += IMAGE_SIZE + 20;
    epd_disp_string(temperature.c_str(), x + OFFSET, y);
}

void showTimestamp(String timestamp) {
    epd_set_en_font(ASCII32);
    epd_disp_string(timestamp.c_str(), 15 + OFFSET, SCREEN_HEIGHT - 40);
}

void showLocation(String location) {
    epd_set_en_font(ASCII64);
    epd_disp_string(location.c_str(), 15 + OFFSET, 50);
}
