#include "creds.h"
#include "helpers.h"
#include "rgb.h"
#include "servo.h"
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <EEPROM.h>
#include <MQTT.h>
#include <WiFiClientSecure.h>

// Switch config defaults (define these in creds.h)
const char *WIFI_SSID_DEFAULT = WIFI_SSID_STR;
const char *WIFI_PASSWORD_DEFAULT = WIFI_PASSWORD_STR;
const char *MQTT_SERVER_DEFAULT = MQTT_SERVER_STR;
const char *MQTT_USERNAME_DEFAULT = MQTT_USERNAME_STR;
const char *MQTT_PASSWORD_DEFAULT = MQTT_PASSWORD_STR;

// Variable inits
const int TOGGLE_BUTTON_PIN = D5;
const int BUTTON_DEBOUNCE_DELAY = 500;
WiFiClientSecure NET;
MQTTClient MQTT(1024);
unsigned long LAST_LOOP_TIME = 0;
unsigned long LAST_BUTTON_PUSH_TIME = 0;
const int SWITCH_ID_LENGTH = 6;
const int CONFIG_CODE_LENGTH = 6;
String CONFIG_CODE = generateRandomNumString(CONFIG_CODE_LENGTH);
bool NEW_ID_WAS_GENERATED = false;
bool BLUETOOTH_UUID_WAS_GENERATED = false;
bool CONFIG_CHARACTERISTIC_UUID_WAS_GENERATED = false;

// Define switch configuration struct
struct SwitchConfig {
  char SWITCH_ID[SWITCH_ID_LENGTH + 1];
  char BLUETOOTH_UUID[37];
  char CONFIG_CHARACTERISTIC_UUID[37];
  char STATE_CHARACTERISTIC_UUID[37];
  char WIFI_SSID[33];
  char WIFI_PASSWORD[65];
  char MQTT_SERVER[257];
  char MQTT_USERNAME[65];
  char MQTT_PASSWORD[65];
};

// Other variables initialized during setup
String SWITCH_NAME;
String TOPIC_BASE;
String CONFIG_TOPIC;
String STATE_TOPIC;
String COMMAND_TOPIC;
String INTRODUCTION_PAYLOAD;
SwitchConfig SWITCH_CONFIG;
BLEServer *BLE_SERVER;
BLEService *BLE_SERVICE;
BLECharacteristic *BLE_CHAR_SWITCH_CONFIG;
BLEAdvertising *BLE_ADVERTISING;

// Switch config helper funcs
String getSwitchConfigForBLE(const SwitchConfig &config) {
  return "Change this to: "
         "'WIFI_SSID|WIFI_PASSWORD|MQTT_SERVER|MQTT_USERNAME|MQTT_PASSWORD|"
         "CONFIG_(STARTUP_BLINK)_CODE'";
}
int checkSwitchConfigFromBLE(const String &serialized, SwitchConfig &config) {
  char configCode[CONFIG_CODE_LENGTH + 1];
  int count = sscanf(serialized.c_str(), "%[^|]|%[^|]|%[^|]|%[^|]|%[^|]|%s",
                     config.WIFI_SSID, config.WIFI_PASSWORD, config.MQTT_SERVER,
                     config.MQTT_USERNAME, config.MQTT_PASSWORD, configCode);
  return (count != 6 || String(configCode).length() != CONFIG_CODE_LENGTH) ? 1
         : !(String(configCode).equals(CONFIG_CODE))                       ? 2
                                                                           : 0;
}
void printFullReadableSwitchConfig(const SwitchConfig &config,
                                   bool idIsNew = false) {
  Serial.println("Full Switch Configuration (" +
                 String((isSwitchConfigValid() ? "" : "not ")) + "valid):");
  Serial.println("SWITCH_ID:                  " + String(config.SWITCH_ID) +
                 String(idIsNew ? " (NEWLY GENERATED)" : ""));
  Serial.println("BLUETOOTH_UUID:             " +
                 String(config.BLUETOOTH_UUID));
  Serial.println("CONFIG_CHARACTERISTIC_UUID: " +
                 String(config.CONFIG_CHARACTERISTIC_UUID));
  Serial.println("WIFI_SSID:                  " + String(config.WIFI_SSID));
  Serial.println("WIFI_PASSWORD:              " + String(config.WIFI_PASSWORD));
  Serial.println("MQTT_SERVER:                " + String(config.MQTT_SERVER));
  Serial.println("MQTT_USERNAME:              " + String(config.MQTT_USERNAME));
  Serial.println("MQTT_PASSWORD:              " + String(config.MQTT_PASSWORD));
  Serial.println("CONFIG_CODE:                " + String(CONFIG_CODE));
}
void resetSwitchConfigEditable(SwitchConfig &config) {
  strcpy(SWITCH_CONFIG.WIFI_SSID, WIFI_SSID_DEFAULT);
  strcpy(SWITCH_CONFIG.WIFI_PASSWORD, WIFI_PASSWORD_DEFAULT);
  strcpy(SWITCH_CONFIG.MQTT_SERVER, MQTT_SERVER_DEFAULT);
  strcpy(SWITCH_CONFIG.MQTT_USERNAME, MQTT_USERNAME_DEFAULT);
  strcpy(SWITCH_CONFIG.MQTT_PASSWORD, MQTT_PASSWORD_DEFAULT);
}
bool isSwitchConfigValid() {
  return !String(SWITCH_CONFIG.SWITCH_ID).isEmpty() &&
         !String(SWITCH_CONFIG.MQTT_SERVER).isEmpty() &&
         !String(SWITCH_CONFIG.MQTT_USERNAME).isEmpty() &&
         !String(SWITCH_CONFIG.MQTT_PASSWORD).isEmpty() &&
         !String(SWITCH_CONFIG.WIFI_SSID).isEmpty() &&
         !String(SWITCH_CONFIG.WIFI_PASSWORD).isEmpty();
}

// BLE callback for configuration - save new config if valid and restart
String bleCommandChunks = ""; // Long commands come in multiple callbacks
class configBLECallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = String((pCharacteristic->getValue()).c_str());
    Serial.println("Got a BLE config update with value: " + value);
    value = bleCommandChunks + value;
    SwitchConfig newConfig;
    strcpy(newConfig.SWITCH_ID, SWITCH_CONFIG.SWITCH_ID);
    strcpy(newConfig.BLUETOOTH_UUID, SWITCH_CONFIG.BLUETOOTH_UUID);
    strcpy(newConfig.CONFIG_CHARACTERISTIC_UUID,
           SWITCH_CONFIG.CONFIG_CHARACTERISTIC_UUID);
    int validityCode = checkSwitchConfigFromBLE(value, newConfig);
    if (validityCode == 0) {
      Serial.println("BLE config updated.");
      EEPROM.put(0, newConfig);
      EEPROM.commit();
      ESP.restart();
    } else {
      bleCommandChunks = value;
      pCharacteristic->setValue(
          std::string(getSwitchConfigForBLE(SWITCH_CONFIG).c_str()));
      if (validityCode == 2) { // Full command, wrong code
        bleCommandChunks = "";
        Serial.println("BLE config command ignored - incorrect config code!");
      }
    }
  }
};

// BLE callback for state
class stateBLECallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String value = String((pCharacteristic->getValue()).c_str());
    Serial.println("Got BLE state update with value: " + value);
    String state;
    int angle;
    parseFlipCommand(value, state, angle);
    if (value.indexOf(state + " ") == 0 &&
        SWITCH_STATE != (state == "ON")) { // Prevent repeats
      flipSwitch(state == "ON");
    }
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
  EEPROM.begin(1024);

  delay(5000); // For debugging - give me time to switch to serial monitor

  // Load the switch configuration and generate a new switch ID and BLE UUIDs if
  // needed
  EEPROM.get(0, SWITCH_CONFIG);
  if (String(SWITCH_CONFIG.SWITCH_ID).length() != SWITCH_ID_LENGTH) {
    strcpy(SWITCH_CONFIG.SWITCH_ID,
           generateRandomString(SWITCH_ID_LENGTH).c_str());
    resetSwitchConfigEditable(SWITCH_CONFIG);
    EEPROM.put(0, SWITCH_CONFIG);
    EEPROM.commit();
    NEW_ID_WAS_GENERATED = true;
  }
  if (!isValidUUID(SWITCH_CONFIG.BLUETOOTH_UUID)) {
    strcpy(SWITCH_CONFIG.BLUETOOTH_UUID,
           "091AC3FB-CF49-BF3D-F6B3-548BED0E7D87"); // Unlear whether UUIDs must
    // be unique per-switch;
    // hardcoded for now
    EEPROM.put(0, SWITCH_CONFIG);
    EEPROM.commit();
  }
  if (!isValidUUID(SWITCH_CONFIG.CONFIG_CHARACTERISTIC_UUID)) {
    strcpy(SWITCH_CONFIG.CONFIG_CHARACTERISTIC_UUID,
           "1726868D-DC3B-3CDA-273F-CAE3BF8B38B8");
    EEPROM.put(0, SWITCH_CONFIG);
    EEPROM.commit();
  }
  printFullReadableSwitchConfig(SWITCH_CONFIG, NEW_ID_WAS_GENERATED);

  // Initialize all remaining variables using the switch ID
  SWITCH_NAME = "Switch-" + String(SWITCH_CONFIG.SWITCH_ID);
  TOPIC_BASE = "homeassistant/switch/" + String(SWITCH_CONFIG.SWITCH_ID);
  CONFIG_TOPIC = TOPIC_BASE + "/config";
  STATE_TOPIC = TOPIC_BASE + "/state";
  COMMAND_TOPIC = TOPIC_BASE + "/command";
  INTRODUCTION_PAYLOAD = "{ \
    \"name\": \"Switch " +
                         String(SWITCH_CONFIG.SWITCH_ID) + "\", \
    \"unique_id\": \"" + String(SWITCH_CONFIG.SWITCH_ID) +
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

  // Give the user their config code
  blinkRGBCode(CONFIG_CODE);
  delay(300);

  // If the device has been configured, initialize WiFi and MQTT
  if (isSwitchConfigValid()) {
    WiFi.begin(SWITCH_CONFIG.WIFI_SSID, SWITCH_CONFIG.WIFI_PASSWORD);
    MQTT.begin(SWITCH_CONFIG.MQTT_SERVER, 8883, NET);
    MQTT.onMessage(messageReceived);
    connect();
    MQTT.publish(CONFIG_TOPIC, INTRODUCTION_PAYLOAD, true, 1);
  } else {
    resetSwitchConfigEditable(SWITCH_CONFIG);
    EEPROM.put(0, SWITCH_CONFIG);
    EEPROM.commit();
  }

  // Initialize Bluetooth with callback
  BLEDevice::init(std::string(SWITCH_NAME.c_str()));
  BLEDevice::setMTU(65536);
  BLE_SERVER = BLEDevice::createServer();
  BLE_SERVICE =
      BLE_SERVER->createService(std::string(SWITCH_CONFIG.BLUETOOTH_UUID));

  BLE_CHAR_SWITCH_CONFIG = BLE_SERVICE->createCharacteristic(
      SWITCH_CONFIG.CONFIG_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  BLE_CHAR_SWITCH_CONFIG->setValue(
      std::string(getSwitchConfigForBLE(SWITCH_CONFIG).c_str()));
  BLE_CHAR_SWITCH_CONFIG->setCallbacks(new configBLECallbacks());
  BLEDescriptor BLE_DESC_SWITCH_CONFIG("5f0ef68d-7dff-4af0-b561-4f3271a1f782");
  BLE_DESC_SWITCH_CONFIG.setValue("Switch Configuration String");
  BLE_CHAR_SWITCH_CONFIG->addDescriptor(&BLE_DESC_SWITCH_CONFIG);

  BLE_SERVICE->start();
  BLE_ADVERTISING = BLE_SERVER->getAdvertising();
  BLE_ADVERTISING->start();
}

// Connect to WiFi, then MQTT server, then subscribe to the 'flip' topic
void connect() {
  setRGB(32, 0, 0);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(2000);
  }

  // Prevent TLS certificate verification
  // TODO: Figure this out. See:
  // https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino
  NET.setInsecure();
  while (!MQTT.connect(SWITCH_NAME.c_str(), SWITCH_CONFIG.MQTT_USERNAME,
                       SWITCH_CONFIG.MQTT_PASSWORD, false)) {
    Serial.println("Connecting to MQTT server...");
    delay(1000);
  }

  MQTT.subscribe(COMMAND_TOPIC);
  fadeInRGB(32, 0, 0, 255, 255, 255, 2);
  delay(60);
  fadeInRGB(255, 255, 255, 0, 0, 0, 1);
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
}
