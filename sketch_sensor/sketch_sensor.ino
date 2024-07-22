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


#define LOCATION_LATITUDE "51.935941"
#define LOCATION_LONGITUDE "8.878579"

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


#include "DHT.h"
#define DHTPIN 16
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

void setup() {
    Serial.begin(115200);

    dht.begin();

    // Setup wifi
    WiFi.mode(WIFI_STA);
    wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  
    Serial.print("Connecting to wifi");
    while (wifiMulti.run() != WL_CONNECTED) {
      Serial.print(".");
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
   
  }
void loop() {
    // Clear fields for reusing the point. Tags will remain the same as set above.
    sensor.clearFields();
  
    // Store measured value into point
    sensor.addField("temperature",  dht.readTemperature()-2);
    sensor.addField("humidity",  dht.readHumidity());
  
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
  
    startDeepSleep();
  }

void startDeepSleep(){
  Serial.println("Going to sleep...");
  Serial.flush();
  delay(500);
  esp_sleep_enable_timer_wakeup(60 * 1000000);
  esp_deep_sleep_start();
}