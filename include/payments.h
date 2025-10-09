#pragma once




void start_payment(int amount);

void process_POS_received_data();

void handle_successful_payment();
void handle_failed_payment();
void handle_payment_timeout();

struct tlv {
  String mesName;
  int32_t amount;
  unsigned long lastTime;
  bool isMesProcessed;
};