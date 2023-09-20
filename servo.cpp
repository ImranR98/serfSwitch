#include "servo.h"
#include <Arduino.h>

int SWITCH_ON_SERVO_ANGLE = 145;
int SWITCH_OFF_SERVO_ANGLE = 20;
bool SWITCH_STATE = false;
bool PENDING_STATE_UPDATE = false;

// A flip command can be 'ON' or 'OFF, or 'ON <angle>' or 'OFF <angle>' where
// the latter pair of commands will update the default on/off angle
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

// Helper to simplify servo movements
void moveServoToApproxAngle(int angle) {
  Serial.println("Moving to angle: " + String(angle));
  int pulseWidth = map(angle, SERVO_ANGLE_MIN, SERVO_ANGLE_MAX, SERVO_PWM_MIN,
                       SERVO_PWM_MAX);
  for (int i = 0; i < 3;
       i++) { // Large angle changes need more than one "signal"
    for (int j = 0; j < SERVO_PIN_LENGTH; j++) {
      digitalWrite(SERVO_PINS[j], HIGH);
      delayMicroseconds(pulseWidth);
      digitalWrite(SERVO_PINS[j], LOW);
      delay(30);
    }
  }
}

// Helper to flip the switch
void flipSwitch(bool val) {
  SWITCH_STATE = val;
  moveServoToApproxAngle(SWITCH_STATE ? SWITCH_ON_SERVO_ANGLE
                                      : SWITCH_OFF_SERVO_ANGLE);
  digitalWrite(INDICATOR_PIN, SWITCH_STATE);
  PENDING_STATE_UPDATE = true;
}