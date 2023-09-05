#include "EEPROM.h"
#include <PubSubClient.h>
#include <SPI.h>
#include <ArduinoJson.h>
#define EEPROM_SIZE 128
#include "DHT.h"

#define DHTPIN 17
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

#include <WiFi.h>
#include <ESP32Ping.h>
const char* remote_host = "www.google.com";
const char* mqtt_server;

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;
bool pairingDone = false;

const int ledPin = 22;
const int modeAddr = 0;
const int wifiAddr = 10;

int modeIdx;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      BLEDevice::startAdvertising();
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic){
    std::string value = pCharacteristic->getValue();

    if(value.length() > 0){
      Serial.print("Value : ");
      Serial.println(value.c_str());
      writeString(wifiAddr, value.c_str());
    }
  }

  void writeString(int add, String data){
    int _size = data.length();
    for(int i=0; i<_size; i++){
      EEPROM.write(add+i, data[i]);
    }
    EEPROM.write(add+_size, '\0');
    EEPROM.commit();
  }
};


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  pinMode(ledPin, OUTPUT);
  dht.begin();

  if(!EEPROM.begin(EEPROM_SIZE)){
    delay(1000);
  }

  modeIdx = EEPROM.read(modeAddr);
  Serial.print("modeIdx : ");
  Serial.println(modeIdx);

  EEPROM.write(modeAddr, modeIdx !=0 ? 0 : 1);
  EEPROM.commit();

  if(modeIdx != 0){
    //BLE MODE
    digitalWrite(ledPin, true);
    Serial.println("BLE MODE");
    bleTask();
  }else{
    //WIFI MODE
    digitalWrite(ledPin, false);
    Serial.println("WIFI MODE");
    wifiTask();
  }

}

void bleTask(){
  // Create the BLE Device
  BLEDevice::init("ESP32 THAT PROJECT");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
}
char server_mqtt[32];
void wifiTask() {
  String receivedData;
  receivedData = read_String(wifiAddr);

  if(receivedData.length() > 0){
    String wifiName = getValue(receivedData, ',', 0);
    String wifiPassword = getValue(receivedData, ',', 1);
    String ipMQTT = getValue(receivedData, ',', 2);
    mqtt_server = ipMQTT.c_str();
    Serial.print("Ip MQTT : ");
    Serial.println(ipMQTT);
    Serial.print("Ip MQTT char format : ");
    Serial.println(mqtt_server);
    ipMQTT.toCharArray(server_mqtt, 32);
    Serial.print("Ip MQTT char new important : ");
    Serial.println(server_mqtt);
    

    if(wifiName.length() > 0 && wifiPassword.length() > 0){
      Serial.print("WifiName : ");
      Serial.println(wifiName);

      Serial.print("wifiPassword : ");
      Serial.println(wifiPassword);

      WiFi.begin(wifiName.c_str(), wifiPassword.c_str());
      Serial.print("Connecting to Wifi");
      while(WiFi.status() != WL_CONNECTED){
        Serial.print(".");
        delay(300);
      }
      Serial.println();
      Serial.print("Connected with IP: ");
      Serial.println(WiFi.localIP());
      

      Serial.print("Ping Host: ");
      Serial.println(remote_host);

      if(Ping.ping(remote_host)){
        Serial.println("Success!!");
        pairingDone = true;
      }else{
        Serial.println("ERROR!!");
      }
      
    }
  }
}

String read_String(int add){
  char data[100];
  int len = 0;
  unsigned char k;
  k = EEPROM.read(add);
  while(k != '\0' && len< 500){
    k = EEPROM.read(add+len);
    data[len] = k;
    len++;
  }
  data[len] = '\0';
  return String(data);
}

String getValue(String data, char separator, int index){
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found <=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
      found++;
      strIndex[0] = strIndex[1]+1;
      strIndex[1] = (i==maxIndex) ? i+1 : i;
    }
  }
  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}
WiFiClient espClient;
PubSubClient client(espClient);
void callback(char* topic, byte* payload, unsigned int length) {
  // handle message arrived
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("mqtt_server :");
    Serial.print(mqtt_server);
    Serial.println("");
    Serial.println("mqtt_server NEW:");
    Serial.print(server_mqtt);
    Serial.println("");
    client.setServer(server_mqtt, 1883);
    client.setCallback(callback);
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void loop() {
  delay(400)
  DynamicJsonDocument doc(1024);
  char payload[1024 + 2];
  // put your main code here, to run repeatedly:
  if(pairingDone){
    
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    String humidity = String(h,3);
    String temperature = String(h,3);
    doc["id"] = 123123123;
    doc["sensorType"] = "temperature";
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    serializeJson(doc,payload);
    if (client.publish("esp32/output", payload) == true) {
      Serial.println("Success sending message");
    } else {
      Serial.println("Error sending message");

    }

  }
}