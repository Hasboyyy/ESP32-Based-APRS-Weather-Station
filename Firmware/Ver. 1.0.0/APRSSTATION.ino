//Components Library
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <RtcDS1302.h>
#include "DHT.h"
#include <Adafruit_BMP280.h>
#include <BH1750.h>
#include <Wire.h>

//NTP Library 
#include <NTPClient.h>
#include <WiFiUdp.h>

//APRS Library
#include <APRS-IS.h>
#include <APRS-Decoder.h>
#include <PubSubClient.h>

#include "configuration.h" //Header Import

//Wifi Configuration
const char* ssid = "Hasboyyy";
const char* password = "21062002";

//MQTT Configuration
const char* mqtt_server = "broker.mqtt-dashboard.com";
const char* publish_topic = "sensor/data/hasboyyyWXStation";
const char* subscribe_topic = "hasboyyyWXStation/subscribe/comment";

//Component pins Configuration
LiquidCrystal_I2C lcd(0x27, 20, 4);
DHT dht(DHTPIN, DHTTYPE);
ThreeWire myWire(4, 5, 2); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);
Adafruit_BMP280 bmp;
BH1750 Lumos (0x23);
const int buzzerPin = 14;


//NTP Configuration
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

bool useRTC = true;
bool wifiConnected = false;

//APRS Definition
APRS_IS aprs_is(USER, PASS, TOOL, VERS);

//MQTT Definition
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
char payloadsensor[MSG_BUFFER_SIZE];
int value = 0;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(2, 0);
  lcd.print("APRS Weather");
  lcd.setCursor(4, 1);
  lcd.print("Station");
  delay(3000);
  lcd.clear();

  // sensor initialize
  dht.begin();
  bmp.begin(0x76);
  bmp.setSampling(Adafruit_BMP280::MODE_FORCED,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X1,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X1,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_OFF,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_1000); /* Standby time. */
  Lumos.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  pinMode(buzzerPin, OUTPUT);

  // Try to connect to WiFi
  lcd.setCursor(3, 0);
  lcd.print("Connecting");
  lcd.setCursor(6, 1);
  lcd.print("....");
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
    delay(1000);
    lcd.setCursor(6, 1);
    lcd.print("....");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("WiFi Connected!");
    delay(3000);
  } else {
    lcd.clear();
    lcd.setCursor(1, 0);
    lcd.print("WiFi Failed...");
    delay(3000);
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Using RTC Device");
  lcd.setCursor(6, 1);
  lcd.print("....");
  if (wifiConnected) {
    useRTC = false;
    lcd.setCursor(0, 0);
    lcd.print("Using NTP Time..");
    timeClient.begin();
    timeClient.forceUpdate();
    if (timeClient.isTimeSet()) {
      useRTC = false;
      lcd.setCursor(2, 1);
      lcd.print("NTP Success!");
      delay(3000);
    } else {
    lcd.setCursor(2, 1);
    lcd.print("NTP Failed!");
    delay(3000);
    Rtc.Begin();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Using RTC Device");
    delay(3000);
    if (!Rtc.IsDateTimeValid()) {
      Serial.println("RTC lost confidence in the DateTime!");
      lcd.setCursor(1, 0);
      lcd.print("RTC Failed....");
      delay(3000);
    } else{
      lcd.setCursor(2, 1);
      lcd.print("RTC Success!");
      delay(3000);
    }
    }
  } else {
    lcd.setCursor(2, 1);
    lcd.print("NTP Failed!");
    delay(3000);
    Rtc.Begin();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Using RTC Device");
    delay(3000);
    if (!Rtc.IsDateTimeValid()) {
      Serial.println("RTC lost confidence in the DateTime!");
      lcd.setCursor(1, 0);
      lcd.print("RTC Failed....");
      delay(3000);
    } else{
      lcd.setCursor(2, 1);
      lcd.print("RTC Success!");
      delay(3000);
    }
  }

  //MQTT 
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

}

// initial value 
unsigned long lastAPRSTime = 0;
unsigned long lastDisplayTime = 0;

// loop interval
const unsigned long aprsInterval = 5*60000; 
const unsigned long displayInterval = 10000; 

void loop() {
  reconnectWiFi();
  unsigned long currentMillis = millis();

  // upload APRS package and MQTT every 10 minutes
  if (currentMillis - lastAPRSTime >= aprsInterval) {
    lastAPRSTime = currentMillis;
    aprsinitiate();
    mqttinitiate();
  }

  // Display change every 10 sec
  if (currentMillis - lastDisplayTime >= displayInterval) {
    lastDisplayTime = currentMillis;
    displaysensor();
  }
}

// getting time from NTP function
void getTimeFromNTP() {
  timeClient.begin();
  int maxAttempts = 2;
  int attemptCount = 0;
  while (!timeClient.update() && attemptCount < maxAttempts) {
    timeClient.forceUpdate();
    attemptCount++;
    delay(500); 
  }
  if (attemptCount >= maxAttempts) {
    Rtc.GetDateTime();
    timeClient.end();
    return;
  }
  unsigned long epochTime = timeClient.getEpochTime();
  RtcDateTime ntpTime = convertEpochToRtc(epochTime);

  Rtc.SetDateTime(ntpTime);
  Serial.println("RTC updated with NTP time.");

  timeClient.end();
}

//Epoch Conversion
RtcDateTime convertEpochToRtc(unsigned long epochTime) {
  uint16_t year = 1970;
  uint8_t month = 1, day = 1, hour = 0, minute = 0, second = 0;

  second = epochTime % 60;
  epochTime /= 60;
  minute = epochTime % 60;
  epochTime /= 60;
  hour = epochTime % 24;
  epochTime /= 24;

  while (epochTime >= 365 + isLeapYear(year)) {
    epochTime -= 365 + isLeapYear(year);
    year++;
  }

  month = 1;
  while (epochTime >= daysInMonth(month, year)) {
    epochTime -= daysInMonth(month, year);
    month++;
  }

  day += epochTime;

  return RtcDateTime(year, month, day, hour, minute, second);
}

bool isLeapYear(uint16_t year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t daysInMonth(uint8_t month, uint16_t year) {
  const uint8_t daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  return daysInMonth[month - 1];
}

//Sensor values and time display function
void displaysensor() {
  int sensordisplay[4];
  sensordisplay[0] = dht.readTemperature();
  sensordisplay[1] = dht.readHumidity();
  if (isnan(sensordisplay[0]) || isnan(sensordisplay[1])) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  if (bmp.takeForcedMeasurement()) {
    sensordisplay[2] = bmp.readPressure() / 10;
  } else {
    Serial.println("Forced BMP measurement failed!");
  }
  sensordisplay[3] = Lumos.readLightLevel()/126.7;

  char t[4];
  sprintf(t, "%03d", sensordisplay[0]);

  char h[4];
  sprintf(h, "%03d", sensordisplay[1]); 

  char p[6];
  sprintf(p, "%05d", sensordisplay[2]);  

  char l[4];
  sprintf(l, "%03d", sensordisplay[3]);  

  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("H: ");
  lcd.print(h);
  lcd.print("% T: ");
  lcd.print(t);
  lcd.print("\xDF""C");

  static bool showTime = true;
  RtcDateTime currentTime;

  // Prioritize NTP if WiFi is connected
  if (wifiConnected) {
    useRTC = false;
    currentTime = getTimeFromNTPDateTime();
  } else {
    // Use RTC if WiFi is not available
    if (useRTC) {
      currentTime = Rtc.GetDateTime();
    } else {
      currentTime = getTimeFromNTPDateTime();  // Fallback to NTP if RTC is not available
    }
  }

  lcd.setCursor(0, 1);

  if (showTime) {
    lcd.print("  P: ");
    lcd.print(p);
    lcd.print("mbar");
  } else {
    lcd.print("  L: ");
    lcd.print(l);
    lcd.print("W/m""\x5E""2");
  }

  showTime = !showTime;
}


//Getting time from NTP function
RtcDateTime getTimeFromNTPDateTime() {
  unsigned long epochTime = timeClient.getEpochTime();
  return convertEpochToRtc(epochTime);
}

String formatDateTime(const RtcDateTime& dt) {
  char buffer[20];
  sprintf(buffer, "%04u-%02u-%02u %02u:%02u:%02u", 
          dt.Year(), dt.Month(), dt.Day(), 
          dt.Hour(), dt.Minute(), dt.Second());
  return String(buffer);
}

// tone function
void playSuccessTone() {
  tone(buzzerPin, NOTE_C4, 200);
  delay(250);
  tone(buzzerPin, NOTE_E4, 200);
  delay(250);
  tone(buzzerPin, NOTE_G4, 300);
  delay(350);
  noTone(buzzerPin);
}

// APRS initialize function
void aprsinitiate() {
  if (!aprs_is.connected()) {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("Connecting");
    lcd.setCursor(1, 1);
    lcd.print("to APRS server");

    if (!aprs_is.connect(SERVER, PORT, FILTER)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Connection fail");
      lcd.setCursor(1, 1);
      lcd.print("Re-Connecting");
      return;
    }
    delay(5000);
  }

  // sensor values read
  int sensoraprs[5];
  sensoraprs[0] = dht.readTemperature(true);
  sensoraprs[1] = dht.readHumidity();
  if (bmp.takeForcedMeasurement()) {
    sensoraprs[2] = bmp.readPressure() / 10;
  } else {
    Serial.println("Forced BMP measurement failed!");
  }
  sensoraprs[3] = Lumos.readLightLevel() / 126.7;
  sensoraprs[4] = dht.readTemperature();

  // formatting parameter for CWOP APRS format
  char temp[4];
  sprintf(temp, "%03d", sensoraprs[0]);
  String aprstemp = "t" + String(temp);

  char hum[4];
  sprintf(hum, "%03d", sensoraprs[1]);
  String aprshum = "h" + String(hum);

  char press[6];
  sprintf(press, "%05d", sensoraprs[2]);
  String aprspress = "b" + String(press);

  char lux[4];
  sprintf(lux, "%03d", sensoraprs[3]);
  String aprslumi = "L" + String(lux);

  // error condition logic
  if (isnan(sensoraprs[0]) || isnan(sensoraprs[1])) {
    aprstemp = "t...";
    aprshum = "h..";
    Serial.println("Failed to read from DHT11 sensor!");
  }

  if (isnan(sensoraprs[2])) {
    aprspress = "b.....";
    Serial.println("Failed to read from BMP280 sensor!");
  }

  if (isnan(sensoraprs[3])) {
    aprslumi = "L...";
    Serial.println("Failed to read from BH1750 sensor!");
  }

  // aprs message builder
  String message = String(USER) + ">APRS,TCPIP*:=" + String(GPS) + 
                   String(aprswinddir) + String(aprswindspeed) + String(aprswindgust) +
                   aprstemp + String(aprsrainh) + String(aprsraind) + String(aprsrainm) + 
                   aprshum + aprspress + aprslumi + String(COMMENT) +
                   "T: " + String(sensoraprs[4]) + "°C" +
                   " RH: " + String(sensoraprs[1]) + "%" +
                   " P: " + String(sensoraprs[2]) + "mbar" +
                   " G : " + String(sensoraprs[3]) + "W/m^2";

  // send the message
  aprs_is.sendMessage(message);

  RtcDateTime currentTime;
  if (wifiConnected) {
    useRTC = false;
    currentTime = getTimeFromNTPDateTime();
  } else if (useRTC) {
    currentTime = Rtc.GetDateTime();
  } else {
    currentTime = getTimeFromNTPDateTime();
  }

  // LCD notification
  Serial.print("Package Uploaded on " + formatDateTime(currentTime) + " : ");
  Serial.println(message);
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Package Uploaded!");
  
  // tone notification
  playSuccessTone();

  // APRS message checker and decoder
  String msg_ = aprs_is.getMessage();
  if (msg_.startsWith("#")) {
    Serial.println(msg_);
  } else {
    APRSMessage msg;
    msg.decode(msg_);
    Serial.println(msg.toString());
  }
}

//Callback for MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
}

//MQTT initialize function
void mqttinitiate() {
  int maxAttempts = 2;
  int attemptCount = 0;

  while (!client.connected() && attemptCount < maxAttempts) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "ESP32WXStationYD1BDE";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      attemptCount++;
    }
  }

  if (!client.connected()) {
    Serial.println("Failed to connect to MQTT server after maximum attempts.");
    return;
  }

  float sensormqtt[5];
  sensormqtt[4] = dht.readTemperature();
  sensormqtt[0] = dht.readTemperature(true);
  sensormqtt[1] = dht.readHumidity();
  if (bmp.takeForcedMeasurement()) {
    sensormqtt[2] = bmp.readPressure() / 10;
  } else {
    Serial.println("Forced BMP measurement failed!");
  }
  sensormqtt[3] = Lumos.readLightLevel() / 126.7;

  RtcDateTime currentTime;
  unsigned long epochTime;
  char formattedTime[20];  // Buffer for String time

  if (wifiConnected) {
    useRTC = false;
    currentTime = getTimeFromNTPDateTime();
  } else {
    if (useRTC) {
      currentTime = Rtc.GetDateTime();
    } else {
      currentTime = getTimeFromNTPDateTime();
    }
  }

  // epoch time conversion
  epochTime = currentTime.Epoch32Time();

  // string format for time "YYYY-MM-DD HH:MM:SS"
  snprintf(formattedTime, sizeof(formattedTime), "%04u-%02u-%02u %02u:%02u:%02u",
           currentTime.Year(), currentTime.Month(), currentTime.Day(),
           currentTime.Hour(), currentTime.Minute(), currentTime.Second());

  // message package builder
  snprintf(payloadsensor, sizeof(payloadsensor),
         "[%lu, \"%s\", \"%s\", %.2f, %.2f, %.2f, %.2f]",
         epochTime, formattedTime, GPS, sensormqtt[4], sensormqtt[1], sensormqtt[2], sensormqtt[3]);

  // send the package
  if (client.publish(publish_topic, payloadsensor)) {
    Serial.print("Data sent to MQTT: ");
    Serial.println(payloadsensor);
    lcd.setCursor(0,1);
    lcd.print("MQTT Published!");
    digitalWrite(buzzerPin, HIGH);
    delay(300);
    digitalWrite(buzzerPin, LOW);
    delay(2000);
  } else {
    Serial.println("Failed to publish data to MQTT.");
  }
}

// wifi check function
void reconnectWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected! Attempting to reconnect...");
    WiFi.disconnect();  // Disconnect in case of previous failed connection
    WiFi.begin(ssid, password);  // Reconnect to WiFi

    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {  // Retry for 10 seconds
      delay(500);
      Serial.print(".");
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nWiFi reconnected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("\nFailed to reconnect to WiFi.");
    }
  }
}








