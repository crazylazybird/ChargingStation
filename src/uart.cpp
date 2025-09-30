#include "main.h"



volatile int operationNumber = 0;
uint8_t receiveBuffer[BUFFER_SIZE];
int bufferIndex = 0;


bool stayIDLE = false; // Флаг периодической отправки сообщения IDL



static unsigned long lastRXTime = 0;
static unsigned long lastRX2Time = 0;
static unsigned long lastSendTime = millis();

const byte PROTOCOL_DISCRIMINATOR_POS_HIGH = 0x97;
const byte PROTOCOL_DISCRIMINATOR_LOW = 0xFB;

const int MIN_MESSAGE_SIZE = 10;
const byte START_BYTE = 0x1F;                       // Стартовый байт


const unsigned long IDL_INTERVAL = 3000; // Интервал в 10 секунд для IDLE
int message_counter = 1;                 // Счетсчик сообщений

void UART_Setup(){
    UART0_DEBUG_PORT.begin(UART0_DEBUG_PORT_BAUDRATE);
    UART1_VMC_PORT.begin(UART1_VMC_PORT_BAUDRATE, SERIAL_8N1, UART1_VMC_PORT_RX_PIN, UART1_VMC_PORT_TX_PIN); //настройка порта UART1 - для Vendotek
    

    while (!UART0_DEBUG_PORT || !UART1_VMC_PORT) {
        UART0_DEBUG_PORT.println("Порты не инициализируются");
        delay(1000);
    }

    UART0_DEBUG_PORT.println("Система запущена");
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void UART_VMT_recieved_data(){
    while (UART1_VMC_PORT.available() > 0) {
    if (bufferIndex < BUFFER_SIZE) {
      receiveBuffer[bufferIndex++] = UART1_VMC_PORT.read();
      lastRXTime = millis();
    } else {
      UART0_DEBUG_PORT.println("Ошибка: буфер переполнен!");
      bufferIndex = 0;
    }
  }

    if (bufferIndex > 0 && ((millis() - lastRXTime) > 100)) {
    process_received_data();
    bufferIndex = 0;
  }
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void process_received_data() {
  if (bufferIndex == 0) return;

  // Формируем строку с принятыми байтами
  String receivedBytes = "Принятые байты: ";
  for (int i = 0; i < bufferIndex; i++) {
    if (receiveBuffer[i] < 0x10) receivedBytes += "0";
    receivedBytes += String(receiveBuffer[i], HEX);
    receivedBytes += " ";
  }
  UART0_DEBUG_PORT.println(receivedBytes);

  // Проверяем минимальную длину сообщения
  if (bufferIndex < MIN_MESSAGE_SIZE) {
    UART0_DEBUG_PORT.println("Ошибка: сообщение слишком короткое");
    clear_buffer();
    return;
  }

  // Проверяем стартовый байт
  if (receiveBuffer[0] != START_BYTE) {
    UART0_DEBUG_PORT.println("Ошибка: неверный стартовый байт");
    clear_buffer();
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
    UART0_DEBUG_PORT.println("Ошибка: неверный протокол");
  }

  // Проверяем CRC16
  uint16_t receivedCRC = (receiveBuffer[bufferIndex - 2] << 8) | receiveBuffer[bufferIndex - 1];
  uint16_t calculatedCRC = calculate_CRC16(receiveBuffer, bufferIndex - 2);

  if (receivedCRC != calculatedCRC) {
    UART0_DEBUG_PORT.println("Ошибка: неверная CRC16");
    clear_buffer();
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
  UART0_DEBUG_PORT.print("Парсинг сообщения:\n");
  UART0_DEBUG_PORT.print("Стартовый байт: 0x");
  UART0_DEBUG_PORT.println(startByte, HEX);
  UART0_DEBUG_PORT.print("Зарезервировано: 0x");
  UART0_DEBUG_PORT.println(reserved, HEX);
  UART0_DEBUG_PORT.print("Длина сообщения: ");
  UART0_DEBUG_PORT.println(messageLength);
  UART0_DEBUG_PORT.print("Протокол (H): 0x");
  UART0_DEBUG_PORT.println(protocolHigh, HEX);
  UART0_DEBUG_PORT.print("Протокол (L): 0x");
  UART0_DEBUG_PORT.println(protocolLow, HEX);
  UART0_DEBUG_PORT.print("ID сообщения: 0x");
  UART0_DEBUG_PORT.println(messageID, HEX);
  UART0_DEBUG_PORT.print("Длина имени: ");
  UART0_DEBUG_PORT.println(nameLength);
  UART0_DEBUG_PORT.print("Имя сообщения: ");
  UART0_DEBUG_PORT.println(messageName);
  UART0_DEBUG_PORT.print("Сумма оплаты: ");
  UART0_DEBUG_PORT.print(amount / 100);
  UART0_DEBUG_PORT.print("руб. ");
  UART0_DEBUG_PORT.print(amount - ((amount / 100) * 100));
  UART0_DEBUG_PORT.println("коп.");
  UART0_DEBUG_PORT.print("номер операции: ");
  UART0_DEBUG_PORT.println(operationNumber);
  UART0_DEBUG_PORT.print("CRC проверен: 0x");
  UART0_DEBUG_PORT.println(receivedCRC, HEX);

  // Обработка полезной нагрузки
  payloadStart = 7 + nameLength;
  int payloadLength = bufferIndex - 2 - (7 + nameLength);

  UART0_DEBUG_PORT.print("Полезная нагрузка: ");
  for (int i = 0; i < payloadLength; i++) {
    UART0_DEBUG_PORT.print(receiveBuffer[payloadStart + i], HEX);
    UART0_DEBUG_PORT.print(" ");
  }
  UART0_DEBUG_PORT.println();
  clear_buffer();
  // Очищаем буфер после обработки
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void UART_Commands_processing(){
    if (UART0_DEBUG_PORT.available() > 0) {
    String command = UART0_DEBUG_PORT.readStringUntil('\n');
    command.trim();

    if (command == "IDLE") {
      stayIDLE = !stayIDLE;
          UART0_DEBUG_PORT.print("режим stay-IDLE is ");
          UART0_DEBUG_PORT.println(stayIDLE);
    } else if (command == "IDL") {
      send_IDL();
    } else if (command == "DIS") {
      send_DIS();
    } else if (command.startsWith("VRP")) {
      // Обработка обычной оплаты
      int spacePos = command.indexOf(' ');
      if (spacePos != -1) {
        String amountStr = command.substring(spacePos + 1);
        int amount = amountStr.toInt();
        if (amount > 0 && amount <= 1000000) {
          send_VRP(amount);
        } else {
          UART0_DEBUG_PORT.print("Ошибка: диапазон платежа (1-1000000 коп.) ");
          UART0_DEBUG_PORT.println(amount);
        }
      }
    } else if (command.startsWith("PAY")) {
          // Обработка команды оплаты
          int spacePos = command.indexOf(' ');
          if (spacePos != -1) {
              String amountStr = command.substring(spacePos + 1);
              int amount = amountStr.toInt();
              start_payment(amount);
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
            //sendREFUND(amount, operationNumber);
          } else {
            UART0_DEBUG_PORT.println("Ошибка: неверные параметры возврата");
          }
        }
      }
    } else if (command.startsWith("HEX")) {
      String hexString = command.substring(8);
      send_HEX(hexString);
    } else if (command.startsWith("dcdHEX")) {
      // Извлекаем HEX-строку из команды
      String hexString = command.substring(10);
      // Проверяем, что строка не пустая
      if (hexString.length() > 0) {
        // Парсим сообщение
        decode_HEX(hexString);
      } else {
        UART0_DEBUG_PORT.println("Ошибка: пустая HEX-строка");
      }
    } else if (command.startsWith("CRC")) {
      String hexString = command.substring(8);
      if (hexString.length() > 0) {
        calculate_CRC(hexString);
      } else {
        UART0_DEBUG_PORT.println("Ошибка: пустая HEX-строка");
      }
    } else {
      UART0_DEBUG_PORT.println("Неизвестная команда");
    }
  }
}
/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void send_IDLE(){
    if ((stayIDLE)&&(millis() - lastSendTime > IDL_INTERVAL)) {
      UART0_DEBUG_PORT.print("Периодическая отправка: сообщение №");
      UART0_DEBUG_PORT.println(message_counter);
      send_IDL();
      lastSendTime = millis();
      message_counter = (message_counter < 99999) ? message_counter + 1 : 1;
  }
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void send_message(byte* message, int messageLength){
    
  UART1_VMC_PORT.write(message, messageLength);

  UART0_DEBUG_PORT.print("Отправлено: ");
  for (int i = 0; i < messageLength; i++) {
    if (message[i] < 0x10) UART0_DEBUG_PORT.print("0");
    UART0_DEBUG_PORT.print(message[i], HEX);
    UART0_DEBUG_PORT.print(" ");
  }
  UART0_DEBUG_PORT.println("");
  UART0_DEBUG_PORT.println("-   -   --   ---  ---- ----- ------ ------- ---------");
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void send_HEX(const String& hexString) {
  static byte messageBuffer[256]; // Статический для повторного использования
  size_t bufferIndex = 0;
  
  const char* ptr = hexString.c_str();
  size_t length = hexString.length();
  
  // Быстрый парсинг HEX
  for (size_t i = 0; i < length && bufferIndex < sizeof(messageBuffer); ) {
    // Пропускаем не-HEX символы
    while (i < length && !isxdigit(ptr[i])) i++;
    
    if (i + 1 >= length) break;
    
    // Преобразуем HEX
    char high = ptr[i++];
    char low = ptr[i++];
    
    messageBuffer[bufferIndex++] = 
      (hex_char_to_byte(high) << 4) | hex_char_to_byte(low);
  }
  
  // Проверки
  if (bufferIndex < 5) { // Минимальная длина VTK сообщения
    UART0_DEBUG_PORT.println("Ошибка: слишком короткое сообщение");
    return;
  }
  
  if (messageBuffer[0] != 0x1F) {
    UART0_DEBUG_PORT.println("Ошибка: неверный STX байт");
    return;
  }
  
  // Дополнительная проверка длины из заголовка
  if (bufferIndex >= 3) {
    uint16_t declaredLength = (messageBuffer[1] << 8) | messageBuffer[2];
    if (bufferIndex != declaredLength + 5) { // +3 заголовок +2 CRC
      UART0_DEBUG_PORT.println("Предупреждение: несоответствие длины");
    }
  }
  
  // Отправка и логирование
  UART0_DEBUG_PORT.print("Отправка HEX: ");
  UART0_DEBUG_PORT.println(hexString);
  
  UART1_VMC_PORT.write(messageBuffer, bufferIndex);
  
  UART0_DEBUG_PORT.print("Отправлено: ");
  for (size_t i = 0; i < bufferIndex; i++) {
    if (messageBuffer[i] < 0x10) UART0_DEBUG_PORT.print("0");
    UART0_DEBUG_PORT.print(messageBuffer[i], HEX);
    UART0_DEBUG_PORT.print(" ");
  }
  UART0_DEBUG_PORT.println();
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/

void decode_HEX(const String& hexString) {
  const char* ptr = hexString.c_str();
  size_t length = hexString.length();
  
  int bufferIndex = 0;
  
  // Парсим HEX напрямую в receiveBuffer
  for (size_t i = 0; i < length && bufferIndex < BUFFER_SIZE; ) {
    // Пропускаем не-HEX символы
    while (i < length && !isxdigit(ptr[i])) i++;
    
    // Проверяем, что есть два HEX символа
    if (i + 1 >= length) {
      if (bufferIndex % 2 != 0) {
        UART0_DEBUG_PORT.println("Ошибка: нечётная длина HEX-строки");
        bufferIndex = 0;
        return;
      }
      break;
    }
    
    // Быстрое преобразование HEX в байт
    char high = ptr[i];
    char low = ptr[i + 1];
    
    receiveBuffer[bufferIndex++] = 
      (hex_char_to_byte(high) << 4) | hex_char_to_byte(low);
    
    i += 2;
  }
  
  // Проверяем минимальную длину сообщения
  if (bufferIndex < 5) {
    UART0_DEBUG_PORT.println("Ошибка: слишком короткое сообщение");
    bufferIndex = 0;
    return;
  }
  
  // Вызываем обработку
  //processReceivedData();
}






