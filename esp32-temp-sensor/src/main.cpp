#include <FS.h>
#include <Arduino.h>
#include <WiFiManager.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>  
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Adafruit_AHTX0.h>
#include <secrets.h>
// #include <DHT.h>
// #include <DHT_U.h>

#define RESETPIN 13
// #define DHTPIN 23     // Digital pin connected to the DHT sensor
// #define DHTTYPE    DHT11     // DHT 11
//#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

#define uS_TO_S_FACTOR 1000000
#define TIME_TO_SLEEP 15 * 60
#define JSON_CONFIG_FILE "/test_config.json"

// DHT_Unified dht(DHTPIN, DHTTYPE);
Adafruit_AHTX0 aht;

sensors_event_t humidity, temp;

WiFiManager wifiManager;
WiFiClientSecure client;
HTTPClient https;

char srvHost[50] = SRV_HOST;
char apiKey[50] = "";
char hgSensorTempID[50] = "";
char hgSensorHumID[50] = "";

WiFiManagerParameter custom_apiKey("apiKey", "API Key", "", 50);
WiFiManagerParameter custom_srvHost("srvHost", "API hostname", srvHost, 50);
WiFiManagerParameter custom_hgSensorTempID("hgSensorTempID", "Temperature sensor ID", "", 50);
WiFiManagerParameter custom_hgSensorHumID("hgSensorHumID", "Humidity sensor ID", "", 50);

void readAHTSensorData() {
  aht.getEvent(&humidity, &temp);

  if (isnan(temp.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    Serial.print(temp.temperature);
    Serial.println(F("°C"));
  }

  if (isnan(humidity.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
    Serial.print(humidity.relative_humidity);
    Serial.println(F("%"));
  }
}

// void readDHTSensorData() {
//   dht.temperature().getEvent(&temp);
//   if (isnan(temp.temperature)) {
//     Serial.println(F("Error reading temperature!"));
//   }
//   else {
//     Serial.print(F("Temperature: "));
//     Serial.print(temp.temperature);
//     Serial.println(F("°C"));
//   }
//   dht.humidity().getEvent(&humidity);
//   if (isnan(humidity.relative_humidity)) {
//     Serial.println(F("Error reading humidity!"));
//   }
//   else {
//     Serial.print(F("Humidity: "));
//     Serial.print(humidity.relative_humidity);
//     Serial.println(F("%"));
//   }
// }

void connectWifi() {
  WiFi.mode(WIFI_STA);  // Set the ESP32 to station mode (client)
  bool connection = wifiManager.autoConnect();

  if (!connection) {
    Serial.println("Failed to connect to Wifi");
    esp_deep_sleep_start();
  } else {  
    Serial.println("Connected to WiFi");
  }
}

void disconnectWifi() {
  wifiManager.disconnect();
  WiFi.mode(WIFI_OFF);
}

void sendRequest(String sensorId, String value) {
    https.begin(client, "https://" + String(srvHost) + "/api/sensors/readings");
    https.addHeader("Content-Type", "application/json");
    https.addHeader("x-static-auth", apiKey);

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
  client.setInsecure();

  if (client.connect(srvHost, 443)) {
    if (!isnan(temp.temperature)) {
      sendRequest(hgSensorTempID, String(temp.temperature));
    }
    if (!isnan(humidity.relative_humidity)) {
      sendRequest(hgSensorHumID, String(humidity.relative_humidity));
    }
  }

  client.stop();
  disconnectWifi();
}

bool loadConfigFile() {
  // Uncomment if we need to format filesystem
  // SPIFFS.format();

  Serial.println("Mounting File System...");
 
  if (SPIFFS.begin(false) || SPIFFS.begin(true)) {
    Serial.println("mounted file system");
    if (SPIFFS.exists(JSON_CONFIG_FILE))
    {
      Serial.println("reading config file");
      File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
      
      if (configFile) {
        Serial.println("Opened configuration file");
        JsonDocument json;
        DeserializationError error = deserializeJson(json, configFile);
        serializeJsonPretty(json, Serial);

        if (!error) {
          Serial.println("Parsing JSON");
 
          strcpy(apiKey, json["apiKey"]);
          strcpy(srvHost, json["srvHost"]);
          strcpy(hgSensorTempID, json["hgSensorTempID"]);
          strcpy(hgSensorHumID, json["hgSensorHumID"]);
 
          return true;
        } else {
          Serial.println("Failed to load json config");
        }
      }
    }
  } else {
    Serial.println("Failed to mount FS");
  }
 
  return false;
}

void saveConfigFile() {
  Serial.println(F("Saving configuration..."));
  
  JsonDocument json;
  json["apiKey"] = apiKey;
  json["srvHost"] = srvHost;
  json["hgSensorTempID"] = hgSensorTempID;
  json["hgSensorHumID"] = hgSensorHumID;
 
  File configFile = SPIFFS.open(JSON_CONFIG_FILE, "w");
  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }
 
  serializeJsonPretty(json, Serial);
  if (serializeJson(json, configFile) == 0) {
    Serial.println(F("Failed to write to file"));
  }
  configFile.close();
}

void saveConfigCallback () {
  strcpy(apiKey, custom_apiKey.getValue());
  strcpy(srvHost, custom_srvHost.getValue());
  strcpy(hgSensorTempID, custom_hgSensorTempID.getValue());
  strcpy(hgSensorHumID, custom_hgSensorHumID.getValue());
  saveConfigFile();
}

bool factoryResetPushed() {
  return (digitalRead(RESETPIN) == LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1000); // wait for serial to open
  pinMode(RESETPIN, INPUT_PULLUP);

  if (factoryResetPushed()) {
    Serial.println("Factory reset initiated");
    SPIFFS.format();
    wifiManager.resetSettings();
  }

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  // dht.begin();
  if (!aht.begin()) {
    Serial.println("Could not find AHT. Check wiring");
  }
  sensor_t sensor;
  delay(sensor.min_delay + 1000); // wait for sensor

  wifiManager.setTitle("Sensor configuration portal");
  wifiManager.setClass("invert");
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConnectTimeout(30);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_apiKey);
  wifiManager.addParameter(&custom_srvHost);
  wifiManager.addParameter(&custom_hgSensorTempID);
  wifiManager.addParameter(&custom_hgSensorHumID);

  if (!loadConfigFile()) {
    wifiManager.startConfigPortal();
  }
}

void loop() {
  readAHTSensorData();
  // readDHTSensorData();
  syncData();
  esp_deep_sleep_start();
}