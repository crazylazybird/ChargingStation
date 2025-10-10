#pragma once
#include "main.h"





void parse_energy_data(byte* data, int length);
int bcd2dec(byte bcd);
int bcd2dec(byte high, byte low);
int bcd2dec(byte b1, byte b2, byte b3, byte b4);

float get_power();
float get_energy_total();