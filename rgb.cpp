#include "rgb.h"

void setRGB(int redValue, int greenValue, int blueValue) {
  analogWrite(RGB_LED_RED, 255 - redValue);
  analogWrite(RGB_LED_GREEN, 255 - greenValue);
  analogWrite(RGB_LED_BLUE, 255 - blueValue);
}

void turnOffRGB() { setRGB(0, 0, 0); }

void fadeInRGB(int redStart, int greenStart, int blueStart, int redEnd,
               int greenEnd, int blueEnd, int loopDelay) {
  bool complete = false;
  int red = redStart < redEnd ? redStart : redEnd;
  int green = greenStart < greenEnd ? greenStart : greenEnd;
  int blue = blueStart < blueEnd ? blueStart : blueEnd;
  int redComplete = redStart < redEnd ? redEnd : redStart;
  int greenComplete = greenStart < greenEnd ? greenEnd : greenStart;
  int blueComplete = blueStart < blueEnd ? blueEnd : blueStart;

  while (!complete) {
    complete = true;
    if (red != redComplete) {
      red++;
      complete = false;
    }
    if (green != greenComplete) {
      green++;
      complete = false;
    }
    if (blue != blueComplete) {
      blue++;
      complete = false;
    }
    setRGB(red, green, blue);
    delay(loopDelay);
  }
}

void blinkRGBCode(String str) {
  fadeInRGB(0, 0, 0, 255, 255, 255, 15);
  turnOffRGB();
  delay(500);

  for (int i = 0; i < str.length(); i++) {
    char digit = str.charAt(i);
    int blinkCount = digit - '0'; // Convert the character to an integer
    long red = i % 2 == 0 ? 0 : 255;
    long green = i % 2 == 0 ? 255 : 0;
    long blue = 0;

    Serial.println("Digit: " + String(digit) +
                   " Blinks: " + String(blinkCount) + " RGB: " + String(red) +
                   " " + String(green) + " " + String(blue));

    // Blink the RGB LED to represent the current digit
    delay(1000);
    for (int j = 0; j < blinkCount; j++) {
      setRGB(red, green, blue);
      delay(700);
      turnOffRGB();
      delay(300);
    }
  }
}