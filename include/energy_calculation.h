#pragma once
#include "main.h"

void process_received2_data();
bool check_CRC(const uint8_t* data, int length);
void parse_energy_data(byte* data, int length);
int bcd2dec(byte bcd);
int bcd2dec(byte high, byte low);
int bcd2dec(byte b1, byte b2, byte b3, byte b4);