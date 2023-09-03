#include <EEPROM.h>
#include <WiFiClientSecure.h>
#include <MQTT.h>
#include "creds.h"

// Switch/server config (define these in creds.h)
const char* WIFI_SSID = WIFI_SSID_STR;
const char* WIFI_PASSWORD = WIFI_PASSWORD_STR;
const char* MQTT_SERVER = MQTT_SERVER_STR;
const char* MQTT_USERNAME = MQTT_USERNAME_STR;
const char* MQTT_PASSWORD = MQTT_PASSWORD_STR;

// Hardware config (Arduino Nano ESP32, SG90 motor, built in LED, push button)
const int SERVO_PIN_A = D7;
const int SERVO_PIN_B = D6;
const int INDICATOR_PIN=LED_BUILTIN;
const int TOGGLE_BUTTON_PIN = D5;
const int BUTTON_DEBOUNCE_DELAY = 500;
int SERVO_ANGLE_MIN = 0;
int SERVO_ANGLE_MAX = 188;
int SERVO_PWM_MIN = 500;
int SERVO_PWM_MAX = 2500;
int SWITCH_ON_SERVO_ANGLE=145;
int SWITCH_OFF_SERVO_ANGLE=20; 

// Other variables initialized during setup
int SWITCH_ID_LENGTH = 5;
bool NEW_ID_WAS_GENERATED = false;
String SWITCH_ID;
String TOPIC_BASE;
String CONFIG_TOPIC;
String STATE_TOPIC;
String COMMAND_TOPIC;
String INTRODUCTION_PAYLOAD;

// Variable inits
WiFiClientSecure NET;
MQTTClient MQTT(1024);const char WIFI_SSID* = WIFI_SSID_STR;
const char WIFI_PASSWORD* = WIFI_PASSWORD_STR;
const char MQTT_SERVER* = MQTT_SERVER_STR;
const char MQTT_USERNAME* = MQTT_USERNAME_STR;
const char MQTT_PASSWORD* = MQTT_PASSWORD_STR;
unsigned long LAST_LOOP_TIME = 0;
unsigned long LAST_BUTTON_PUSH_TIME = 0;
bool SWITCH_STATE = false;
bool PENDING_STATE_UPDATE = false;

// Helper to simplify servo movements
void moveServoToApproxAngle(int angle) {
  Serial.println("Moving to angle: " + String(angle));
  int pulseWidth = map(angle, SERVO_ANGLE_MIN, SERVO_ANGLE_MAX, SERVO_PWM_MIN, SERVO_PWM_MAX);
  digitalWrite(SERVO_PIN_A, HIGH);
  digitalWrite(SERVO_PIN_B, HIGH);
  delayMicroseconds(pulseWidth);
  digitalWrite(SERVO_PIN_A, LOW);
  digitalWrite(SERVO_PIN_B, LOW);
  delay(30);
  digitalWrite(SERVO_PIN_A, HIGH);
  digitalWrite(SERVO_PIN_B, HIGH);
  delayMicroseconds(pulseWidth);
  digitalWrite(SERVO_PIN_A, LOW);
  digitalWrite(SERVO_PIN_B, LOW);
}

// Helper to flip the switch
void flipSwitch(bool val) {
  SWITCH_STATE = val;
  moveServoToApproxAngle(SWITCH_STATE ? SWITCH_ON_SERVO_ANGLE : SWITCH_OFF_SERVO_ANGLE);
  digitalWrite(INDICATOR_PIN, SWITCH_STATE);
  PENDING_STATE_UPDATE = true;
}

// Connect to WiFi, then MQTT server, then subscribe to the 'flip' topic
void connect() {
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(2000);
  }

  // do not verify tls certificate
  // check the following example for methods to verify the server:
  // https://github.com/espressif/arduino-esp32/blob/master/libraries/WiFiClientSecure/examples/WiFiClientSecure/WiFiClientSecure.ino
  NET.setInsecure();
  while (!MQTT.connect(("switch-" + SWITCH_ID).c_str(), MQTT_USERNAME, MQTT_PASSWORD, false)) {
    Serial.println("Connecting to MQTT server...");
    delay(1000);
  }

  MQTT.subscribe(COMMAND_TOPIC);
  Serial.println("Ready!");
}

// A flip command can be 'ON' or 'OFF, or 'ON <angle>' or 'OFF <angle>' where the latter pair of commands will update the default on/off angle
void parseFlipCommand(const String &input, String &state, int &angle) {
  String words[2];
  int wordCount = 0;
  int i = 0;

  for (i = 0; i < input.length(); i++) {
    char c = input.charAt(i);
    if (c == ' ') {
      wordCount++;
    } else {
      words[wordCount] += c;
    }
  }
  if (input.charAt(i) != ' ') {
    wordCount++;
  }

  if (wordCount >= 1) {
    state = words[0];
  } else {
    state = 'OFF';
  }

  if (wordCount == 2) {
    angle = words[1].toInt();
  } else {
    angle = -1;
  }
}

// On message received, flip the switch accordingly (skip checking topic as we only have the one subscription)
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

// Prepare hardware and MQTT
void setup() {
  Serial.begin(115200);
  pinMode(SERVO_PIN_A, OUTPUT);
  pinMode(SERVO_PIN_B, OUTPUT);
  pinMode(INDICATOR_PIN, OUTPUT);
  pinMode(TOGGLE_BUTTON_PIN, INPUT_PULLUP);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  btStop(); // Turn off Bluetooth to save power
  EEPROM.begin(SWITCH_ID_LENGTH + 1);

  delay(5000);

  SWITCH_ID = EEPROM.readString(0);
  if (SWITCH_ID.length() != SWITCH_ID_LENGTH) {
    String characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    SWITCH_ID = "";
    for (int i = 0; i < SWITCH_ID_LENGTH; i++) {
      SWITCH_ID += characters[random(characters.length())];
    }
    EEPROM.writeString(0, SWITCH_ID.c_str());
    EEPROM.commit();
    EEPROM.end();
    NEW_ID_WAS_GENERATED = true;
  }
  TOPIC_BASE = "homeassistant/switch/" + SWITCH_ID;
  CONFIG_TOPIC = TOPIC_BASE + "/config";
  STATE_TOPIC = TOPIC_BASE + "/state";
  COMMAND_TOPIC = TOPIC_BASE + "/command";
  INTRODUCTION_PAYLOAD = "{ \
    \"name\": \"Switch " + SWITCH_ID + "\", \
    \"unique_id\": \"" + SWITCH_ID + "\", \
    \"device_class\": \"switch\", \
    \"state_topic\": \"" + STATE_TOPIC + "\", \
    \"command_topic\": \"" + COMMAND_TOPIC + "\", \
    \"payload_on\": \"ON\", \
    \"payload_off\": \"OFF\", \
    \"state_on\": \"ON\", \
    \"state_off\": \"OFF\", \
    \"optimistic\": false, \
    \"qos\": 0, \
    \"retain\": true \
  }";

  Serial.println("Switch ID: " + SWITCH_ID + (NEW_ID_WAS_GENERATED ? " (newly generated)" : ""));

  // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported
  // by Arduino. You need to set the IP address directly.
  //
  // MQTT brokers usually use port 8883 for secure connections.
  MQTT.begin(MQTT_SERVER, 8883, NET);
  MQTT.onMessage(messageReceived);

  connect();

  MQTT.publish(CONFIG_TOPIC, INTRODUCTION_PAYLOAD, true, 1);
}

// Run the MQTT loop, manage manual button toggles, ublish state messages when needed
void loop() {
  MQTT.loop();
  delay(10);  // <- fixes some issues with WiFi stability
  
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
}
