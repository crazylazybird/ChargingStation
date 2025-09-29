#pragma once

#define UART0_DEBUG_PORT Serial
#define UART0_DEBUG_PORT_BAUDRATE 115200

#define UART1_POS_PORT Serial1
#define UART1_POS_PORT_BAUDRATE 115200
#define UART1_POS_PORT_RX_PIN 6
#define UART1_POS_PORT_TX_PIN 7

#define BUFFER_SIZE 256




void UART_Setup();
void UART_Commands_processing();
void sendHEX(const String& hexString);
void dcodeHEX(const String& hexString);
void sendMessage(byte* message, int messageLength);
