#include "main.h"

// Константы протокола связи VTK
const int KEEPALIVE_INTERVAL = 10000;   // Интервал keepalive в мс
const int OPERATION_NUMBER_LENGTH = 8;  // Длина номера операции
const int MAX_OPERATION_NUMBER = 65535;
const int MIN_MESSAGE_SIZE = 10;

byte* create_IDL_message(int messageLength) {

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
  uint16_t crc = calculate_CRC16(message, messageLength);
  message[messageLength] = crc >> 8;
  message[messageLength + 1] = crc & 0xFF;

  return message;
}

byte* create_VTK_message(const std::string& messageName, int operationNumber, int& messageLength, const std::map<int, std::vector<byte>>& additionalParams = {}) {
  // Расчет базовой длины с учетом двухбайтовой длины
  int opLength = operationNumber < 10 ? 1 : static_cast<int>(log10(operationNumber)) + 1;
  int baseLength = 2 + 2 + messageName.length() + 2 + opLength;  // 2 байта протокол + 2 тег имени + байты имени + тег операции + длина номера операции

  for (const auto& param : additionalParams) {
    baseLength += 2 + param.second.size();  //добавляем длину параметров
  }

  messageLength = baseLength + 3;  //without CRC

  byte* message = new byte[messageLength + 2];  //with CRC
  int pos = 0;

  // Формирование заголовка
  message[pos++] = 0x1F;  // STX

  // Длина сообщения в двух байтах
  message[pos++] = (baseLength) >> 8;  // Старший байт
  message[pos++] = (baseLength)&0xFF;  // Младший байт

  message[pos++] = 0x96;
  message[pos++] = 0xFB;

  // Имя сообщения
  message[pos++] = 0x01;
  message[pos++] = messageName.length();
  for (size_t i = 0; i < messageName.length(); i++) {
    message[pos++] = static_cast<byte>(messageName[i]);
  }

  // Номер операции в ASCII
  message[pos++] = 0x03;      // Тег операции
  message[pos++] = opLength;  // Длина поля операции

  if (operationNumber == 0) {
    message[pos++] = '0';
  } else {
    int temp = operationNumber;
    int writePos = pos + opLength - 1;
    while (temp > 0) {
      message[writePos--] = '0' + temp % 10;
      temp /= 10;
    }
    pos += opLength;
  }

  // Дополнительные параметры
  for (const auto& param : additionalParams) {
    message[pos++] = static_cast<byte>(param.first);
    message[pos++] = static_cast<byte>(param.second.size());
    for (size_t i = 0; i < param.second.size(); i++) {
      message[pos++] = param.second[i];
    }
  }

  uint16_t crc = calculate_CRC16(message, messageLength);
  message[messageLength] = static_cast<byte>(crc >> 8);
  message[messageLength + 1] = static_cast<byte>(crc & 0xFF);

  //отладочный вывод в порт USB
  //   USB_PORT.print("сформировано ");
  //   USB_PORT.print(messageLength);
  //   USB_PORT.print("байт сообщения: ");
  // for (int i = 0; i < messageLength + 2; i++) {
  //   if (message[i] < 0x10) USB_PORT.print("0");
  //   USB_PORT.print(message[i], HEX);
  //   USB_PORT.print(" ");
  // }
  // USB_PORT.println();


  return message;
}

void send_IDL() {
    int operationNumber = get_current_operation_number();
    int messageLength = 10;
    byte* message = create_IDL_message(messageLength);

    // Логирование отправки
    UART0_DEBUG_PORT.print("Отправка IDL, операция: ");
    UART0_DEBUG_PORT.println(operationNumber);

    // Отправка сообщения
    send_message(message, messageLength + 2);

    delete[] message;
}

void send_DIS() {
  int messageLength;
  byte* message = create_VTK_message("DIS", get_current_operation_number(), messageLength);

  UART0_DEBUG_PORT.print("Отправка DIS, длина: ");
  UART0_DEBUG_PORT.println(messageLength + 2);

  send_message(message, messageLength + 2);

  delete[] message;
}

void send_VRP(int amount) {
  int messageLength;
  std::map<int, std::vector<byte>> params;

  // Преобразуем сумму в строку
  String amountStr = String(amount);
  
  // Создаем вектор байтов из строки
  std::vector<byte> amountBytes;
  for (int i = 0; i < amountStr.length(); i++) {
      amountBytes.push_back(amountStr.charAt(i));
  }

  params[0x04] = amountBytes;
  increment_operation_number();
  byte* message = create_VTK_message("VRP", get_current_operation_number(), messageLength, params);

  UART0_DEBUG_PORT.print("Отправка платежа, сумма: ");
  UART0_DEBUG_PORT.print(amount);
  UART0_DEBUG_PORT.print(", длина: ");
  UART0_DEBUG_PORT.println(messageLength + 2);

  send_message(message, messageLength + 2);

  delete[] message;
}

void sendREFUND(int amount, int operationNumber) {
  int messageLength;
  std::map<int, std::vector<byte>> params;

  // Формируем сумму до 1000000
  std::vector<byte> amountBytes;
  amountBytes.push_back((amount / 1000000) + '0');
  amountBytes.push_back(((amount / 100000) % 10) + '0');
  amountBytes.push_back(((amount / 10000) % 10) + '0');
  amountBytes.push_back(((amount / 1000) % 10) + '0');
  amountBytes.push_back(((amount / 100) % 10) + '0');
  amountBytes.push_back(((amount / 10) % 10) + '0');
  amountBytes.push_back((amount % 10) + '0');

  params[0x04] = amountBytes;

  // Добавляем номер операции
  std::vector<byte> opNumberBytes;
  opNumberBytes.push_back((operationNumber >> 8) & 0xFF);
  opNumberBytes.push_back(operationNumber & 0xFF);
  params[0x03] = opNumberBytes;

  byte* message = create_VTK_message("VRP", get_current_operation_number(), messageLength, params);

  UART0_DEBUG_PORT.print("Отправка возврата, сумма: ");
  UART0_DEBUG_PORT.print(amount);
  UART0_DEBUG_PORT.print(", операция: ");
  UART0_DEBUG_PORT.print(operationNumber);
  UART0_DEBUG_PORT.print(", длина: ");
  UART0_DEBUG_PORT.println(messageLength + 2);

  send_message(message, messageLength + 2);

  delete[] message;
}

void send_FIN(int amount){
  int messageLength;
  std::map<int, std::vector<byte>> params;

  // Преобразуем сумму в строку
  String amountStr = String(amount);
  
  // Создаем вектор байтов из строки
  std::vector<byte> amountBytes;
  for (int i = 0; i < amountStr.length(); i++) {
      amountBytes.push_back(amountStr.charAt(i));
  }

  params[0x04] = amountBytes;
  increment_operation_number();
  byte* message = create_VTK_message("FIN", get_current_operation_number(), messageLength, params);

  UART0_DEBUG_PORT.print("Подтверждение платежа от терминала");
  UART0_DEBUG_PORT.print(amount);
  UART0_DEBUG_PORT.print(", длина: ");
  UART0_DEBUG_PORT.println(messageLength + 2);

  send_message(message, messageLength + 2);

  delete[] message;
}

void increment_operation_number() {
  noInterrupts();
  operationNumber = (operationNumber < MAX_OPERATION_NUMBER) ? operationNumber + 1 : 0;
  interrupts();
}

int get_current_operation_number() {
  int currentNumber;

  noInterrupts();
  currentNumber = operationNumber;
  interrupts();

  return currentNumber;
}