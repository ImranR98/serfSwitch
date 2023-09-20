#ifndef SERVO_H
#define SERVO_H

const int SERVO_PINS[] = {D3, D6, D7};
const int SERVO_PIN_LENGTH = sizeof(SERVO_PINS) / sizeof(SERVO_PINS[0]);
const int INDICATOR_PIN = LED_BUILTIN;
const int SERVO_ANGLE_MIN = 0;
const int SERVO_ANGLE_MAX = 188;
const int SERVO_PWM_MIN = 500;
const int SERVO_PWM_MAX = 2500;

extern int SWITCH_ON_SERVO_ANGLE;
extern int SWITCH_OFF_SERVO_ANGLE;
extern bool SWITCH_STATE;
extern bool PENDING_STATE_UPDATE;

void parseFlipCommand(const String &input, String &state, int &angle);
void moveServoToApproxAngle(int angle);
void flipSwitch(bool val);

#endif