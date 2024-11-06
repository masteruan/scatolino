// SCATOLINO, by Davide Gatti www.survivalhacking.it
// Usa ESP32 Dev Module 
// Revised version by Giovani Gentile Nov 2024
// A small device for displaying multiple pieces of information.
// It can be used to see:
// 1) information about stocks or cryptocurrencies.
// 2) Weather forecasts
// 3) Operating status of servers or computers
// 4) News viewer
// 5) Remote messaging
// 
// * Configurable via WEB
// * Battery operated with sleep mode management
// * E-ink display for excellent viewing and very low power consumption
// * One RGB LED to indicate states
// * Rotation sensor
// * Temperature Sensor
//
// Library to be installed:
// GxEPD2 alla versione 1.5.8 
// Adafruit GFX Library alla versione 1.11.10
// Adafruit BusIO alla versione 1.16.1
// Wire alla versione 3.0.3
// SPI alla versione 3.0.3
// Adafruit MPU6050 alla versione 2.2.6
// Uso la libreria Adafruit Unified Sensor alla versione 1.1.14
// ArduinoJson alla versione 7.1.0
// NTPClient alla versione 3.2.1
// WiFi alla versione 3.0.3
// Networking alla versione 3.0.3
// HTTPClient alla versione 3.0.3
// NetworkClientSecure alla versione 3.0.3
// Preferences alla versione 3.0.3
// WebServer alla versione 3.0.3
// FS alla versione 3.0.3 
// Adafruit NeoPixel alla versione 1.12.3 
// Font converted by https://rop.nl/truetype2gfx/

#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include "OpenSans_Condensed_Bold9pt7b.h"
#include "OpenSans_Condensed_Bold18pt7b.h"
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>

#include <ArduinoJson.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>  
#include <WiFi.h> 
#include <Preferences.h>
#include <WebServer.h>
#include <Adafruit_NeoPixel.h>

#define ENABLE_GxEPD2_GFX 0

#define PIN_WS2812B  4       // ESP32 pin that connects to WS2812B
#define NUM_PIXELS   1       // The number of LEDs (pixels) on WS2812B

#define BATT_MON     35      // Battery monitor PIN 

#define XSENSOR      12      // X Orientation Sensor
#define YSENSOR      14      // Y Orientation Sensor

#define EINK_BN              // DEFINE IF DISPLAY IS BN , commendt this line for color eink display

WiFiUDP ntpUDP;              // Initialize NTP

Adafruit_MPU6050 mpu;        // Intialize rotation sensor

NTPClient timeClient(ntpUDP, "pool.ntp.org");  // Setup NTP serber
String months[12]={"Gen", "Feb", "Mar", "Apr", "Mag", "Giu", "Lug", "Ago", "Set", "Ott", "Nov", "Dic"};
String currentDate;          // Variables for date-time      
String currentTimes;
String company="";
String ncompany="";
String ProductName="SCATOLINO";
String Version="1.2";

uint16_t rotation = 1;
uint16_t bright = 0;                            // for brightness management
uint32_t targetTime = 0;                        //
uint16_t currentStock = 0;                      // 
uint8_t enableSleep = 1;                       // sleep enable indicator
uint8_t luma = 30;                              // brigtness of LED
const char* ssid     = "Xxxxxx";   // Setup WIFI credentials   
const char* password = "Xxxxxxx";           

String payload="";
double current=0;
unsigned long eventInterval= 30000;            // time to change company to be shown min 30 sec = 30000 - suggested 5 min = 300000
unsigned long currentMills = 0;
unsigned long previousMills = eventInterval;
StaticJsonDocument<6000> doc;                  // Intialize Json engine

// Declare our NeoPixel strip object:
Adafruit_NeoPixel WS2812B(NUM_PIXELS, PIN_WS2812B, NEO_RGB + NEO_KHZ400);  // Initialize WS2812 LED

// CS   PIN 5
// DC   PIN 19
// RES  PIN 2 
// BUSY PIN 15
// SDA  PIN 23
// SCL  PIN 18   
// Initialize E-INK disply

GxEPD2_3C<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(GxEPD2_213_BN(/*CS=5*/ 5, /*DC=*/ 19, /*RES=*/ 2, /*BUSY=*/ 15)); // GDEY0213Z98 122x250, SSD1680               BN EINK Display
//  GxEPD2_3C<GxEPD2_213_Z98c, GxEPD2_213_Z98c::HEIGHT> display(GxEPD2_213_Z98c(/*CS=5*/ 5, /*DC=*/ 19, /*RES=*/ 2, /*BUSY=*/ 15)); // GDEY0213Z98 122x250, SSD1680    COL EINK Display

WebServer server(80);        // start web server su porta 80
Preferences preferences;     // start preferenze


void setup() {
  WS2812B.begin();                                     // INITIALIZE WS2812B strip object (REQUIRED)
  WS2812B.setBrightness(60);                           // a value from 0 to 255
  WS2812B.setPixelColor(0, WS2812B.Color(0, 0, 0));    // set black color
  WS2812B.show();                                      // update to the WS2812B Led Strip

  mpu.begin();                                         // Start inclination sensor instance
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);   
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.begin(115200);                                // Initialize serial for debugging

  pinMode(XSENSOR,INPUT);                              // set Xsensor port direction
  pinMode(YSENSOR,INPUT);                              // set Ysensor port direction

  preferences.begin("SurvivalShow", false);            // Get preferences and set defaults

  uint8_t cfg = preferences.getUChar("CONFIG");        // if first time, value must be 0 than force a default configuration
  if (cfg != 55) {          
    preferences.putUChar("CONFIG", 55);                // write 55 marker   55=configured 
    preferences.putUShort("LUMA", 35);
    preferences.putUShort("INTERVALLO", 2);
    preferences.putUShort("STOCKNUM", 10);
    preferences.putString("STOCK1", "BINANCE:BTCUSDT");
    preferences.putString("STOCK2", "BINANCE:ETHUSDT");
    preferences.putString("STOCK3", "BINANCE:ADAUSDT");
    preferences.putString("STOCK4", "BINANCE:DOTUSDT");
    preferences.putString("STOCK5", "MSFT");
    preferences.putString("STOCK6", "AMZN");
    preferences.putString("STOCK7", "TSLA");
    preferences.putString("STOCK8", "AAPL");
    preferences.putString("STOCK9", "NVDA");
    preferences.putString("STOCK10", "ADBE");
    
    preferences.putString("NSTOCK1", "BTC");
    preferences.putString("NSTOCK2", "ETH");
    preferences.putString("NSTOCK3", "ADA");
    preferences.putString("NSTOCK4", "DOT");
    preferences.putString("NSTOCK5", "MSFT");
    preferences.putString("NSTOCK6", "AMZN");
    preferences.putString("NSTOCK7", "TSLA");
    preferences.putString("NSTOCK8", "AAPL");
    preferences.putString("NSTOCK9", "NVDA");
    preferences.putString("NSTOCK10", "ADBE");
    preferences.putString("TOKEN", "INSERISCI QUI IL TOKEN DI FINNHUB");
    preferences.putUShort("LASTSTOCK", 0);
    preferences.putString("TZOFFSET", "+7200");
  }

  luma = (preferences.getUShort("LUMA"));                      // extract brightness
  currentStock = preferences.getUShort("LASTSTOCK");           // extract current stock saved prior to sleep
  int secsleep = preferences.getUShort("INTERVALLO")*60;       // extract sleep timeout
  esp_sleep_enable_timer_wakeup(secsleep * 1000000ull);        // set timer for sleep mode
  // esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0); //1 = High, 0 = Low
  
  eventInterval= (preferences.getUShort("INTERVALLO")*60000);  // extract interval
  previousMills = eventInterval;                               // set interval

  luma = (preferences.getUShort("LUMA"));                      // extract brightness
  display.init(115200,true,50,false);                          // Initialize display
  connect_to_wifi();                                           // connect to wifi
  timeClient.begin();                                          // Time Client startup
  int TZ= preferences.getString("TZOFFSET").toInt();
  timeClient.setTimeOffset(TZ);                                // Set timezone, mybe fixed for LT

  // Webserver pages callbacks
  server.on("/", webConfig);
  server.on("/reset", WifiReset);
  server.on("/parameter", WriteParameter);
  server.onNotFound(handle_NotFound);

  server.begin(); // Start web server
}

void loop() {
// int potValue = analogRead(BATT_MON);
// Serial.print("Battery Voltage: ");
// Serial.println(potValue);
// Serial.println(String(get_orientation()));

 server.handleClient();                         // check web server changes
 currentMills = millis();                       // Main loop timier, to execute tast at configured time
 timeClient.update();                           // get current timedate    
 time_t epochTime = timeClient.getEpochTime();     
 struct tm *ptm = gmtime ((time_t *)&epochTime);   
 int monthDay = ptm->tm_mday;
 int currentMonth = ptm->tm_mon+1;
 String currentMonthName = months[currentMonth-1];
 int currentYear = ptm->tm_year+1900;
 int currHour = ptm->tm_hour;
 int currMin = ptm->tm_min;

 String LeadingHour = "";
 String LeadingMin = "";
 String LeadingDay = "";
 String LeadingMonth = "";
 if (currHour <=9) LeadingHour = "0";             // fix trailing zero
 if (currMin <=9) LeadingMin = "0";               // fix trailing zero
 if (monthDay <=9) LeadingDay = "0";              // fix trailing zero
 if (currentMonth <=9) LeadingMonth = "0";        // fix trailing zero
 currentTimes = LeadingHour + String(currHour) + ":" + LeadingMin + String(currMin); 
 currentDate =  LeadingDay + String(monthDay) + " " + months[currentMonth-1] + " " +  String(currentYear);

 if (currentMills - previousMills >= eventInterval) {    // if timeout reached show data
 
    Serial.println("Time to read stock");

    previousMills = currentMills;                        // setup timer for next task
    timeClient.update();
  
    switch (currentStock) {                              // cycle for configured stocks
    case 0:
      company=preferences.getString("STOCK1");
      ncompany=preferences.getString("NSTOCK1");
      break;
    case 1:
      company=preferences.getString("STOCK2");
      ncompany=preferences.getString("NSTOCK2");
      break;
    case 2:
      company=preferences.getString("STOCK3");
      ncompany=preferences.getString("NSTOCK3");
      break;
    case 3:
      company=preferences.getString("STOCK4");
      ncompany=preferences.getString("NSTOCK4");
      break;
    case 4:
      company=preferences.getString("STOCK5");
      ncompany=preferences.getString("NSTOCK5");
      break;
    case 5:
      company=preferences.getString("STOCK6");
      ncompany=preferences.getString("NSTOCK6");
      break;
    case 6:
      company=preferences.getString("STOCK7");
      ncompany=preferences.getString("NSTOCK7");
      break;
    case 7:
      company=preferences.getString("STOCK8");
      ncompany=preferences.getString("NSTOCK8");
      break;
    case 8:
      company=preferences.getString("STOCK9");
      ncompany=preferences.getString("NSTOCK9");
      break;
    case 9:
      company=preferences.getString("STOCK10");
      ncompany=preferences.getString("NSTOCK10");
      break;
    }
    
    currentStock+=1;
    if (currentStock >= preferences.getUShort("STOCKNUM")) currentStock=0;
    preferences.putUShort("LASTSTOCK", currentStock);
    read_price(company,ncompany);                        // Update display information with new stock
  }
}


int ReadBatt() {                               // Read Battery Voltage
  int average = 0;
  for (int i=0; i < 10; i++) {
    average = average + analogRead(BATT_MON);
  }
  average = average/10;
  return average;
}


void connect_to_wifi() {                        // connect to wifi function
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);                   
  int retry=0;
  
  while (WiFi.status() != WL_CONNECTED) {       // while try connect
    if (bright == 1) {                          // blink led  
      bright = 0;
      WS2812B.setPixelColor(0, WS2812B.Color(luma, luma, 0));  // orange
    } else {
      bright = 1;
      WS2812B.setPixelColor(0, WS2812B.Color(0, 0, 0));  // black
    }
    WS2812B.show();                             // update to the WS2812B Led Strip
    delay(100);  
    Serial.print(".");
    retry+=1;
    if (retry > 60) {                           // in case cant connect to wifi, restart ESP32
      esp_sleep_enable_timer_wakeup(60 * 1000000ull);        // set timer for sleep mode 1 minutes
      Serial.println("");
      Serial.println("Failed to conect to WIFI Going to sleep now for 1 minute then retry");
      delay(100);
      esp_deep_sleep_start();
    }
  }
   
  Serial.println("\nWiFi connected.");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  
  WS2812B.setPixelColor(0, WS2812B.Color(luma, luma, luma));   // White LED for connected
  WS2812B.show();                                              // update to the WS2812B Led Strip
  delay(2000);
  WS2812B.setPixelColor(0, WS2812B.Color(0, 0, 0));            // led off, waiting for stock reading
  WS2812B.show();                                              // update to the WS2812B Led Strip

}

int get_orientation() {                      // Read direction sensor return direction
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);
  int xa =0;
  int ya =0;
  int za =0;
  int ori =0;
  if (abs(a.acceleration.x) < 5) {
    xa=0;
  } else { 
    xa=a.acceleration.x;
  }

  if (abs(a.acceleration.y) < 5) {
    ya=0;
  } else { 
    ya=a.acceleration.y;
  }
  
  za=a.acceleration.z;
  
  if (za > 5 ) {                       //     0     shoulder
    Serial.println("Rotation = 4");
    ori=4;
  } else if (xa < 0 and ya == 0) {     //     0     x=-9         y+/-0.15
    Serial.println("Rotation = 0");
    ori=3;
  } else if (xa == 0 and ya < 0) {     //     90    x=+/-0.15    y-9
    Serial.println("Rotation = 1");
    ori=2;
  } else if (xa > 0 and ya == 0) {     //     180   x=+9         y+/-0.15
    Serial.println("Rotation = 2"); 
    ori=1;
  } else if (xa == 0 and ya > 0) {     //     270   x=+/-0-15    y+9
    Serial.println("Rotation = 3");
    ori=0;
  } else {                             //     OBSCURE ANGLE
    Serial.println("Rotation = 0");
    ori=3;
  }
  return ori;
}


void read_price(String stock_name, String nome) {      // read stock from finnhub and update display
  float currentPrice;
  float differenceInPrice; 
  float percentchange; 
  float change; 
  float previousclose; 
  float vhigh;
  float vlow;
  float open;

  Serial.print("Read Stock:");
  Serial.println(stock_name);
  Serial.println(nome);

  HTTPClient http; 
  http.begin("https://finnhub.io/api/v1/quote?symbol="+stock_name+"&token="+preferences.getString("TOKEN")); 
  //{"c":217.96,"d":0.47,"dp":0.2161,"h":219.49,"l":216.01,"o":218.85,"pc":217.49,"t":1722024001}  
  int httpCode = http.GET();                       // get HTTP page

  if (httpCode > 0) {                              // if there are returned data, parse it 
    payload = http.getString();
    char inp[payload.length()];
    payload.toCharArray(inp,payload.length());
    deserializeJson(doc,inp);                      // parse json 

    //c = Current price, d = Change, dp = Percent change, pc = Previous close price, l=lower, h=higher, o=open
    String v=doc["c"];                             // extract stock info
    String c=doc["dp"];  
    String d=doc["d"];  
    String pc=doc["pc"];  
    String h=doc["h"];  
    String l=doc["l"];
    String o=doc["o"];
    vhigh=h.toDouble();   
    vlow=l.toDouble();   
    open=o.toDouble();   
    current=v.toDouble(); 
    percentchange=c.toDouble();
    previousclose=pc.toDouble();
    change=d.toDouble();

    if (current != 0) {                     // se il valore è diversa da zero allro mostrala, diversamente è un errore e saltala

      rotation = get_orientation();
      Serial.print("Rotation:");
      Serial.println(String(rotation));
      if (rotation != 4) {
        enableSleep=1;                             // enable sleep
        display.setRotation(rotation);
      } else {
        enableSleep=0;                             // disable sleep
        display.setRotation(3);
      }

      
      display.setFullWindow();
      display.firstPage();
//      WS2812B.setPixelColor(0, WS2812B.Color(0, 0, luma));     //  LED Color for COL EINK display
      WS2812B.setPixelColor(0, WS2812B.Color(0, 0, 0));          //  LED color for BN  EINK dispaly   
      WS2812B.show();       
      do {
      //  display.fillScreen(GxEPD_WHITE);
      //  display.fillRect(0, 0, 250, 122, GxEPD_WHITE); // 122x250
        if (rotation == 4)  {                        // shoulder
          display.setFont(&FreeSansBold9pt7b);
          display.setTextColor(GxEPD_BLACK);
          display.setCursor(2, 15);
          display.print(ProductName+" V"+Version);
          display.setCursor(2, 30);
          display.print("IP:");
          display.setCursor(25, 30);
          display.print(WiFi.localIP());
          display.setCursor(2, 45);
          display.print("Vbatt:"+String(ReadBatt()));
          display.setCursor(2, 60);
          display.print("LED Lum:");
          display.setCursor(100, 60);
          display.print(String(preferences.getUShort("LUMA"))); 
          display.setCursor(2, 75);
          display.print("Interval:");
          display.setCursor(100, 75);
          display.print(String(preferences.getUShort("INTERVALLO"))); 

          display.setCursor(2, 105);
          display.print("www.survivalhacking.it");

        } else if (rotation == 0 || rotation == 2) {   // Vertical
          // Draw company
          display.fillRect(0, 0, 122, 50, GxEPD_BLACK); // 122x250
          display.drawRect(0, 0, 122, 110, GxEPD_BLACK); // 122x250
          display.setFont(&FreeSansBold18pt7b);
          display.setTextColor(GxEPD_WHITE);
          display.setCursor(7, 37);
          //display.print(stock_name);
          display.print(nome);

          // Draw value
          //display.setFont(&FreeSans18pt7b);
          //display.setFont(&OpenSans_Condensed_Bold18pt7b);
          display.setFont(&FreeSansBold12pt7b);
          display.setTextColor(GxEPD_BLACK);
          display.setCursor(1, 80);
          display.print("$"+String(current));

          // Draw Date Time
          display.drawRect(0, 178, 122, 70, GxEPD_BLACK); // 122x250
          display.setFont(&FreeSansBold9pt7b);
          display.setTextColor(GxEPD_BLACK);
          display.setCursor(35, 200);
          display.print(currentTimes);
          display.setCursor(8, 220);
          display.print(currentDate);
          display.setCursor(1, 240);
          showBatt(0);   

          // Draw profit/Loss and other stats
          display.drawRect(0, 109, 122, 70, GxEPD_BLACK); // 122x250
//          display.setFont(&FreeSans9pt7b);
          display.setFont(&OpenSans_Condensed_Bold9pt7b);
          display.setCursor(2, 130);
          display.print("MIN=$" + String(vlow)); 
          display.setCursor(2, 150);
          display.print("MAX=$"+ String(vhigh));
          display.setCursor(2, 170);
          display.print("PRE=$" + String(previousclose));
          display.setCursor(2, 100);

          if(percentchange<0.0) { // loss 
            // Display loss
            display.print("PERD=" + String(percentchange) + "%");
            WS2812B.setPixelColor(0, WS2812B.Color(luma, 0, 0));

          } else { 
            // Display profit
            display.print("PROF=" + String(percentchange) + "%");
            WS2812B.setPixelColor(0, WS2812B.Color(0, luma, 0));
          }
        } else if (rotation == 1 || rotation == 3) {    // horizontal
          // Draw company hor
          display.fillRect(0, 0, 115, 50, GxEPD_BLACK); // 122x250
          display.drawRect(0, 0, 250, 50, GxEPD_BLACK); // 122x250
          display.setFont(&FreeSansBold18pt7b);
          display.setTextColor(GxEPD_WHITE);
          display.setCursor(2, 37);
          //display.print(stock_name);
          display.print(nome);

          // Draw value hor
          //display.setFont(&FreeSansBold18pt7b);
          display.setFont(&FreeSansBold12pt7b);
          display.setTextColor(GxEPD_BLACK);
          display.setCursor(117, 36);
          display.print("$"+String(current));

          // Draw Date Time hor
          display.drawRect(0, 97, 250, 25, GxEPD_BLACK); // 122x250
          display.setFont(&FreeSansBold9pt7b);
          display.setTextColor(GxEPD_BLACK);
          display.setCursor(3, 115);
          display.print(currentTimes);
          display.setCursor(65, 115);
          display.print(currentDate);
          display.setCursor(190, 115);
          showBatt(1);   

          // Draw profit/Loss and other stats hor
          display.drawRect(0, 49, 250, 49, GxEPD_BLACK); // 122x250
          display.setFont(&FreeSansBold9pt7b);
          display.setCursor(8, 78);
          display.print("Min=$" + String(vlow) + "  Max=$"+ String(vhigh));
          display.setCursor(25, 93);
          display.print("Chiusura prec.=$" + String(previousclose));
          display.setCursor(55, 63);

          if(percentchange<0.0) { // loss 
            // Display loss
            display.print("Perdita = " + String(percentchange) + "%");
            WS2812B.setPixelColor(0, WS2812B.Color(luma, 0, 0));

          } else { 
            // Display profit hor
            display.print("Profitto = " + String(percentchange) + "%");
            WS2812B.setPixelColor(0, WS2812B.Color(0, luma, 0));
          }
        }


      }
      while (display.nextPage()); 
      display.hibernate();
      WS2812B.show();                                          // update to the WS2812B Led Strip
      //Go to deep sleep now
      if (enableSleep==1) {                                   // if sleep enabled 
        Serial.println("Going to sleep now");
        delay(100);
        esp_deep_sleep_start();
      } else {
        Serial.println("Do not sleep");
      }  
    } else {
      Serial.print("Value = 0 mean finnhub can' give the proper value, company skipped.");
      previousMills = eventInterval;    // reset timeout
    }
  } else {
    Serial.print("Failed to retrieve values try to restart ESP");
    ESP.restart(); 
  }
}


void showBatt(int orientation) {  
   uint8_t percentage = 100;
   float voltage = analogRead(35) / 4096.0 * 7.46;
   Serial.println("Voltage = " + String(voltage));
   percentage = 2836.9625 * pow(voltage, 4) - 43987.4889 * pow(voltage, 3) + 255233.8134 * pow(voltage, 2) - 656689.7123 * voltage + 632041.7303;
   if (voltage >= 4.20) percentage = 100;
   if (voltage <= 3.50) percentage = 0;

   if (orientation == 1) {                            // Horizontal
      display.drawRect(180, 102, 62, 15, GxEPD_BLACK); // draw battery body   122x250
      display.drawRect(242, 104, 4, 10, GxEPD_BLACK);  // draw batteri edge

      if (percentage > 10) {                               
        display.fillRect(182, 104, 10, 11, GxEPD_BLACK); 
      }
      if (percentage > 30) {                           
        display.fillRect(194, 104, 10, 11, GxEPD_BLACK); 
      }
      if (percentage > 50) {                           
        display.fillRect(206, 104, 10, 11, GxEPD_BLACK); 
      }
      if (percentage > 70) {                            
        display.fillRect(218, 104, 10, 11, GxEPD_BLACK); 
      }
      if (percentage > 90) {                           
        display.fillRect(230, 104, 10, 11, GxEPD_BLACK); 
      }
    } else {  // VERTICAL
      display.drawRect(30, 230, 62, 15, GxEPD_BLACK); // draw battery body
      display.drawRect(92, 232, 4, 10, GxEPD_BLACK);  // draw batteri edge

      if (percentage > 10) {
        display.fillRect(32, 232, 10, 11, GxEPD_BLACK); 
      }
      if (percentage > 30) {
        display.fillRect(44, 232, 10, 11, GxEPD_BLACK); 
      }
      if (percentage > 50) { 
        display.fillRect(56, 232, 10, 11, GxEPD_BLACK); 
      }
      if (percentage > 70) { 
        display.fillRect(68, 232, 10, 11, GxEPD_BLACK); 
      }
      if (percentage > 90) { 
        display.fillRect(80, 232, 10, 11, GxEPD_BLACK); 
      }
    }    

}



void showBatt2(int orientation) {  
   int adcb= ReadBatt();                               // 1725 = dead battery  -   2509 = full charge
   Serial.println("Vbat:"+String(adcb));
   if (orientation == 1) {                            // Horizontal
      display.drawRect(180, 102, 62, 15, GxEPD_BLACK); // draw battery body   122x250
      display.drawRect(242, 104, 4, 10, GxEPD_BLACK);  // draw batteri edge

      if (adcb > 1800) {                               // 20% 
        display.fillRect(182, 104, 10, 11, GxEPD_BLACK); 
      }
      if (adcb > 1800+137) {                           // 40%
        display.fillRect(194, 104, 10, 11, GxEPD_BLACK); 
      }
      if (adcb > 1800+274) {                           // 60%
        display.fillRect(206, 104, 10, 11, GxEPD_BLACK); 
      }
      if (adcb > 1800+412) {                           // 80% 
        display.fillRect(218, 104, 10, 11, GxEPD_BLACK); 
      }
      if (adcb > 1800+550) {                           // 100%
        display.fillRect(230, 104, 10, 11, GxEPD_BLACK); 
      }
    } else {  // VERTICAL
      display.drawRect(30, 230, 62, 15, GxEPD_BLACK); // draw battery body
      display.drawRect(92, 232, 4, 10, GxEPD_BLACK);  // draw batteri edge

      if (adcb > 1800) {                               // 20% 
        display.fillRect(32, 232, 10, 11, GxEPD_BLACK); 
      }
      if (adcb > 1800+137) {                           // 40%
        display.fillRect(44, 232, 10, 11, GxEPD_BLACK); 
      }
      if (adcb > 1800+274) {                           // 60%
        display.fillRect(56, 232, 10, 11, GxEPD_BLACK); 
      }
      if (adcb > 1800+412) {                           // 80% 
        display.fillRect(68, 232, 10, 11, GxEPD_BLACK); 
      }
      if (adcb > 1800+550) {                           // 100%
        display.fillRect(80, 232, 10, 11, GxEPD_BLACK); 
      }
    }    

}



// Wifi Reset Web Page
void WifiReset() {
  String s = "<h1>Le impostazioni Wi-Fi e timezone sono state reimpostate.";
  server.send(200, "text/html", makePage("Reimpostazione configurazione Wi-Fi", s));
 // wifi.ResetWIFI();
  delay(3000);
  Serial.println("Forced Restart");
  ESP.restart();
}

// Configuration web page
void webConfig() {
  String s = "<h1>"+ProductName+" V"+Version+"</h1>";
  s += "<form method=\"get\" action=\"parameter\"><br>Elementi da vedere (0..10) <input name=\"stocknum\" length=2 type=\"number\" onchange=\"form.submit()\" value=" + (String)preferences.getUShort("STOCKNUM") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  1 <input name=\"stock1\" length=4 id=\"stock1\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK1") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  2 <input name=\"stock2\" length=4 id=\"stock2\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK2") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  3 <input name=\"stock3\" length=4 id=\"stock3\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK3") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  4 <input name=\"stock4\" length=4 id=\"stock4\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK4") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  5 <input name=\"stock5\" length=4 id=\"stock5\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK5") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  6 <input name=\"stock6\" length=4 id=\"stock6\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK6") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  7 <input name=\"stock7\" length=4 id=\"stock7\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK7") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  8 <input name=\"stock8\" length=4 id=\"stock8\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK8") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock  9 <input name=\"stock9\" length=4 id=\"stock9\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK9") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Codice stock 10 <input name=\"stock10\" length=4 id=\"stock10\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("STOCK10") + ">";

  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 1 <input name=\"nstock1\" length=4 id=\"nstock1\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK1") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 2 <input name=\"nstock2\" length=4 id=\"nstock2\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK2") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 3 <input name=\"nstock3\" length=4 id=\"nstock3\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK3") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 4 <input name=\"nstock4\" length=4 id=\"nstock4\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK4") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 5 <input name=\"nstock5\" length=4 id=\"nstock5\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK5") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 6 <input name=\"nstock6\" length=4 id=\"nstock6\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK6") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 7 <input name=\"nstock7\" length=4 id=\"nstock7\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK7") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 8 <input name=\"nstock8\" length=4 id=\"nstock8\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK8") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 9 <input name=\"nstock9\" length=4 id=\"nstock9\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK9") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Nome stock 10 <input name=\"nstock10\" length=4 id=\"nstock10\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("NSTOCK10") + ">";
  
  s += "<form method=\"get\" action=\"parameter\"><br>Luminosita' LED <input name=\"luma\" length=3 type=\"range\" onchange=\"form.submit()\" min=\"0\" max=\"255\" value=" + (String)preferences.getUShort("LUMA") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Intervallo (Minuti) <input name=\"interval\" length=3 type=\"number\" onchange=\"form.submit()\" value=" + (String)preferences.getUShort("INTERVALLO") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>Finnhub Token <input name=\"token\" length=50 id=\"token\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("TOKEN") + ">";
  s += "<form method=\"get\" action=\"parameter\"><br>TZ Offset (+7200) <input name=\"tzoffset\" length=6 id=\"tzoffset\" type=\"text\" onchange=\"form.submit()\" value=" + (String)preferences.getString("TZOFFSET") + ">";
  server.send(200, "text/html", s);
}

// store config changed via web
void WriteParameter() {

  // Store config
  Serial.println("");
  
  uint8_t ltemp = atoi((urlDecode(server.arg("luma"))).c_str()); 
  preferences.putUShort("LUMA", ltemp);
  Serial.print("Luma:");
  Serial.println(String(ltemp));

  uint8_t stocknum = atoi((urlDecode(server.arg("stocknum"))).c_str()); 
  if (stocknum < 1) stocknum=1; // 
  if (stocknum > 10) stocknum=10; // 
  preferences.putUShort("STOCKNUM", stocknum);
  Serial.print("Curr Stock:");
  Serial.println(String(stocknum));

  uint8_t linterval = atoi((urlDecode(server.arg("interval"))).c_str()); 
  if (linterval < 1) linterval=1; // 
  if (linterval > 999) linterval=999; // 
  preferences.putUShort("INTERVALLO", linterval);
  Serial.print("Interval:");
  Serial.println(String(linterval));

  preferences.putString("STOCK1", urlDecode(server.arg("stock1")));
  preferences.putString("STOCK2", urlDecode(server.arg("stock2")));
  preferences.putString("STOCK3", urlDecode(server.arg("stock3")));
  preferences.putString("STOCK4", urlDecode(server.arg("stock4")));
  preferences.putString("STOCK5", urlDecode(server.arg("stock5")));
  preferences.putString("STOCK6", urlDecode(server.arg("stock6")));
  preferences.putString("STOCK7", urlDecode(server.arg("stock7")));
  preferences.putString("STOCK8", urlDecode(server.arg("stock8")));
  preferences.putString("STOCK9", urlDecode(server.arg("stock9")));
  preferences.putString("NSTOCK1", urlDecode(server.arg("nstock1")));
  preferences.putString("NSTOCK2", urlDecode(server.arg("nstock2")));
  preferences.putString("NSTOCK3", urlDecode(server.arg("nstock3")));
  preferences.putString("NSTOCK4", urlDecode(server.arg("nstock4")));
  preferences.putString("NSTOCK5", urlDecode(server.arg("nstock5")));
  preferences.putString("NSTOCK6", urlDecode(server.arg("nstock6")));
  preferences.putString("NSTOCK7", urlDecode(server.arg("nstock7")));
  preferences.putString("NSTOCK8", urlDecode(server.arg("nstock8")));
  preferences.putString("NSTOCK9", urlDecode(server.arg("nstock9")));
  preferences.putString("NSTOCK10", urlDecode(server.arg("nstock10")));
  
  preferences.putString("TOKEN", urlDecode(server.arg("token")));
  preferences.putString("TZOFFSET", urlDecode(server.arg("tzoffset")));

  int TZ= preferences.getString("TZOFFSET").toInt();
  timeClient.setTimeOffset(TZ);                                // Set timezone, mybe fixed for LT
  Serial.print("TZ Offset:");
  Serial.println(String(TZ));
  Serial.println("");


  // Reload updated data
  eventInterval= (preferences.getUShort("INTERVALLO")*60000);  // extract interval
  previousMills = eventInterval;
  luma = (preferences.getUShort("LUMA"));                      // extract brightness
  webConfig();
}

// web page not found
void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

// standard web page header
String makePage(String title, String contents) {
  String s = "<!DOCTYPE html><html><head>";
  s += "<meta name=\"viewport\" content=\"width=device-width,user-scalable=0\">";
  s += "<title>";
  s += title;
  s += "</title></head><body>";
  s += contents;
  s += "</body></html>";
  return s;
}

// url decode replaces
String urlDecode(String input) {
  String s = input;
  s.replace("%20", " ");
  s.replace("+", " ");
  s.replace("%21", "!");
  s.replace("%22", "\"");
  s.replace("%23", "#");
  s.replace("%24", "$");
  s.replace("%25", "%");
  s.replace("%26", "&");
  s.replace("%27", "\'");
  s.replace("%28", "(");
  s.replace("%29", ")");
  s.replace("%30", "*");
  s.replace("%31", "+");
  s.replace("%2C", ",");
  s.replace("%2E", ".");
  s.replace("%2F", "/");
  s.replace("%2C", ",");
  s.replace("%3A", ":");
  s.replace("%3A", ";");
  s.replace("%3C", "<");
  s.replace("%3D", "=");
  s.replace("%3E", ">");
  s.replace("%3F", "?");
  s.replace("%40", "@");
  s.replace("%5B", "[");
  s.replace("%5C", "\\");
  s.replace("%5D", "]");
  s.replace("%5E", "^");
  s.replace("%5F", "-");
  s.replace("%60", "`");
  return s;
}

long stringtolong(String recv) {
  char c[recv.length() + 1];
  recv.toCharArray(c, recv.length() + 1);
  return strtol(c, NULL, 16);
}
