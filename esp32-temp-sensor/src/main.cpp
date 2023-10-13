/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <BLE2904.h>
#include <Adafruit_AHTX0.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID (uint16_t)0x181A
#define TEMP_CHARACTERISTIC_UUID (uint16_t)0x2A6E
#define HUM_CHARACTERISTIC_UUID (uint16_t)0x2A6F
#define HUM_DESC_UUID "0000290C-0000-1000-8000-00805F9B34FB"

Adafruit_AHTX0 aht;

BLEServer *sensorServer;
BLEService *sensorService;
BLECharacteristic *tempSensorCharacteristic;
BLECharacteristic *humSensorCharacteristic;

bool deviceConnected = false;
sensors_event_t humidity, temp;

//Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    Serial.println("Device connected!");
    deviceConnected = true;
  };
  void onDisconnect(BLEServer* pServer) {
    Serial.println("Device disconnected! Advertising again");
    deviceConnected = false;
    BLEDevice::startAdvertising();
  }
};

void readSensorData() {
  aht.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  Serial.print("Temperature: "); 
  Serial.print(temp.temperature); 
  Serial.println(" degrees C");
  Serial.print("Humidity: "); 
  Serial.print(humidity.relative_humidity); 
  Serial.println("% rH");
}

void writeNotify() {
  tempSensorCharacteristic->setValue(temp.temperature);
  tempSensorCharacteristic->notify();
  humSensorCharacteristic->setValue(humidity.relative_humidity);
  humSensorCharacteristic->notify();
}

void setup() {
  Serial.begin(115200);

  if (! aht.begin()) {
    Serial.println("Could not find AHT? Check wiring");
    while (1) delay(10);
  }
  
  Serial.println("AHT10 or AHT20 found");
  Serial.println("Starting BLE");

  BLEDevice::init("Sensor: T&H");
  sensorServer = BLEDevice::createServer();
  sensorServer->setCallbacks(new MyServerCallbacks());

  sensorService = sensorServer->createService(SERVICE_UUID);

  tempSensorCharacteristic = sensorService->createCharacteristic(
                                    TEMP_CHARACTERISTIC_UUID,
                                    BLECharacteristic::PROPERTY_READ |
                                    BLECharacteristic::PROPERTY_NOTIFY
                                  );

  tempSensorCharacteristic->setValue("Init temp val");

  humSensorCharacteristic = sensorService->createCharacteristic(
                                    HUM_CHARACTERISTIC_UUID,
                                    BLECharacteristic::PROPERTY_READ |
                                    BLECharacteristic::PROPERTY_NOTIFY
                                  );

  humSensorCharacteristic->setValue("Init hum val");
  BLE2902* humSensorDescriptor = new BLE2902();
  humSensorDescriptor->setNotifications(true);
  humSensorCharacteristic->addDescriptor(humSensorDescriptor);

  sensorService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println("Setup done! Advertising...");
}

void loop() {

  if (deviceConnected) {
    readSensorData();
    writeNotify();
  }

  delay(2000);
}