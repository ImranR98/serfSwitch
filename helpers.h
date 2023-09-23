#ifndef HELPERS_H
#define HELPERS_H

#include <Arduino.h>

String generateRandomString(int length);
String generateRandomNumString(int length);
String generateUUID();
bool isValidUUID(String uuidStr);

#endif