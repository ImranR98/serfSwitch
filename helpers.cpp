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
    char randomChar = random(1, 10) + '0'; // Generate a random digit (0-9)
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

bool isValidUUID(String uuidStr) {
  if (uuidStr.length() != 36) {
    return false;
  }
  if (uuidStr[8] != '-' || uuidStr[13] != '-' || uuidStr[18] != '-' ||
      uuidStr[23] != '-') {
    return false;
  }
  for (int i = 0; i < 36; i++) {
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      continue;
    }
    char c = uuidStr[i];
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
          (c >= 'A' && c <= 'F'))) {
      return false;
    }
  }
  return true;
}