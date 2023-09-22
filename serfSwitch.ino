#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <EEPROM.h>
#include <MQTT.h>
#include <WiFiClientSecure.h>

#include "helpers.h"
#include "rgb.h"
#include "servo.h"

// Define switch configuration struct
struct SwitchConfig {
  String SWITCH_ID;
  String BLUETOOTH_UUID;
  String CONFIG_CHARACTERISTIC_UUID;
  String WIFI_SSID;
  String WIFI_PASSWORD;
  String MQTT_SERVER;
  String MQTT_USERNAME;
  String MQTT_PASSWORD;
  String CONFIG_CODE;
};

// Variable inits
const int TOGGLE_BUTTON_PIN = D5;
const int BUTTON_DEBOUNCE_DELAY = 500;
WiFiClientSecure NET;
MQTTClient MQTT(1024);
unsigned long LAST_LOOP_TIME = 0;
unsigned long LAST_BUTTON_PUSH_TIME = 0;
const int SWITCH_ID_LENGTH = 5;
bool NEW_ID_WAS_GENERATED = false;
SwitchConfig switchConfig;

// Other variables initialized during setup
String SWITCH_NAME;
String TOPIC_BASE;
String CONFIG_TOPIC;
String STATE_TOPIC;
String COMMAND_TOPIC;
String INTRODUCTION_PAYLOAD;

// Switch config helper funcs
String serializeSwitchConfigEditable(const SwitchConfig &config) {
  String serialized = "";
  serialized += config.WIFI_SSID + "|";
  serialized += config.WIFI_PASSWORD + "|";
  serialized += config.MQTT_SERVER + "|";
  serialized += config.MQTT_USERNAME + "|";
  serialized += config.MQTT_PASSWORD;
  serialized += config.CONFIG_CODE + "|";
  return serialized;
}
bool deserializeSwitchConfigEditable(const String &serialized,
                                     SwitchConfig &config) {
  int count =
      sscanf(serialized.c_str(), "%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%s",
             config.WIFI_SSID, config.WIFI_PASSWORD, config.MQTT_SERVER,
             config.MQTT_USERNAME, config.MQTT_PASSWORD, config.CONFIG_CODE);
  return count == 5;
}
bool isSwitchConfigValid() {
  return !switchConfig.SWITCH_ID.isEmpty() &&
         !switchConfig.MQTT_SERVER.isEmpty() &&
         !switchConfig.MQTT_USERNAME.isEmpty() &&
         !switchConfig.MQTT_PASSWORD.isEmpty() &&
         !switchConfig.WIFI_SSID.isEmpty() &&
         !switchConfig.WIFI_PASSWORD.isEmpty();
}

// BLE callback for configuration - save new config if valid and restart
class BluetoothLECallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = String((pCharacteristic->getValue()).c_str());
    SwitchConfig newConfig;
    newConfig.SWITCH_ID = switchConfig.SWITCH_ID;
    newConfig.BLUETOOTH_UUID = switchConfig.BLUETOOTH_UUID;
    newConfig.CONFIG_CHARACTERISTIC_UUID =
        switchConfig.CONFIG_CHARACTERISTIC_UUID;
    newConfig.CONFIG_CODE = switchConfig.CONFIG_CODE;
    bool isValid = deserializeSwitchConfigEditable(value, newConfig);
    if (isValid) {
      EEPROM.put(0, newConfig);
      EEPROM.commit();
    }
    ESP.restart();
  }
};

// Prepare hardware and connect to MQTT
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < SERVO_PIN_LENGTH; i++) {
    pinMode(SERVO_PINS[i], OUTPUT);
  }
  pinMode(INDICATOR_PIN, OUTPUT);
  pinMode(RGB_LED_RED, OUTPUT);
  pinMode(RGB_LED_GREEN, OUTPUT);
  pinMode(RGB_LED_BLUE, OUTPUT);
  pinMode(TOGGLE_BUTTON_PIN, INPUT_PULLUP);
  EEPROM.begin(512);

  delay(5000); // For debugging - give me time to switch to serial monitor

  // Generate a new random switch ID or use a saved one (in EEPROM)
  EEPROM.get(0, switchConfig);
  switchConfig.CONFIG_CODE = generateRandomNumString(6);
  Serial.println("Config from flash: " +
                 serializeSwitchConfigEditable(switchConfig));
  if (switchConfig.SWITCH_ID.length() != SWITCH_ID_LENGTH) {
    switchConfig.SWITCH_ID = generateRandomString(SWITCH_ID_LENGTH);
    switchConfig.BLUETOOTH_UUID = generateUUID();
    switchConfig.CONFIG_CHARACTERISTIC_UUID = generateUUID();
    Serial.println(serializeSwitchConfigEditable(switchConfig));
    EEPROM.put(0, switchConfig);
    EEPROM.commit();
    NEW_ID_WAS_GENERATED = true;
  }
  Serial.println("Switch ID: " + switchConfig.SWITCH_ID +
                 (NEW_ID_WAS_GENERATED ? " (newly generated)" : ""));

  // Initialize all remaining variables using the switch ID
  SWITCH_NAME = "switch-" + switchConfig.SWITCH_ID;
  TOPIC_BASE = "homeassistant/switch/" + switchConfig.SWITCH_ID;
  CONFIG_TOPIC = TOPIC_BASE + "/config";
  STATE_TOPIC = TOPIC_BASE + "/state";
  COMMAND_TOPIC = TOPIC_BASE + "/command";
  INTRODUCTION_PAYLOAD = "{ \
    \"name\": \"Switch " +
                         switchConfig.SWITCH_ID + "\", \
    \"unique_id\": \"" + switchConfig.SWITCH_ID +
                         "\", \
    \"device_class\": \"switch\", \
    \"state_topic\": \"" +
                         STATE_TOPIC + "\", \
    \"command_topic\": \"" +
                         COMMAND_TOPIC + "\", \
    \"payload_on\": \"ON\", \
    \"payload_off\": \"OFF\", \
    \"state_on\": \"ON\", \
    \"state_off\": \"OFF\", \
    \"optimistic\": false, \
    \"qos\": 0, \
    \"retain\": true \
  }";

  // Initialize Bluetooth with callback
  // BLEDevice::init(std::string(SWITCH_NAME.c_str()));
  // BLEServer *pServer = BLEDevice::createServer();
  // BLEService *pService =
  // pServer->createService(std::string(switchConfig.BLUETOOTH_UUID.c_str()));
  // BLECharacteristic *pCharacteristic = pService->createCharacteristic(
  //                                        switchConfig.CONFIG_CHARACTERISTIC_UUID.c_str(),
  //                                        BLECharacteristic::PROPERTY_READ |
  //                                        BLECharacteristic::PROPERTY_WRITE
  //                                      );
  // pCharacteristic->setValue(std::string(serializeSwitchConfigEditable(switchConfig).c_str()));
  // pCharacteristic->setCallbacks(new BluetoothLECallbacks());
  // pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();
  // pAdvertising->start();

  blinkRGBCode(switchConfig.CONFIG_CODE);

  // If the device has been configured, initialize WiFi and MQTT
  if (isSwitchConfigValid()) {
    WiFi.begin(switchConfig.WIFI_SSID, switchConfig.WIFI_PASSWORD);
    MQTT.begin(switchConfig.MQTT_SERVER.c_str(), 8883, NET);
    MQTT.onMessage(messageReceived);

    connect();

    MQTT.publish(CONFIG_TOPIC, INTRODUCTION_PAYLOAD, true, 1);
  } else {
    Serial.println("Config was not valid - will skip connecting.");
  }
}

// Connect to WiFi, then MQTT server, then subscribe to the 'flip' topic
void connect() {
  setRGB(255, 0, 0);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(2000);
  }

  // Prevent TLS certificate verification
  // TODO: Figure this out. See:
  // https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino
  NET.setInsecure();
  while (!MQTT.connect(SWITCH_NAME.c_str(), switchConfig.MQTT_USERNAME.c_str(),
                       switchConfig.MQTT_PASSWORD.c_str(), false)) {
    Serial.println("Connecting to MQTT server...");
    delay(1000);
  }

  MQTT.subscribe(COMMAND_TOPIC);
  setRGB(0, 0, 0);
  Serial.println("Ready!");
}

// On message received, flip the switch accordingly (skip checking topic as we
// only have the one subscription)
void messageReceived(String &topic, String &payload) {
  Serial.println("Received: " + topic + " - " + payload);

  String state;
  int angle;
  parseFlipCommand(payload, state, angle);

  if (angle >= 0) {
    if (state == "ON") {
      SWITCH_ON_SERVO_ANGLE = angle;
    } else if (state == "OFF") {
      SWITCH_OFF_SERVO_ANGLE = angle;
    }
  }
  flipSwitch(state == "ON");

  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}

// Run the MQTT loop, manage manual button toggles, publish state messages
void loop() {
  if (isSwitchConfigValid()) {
    MQTT.loop();
    delay(10); // Fixes some issues with WiFi stability

    if (!MQTT.connected()) {
      Serial.println("Lost connection...");
      connect();
    }

    if ((millis() - LAST_BUTTON_PUSH_TIME) > BUTTON_DEBOUNCE_DELAY) {
      if (!digitalRead(TOGGLE_BUTTON_PIN)) {
        LAST_BUTTON_PUSH_TIME = millis();
        flipSwitch(!SWITCH_STATE);
      }
    }

    if (PENDING_STATE_UPDATE) {
      MQTT.publish(STATE_TOPIC, SWITCH_STATE ? "ON" : "OFF");
      PENDING_STATE_UPDATE = false;
    }
  } else {
    Serial.println("Blinking config code...");
    blinkRGBCode(switchConfig.CONFIG_CODE);
    delay(5000);
  }
}
