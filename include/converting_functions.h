#pragma once
#include "main.h"

uint16_t calculateCRC16(const uint8_t* data, uint16_t length);
void calculateCRC(const String& hexString);
byte hexCharToByte(char c);
byte convertHexStringToByte(const String& hexStr);