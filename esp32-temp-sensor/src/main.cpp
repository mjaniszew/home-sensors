#include <secrets.h>
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Adafruit_AHTX0.h>
#include <DHT.h>
#include <DHT_U.h>

#define DHTPIN 10     // Digital pin connected to the DHT sensor
#define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

DHT_Unified dht(DHTPIN, DHTTYPE);
// Adafruit_AHTX0 aht;

sensors_event_t humidity, temp;

WiFiClientSecure client;
HTTPClient https;

const char* wifiSSID = WIFI_SSID;
const char* wifiPWD = WIFI_PASS;

String srvHost = SRV_HOST; 
String apiEndpoint = SRV_API_ENDPOINT; 
String apiKey = SRV_API_KEY;
String hgSensorTempID = HG_SENSOR_TEMP_ID;
String hgSensorHumID = HG_SENSOR_HUM_ID;

uint32_t loopDelay = 60 * 1000;

// void readAHTSensorData() {
//   aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
//   Serial.print("Temperature: "); 
//   Serial.print(temp.temperature); 
//   Serial.println(" degrees C");
//   Serial.print("Humidity: "); 
//   Serial.print(humidity.relative_humidity); 
//   Serial.println("% rH");
// }

void readDHTSensorData() {
  dht.temperature().getEvent(&temp);
  if (isnan(temp.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    Serial.print(temp.temperature);
    Serial.println(F("°C"));
  }

  dht.humidity().getEvent(&humidity);
  if (isnan(humidity.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
    Serial.print(humidity.relative_humidity);
    Serial.println(F("%"));
  }
}

void connectWifi() {

  WiFi.mode(WIFI_STA);  // Set the ESP32 to station mode (client)
  WiFi.begin(wifiSSID, wifiPWD);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }

  Serial.println("Connected to WiFi");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void sendRequest(String sensorId, String value) {
    https.begin(client, apiEndpoint.c_str());
    https.addHeader("Content-Type", "application/json");
    https.addHeader("x-static-auth", apiKey.c_str());

    String jsonData = "{\"sensorId\":\"" + sensorId + "\",\"value\":\"" + value + "\"}";

    int httpResponseCode = https.POST(jsonData);
    
    if(httpResponseCode > 0) {
      String response = https.getString();
      
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      
      Serial.print("Server response: ");
      Serial.println(response); 
      
    } else {
      Serial.println("Error in sending HTTP GET request!");
    }

    https.end();
}

void syncData() {
  connectWifi();

  if (client.connect(srvHost.c_str(), 443)) {
    if (!isnan(temp.temperature)) {
      sendRequest(hgSensorTempID, String(temp.temperature));
    }
    if (!isnan(humidity.relative_humidity)) {
      sendRequest(hgSensorHumID, String(humidity.relative_humidity));
    }
  }

  client.stop();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();
  sensor_t sensor;

  // if (! aht.begin()) {
  //   Serial.println("Could not find AHT? Check wiring");
  //   while (1) delay(10);
  // }

  client.setInsecure();
}

void loop() {
  readDHTSensorData();
  syncData();

  delay(loopDelay);
}