#include <EEPROM.h>
#include <MQTT.h>
#include <WiFiClientSecure.h>

#include "rgb.h"
#include "servo.h"

// Hardware config (Arduino Nano ESP32, SG90 motor, built in LED, push button)
const int TOGGLE_BUTTON_PIN = D5;
const int BUTTON_DEBOUNCE_DELAY = 500;

struct SwitchConfig {
  String SWITCH_ID;
  String WIFI_SSID;
  String WIFI_PASSWORD;
  String MQTT_SERVER;
  String MQTT_USERNAME;
  String MQTT_PASSWORD;
};
SwitchConfig switchConfig;

// Other variables initialized during setup
int SWITCH_ID_LENGTH = 5;
bool NEW_ID_WAS_GENERATED = false;
String SWITCH_NAME;
String TOPIC_BASE;
String CONFIG_TOPIC;
String STATE_TOPIC;
String COMMAND_TOPIC;
String INTRODUCTION_PAYLOAD;

// Variable inits
WiFiClientSecure NET;
MQTTClient MQTT(1024);
unsigned long LAST_LOOP_TIME = 0;
unsigned long LAST_BUTTON_PUSH_TIME = 0;

bool isConfigValid() {
  return !switchConfig.SWITCH_ID.isEmpty() &&
         !switchConfig.MQTT_SERVER.isEmpty() &&
         !switchConfig.MQTT_USERNAME.isEmpty() &&
         !switchConfig.MQTT_PASSWORD.isEmpty() &&
         !switchConfig.WIFI_SSID.isEmpty() &&
         !switchConfig.WIFI_PASSWORD.isEmpty();
}

// Prepare hardware and connect to MQTT
void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(0));
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
  if (switchConfig.SWITCH_ID.length() != SWITCH_ID_LENGTH) {
    String characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    switchConfig.SWITCH_ID = "";
    for (int i = 0; i < SWITCH_ID_LENGTH; i++) {
      switchConfig.SWITCH_ID += characters[random(characters.length())];
    }
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

  // TODO: Generate and save bluetooth code
  // TODO: Initialize Bluetooth with callback that parses input, saves if valid,
  // and resets
  // TODO: Blink bluetooth code

  // If the device has been configured, initialize WiFi and MQTT
  if (isConfigValid()) {
    WiFi.begin(switchConfig.WIFI_SSID, switchConfig.WIFI_PASSWORD);
    MQTT.begin(switchConfig.MQTT_SERVER.c_str(), 8883, NET);
    MQTT.onMessage(messageReceived);

    connect();

    MQTT.publish(CONFIG_TOPIC, INTRODUCTION_PAYLOAD, true, 1);
  }
}

// Connect to WiFi, then MQTT server, then subscribe to the 'flip' topic
void connect() {
  setColor(256, 0, 0);
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
  setColor(0, 0, 0);
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

String generateRandomString(int length) {
  String randomStr = "";
  for (int i = 0; i < length; i++) {
    char randomChar = random(10) + '0'; // Generate a random digit (0-9)
    randomStr += randomChar;
  }
  return randomStr;
}

// Run the MQTT loop, manage manual button toggles, publish state messages
void loop() {
  if (isConfigValid()) {
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
    // TODO: Blink bluetooth code and sleep for a while
  }
}
