#include <FS.h>
#include <Arduino.h>
#include <WiFiManager.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>  
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <secrets.h>

#define RESETPIN 13
#define FLOOD_DETECT_PIN 12
#define WAKEUP_GPIO GPIO_NUM_12

#define JSON_CONFIG_FILE "/test_config.json"

WiFiManager wifiManager;
WiFiClientSecure client;
HTTPClient https;

uint64_t TIME_TO_SLEEP = uint64_t(180 * 60) * uint64_t(1000000); // 180 * 60 - minutes * seconds
char srvHost[50] = SRV_HOST;
char apiKey[50] = "";
char hgSensorID[50] = "";

WiFiManagerParameter custom_apiKey("apiKey", "API Key", "", 50);
WiFiManagerParameter custom_srvHost("srvHost", "API hostname", srvHost, 50);
WiFiManagerParameter custom_hgSensorID("hgSensorID", "Sensor ID", "", 50);

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

bool floodPinTriggered() {
  return (digitalRead(FLOOD_DETECT_PIN) == LOW);
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
    if (floodPinTriggered()) {
      Serial.println("Flood detected");
      sendRequest(hgSensorID, String("ALERT"));
    } else {
      Serial.println("No flood detected");
      sendRequest(hgSensorID, String("OK"));
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
          strcpy(hgSensorID, json["hgSensorID"]);
 
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
  json["hgSensorID"] = hgSensorID;
 
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
  strcpy(hgSensorID, custom_hgSensorID.getValue());
  saveConfigFile();
}

bool factoryResetPushed() {
  return (digitalRead(RESETPIN) == LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1000); // wait for serial to open
  pinMode(RESETPIN, INPUT_PULLUP);
  pinMode(FLOOD_DETECT_PIN, INPUT_PULLUP);

  if (factoryResetPushed()) {
    Serial.println("Factory reset initiated");
    SPIFFS.format();
    wifiManager.resetSettings();
  }

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP);
  esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 0);

  wifiManager.setTitle("Sensor configuration portal");
  wifiManager.setClass("invert");
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.setConnectTimeout(30);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_apiKey);
  wifiManager.addParameter(&custom_srvHost);
  wifiManager.addParameter(&custom_hgSensorID);

  if (!loadConfigFile()) {
    wifiManager.startConfigPortal();
  }
}

void loop() {
  syncData();
  if (floodPinTriggered()) {
    delay(30 * 1000);
  }
  esp_deep_sleep_start();
}