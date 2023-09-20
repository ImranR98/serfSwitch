#include "rgb.h"

void blinkRGB(String str) {
  for (int i = 0; i < str.length(); i++) {
    char digit = str.charAt(i);
    int blinkCount = digit - '0'; // Convert the character to an integer
    long red = random(256);
    long green = random(256);
    long blue = random(256);

    // Blink the RGB LED to represent the current digit
    for (int j = 0; j < blinkCount; j++) {
      setColor(red, green, blue);
      delay(500);
      turnOffLED();
      delay(500);
    }
  }
}

void setColor(int redValue, int greenValue, int blueValue) {
  analogWrite(RGB_LED_RED, redValue);
  analogWrite(RGB_LED_GREEN, greenValue);
  analogWrite(RGB_LED_BLUE, blueValue);
}

void turnOffLED() { setColor(0, 0, 0); }