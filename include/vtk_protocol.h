#pragma once

extern volatile int operationNumber;

byte* create_IDL_message(int messageLength);
byte* create_VTK_message(const std::string& messageName, int operationNumber, int& messageLength, const std::map<int, std::vector<byte>>& additionalParams);
void send_IDL();
void send_DIS();
void send_VRP(long amount);
void increment_operation_number();
int get_current_operation_number();
void send_FIN(float amount, int opNumber);
void sendREFUND(float amount, int operationNumber);