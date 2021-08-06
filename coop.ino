
// TODO: Set up sound sensor
// TODO: Set up time keeping with wifi

#include "variables.h"          // Contains Wifi credentials
#include <Adafruit_NeoPixel.h>  // LEDs
#include <DHT.h>                // Humidity and temperature sensor
#include <SoftwareSerial.h>     // Make TX/RX of other ports -> Needed for WiFi-pins
#include <DS1307RTC.h>          // Real Time Clock - defines RTC variable
#include <Timezone.h>           // Handles DST - https://github.com/JChristensen/Timezone
#include <TimeLib.h>            // 

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
//RTC_DS1307 rtc;

// Get wifi credentials from seperate file
String WIFI = String(WIFI_SSID);
String PASS = String(WIFI_PASS);

// Set up global variables
int countTimeCommand;
boolean found = false;
boolean dhtTrigger = false;

// Time variables 
tmElements_t time;
const char *monthName[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};
const char *weekDay[7] = {
  "Sat", "Sun", "Mon", "Tue", "Wed", "Thu", "Fri"
};
// Timezone variables
TimeChangeRule DST = {"CEST", First, Sun, Mar, 2, 120};
TimeChangeRule STD = {"CET", Last, Sun, Oct, 2, 60};
Timezone timeZone(DST, STD);
TimeChangeRule *tcr;



void setup() {
  // Start components
  Serial.begin(9600);
  wifi.begin(115200);
  dht.begin();
  pixels.begin();

  // Print
  Serial.println("Coop automatization system");

  // Get time from compile and config RTC with it
  if (getDate(__DATE__) && getTime(__TIME__)) {
    RTC.write(time);
  }
  setSyncProvider(RTC.get);

  RTC.set(now());
  RTC.write(time);
  // rtc.adjust(DateTime(YYYY, M, D, h, m, s));
  setSyncProvider(RTC.get);

  sendWifiCall("AT", 5, "OK");
  sendWifiCall("AT+CWMODE=1", 5, "OK");
  sendWifiCall("AT+CWJAP=\"" + WIFI + "\",\"" + PASS + "\"", 20, "OK");
}

void loop() {
  // Get time
  tmElements_t timeNow;
  RTC.read(timeNow); 

  if (timeNow.Day == 1 && timeNow.Month == 1) {
    // Get updated time
  }

  // Trigger once a minute
  if (timeNow.Second == 0) {
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
      
        printValues(timeZone.toLocal(now(), &tcr), humidity, temperature, heatIndex);
        setLights(humidity, temperature, heatIndex);
        dhtTrigger = true;
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

  // Small delay to not constantly trigger everything
  delay(10);
}



//***** Utility functions *****

// Starts number with leading 0 if less than 10
void printTwoDigits(int number) {
  if (number >= 0 && number < 10) {
    Serial.write('0');
  }
  Serial.print(number);
}

// Gets & sets time values from string
bool getTime(const char *str)
{
  int Hour, Min, Sec;

  if (sscanf(str, "%d:%d:%d", &Hour, &Min, &Sec) != 3) return false;
  time.Hour = Hour;
  time.Minute = Min;
  time.Second = Sec;
  return true;
}

// Gets & sets date values from string
bool getDate(const char *str)
{
  char Month[12];
  int Day, Year;
  uint8_t monthIndex;

  if (sscanf(str, "%s %d %d", Month, &Day, &Year) != 3) return false;
  for (monthIndex = 0; monthIndex < 12; monthIndex++) {
    if (strcmp(Month, monthName[monthIndex]) == 0) break;
  }
  if (monthIndex >= 12) return false;
  time.Day = Day;
  time.Month = monthIndex + 1;
  time.Year = CalendarYrToTm(Year);
  return true;
}

// Print all values sent to function to Serial
void printValues(time_t time, float humidity, float temperature, float heatIndex) {
  printDateTime(time, tcr -> abbrev);

  // Print other values
  Serial.print(F("- Humidity: "));
  Serial.print(humidity);
  Serial.print(F("%  Temperature: "));
  Serial.print(temperature);
  Serial.print(F("°C  Heat index: "));
  Serial.print(heatIndex);
  Serial.print(F("°C\n"));
}

// Format and print a time_t value, with a time zone appended.
void printDateTime(time_t t, const char *tz)
{
    char buf[32]; // Buffer to store format in
    sprintf(buf, "%s %.2d/%.2d-%d %.2d:%.2d:%.2d %s", 
        dayShortStr(weekday(t)), day(t), month(t), year(t), hour(t), minute(t), second(t), tz);
    Serial.println(buf);
}

// Sends wifi call - command: Full AT-command, maxTimes: #times to retry call, expectedReply: what the call should return - usually "OK"
void sendWifiCall(String command, int maxTimes, String expectedReply) {
  // Create char array from string to use for wifi.find
  char expectedReplyArray[sizeof(expectedReply)];
  expectedReply.toCharArray(expectedReplyArray, expectedReply.length());

  // Print command being sent
  Serial.print("Command: ");
  Serial.print(command);
  Serial.print(" ");
  while (countTimeCommand <= maxTimes) {
    // AT+CIPSEND
    wifi.println(command);
    
    // OK
    if(wifi.find(expectedReplyArray)) {
      found = true;
      break;
    }
  
    countTimeCommand++;
  }
  
  // Check if found after maxTimes
  if (found) {
    Serial.println("- Success!");
  } else {
    Serial.println("- Fail...");
  }
  
  // Reset check variables
  countTimeCommand = 0;
  found = false;
 }

// Sets colors of lights based on input
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

