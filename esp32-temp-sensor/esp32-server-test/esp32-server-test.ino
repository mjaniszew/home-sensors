/*
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleServer.cpp
    Ported to Arduino ESP32 by Evandro Copercini
    updates by chegewara
*/

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define TEMP_SERVICE_UUID "b0a1671a-7d3e-4fb5-8e6f-79fa1aac6f72"
#define HUM_SERVICE_UUID "da77106e-88c9-46c5-a73c-ef2deda9a736"
#define TEMP_CHARACTERISTIC_UUID "3f8a0e0f-fc19-49a9-b653-fbc10db8f806"
#define HUM_CHARACTERISTIC_UUID "f1f07dee-1d98-4b56-a910-9b5f19f1671a"

BLEServer *sensorServer;

BLEService *tempSensorService;
BLECharacteristic *tempSensorCharacteristic;

BLEService *humSensorService;
BLECharacteristic *humSensorCharacteristic;

bool deviceConnected = false;
float temp = 20;
float hum = 40;

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
  temp++;
  hum++;
}

void writeNotify() {
  tempSensorCharacteristic->setValue(temp);
  tempSensorCharacteristic->notify();
  humSensorCharacteristic->setValue(hum);
  humSensorCharacteristic->notify();
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting BLE");

  BLEDevice::init("Sensor: T&H");
  sensorServer = BLEDevice::createServer();
  sensorServer->setCallbacks(new MyServerCallbacks());

  tempSensorService = sensorServer->createService(TEMP_SERVICE_UUID);

  tempSensorCharacteristic = tempSensorService->createCharacteristic(
                                    TEMP_CHARACTERISTIC_UUID,
                                    BLECharacteristic::PROPERTY_READ |
                                    BLECharacteristic::PROPERTY_NOTIFY
                                  );

  tempSensorCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  BLE2902* tempSensorDescriptor = new BLE2902();
  tempSensorDescriptor->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  tempSensorCharacteristic->addDescriptor(tempSensorDescriptor);
  tempSensorCharacteristic->setValue("Init temp val");

  humSensorService = sensorServer->createService(HUM_SERVICE_UUID);
  humSensorCharacteristic = humSensorService->createCharacteristic(
                                    HUM_CHARACTERISTIC_UUID,
                                    BLECharacteristic::PROPERTY_READ |
                                    BLECharacteristic::PROPERTY_NOTIFY
                                  );

  humSensorCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  BLE2902* humSensorDescriptor = new BLE2902();
  humSensorDescriptor->setNotifications(true);
  humSensorDescriptor->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);
  humSensorCharacteristic->addDescriptor(humSensorDescriptor);
  humSensorCharacteristic->setValue("Init hum val");

  tempSensorService->start();
  humSensorService->start();

  BLESecurity *sensorSecurity = new BLESecurity();
  sensorSecurity->setStaticPIN(123456); 

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(TEMP_SERVICE_UUID);
  pAdvertising->addServiceUUID(HUM_SERVICE_UUID);
  pAdvertising->setMinPreferred(0x06);

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