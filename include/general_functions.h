#pragma once
#include "main.h"

uint16_t calculate_CRC16_ccitt(const uint8_t* data, uint16_t length);
void calculate_CRC(const String& hexString);
bool check_CRC(const uint8_t* data, int length);
byte hex_char_to_byte(char c);
byte convert_hex_string_to_byte(const String& hexStr);
void clear_buffer();
uint16_t calculate_CRC16(const uint8_t* data, int length);

void init_time_client();
String getISO8601Time();