#pragma once
#include "main.h"

#define RX2_PIN 20 //для связи с энергосчетчика
#define TX2_PIN 21


void process_received_energy_data();
bool check_CRC(const uint8_t* data, int length);
void parse_energy_data(byte* data, int length);
int bcd2dec(byte bcd);
int bcd2dec(byte high, byte low);
int bcd2dec(byte b1, byte b2, byte b3, byte b4);