#include "helpers.h"

String generateRandomString(int length) {
  String characters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  String result = "";
  for (int i = 0; i < length; i++) {
    result += characters[random(characters.length())];
  }
  return result;
}

String generateRandomNumString(int length) {
  String randomStr = "";
  for (int i = 0; i < length; i++) {
    char randomChar = random(10) + '0'; // Generate a random digit (0-9)
    randomStr += randomChar;
  }
  return randomStr;
}

String generateUUID() {
  char uuidString[37];
  snprintf(uuidString, sizeof(uuidString),
           "%04X%04X-%04X-%04X-%04X-%04X%04X%04X", random(0xFFFF),
           random(0xFFFF), random(0xFFFF), random(0xFFFF), random(0xFFFF),
           random(0xFFFF), random(0xFFFF), random(0xFFFF));
  return String(uuidString);
}