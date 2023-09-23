#ifndef RGB_H
#define RGB_H

#include <Arduino.h>

const int RGB_LED_RED = LED_RED;
const int RGB_LED_GREEN = LED_GREEN;
const int RGB_LED_BLUE = LED_BLUE;

void setRGB(int redValue, int greenValue, int blueValue);
void turnOffRGB();
void fadeInRGB(int redStart, int greenStart, int blueStart, int redEnd,
               int greenEnd, int blueEnd, int loopDelay);
void blueFlick(bool reverse);
void blinkRGBCode(String str);

#endif