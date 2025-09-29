#pragma once

extern volatile int operationNumber;

byte* createIDLmessage(int messageLength);
void sendMessage (byte* message, int messageLength);
void sendIDL();
int getCurrentOperationNumber();