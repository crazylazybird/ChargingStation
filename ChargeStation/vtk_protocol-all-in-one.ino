#include <SoftwareSerial.h>
#include <map>
#include <vector>
#include <string>


// Скорость передачи
#define USB_BAUD 115200
#define UART_BAUD 115200
#define softUART_BAUD 9600


  // Настройки UART
#define RX_PIN 6 //RS-232 от терминала
#define TX_PIN 7
#define RX2_PIN 20 //для связи с энергосчетчика
#define TX2_PIN 21

// Настройка портов
#define USB_PORT Serial //USB-порт для связи с ПК монитором порта
#define UART1_PORT Serial1 //RS-232 от терминала
  SoftwareSerial SerialE(RX2_PIN, TX2_PIN); //для связи с энергосчетчика

// Константы протокола связи VTK
const int KEEPALIVE_INTERVAL = 10000;   // Интервал keepalive в мс
const int OPERATION_NUMBER_LENGTH = 8;  // Длина номера операции
const int MAX_OPERATION_NUMBER = 65535;
const int MIN_MESSAGE_SIZE = 10;

// Специфичные константы протокола
const byte START_BYTE = 0x1F;                       // Стартовый байт
const byte PROTOCOL_DISCRIMINATOR_HIGH = 0x96;      // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_POS_HIGH = 0x97;  // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_LOW = 0xFB;       // Дискриминатор протокола (младшие биты)
const byte MESSAGE_ID_IDL = 0x01;                   // ID сообщения IDL

// Буфер для входящих данных
const int BUFFER_SIZE = 256;
uint8_t receiveBuffer[BUFFER_SIZE];
int bufferIndex = 0;
uint8_t receiveBuffer2[BUFFER_SIZE];  //буфер данных от энергосчетчика
int buffer2Index = 0;
bool stayIDLE = true;
int amountLength;

// Счетчик сообщений для отправки
int message_counter = 1;

// Глобальные переменные
volatile int operationNumber = 0;

const unsigned long PAYMENT_TIMEOUT = 8000;  // Таймаут в миллисекундах
bool paymentInProgress = false;
unsigned long paymentStartTime = 0;
int requestedAmount = 0;

// Функция получения текущего номера операции VTK
int getCurrentOperationNumber() {
  int currentNumber;

  noInterrupts();
  currentNumber = operationNumber;
  interrupts();

  return currentNumber;
}

// Функция для инкремента номера операции VTK
void incrementOperationNumber() {
  noInterrupts();
  operationNumber = (operationNumber < MAX_OPERATION_NUMBER) ? operationNumber + 1 : 0;
  interrupts();
}

// Функция расчета CRC-16 CCITT
uint16_t calculateCRC16(const uint8_t* data, uint16_t length) {
  static const uint16_t crc16_ccitt_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5Cf, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
  };

  uint16_t crc = 0xFFFF;
  for (uint16_t i = 0; i < length; i++) {
    uint16_t tmp = (crc >> 8) ^ data[i];
    crc = (crc << 8) ^ crc16_ccitt_table[tmp];
  }
  return crc;
}

// составление сообщения по протоколу VTK для Vendotek
byte* createVTKMessage(const std::string& messageName, int operationNumber, int& messageLength, const std::map<int, std::vector<byte>>& additionalParams = {}) {
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

  uint16_t crc = calculateCRC16(message, messageLength);
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

//создание сообщения IDL в упрощенном формате (как формируется в программе TestVTK)
byte* createIDLmessage(int operationNumber, int messageLength) {

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


// Функция для обычного платежа
void sendVRP(int amount) {
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
  incrementOperationNumber();
  byte* message = createVTKMessage("VRP", getCurrentOperationNumber(), messageLength, params);

  USB_PORT.print("Отправка платежа, сумма: ");
  USB_PORT.print(amount);
  USB_PORT.print(", длина: ");
  USB_PORT.println(messageLength + 2);

  sendMessage(message, messageLength + 2);

  delete[] message;
}

// Функция для возврата средств - не проверено
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

  byte* message = createVTKMessage("VRP", getCurrentOperationNumber(), messageLength, params);

  USB_PORT.print("Отправка возврата, сумма: ");
  USB_PORT.print(amount);
  USB_PORT.print(", операция: ");
  USB_PORT.print(operationNumber);
  USB_PORT.print(", длина: ");
  USB_PORT.println(messageLength + 2);

  sendMessage(message, messageLength + 2);

  delete[] message;
}

// Функция отправки IDL (упрощенного формата)
void sendIDL() {
  int operationNumber = getCurrentOperationNumber();
  int messageLength = 10;
  byte* message = createIDLmessage(operationNumber, messageLength);

  // Логирование отправки
  USB_PORT.print("Отправка IDL, операция: ");
  USB_PORT.println(operationNumber);

  // Отправка сообщения
  sendMessage(message, messageLength + 2);

  delete[] message;
}

//функция отправки DIS-сообщения - отмена операции, перевод в неактивное сотояние
void sendDIS() {
  int messageLength;
  byte* message = createVTKMessage("DIS", getCurrentOperationNumber(), messageLength);

  USB_PORT.print("Отправка DIS, длина: ");
  USB_PORT.println(messageLength + 2);

  sendMessage(message, messageLength + 2);

  delete[] message;
}

void sendMessage (byte* message, int messageLength){
    
  UART1_PORT.write(message, messageLength);

  USB_PORT.print("Отправлено: ");
  for (int i = 0; i < messageLength + 2; i++) {
    if (message[i] < 0x10) USB_PORT.print("0");
    USB_PORT.print(message[i], HEX);
    USB_PORT.print(" ");
  }
  USB_PORT.println("");
  USB_PORT.println("-   -   --   ---  ---- ----- ------ ------- ---------");
}

//парсинг сообщений в HEX-формате, полученных в команде dcdHEX через монитор порта
void dcodeHEX(const String& hexString) {
  // Очищаем строку от пробелов
  String cleanHex = hexString;  // Создаем копию
  cleanHex.replace(" ", "");    // Удаляем пробелы

  // Проверяем чётность длины
  if (cleanHex.length() % 2 != 0) {
    USB_PORT.println("Ошибка: нечётная длина HEX-строки");
    return;
  }

  // Преобразуем HEX в байты
  byte* data = new byte[cleanHex.length() / 2];  // Исправлено объявление массива
  int pos = 0;

  for (int i = 0; i < cleanHex.length(); i += 2) {
    String byteStr = cleanHex.substring(i, i + 2);
    data[pos++] = byte(strtol(byteStr.c_str(), NULL, 16));
  }

  // Копируем данные во временный буфер
  for (int i = 0; i < pos; i++) {
    receiveBuffer[i] = data[i];
  }

  bufferIndex = pos;  // Устанавливаем размер буфера

  // Вызываем обработку данных и выполняем парсинг
  processReceivedData();

  // Освобождаем память
  delete[] data;  // Используем delete[] для массива
}

//отправка сообщений в HEX-формате (вставлять в виде 02 03 0F AB D0, можно копировать из testVTK)
void sendHEX(const String& hexString) {
  // Создаем копию строки для модификации
  String workingString = hexString;
  String delimiters = " ";
  String subString;
  int pos = 0;

  // Массив для хранения байтов
  std::vector<byte> messageBytes;

  // Парсим строку и преобразуем HEX в байты
  while ((pos = workingString.indexOf(delimiters)) != -1) {
    subString = workingString.substring(0, pos);
    if (subString.length() == 2) {
      messageBytes.push_back(convertHexStringToByte(subString));
    }
    workingString = workingString.substring(pos + delimiters.length());
  }

  // Обрабатываем последний элемент
  if (workingString.length() == 2) {
    messageBytes.push_back(convertHexStringToByte(workingString));
  }

  // Проверяем корректность длины сообщения
  if (messageBytes.size() < 12) {
    USB_PORT.println("Ошибка: слишком короткое сообщение");
    return;
  }

  // Проверяем STX байт
  if (messageBytes[0] != 0x1F) {
    USB_PORT.println("Ошибка: неверный STX байт");
    return;
  }

  // Отправляем сообщение
  USB_PORT.print("Отправка HEX сообщения: ");
  USB_PORT.println(hexString);

  // Конвертируем вектор в массив
  byte* message = new byte[messageBytes.size()];
  for (size_t i = 0; i < messageBytes.size(); i++) {
    message[i] = messageBytes[i];
  }

  UART1_PORT.write(message, messageBytes.size());

  // Логирование
  USB_PORT.print("Отправлено: ");
  for (size_t i = 0; i < messageBytes.size(); i++) {
    if (messageBytes[i] < 0x10) USB_PORT.print("0");
    USB_PORT.print(messageBytes[i], HEX);
    USB_PORT.print(" ");
  }
  USB_PORT.println();

  delete[] message;
}

void calculateCRC(const String& hexString) {
  String cleanHex = hexString;  // Создаем копию
  cleanHex.replace(" ", "");    // Удаляем пробелы

  // Проверяем чётность длины
  if (cleanHex.length() % 2 != 0) {
    USB_PORT.println("Ошибка: нечётная длина HEX-строки");
    return;
  }

  // Правильно выделяем память для массива
  byte* data = new byte[cleanHex.length() / 2];  // Исправленный синтаксис
  int pos = 0;

  for (int i = 0; i < cleanHex.length(); i += 2) {
    String byteStr = cleanHex.substring(i, i + 2);
    data[pos++] = byte(strtol(byteStr.c_str(), NULL, 16));
  }

  // Рассчитываем CRC
  uint16_t crc = calculateCRC16(data, pos);

  // Выводим результат
  USB_PORT.print("Исходная HEX-строка: ");
  USB_PORT.println(hexString);
  USB_PORT.print("Рассчитанный CRC16: 0x");
  USB_PORT.print(crc >> 8, HEX);
  USB_PORT.print(" 0x");
  USB_PORT.println(crc & 0xFF, HEX);

  delete[] data;  // Не забываем освободить память
}


// Вспомогательная функция преобразования HEX строки в байт
byte convertHexStringToByte(const String& hexStr) {
  return (byte)(strtol(hexStr.c_str(), NULL, 16));
}

//очистить буфер
void clearBuffer() {
  // Очищаем буфер приема
  memset(receiveBuffer, 0, BUFFER_SIZE);

  // Сбрасываем индекс буфера
  bufferIndex = 0;

  // Дополнительно можно добавить:
  USB_PORT.println("Буфер очищен");
  USB_PORT.println("-----------------------------------------------");
}


// Обработка успешного платежа
void handleSuccessfulPayment() {
    USB_PORT.println("Оплата успешно проведена");
    paymentInProgress = false;
    requestedAmount = 0;
}

// Обработка ошибки платежа
void handleFailedPayment() {
    USB_PORT.println("Ошибка при проведении оплаты");
    paymentInProgress = false;
    requestedAmount = 0;
}

// Обработка таймаута
void handlePaymentTimeout() {
    USB_PORT.println("Превышено время ожидания оплаты");
    paymentInProgress = false;
    requestedAmount = 0;
}


// Функция проверки статуса оплаты
void checkPaymentStatus() {
    if (!paymentInProgress) return;
    
    // Проверяем таймаут
    if (millis() - paymentStartTime > PAYMENT_TIMEOUT) {
        handlePaymentTimeout();
        return;
    }
    
    // Проверяем минимальную длину буфера
    if (bufferIndex < MIN_MESSAGE_SIZE) return;
    
    // Проверяем стартовый байт
    if (receiveBuffer[0] != START_BYTE) return;
    
    // Парсим имя сообщения
    char messageName[4];
    for (int i = 0; i < receiveBuffer[6]; i++) {
        messageName[i] = receiveBuffer[7 + i];
    }
    messageName[receiveBuffer[6]] = '\0';
    
    // Проверяем, что это ответ VRP
    if (strcmp(messageName, "VRP") != 0) return;
    
    // Проверяем протокол
    if (receiveBuffer[3] != PROTOCOL_DISCRIMINATOR_POS_HIGH || 
        receiveBuffer[4] != PROTOCOL_DISCRIMINATOR_LOW) {
        return;
    }
    
    // Проверяем CRC
    uint16_t receivedCRC = (receiveBuffer[bufferIndex-2] << 8) | receiveBuffer[bufferIndex-1];
    uint16_t calculatedCRC = calculateCRC16(receiveBuffer, bufferIndex-2);
    if (receivedCRC != calculatedCRC) return;
    
    // Парсим сумму из ответа
    int payloadStart = 7 + receiveBuffer[6];
    int amount = 0;
    if (receiveBuffer[payloadStart] == 0x04) {
        amountLength = receiveBuffer[payloadStart + 1];
        String amountStr = "";
        for (int i = 0; i < amountLength; i++) {
            amountStr += char(receiveBuffer[payloadStart + 2 + i]);
        }
        amount = amountStr.toInt();
    }
    
    // Проверяем статус операции
    if (receiveBuffer[payloadStart + amountLength + 3] == 0x05) {
        int statusLength = receiveBuffer[payloadStart + amountLength + 4];
        byte status = receiveBuffer[payloadStart + amountLength + 5];
        
        if (status == 0x00 && amount == requestedAmount) {
            // Успешная операция с правильной суммой
            handleSuccessfulPayment();
        } else if (status == 0x00 && amount == 0) {
            // Платеж не выполнен (сумма 0)
            USB_PORT.println("Ошибка: платеж не выполнен (сумма = 0)");
            handleFailedPayment();
        } else if (status >= 0x01 && status <= 0xFF) {
            // Ошибка с кодом
            USB_PORT.print("Ошибка платежа, код: 0x");
            USB_PORT.println(status, HEX);
            handleFailedPayment();
        } else {
            USB_PORT.println("Неизвестный статус операции");
            handleFailedPayment();
        }
    }
}


// Функция начала процесса оплаты
void startPayment(int amount) {
    if (paymentInProgress) {
        USB_PORT.println("Ошибка: уже идет процесс оплаты");
        return;
    }
    
    if (amount <= 0 || amount > 1000000) {
        USB_PORT.println("Ошибка: неверная сумма оплаты");
        return;
    }
    
    requestedAmount = amount;
    paymentInProgress = true;
    paymentStartTime = millis();
    
    USB_PORT.print("Начата оплата на сумму: ");
    USB_PORT.print(amount / 100);
    USB_PORT.println(" руб.");
    
    // Отправляем терминал в режим приема оплаты
    sendVRP(amount);
}


void processReceivedData() {
  if (bufferIndex == 0) return;

  // Формируем строку с принятыми байтами
  String receivedBytes = "Принятые байты: ";
  for (int i = 0; i < bufferIndex; i++) {
    if (receiveBuffer[i] < 0x10) receivedBytes += "0";
    receivedBytes += String(receiveBuffer[i], HEX);
    receivedBytes += " ";
  }
  USB_PORT.println(receivedBytes);

  // Проверяем минимальную длину сообщения
  if (bufferIndex < MIN_MESSAGE_SIZE) {
    USB_PORT.println("Ошибка: сообщение слишком короткое");
    clearBuffer();
    return;
  }

  // Проверяем стартовый байт
  if (receiveBuffer[0] != START_BYTE) {
    USB_PORT.println("Ошибка: неверный стартовый байт");
    clearBuffer();
    return;
  }

  // Извлекаем поля сообщения
  byte startByte = receiveBuffer[0];
  byte reserved = receiveBuffer[1];
  byte messageLength = receiveBuffer[2];
  byte protocolHigh = receiveBuffer[3];
  byte protocolLow = receiveBuffer[4];
  byte messageID = receiveBuffer[5];
  byte nameLength = receiveBuffer[6];

  // Парсим имя сообщения
  char messageName[4];
  for (int i = 0; i < nameLength; i++) {
    messageName[i] = receiveBuffer[7 + i];
  }
  messageName[nameLength] = '\0';


  // // Добавляем проверку на ответ терминала
  // if (messageName[0] == 'V' && messageName[1] == 'R' && messageName[2] == 'P') {
  //     // Проверяем статус операции
  //     if () {
  //         handleSuccessfulPayment();
  //     } else {
  //         handleFailedPayment();
  //     }
  // }



  // Проверяем протокол
  if (protocolHigh != PROTOCOL_DISCRIMINATOR_POS_HIGH || protocolLow != PROTOCOL_DISCRIMINATOR_LOW) {
    USB_PORT.println("Ошибка: неверный протокол");
  }

  // Проверяем CRC16
  uint16_t receivedCRC = (receiveBuffer[bufferIndex - 2] << 8) | receiveBuffer[bufferIndex - 1];
  uint16_t calculatedCRC = calculateCRC16(receiveBuffer, bufferIndex - 2);

  if (receivedCRC != calculatedCRC) {
    USB_PORT.println("Ошибка: неверная CRC16");
    clearBuffer();
    return;
  }

  // Парсим номер операции (тег 0x03)
  int opLength = 0;
  //    int operationNumber = 0;
  int payloadStart = 7 + nameLength;
  if (payloadStart < bufferIndex - 2 && receiveBuffer[payloadStart] == 0x03) {
    opLength = receiveBuffer[payloadStart + 1];
    String opStr = "";
    for (int i = 0; i < opLength; i++) {
      opStr += char(receiveBuffer[payloadStart + 2 + i]);
    }
    operationNumber = opStr.toInt();
  }

  // Парсим сумму оплаты (тег 0x04)
  int amount = 0;
  int amountPos = payloadStart + 2 + opLength;
  if (amountPos < bufferIndex - 2 && receiveBuffer[amountPos] == 0x04) {
    int amountLength = receiveBuffer[amountPos + 1];
    String amountStr = "";
    for (int i = 0; i < amountLength; i++) {
      amountStr += char(receiveBuffer[amountPos + 2 + i]);
    }
    amount = amountStr.toInt();
  }

  // Логируем результаты парсинга
  USB_PORT.print("Парсинг сообщения:\n");
  USB_PORT.print("Стартовый байт: 0x");
  USB_PORT.println(startByte, HEX);
  USB_PORT.print("Зарезервировано: 0x");
  USB_PORT.println(reserved, HEX);
  USB_PORT.print("Длина сообщения: ");
  USB_PORT.println(messageLength);
  USB_PORT.print("Протокол (H): 0x");
  USB_PORT.println(protocolHigh, HEX);
  USB_PORT.print("Протокол (L): 0x");
  USB_PORT.println(protocolLow, HEX);
  USB_PORT.print("ID сообщения: 0x");
  USB_PORT.println(messageID, HEX);
  USB_PORT.print("Длина имени: ");
  USB_PORT.println(nameLength);
  USB_PORT.print("Имя сообщения: ");
  USB_PORT.println(messageName);
  USB_PORT.print("Сумма оплаты: ");
  USB_PORT.print(amount / 100);
  USB_PORT.print("руб. ");
  USB_PORT.print(amount - ((amount / 100) * 100));
  USB_PORT.println("коп.");
  USB_PORT.print("номер операции: ");
  USB_PORT.println(operationNumber);
  USB_PORT.print("CRC проверен: 0x");
  USB_PORT.println(receivedCRC, HEX);

  // Обработка полезной нагрузки
  payloadStart = 7 + nameLength;
  int payloadLength = bufferIndex - 2 - (7 + nameLength);

  USB_PORT.print("Полезная нагрузка: ");
  for (int i = 0; i < payloadLength; i++) {
    USB_PORT.print(receiveBuffer[payloadStart + i], HEX);
    USB_PORT.print(" ");
  }
  USB_PORT.println();

  // Очищаем буфер после обработки
  clearBuffer();
}

void setup() {
  USB_PORT.begin(USB_BAUD); //настройка порта USB - для монитора порта
  UART1_PORT.begin(UART_BAUD, SERIAL_8N1, RX_PIN, TX_PIN); //настройка порта UART1 - для Vendotek
  SerialE.begin(softUART_BAUD);
 
  while (!USB_PORT) {
    delay(100);
  }

  USB_PORT.println("Система запущена");
}

void loop() {
  static unsigned long lastRXTime = 0;
  static unsigned long lastRX2Time = 0;
  static unsigned long lastSendTime = millis();


  // Прием данных
  while (UART1_PORT.available() > 0) {
    if (bufferIndex < BUFFER_SIZE) {
      receiveBuffer[bufferIndex++] = UART1_PORT.read();
      lastRXTime = millis();
    } else {
      USB_PORT.println("Ошибка: буфер переполнен!");
      bufferIndex = 0;
    }
  }


  while (SerialE.available() > 0) {
    if (buffer2Index < BUFFER_SIZE) {
      receiveBuffer2[buffer2Index++] = SerialE.read();
      lastRX2Time = millis();
    } else {
      USB_PORT.println("Ошибка: буфер #2 переполнен!");
      buffer2Index = 0;
    }
  }

  // Обработка накопленных данных
  if (bufferIndex > 0 && millis() - lastRXTime > 100) {
    processReceivedData();
    bufferIndex = 0;
  }

  // // Обработка накопленных данных
  // if (buffer2Index > 0 && millis() - lastRX2Time > 500) {
  //   processReceived2Data();
  //   buffer2Index = 0;
  // }


  // Таймер для отправки IDL
  const unsigned long IDL_INTERVAL = 3000; // интервал в 10 секунд

  // Периодическая отправка
  if ((stayIDLE)&&(millis() - lastSendTime > IDL_INTERVAL)) {
      USB_PORT.print("периодическая отправка: сообщение №");
      USB_PORT.println(message_counter);
      sendIDL();
      lastSendTime = millis();
      message_counter = (message_counter < 99999) ? message_counter + 1 : 1;
  }

  // Добавляем проверку статуса оплаты
  checkPaymentStatus();

  // Обработка команд
  if (USB_PORT.available() > 0) {
    String command = USB_PORT.readStringUntil('\n');
    command.trim();

    if (command == "IDLE") {
      stayIDLE = !stayIDLE;
          USB_PORT.print("режим stay-IDLE is ");
          USB_PORT.println(stayIDLE);
    } else if (command == "IDL") {
      sendIDL();
    } else if (command == "DIS") {
      sendDIS();
    } else if (command.startsWith("VRP")) {
      // Обработка обычной оплаты
      int spacePos = command.indexOf(' ');
      if (spacePos != -1) {
        String amountStr = command.substring(spacePos + 1);
        int amount = amountStr.toInt();
        if (amount > 0 && amount <= 1000000) {
          sendVRP(amount);
        } else {
          USB_PORT.print("Ошибка: диапазон платежа (1-1000000 коп.) ");
          USB_PORT.println(amount);
        }
      }
    } else if (command.startsWith("PAY")) {
          // Обработка команды оплаты
          int spacePos = command.indexOf(' ');
          if (spacePos != -1) {
              String amountStr = command.substring(spacePos + 1);
              int amount = amountStr.toInt();
              startPayment(amount);
          }
    } else if (command.startsWith("REFUND")) {
      // Обработка возврата
      int spacePos = command.indexOf(' ');
      if (spacePos != -1) {
        String params = command.substring(spacePos + 1);
        int secondSpace = params.indexOf(' ');
        if (secondSpace != -1) {
          int amount = params.substring(0, secondSpace).toInt();
          int operationNumber = params.substring(secondSpace + 1).toInt();

          if (amount > 0 && amount <= 1000000 && operationNumber > 0) {
            sendREFUND(amount, operationNumber);
          } else {
            USB_PORT.println("Ошибка: неверные параметры возврата");
          }
        }
      }
    } else if (command.startsWith("HEX")) {
      String hexString = command.substring(8);
      sendHEX(hexString);
    } else if (command.startsWith("dcdHEX")) {
      // Извлекаем HEX-строку из команды
      String hexString = command.substring(10);
      // Проверяем, что строка не пустая
      if (hexString.length() > 0) {
        // Парсим сообщение
        dcodeHEX(hexString);
      } else {
        USB_PORT.println("Ошибка: пустая HEX-строка");
      }
    } else if (command.startsWith("CRC")) {
      String hexString = command.substring(8);
      if (hexString.length() > 0) {
        calculateCRC(hexString);
      } else {
        USB_PORT.println("Ошибка: пустая HEX-строка");
      }
    } else {
      USB_PORT.println("Неизвестная команда");
    }
  }
}


// Глобальные переменные для хранения данных
float voltage = 0.0; // Напряжение
float current = 0.0; // Ток
float power = 0.0; // Мощность
float energyTotal = 0.0; // Общее потребление энергии

void processReceived2Data() {
    static const int BUFFER_SIZE = 256;
    static byte buffer[BUFFER_SIZE];
    static int bufferIndex = 0;
    
    while (SerialE.available() > 0) {
        char incomingByte = SerialE.read();
        
        // Собираем пакет данных
        if (bufferIndex < BUFFER_SIZE) {
            buffer[bufferIndex++] = incomingByte;
        }
        
        // Проверяем маркер конца пакета (например, 0x0D)
        if (incomingByte == 0x0D) {
            // Проверяем контрольную сумму пакета
            if (checkCRC(buffer, bufferIndex)) {
                // Парсим данные из буфера
                parseEnergyData(buffer, bufferIndex);
            }
            
            // Сбрасываем буфер
            bufferIndex = 0;
        }
    }
}

bool checkCRC(const uint8_t* data, int length) {
    // Рассчитываем CRC для полученных данных
    uint16_t receivedCRC = calculateCRC16(data, length);
    
    // Извлекаем CRC из конца сообщения
    uint16_t expectedCRC = (data[length - 2] << 8) | data[length - 1];
    
    // Сравниваем рассчитанную и полученную CRC
    return receivedCRC == expectedCRC;
}

void parseEnergyData(byte* data, int length) {
    // Пример парсинга данных в формате BCD
    // Предполагаем, что данные приходят в формате BCD
    voltage = bcd2dec(data[2], data[3]) * 0.1; // Пример преобразования BCD в float
    current = bcd2dec(data[4], data[5]) * 0.01;
    power = bcd2dec(data[6], data[7]) * 0.1;
    energyTotal = bcd2dec(data[8], data[9], data[10], data[11]) * 0.001;
    
    // Вывод отладочной информации
    Serial.println("------------------------");
    Serial.print("Напряжение: "); Serial.print(voltage); Serial.println(" В");
    Serial.print("Ток: "); Serial.print(current); Serial.println(" А");
    Serial.print("Мощность: "); Serial.print(power); Serial.println(" Вт");
    Serial.print("Энергия: "); Serial.print(energyTotal); Serial.println(" кВт·ч");
}

// Функция преобразования BCD в десятичное число
int bcd2dec(byte bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}

int bcd2dec(byte high, byte low) {
    return (bcd2dec(high) * 100) + bcd2dec(low);
}

int bcd2dec(byte b1, byte b2, byte b3, byte b4) {
    return (bcd2dec(b1) * 1000000) + 
           (bcd2dec(b2) * 10000) + 
           (bcd2dec(b3) * 100) + 
           bcd2dec(b4);
}


// новая активность: пользователь может с помощью сенсорного экрана выбрать оплату на определенную сумму и тогда терминал пришлет сообщение об этом с указанной суммой.
// Нужно тогда определить значение этой суммы и отправить на терминал VRP с этой суммой оплаты.


// Переменная для хранения суммы от сенсорного экрана
int touchScreenAmount = 0;

    // Обработка данных от сенсорного экрана
    // Здесь нужно добавить конкретную реализацию получения данных
    // от вашего сенсорного контроллера
    // Пример условной проверки:
    // if (/* условие получения данных от сенсора */) {
    //     // Получаем сумму от сенсорного экрана
    //     touchScreenAmount = /* получить сумму из данных сенсора */;
        
    //     if (touchScreenAmount > 0 && touchScreenAmount <= 1000000) {
    //         sendVRP(touchScreenAmount);
    //         touchScreenAmount = 0; // сброс после отправки
    //     } 
    //     else {
    //         USB_PORT.println("Ошибка: неверная сумма оплаты");
