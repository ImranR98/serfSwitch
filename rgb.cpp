#include "rgb.h"
#include <Arduino.h>

void blinkRGB(String str) {
  for (int i = 0; i < str.length(); i++) {
    char digit = str.charAt(i);
    int blinkCount = digit - '0'; // Convert the character to an integer

    // Blink the RGB LED to represent the current digit
    for (int j = 0; j < blinkCount; j++) {
      setColor(random(256), random(256), random(256)); // Set a random color
      delay(500);                                      // Blink duration
      turnOffLED();
      delay(500); // Delay between blinks
    }

    // If it's not the last digit, differentiate with a pause and a different
    // color
    if (i < str.length() - 1) {
      turnOffLED();
      delay(1000); // Pause between digits
      setColor(random(256), random(256),
               random(256)); // Set a different random color
      delay(1000);           // Pause between digits
      turnOffLED();
    }
  }
}

void setColor(int redValue, int greenValue, int blueValue) {
  analogWrite(RGB_LED_RED, redValue);
  analogWrite(RGB_LED_GREEN, greenValue);
  analogWrite(RGB_LED_BLUE, blueValue);
}

void turnOffLED() { setColor(0, 0, 0); }