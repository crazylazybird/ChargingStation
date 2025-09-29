#include "main.h"


byte* createIDLmessage(int messageLength) {

  // Выделяем память под сообщение
  byte* message = new byte[messageLength + 2];

  // Заполняем структуру сообщения
  message[0] = 0x1F;               // STX
  message[1] = 0x00;               // Старший байт длины
  message[2] = messageLength - 3;  // Младший байт длины (11)
  message[3] = 0x96;               // Дискриминатор VMC
  message[4] = 0xFB;

  // Message Name (IDL)
  message[5] = 0x01;  // Тег Message Name
  message[6] = 0x03;  // Длина
  message[7] = 'I';
  message[8] = 'D';
  message[9] = 'L';

  // Расчет CRC16
  uint16_t crc = calculateCRC16(message, messageLength);
  message[messageLength] = crc >> 8;
  message[messageLength + 1] = crc & 0xFF;

  return message;
}

void sendIDL() {
    int operationNumber = getCurrentOperationNumber();
    int messageLength = 10;
    byte* message = createIDLmessage(messageLength);

    // Логирование отправки
    UART0_DEBUG_PORT.print("Отправка IDL, операция: ");
    UART0_DEBUG_PORT.println(operationNumber);

    // Отправка сообщения
    sendMessage(message, messageLength + 2);

    delete[] message;
}


int getCurrentOperationNumber() {
  int currentNumber;

  noInterrupts();
  currentNumber = operationNumber;
  interrupts();

  return currentNumber;
}