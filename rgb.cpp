#include "rgb.h"

void setRGB(int redValue, int greenValue, int blueValue) {
  analogWrite(RGB_LED_RED, 255 - redValue);
  analogWrite(RGB_LED_GREEN, 255 - greenValue);
  analogWrite(RGB_LED_BLUE, 255 - blueValue);
}

void turnOffRGB() { setRGB(0, 0, 0); }

void fadeInRGB(int red, int green, int blue, int redEnd, int greenEnd,
               int blueEnd, int loopDelay) {
  bool complete = false;
  bool redRev = redEnd < red;
  bool greenRev = greenEnd < green;
  bool blueRev = blueEnd < blue;

  while (!complete) {
    complete = true;
    if (red != redEnd) {
      redRev ? red-- : red++;
      complete = false;
    }
    if (green != greenEnd) {
      greenRev ? green-- : green++;
      complete = false;
    }
    if (blue != blueEnd) {
      blueRev ? blue-- : blue++;
      complete = false;
    }
    setRGB(red, green, blue);
    delay(loopDelay);
  }
}

void blueFlick(bool reverse = false) {
  if (!reverse) {
    fadeInRGB(0, 0, 0, 0, 0, 255, 10);
    setRGB(0, 255, 255);
    delay(30);
    turnOffRGB();
  } else {
    setRGB(0, 255, 255);
    delay(30);
    fadeInRGB(0, 0, 255, 0, 0, 0, 10);
  }
}

void blinkRGBCode(String str) {
  blueFlick();
  delay(300);
  for (int i = 0; i < str.length(); i++) {
    char digit = str.charAt(i);
    int blinkCount = digit - '0'; // Convert the character to an integer
    long red = i % 2 == 0 ? 0 : 255;
    long green = i % 2 == 0 ? 255 : 0;
    long blue = 0;
    delay(1500);
    for (int j = 0; j < blinkCount; j++) {
      // fadeInRGB(0, 0, 0, red, green, blue, 1);
      fadeInRGB(red, green, blue, 0, 0, 0, 2);
    }
  }
  delay(300);
  blueFlick(true);
}