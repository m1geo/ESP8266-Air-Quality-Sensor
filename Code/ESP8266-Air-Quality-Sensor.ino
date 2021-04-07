/**
 * George Smart, M1GEO
 * https://www.george-smart.co.uk
 *
 * First Release: 30/04/2019
 * This Release:  07/04/2021
 * 
 * Air Quality Monitor
 **/

// Include the correct display library
// For a connection via I2C using Wire include
#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`
#include <SPI.h>

// Include custom images
#include "images.h"

// Initialize the OLED display using Wire library
SSD1306Wire display(0x3c, D2, D1);

const int led = LED_BUILTIN;

// Networking stuff
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define SAMPLES 1440

// ThinkSpeak Stuff
#include "ThingSpeak.h"

// Include custom settings (WiFi and ThingSpeak)
#include "settings.h"

const char *ssid = STASSID;
const char *password = STAPSK;

ESP8266WebServer server(80);
WiFiClient client;

// Fan Pin
#define FANPIN 12  // NodeMCU D6 is GPIO12

// CCS811 Stuff
#include "Adafruit_CCS811.h"
Adafruit_CCS811 ccs;

//BME280 Stuff
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // I2C

// DHT22 sensor
#include "DHT.h"
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTPIN  14 // NodeMCU D5 is GPIO14    // Digital pin connected to the DHT sensor
DHT dht(DHTPIN, DHTTYPE);

long  rssi = 0.0;
float tmp  = 0.0;
float co2  = 0.0;
float tvoc = 0.0;
float humi = 0.0;
float pres = 0.0;

int clients_connected = 0;
int sensor_busy = 0;
int fan_busy = 0;

void setup() {
  Serial.begin(115200);
  wifi_set_macaddr(0, const_cast<uint8*>(mac)); // override the default MAC address

  pinMode(led, OUTPUT);
  digitalWrite(led, LOW);

  pinMode(FANPIN, OUTPUT);
  
  Serial.println();
  Serial.println();
  Serial.println(F("George Smart (M1GEO)"));

  Serial.print("WiFi MAC: ");
  Serial.println(WiFi.macAddress());

  // Initialising the UI will init the display too.
  display.init();

  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  Wifi_Logo_Splash(F("George Smart (M1GEO)"));
  delay(2000);
  WiFi.mode(WIFI_STA);

  if(!ccs.begin()){
    Serial.println("Failed to start CCS811 sensor!");
    Wifi_Logo_Splash(F("CCS811 Fail!"));
    while(1);
  }

  if(!bme.begin(0x76)){ // some modules use address 0x77
    Serial.println("Failed to start BME280 sensor!");
    Wifi_Logo_Splash(F("BME280 Fail!"));
    while(1);
  }
  
  dht.begin();
  
  //calibrate temperature sensor
  Wifi_Logo_Splash(F("CCS811 Calibrating..."));
  ccs811_calibrate(); // dont use: https://github.com/adafruit/Adafruit_CCS811/issues/13 - works better with it still?!
  delay(1000);

  Wifi_Logo_Splash("BME280 ID: " + bme.sensorID());
  delay(1000);
  
  WiFi.hostname(DEVICENAME); // set network device name
  WiFi.begin(ssid, password);
  //WiFi.begin(ssid); // open wifi
  Serial.println("");
  Wifi_Logo_Splash("Connecting to WiFi.\nMAC: " + String(WiFi.macAddress()));
  delay(1000);
  
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(led, LOW);
    delay(250);
    digitalWrite(led, HIGH);
    Serial.print(".");
    delay(250);
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  if (MDNS.begin(DEVICENAME)) {
    Serial.println("MDNS started");
  }  

  Wifi_Logo_Splash("DHCP IP: " + String(WiFi.localIP().toString()));
  
  delay(2000);
  
  server.on("/", handleRoot);
  server.on("/all", handleALL);
  server.on("/data", handleALL);
  server.on("/values", handleALL);
  server.on("/temp", handleTEMP);
  server.on("/co2", handleCO2);
  server.on("/tvoc", handleTVOC);
  server.on("/humi", handleHUMI);
  server.on("/rssi", handleRSSI);
  server.on("/pres", handlePRES);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  ThingSpeak.begin(client);
  Serial.println("ThinkSpeak client started");

  ThingSpeak.setStatus("MAC: " + String(WiFi.macAddress()) + " \nLAN IP: " + String(WiFi.localIP().toString()) + " \nFW Build: " + String(__DATE__) + " " + String(__TIME__));
}

void loop() {
  UserScreen();
  server.handleClient();
  MDNS.update();

  // Code to take a reading.
  if (millis() % 30000 < 100) {
    fan_busy++;
    digitalWrite(FANPIN, HIGH);
    UserScreen();
    ccs811_calibrate(); // dont use: https://github.com/adafruit/Adafruit_CCS811/issues/13 - works better with it in still?!
    Sdelay(2000);
    
    sensor_busy++;
    UserScreen();
    
    rssi = WiFi.RSSI();
    
    humi = dht.readHumidity();
    tmp = dht.readTemperature();
    
    if (isnan(tmp) || isnan(humi)) {
      Serial.println(F("Failed to read from DHT sensor!"));
    }
    
    if (ccs.available()) {
      //tmp = ccs.calculateTemperature(); // calculated temperature is useless!
      if(!ccs.readData()){
        co2 = ccs.geteCO2(); // ppm
        tvoc = ccs.getTVOC(); // ppb
        pres = bme.readPressure() / 100.0F; // hPa
        UpdateThingSpeak();
      }
    } else {
      co2 = 0.0;
      tvoc = 0.0;
      pres = 0.0;
    }
    Sdelay(100);
    sensor_busy--;
    digitalWrite(FANPIN, LOW);
    fan_busy--;
  }
}

void Sdelay(unsigned long t) {
  unsigned long start_time = millis();
  while (millis() < (start_time + t - 2)) {
    UserScreen();
    delay(2);
  }
}

void UserScreen(void) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  // alternate first line with IP/RSSI.
  if ((millis() % 5000) > 2500) {
    display.drawString(0, 00, "IP: " + String(WiFi.localIP().toString()));
  } else {
    display.drawString(0, 0, "RSSI: " + String(rssi) + " dBm, TS: " + String((int(millis()/1000))));
  }
  
  display.drawString(0, 10, "Temp: " + String(tmp) + " C");
  display.drawString(0, 20, "Humidity: " + String(humi) + " %");
  display.drawString(0, 30, "Pressure: " + String(pres) + " hPa");
  display.drawString(0, 40, "CO2: " + String(co2) + " ppm");
  display.drawString(0, 50, "TVOC: " + String(tvoc) + " ppb");
  
  // Heartbeat
  if(millis() % 1000 > 500) {
    display.drawXbm(128 - heart_width, 0, heart_width, heart_height, heart_bits);
  }

  // Internet Busy
  if(clients_connected > 0) {
    display.drawXbm(128 - heart_width - 3 - globe_width, 0, globe_width, globe_height, globe_bits);
  }

  // Sensor Busy
  if(sensor_busy > 0) {
    display.drawXbm(128 - heart_width - 3 - globe_width - 3 - sensor_width, 0, sensor_width, sensor_height, sensor_bits);
  }

  // Fan On
  if(fan_busy > 0) {
    display.drawXbm(128 - heart_width - 3 - globe_width - 3 - sensor_width - 3 - fan_width, 0, fan_width, fan_height, fan_bits);
  }
  
  display.display();
}

void Wifi_Logo_Splash(String st) {
  // clear the display
  display.clear();
  
  // see http://blog.squix.org/2015/05/esp8266-nodemcu-how-to-create-xbm.html
  // on how to create xbm files
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 0, st);
  display.drawXbm(34, 64-WiFi_Logo_height, WiFi_Logo_width, WiFi_Logo_height, WiFi_Logo_bits);
  display.display();
}

// Web functions
int UpdateThingSpeak() {
  int httpCode = 0;
  int retval = 0;

  ThingSpeak.setField(1, tmp);
  ThingSpeak.setField(2, co2);
  ThingSpeak.setField(3, tvoc);
  ThingSpeak.setField(4, rssi);
  ThingSpeak.setField(5, humi);
  ThingSpeak.setField(6, pres);

  httpCode = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (httpCode == 200) {
    Serial.println("Channel field write successful.");
  } else {
    Serial.println("Problem writing to Channel field. HTTP error code " + String(httpCode));
    retval += 1;
  }
  return httpCode;
}

void ccs811_calibrate() {
  while(!ccs.available());
  digitalWrite(FANPIN, HIGH);
  Sdelay(2000);
  float temp = ccs.calculateTemperature();
  float realt = dht.readTemperature();
  realt = dht.readTemperature();
  realt = dht.readTemperature(); // get nice fresh reading
  realt = dht.readTemperature();
  
  //ccs.setTempOffset(temp - 25.0); // maybe this should use the DHT11 temperature instead of simply 25C?
  ccs.setTempOffset(temp - realt); // maybe this should use the DHT11 temperature instead of simply 25C?
  Serial.print("CCS811 sensor calibrated at ");
  Serial.print(realt);
  Serial.println("C");
  digitalWrite(FANPIN, LOW);
}

void handleTEMP() {
  clients_connected++;
  server.send(200, "text/plain", String(tmp) +" C");
  clients_connected--;
}

void handleCO2() {
  clients_connected++;
  server.send(200, "text/plain", String(co2) +" ppm");
  clients_connected--;
}

void handleTVOC() {
  clients_connected++;
  server.send(200, "text/plain", String(tvoc) +" ppb");
  clients_connected--;
}

void handleRSSI() {
  clients_connected++;
  server.send(200, "text/plain", String(rssi) +" dBm");
  clients_connected--;
}

void handleHUMI() {
  clients_connected++;
  server.send(200, "text/plain", String(humi) +" %");
  clients_connected--;
}

void handlePRES() {
  clients_connected++;
  server.send(200, "text/plain", String(pres) +" hPa");
  clients_connected--;
}

void handleALL() {
  clients_connected++;
  server.send(200, "text/plain", String(tmp) +" C," + String(humi) +" %," + String(co2) +" ppm," + String(tvoc) +" ppb," + String(rssi) +" dBm," + String(pres) +" hPa");
  clients_connected--;
}

void handleNotFound() {
  clients_connected++;
  UserScreen();
  digitalWrite(led, 1);
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
  digitalWrite(led, 0);
  clients_connected--;
}

void handleRoot() {
  clients_connected++;
  UserScreen();
  digitalWrite(led, 1);
  char temp[900];
  int sec = millis() / 1000;
  int min = sec / 60;
  int hr = min / 60;

  snprintf(temp, 900,
 "<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>Air Quality Sensor</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>AQS</h1>\
    <p>The CCS811 Sensor measures air temperature, carbon dioxide, and volatile organic compounds. The BME280 measures air pressure. The DHT sensor measures air temperature and humidity.</p>\
    <ul>\
      <li><a href=\"/temp\">Temperature</a> (%2.2f C)</li>\
      <li><a href=\"/humi\">Humidity</a> (%2.2f %%)</li>\
      <li><a href=\"/co2\">Carbon Dioxide</a> (%3.1f ppm)</li>\
      <li><a href=\"/tvoc\">Total Volatile Organic Compounds</a> (%3.1f ppb)</li>\
      <li><a href=\"/pres\">Air Pressure</a> (%4.2f hPa)</li>\
      <li><a href=\"/rssi\">WiFi Received Signal Strength</a> (%ld dBm)</li>\
    </ul>\
    <p><i>Node uptime: %02d:%02d:%02d<br>George Smart, M1GEO.<br>FW Built: %s %s</i></p>\
  </body>\
</html>",
  tmp, humi, co2, tvoc, pres, rssi,
  hr, min % 60, sec % 60, __DATE__, __TIME__ );
  server.send(200, "text/html", temp);
  digitalWrite(led, 0);
  clients_connected--;
}
