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
  uint32_t uuid_part1 = random(0xFFFFFFFF);
  uint32_t uuid_part2 = random(0xFFFFFFFF);
  char uuidBuffer[37];
  snprintf(
    uuidBuffer, sizeof(uuidBuffer),
    "%08lX-%04lX-%04lX-%04lX-%08lX",
    (uuid_part1 >> 32),
    (uuid_part1 >> 16) & 0xFFFF,
    uuid_part1 & 0xFFFF,
    (uuid_part2 >> 16) & 0xFFFF,
    uuid_part2 & 0xFFFF
  );
  return String(uuidBuffer);
}