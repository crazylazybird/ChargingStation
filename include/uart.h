#pragma once

#define UART0_DEBUG_PORT Serial
#define UART0_DEBUG_PORT_BAUDRATE 115200

#define UART1_VMC_PORT Serial1
#define UART1_VMC_PORT_BAUDRATE 57600
#define UART1_VMC_PORT_RX_PIN 6
#define UART1_VMC_PORT_TX_PIN 7

#define BUFFER_SIZE 256



void UART_Setup();
void UART_Commands_processing();
void send_HEX(const String& hexString);
void decode_HEX(const String& hexString);
void send_message(byte* message, int messageLength);
void process_received_data();
void UART_POS_recieved_data();
void send_IDLE();
void softserial_energy_port_send_command(const String& cmd);



