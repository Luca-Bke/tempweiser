#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>

// WiFi AP SSID
#define WIFI_SSID "Tempweiser"
// WiFi password
#define WIFI_PASSWORD "tempweiser"

#define INFLUXDB_URL "https://influx.bke-net.duckdns.org"
#define INFLUXDB_TOKEN "jJVJ6i_GZtr9ffuXI2-RH25GIXuQ1Sg-9zituf3uzov0hPFzkGnfqukpaiWVJ1OX_bx9UvMsCflN6eCcpZCY5A=="
#define INFLUXDB_ORG "fdf5a7f5e4749563"
#define INFLUXDB_BUCKET "tempwiser"

// Time zone info
#define TZ_INFO "UTC2"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor("local_climate");

// initialize the temperature sensor library
#include "DHT.h"
#define DHTPIN 16
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#include <Arduino.h>
#include <TM1637Display.h>

// Module connection pins (Digital Pins)
#define DISPLAY_CLK 22
#define DISPLAY_DIO 21
TM1637Display display(DISPLAY_CLK, DISPLAY_DIO);

// Button Config
#define BUTTON_COOLER_PIN 5
#define BUTTON_WARMER_PIN 18


int coolerButtonState = 0;
int warmerButtonState = 0;
int coolerButtonStateOld = 0;
int warmerButtonStateOld = 0;

// vars for timing in loop
unsigned long lastTempSend = 0;
unsigned long tempsendDelay = 60 * 1000;

unsigned long lastButtonTrigger = 0;
unsigned long debounceDelay = 50;

String LOCATION_LATITUDE = "51.936450";
String LOCATION_LONGITUDE = "8.865729"; 


#include <math.h>
// Radius der Erde in Metern
#define EARTH_RADIUS 6371000.0

// work vars
int distance = 0;
int bearing = 0;



#include <ESP32Servo.h>
#define SERVO_PIN 33
#define SERVO_ZERO_POS_PIN 4

Servo arrowServo;



#include <FastLED.h>
#define NUM_LEDS 106
#define LED_DATA_PIN 17
CRGB leds[NUM_LEDS];


void setup() {
  FastLED.addLeds<WS2812B, LED_DATA_PIN>(leds, NUM_LEDS);

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Red;
  }
  mirrorLEDState(leds);
  FastLED.setBrightness(40);
  FastLED.show();
  // start serial connection
  Serial.begin(115200);

  // init buttons
  pinMode(BUTTON_COOLER_PIN, INPUT_PULLUP);
  pinMode(BUTTON_WARMER_PIN, INPUT_PULLUP);
  pinMode(SERVO_ZERO_POS_PIN, INPUT_PULLUP);
  coolerButtonState = digitalRead(BUTTON_COOLER_PIN);
  warmerButtonState = digitalRead(BUTTON_WARMER_PIN);
  coolerButtonStateOld = coolerButtonState;
  warmerButtonStateOld = warmerButtonState;

  // start temperature sensor
  dht.begin();

  // show 0000 on display
  display.setBrightness(0x0f);
  display.showNumberDec(0, true);

  // Setup wifi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  int ledCnt = 0;
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    leds[ledCnt] = CRGB::Red;
    leds[ledCnt + 1] = CRGB::Blue;
    mirrorLEDState(leds);
    FastLED.setBrightness(40);
    FastLED.show();
    ledCnt++;
    if (ledCnt >= NUM_LEDS - 2) {
      ledCnt = 0;
    }
    delay(100);
  }
  Serial.println();
  // Accurate time is necessary for certificate validation and writing in batches
  // We use the NTP servers in your area as provided by: https://www.pool.ntp.org/zone/
  // Syncing progress and the time will be printed to Serial.
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");


  sensor.addTag("device", DEVICE);
  sensor.addTag("mac", WiFi.macAddress());
  sensor.addTag("SSID", WiFi.SSID());
  sensor.addTag("longitude", LOCATION_LONGITUDE);
  sensor.addTag("latitude", LOCATION_LATITUDE);
  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }



  // Allow allocation of all timers
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  arrowServo.setPeriodHertz(50);             // standard 50 hz servo
  arrowServo.attach(SERVO_PIN, 1000, 2000);  // attaches the servo on pin 18 to the servo object
  arrowServo.write(90);
  arrowServo.write(45);
  delay(500);
  arrowServo.write(90);
  delay(100);
  arrowServo.write(135);
  delay(500);
  arrowServo.write(90);


  display.setBrightness(7, false);
  display.showNumberDec(0, true);

  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  mirrorLEDState(leds);
  FastLED.show();
  lastTempSend = millis();
  lastTempSend -= tempsendDelay;
}
void loop() {
  unsigned long currentMillis = millis();

  if (lastTempSend + tempsendDelay < currentMillis) {
    // Clear fields for reusing the point. Tags will remain the same as set above.
    sensor.clearFields();

    // Store measured value into point
    float current_temperature = dht.readTemperature();

    int ledsToLight = map(current_temperature, -10, 40, 0, 52);

    for (int i = 0; i < 52; i++) {
      if (i <= ledsToLight) {
        if (i < 10) {
          leds[i] = CRGB::Blue;
        } else {
          leds[i] = CRGB::Red;
        }
      } else {
        leds[i] = CRGB::Black;
      }
    }
    mirrorLEDState(leds);
    FastLED.show();


    sensor.addField("temperature", current_temperature);
    sensor.addField("humidity", dht.readHumidity());

    // Print what are we exactly writing
    Serial.print("Writing: ");
    Serial.println(sensor.toLineProtocol());

    // Check WiFi connection and reconnect if needed
    if (wifiMulti.run() != WL_CONNECTED) {
      Serial.println("Wifi connection lost");
    }

    // Write point
    if (!client.writePoint(sensor)) {
      Serial.print("InfluxDB write failed: ");
      Serial.println(client.getLastErrorMessage());
    }
    lastTempSend = currentMillis;
  }


  coolerButtonState = digitalRead(BUTTON_COOLER_PIN);
  warmerButtonState = digitalRead(BUTTON_WARMER_PIN);
  if (coolerButtonState != coolerButtonStateOld && currentMillis > lastButtonTrigger + debounceDelay) {
    coolerButtonStateOld = coolerButtonState;
    lastButtonTrigger = currentMillis;
    if (coolerButtonState == LOW) {
      Serial.println("Cooler Wished");
      display.setBrightness(0x0f);

      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
      }
      mirrorLEDState(leds);
      FastLED.show();

      String query = "import \"experimental\" import \"date\" from(bucket: \"tempwiser\")  |> range(start: date.sub(d: 1h, from: now()), stop: now())  |> sort(columns: [\"_time\"], desc: true)  |> limit(n:1, offset: 0)  |> filter(fn: (r) => r[\"_field\"] == \"temperature\")  |> filter(fn: (r) => r[\"mac\"] != \"E4:65:B8:0C:18:D4\")  |> group()  |> experimental.min()";

      FluxQueryResult result = client.query(query);
      while (result.next()) {
        String longitudeString = result.getValueByName("longitude").getString();
        float longitude = longitudeString.toFloat();
        Serial.print("Longitude: ");
        Serial.print(longitude, 5);
        Serial.print(" ");

        String latitudeString = result.getValueByName("latitude").getString();
        float latitude = latitudeString.toFloat();
        Serial.print("Latitude: ");
        Serial.print(latitude, 5);
        Serial.println();


        distance = (int)calculateDistance(latitude, longitude, (double)LOCATION_LATITUDE.toFloat(), (double)LOCATION_LONGITUDE.toFloat());
        bearing = (int)calculateBearing((double)LOCATION_LATITUDE.toFloat(), (double)LOCATION_LONGITUDE.toFloat(), latitude, longitude);


        Serial.print("Distanz: ");
        Serial.print(distance);
        Serial.print("m");
        Serial.print(" Grad: ");
        Serial.print(bearing);
        Serial.print("°");
        Serial.println();

        display.showNumberDec(distance, true);
        int msToRotate = map(bearing, 0, 360, 0, 1800);
        Serial.print("MS to Rotate: ");
        Serial.println(msToRotate);
        arrowServo.write(90);
        arrowServo.write(45);
        delay(msToRotate);
        arrowServo.write(90);

        for (int x = 0; x <= 10; x++) {
          for (int i = 0; i <= 52; i++) {
            for (int m = 0; m < NUM_LEDS; m++) {
              leds[m] = CRGB::Black;
            }
            for (int w = 0; w <= 5; w++) {
              if (i + w >= 0 & i + w <= 52) {
                leds[i + w] = CRGB::Blue;
              }
            }
            mirrorLEDState(leds);
            FastLED.show();
            delay(20);
          }
        }
        for (int m = 0; m < NUM_LEDS; m++) {
          leds[m] = CRGB::Black;
        }
        FastLED.show();

        arrowServo.write(135);
        delay(msToRotate);
        // while(digitalRead(SERVO_ZERO_POS_PIN) == HIGH){
        //   int spare = 0;
        // }
        arrowServo.write(90);

        display.showNumberDec(0, true);
        display.setBrightness(7, false);
        display.showNumberDec(0, true);
        delay(100);
      }
      // Check if there was an error
      if (result.getError() != "") {
        Serial.print("Query result error: ");
        Serial.println(result.getError());
      }
      result.close();
          
      lastTempSend = millis();
      lastTempSend -= tempsendDelay;
    }
  }

  if (warmerButtonState != warmerButtonStateOld && currentMillis > lastButtonTrigger + debounceDelay) {
    warmerButtonStateOld = warmerButtonState;
    lastButtonTrigger = currentMillis;

    if (warmerButtonState == LOW) {
      Serial.println("Warmer Wished");
      display.setBrightness(0x0f);

      for (int i = 0; i < NUM_LEDS; i++) {
        leds[i] = CRGB::Black;
      }
      mirrorLEDState(leds);
      FastLED.show();
      
      String query = "import \"experimental\" import \"date\" from(bucket: \"tempwiser\")  |> range(start: date.sub(d: 1h, from: now()), stop: now())  |> sort(columns: [\"_time\"], desc: true)  |> limit(n:1, offset: 0)  |> filter(fn: (r) => r[\"_field\"] == \"temperature\")  |> filter(fn: (r) => r[\"mac\"] != \"E4:65:B8:0C:18:D4\")  |> group()  |> experimental.max()";

      FluxQueryResult result = client.query(query);
      while (result.next()) {
        String longitudeString = result.getValueByName("longitude").getString();
        float longitude = longitudeString.toFloat();
        Serial.print("Longitude: ");
        Serial.print(longitude, 5);
        Serial.print(" ");

        String latitudeString = result.getValueByName("latitude").getString();
        float latitude = latitudeString.toFloat();
        Serial.print("Latitude: ");
        Serial.print(latitude, 5);
        Serial.println();


        distance = (int)calculateDistance(latitude, longitude, (double)LOCATION_LATITUDE.toFloat(), (double)LOCATION_LONGITUDE.toFloat());
        bearing = (int)calculateBearing((double)LOCATION_LATITUDE.toFloat(), (double)LOCATION_LONGITUDE.toFloat(), latitude, longitude);


        Serial.print("Distanz: ");
        Serial.print(distance);
        Serial.print("m");
        Serial.print(" Grad: ");
        Serial.print(bearing);
        Serial.print("°");
        Serial.println();

        display.showNumberDec(distance, true);
        int msToRotate = map(bearing, 0, 360, 0, 1800);
        Serial.print("MS to Rotate: ");
        Serial.println(msToRotate);
        arrowServo.write(90);
        arrowServo.write(45);
        delay(msToRotate);
        arrowServo.write(90);

        for (int x = 0; x <= 10; x++) {
          for (int i = 0; i <= 52; i++) {
            for (int m = 0; m < NUM_LEDS; m++) {
              leds[m] = CRGB::Black;
            }
            for (int w = 0; w <= 5; w++) {
              if (i + w >= 0 & i + w <= 52) {
                leds[i + w] = CRGB::Red;
              }
            }
            mirrorLEDState(leds);
            FastLED.show();
            delay(20);
          }
        }
        for (int m = 0; m < NUM_LEDS; m++) {
          leds[m] = CRGB::Black;
        }
        FastLED.show();

        arrowServo.write(135);
        delay(msToRotate);
        // while(digitalRead(SERVO_ZERO_POS_PIN) == HIGH){
        //   int spare = 0;
        // }
        arrowServo.write(90);

        display.showNumberDec(0, true);
        display.setBrightness(7, false);
        display.showNumberDec(0, true);
        delay(100);
      }
      // Check if there was an error
      if (result.getError() != "") {
        Serial.print("Query result error: ");
        Serial.println(result.getError());
      }
      result.close();

      lastTempSend = millis();
      lastTempSend -= tempsendDelay;
    }
  }
}



// Funktion zur Konvertierung von Grad in Radian
double toRadians(double degree) {
  return degree * (M_PI / 180.0);
}

// Funktion zur Berechnung der Entfernung zwischen zwei Punkten anhand der Haversine-Formel
double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
  // Konvertiere Breitengrade und Längengrade von Grad in Radian
  double lat1Rad = toRadians(lat1);
  double lon1Rad = toRadians(lon1);
  double lat2Rad = toRadians(lat2);
  double lon2Rad = toRadians(lon2);

  // Unterschied der Breitengrade und Längengrade
  double deltaLat = lat2Rad - lat1Rad;
  double deltaLon = lon2Rad - lon1Rad;

  // Haversine-Formel
  double a = sin(deltaLat / 2) * sin(deltaLat / 2) + cos(lat1Rad) * cos(lat2Rad) * sin(deltaLon / 2) * sin(deltaLon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));

  // Entfernung in Metern
  double distance = EARTH_RADIUS * c;

  return distance;
}

// Funktion zur Konvertierung von Radian in Grad
double toDegrees(double radian) {
  return radian * (180.0 / M_PI);
}

// Funktion zur Berechnung des Winkels zwischen zwei Punkten
double calculateBearing(double lat1, double lon1, double lat2, double lon2) {
  // Konvertiere Breitengrade und Längengrade von Grad in Radian
  double lat1Rad = toRadians(lat1);
  double lon1Rad = toRadians(lon1);
  double lat2Rad = toRadians(lat2);
  double lon2Rad = toRadians(lon2);

  // Unterschied der Längengrade
  double deltaLon = lon2Rad - lon1Rad;

  // Berechne den Winkel
  double y = sin(deltaLon) * cos(lat2Rad);
  double x = cos(lat1Rad) * sin(lat2Rad) - sin(lat1Rad) * cos(lat2Rad) * cos(deltaLon);
  double bearingRad = atan2(y, x);

  // Konvertiere den Winkel von Radian in Grad
  double bearingDeg = toDegrees(bearingRad);

  // Normalisiere den Winkel, um sicherzustellen, dass er zwischen 0 und 360 Grad liegt
  bearingDeg = fmod((bearingDeg + 360), 360);

  return bearingDeg;
}

void mirrorLEDState(CRGB leds[NUM_LEDS]) {
  for (int i = 0; i <= 52; i++) {
    leds[NUM_LEDS - (i + 1)] = leds[i];
  }
}