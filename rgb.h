#ifndef RGB_H
#define RGB_H

#include <Arduino.h>

const int RGB_LED_RED = LED_RED;
const int RGB_LED_GREEN = LED_GREEN;
const int RGB_LED_BLUE = LED_BLUE;

void blinkRGB(String str);
void setColor(int redValue, int greenValue, int blueValue);
void turnOffLED();

#endif