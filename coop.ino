// TODO: Set up sound sensor
// TODO: Set up time keeping with wifi

#include "variables.h"          // Contains Wifi credentials
#include <Adafruit_NeoPixel.h>  // LEDs
#include <DHT.h>                // Humidity and temperature sensor
#include <RTClib.h>             // Real Time Clock - clock module
#include <SoftwareSerial.h>     // Make TX/RX of other ports -> Needed for WiFi-pins

// Define pins 
#define DHT_PIN 13
#define DHT_TYPE DHT11
#define MIC_PIN 12
#define LED_PIN 11
#define LED_COUNT 16
#define WifiRX 2
#define WifiTX 3

// Declare necessary classes
SoftwareSerial wifi(WifiRX, WifiTX); 
DHT dht(DHT_PIN, DHT_TYPE); 
Adafruit_NeoPixel pixels(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
RTC_DS1307 rtc;

// Get wifi credentials from seperate file
String WIFI = String(WIFI_SSID);
String PASS = String(WIFI_PASS);

// Set up global variables
int countTrueCommand;
int countTimeCommand;
boolean found = false;
boolean dhtTrigger = false;

void setup() {
  // Start components
  Serial.begin(9600);
  wifi.begin(115200);
  rtc.begin(); // Note: RTC is on 57600 baud
  
  dht.begin();
  pixels.begin();

  // Print
  Serial.println("Coop automatization system");

  // Set the RTC to the date & time this sketch was compiled
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // January 21, 2014 at 3am you would call:
  // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  // rtc.adjust(DateTime(YYYY, M, D, h, m, s));

  sendWifiCommand("AT", 5, "OK");
  sendWifiCommand("AT+CWMODE=1", 5, "OK");
  sendWifiCommand("AT+CWJAP=\"" + WIFI + "\",\"" + PASS + "\"", 20, "OK");
  countTrueCommand = 0;
}

void loop() {
DateTime now = rtc.now(); 
  
if (now.second() % 5 == 0) {
  if (!dhtTrigger) {
      // *** Humidity and Temperature ***
      // Note: Check DHT max every 2 seconds
      float humidity = dht.readHumidity(); 
      float temperature = dht.readTemperature(); 
    
      if (isnan(humidity) || isnan(temperature)) {
        Serial.println(F("Failed to read from DHT sensor!"));
        return;
      }
      // Compute heat index in Celsius (isFahreheit = false)
      float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    
      printValues(now, humidity, temperature, heatIndex);
//    printValues(humidity, temperature, heatIndex);
      setLights(humidity, temperature, heatIndex);
    }
  } else if (dhtTrigger) {
      dhtTrigger = false; 
}

/* Sending a GET-call:
  String getData = "GET /update?api_key=" + APIKEY + "&" + FIELDVALUE + "=" + String(valSensor);
  sendWifiCommand("AT+CIPSTART=0, \"TCP\", \"" + HOST + "\"," + PORT, 15, "OK");
  sendWifiCommand("AT+CIPSEND=0, " + String(getData.length() + 4), 4, ">");
  wifi.println(getData);
  delay(1500);
  sendWifiCommand("AT+CIPCLOSE=0", 5, "OK");
*/
}

void printValues(DateTime time, float humidity, float temperature, float heatIndex) {
  char buf[] = "DDD DD/MM-YY hh:mm:ss";
  Serial.print(time.toString(buf));
  Serial.print(F(" - Humidity: "));
  Serial.print(humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature);
  Serial.print(F("°C  Heat index: "));
  Serial.print(heatIndex);
  Serial.print(F("°C\n"));
}

void setLights(float humidity, float temperature, float heatIndex) {
  pixels.clear();

  int heatIndexLED = 0;
  int temperatureLED = 1;
  int humidityLED = 2; 
  
  // Heat Index - yellow below freezing, red above 30C, else green
  if (heatIndex < 0.0) {
    pixels.setPixelColor(heatIndexLED, pixels.Color(255, 255, 0));
  } else if (heatIndex > 35) {
    pixels.setPixelColor(heatIndexLED, pixels.Color(255, 0, 0));
  } else {
    pixels.setPixelColor(heatIndexLED, pixels.Color(0, 255, 0));
  }
  
  // Temperature - yellow below freezing, red above 30C, else green
  if (temperature < 0.0) {
    pixels.setPixelColor(temperatureLED, pixels.Color(255, 255, 0));
  } else if (temperature > 30) {
    pixels.setPixelColor(temperatureLED, pixels.Color(255, 0, 0));
  } else {
    pixels.setPixelColor(temperatureLED, pixels.Color(0, 255, 0));
  }
  
  // Humidity - green at 40-70% relative humidity
  if (humidity < 40.0 || humidity > 70.0) {
    pixels.setPixelColor(humidityLED, pixels.Color(150, 0, 0));
  } else {
    pixels.setPixelColor(humidityLED, pixels.Color(0, 150, 0));
  }
 
  pixels.show();
}


void sendWifiCommand(String command, int maxTime, char expectedReply[]) {
  Serial.print(countTrueCommand);
  Serial.print(". at command => ");
  Serial.print(command);
  Serial.print(" ");
  while (countTimeCommand <= maxTime) {
    // AT+CIPSEND
    wifi.println(command);
    
    // OK
    if(wifi.find(expectedReply)) {
      found = true;
      break;
    }
  
    countTimeCommand++;
  }
  
  if(found == true){
    Serial.println("- Success!");
    countTrueCommand++;
    countTimeCommand = 0;
  }
  
  if(found == false){
    Serial.println("- Fail...");
    countTrueCommand = 0;
    countTimeCommand = 0;
  }
  
  found = false;
 }
