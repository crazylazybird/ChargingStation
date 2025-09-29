#include "main.h"



volatile int operationNumber = 0;
uint8_t receiveBuffer[BUFFER_SIZE];
bool stayIDLE = true;


void UART_Setup(){
    UART0_DEBUG_PORT.begin(UART0_DEBUG_PORT_BAUDRATE);
    UART1_POS_PORT.begin(UART1_POS_PORT_BAUDRATE, SERIAL_8N1, UART1_POS_PORT_RX_PIN, UART1_POS_PORT_TX_PIN); //настройка порта UART1 - для Vendotek

    while (!UART0_DEBUG_PORT || !UART1_POS_PORT) {
        UART0_DEBUG_PORT.println("Порты не инициализируются");
        delay(1000);
    }

    UART0_DEBUG_PORT.println("Система запущена");
}


void UART_Commands_processing(){
    if (UART0_DEBUG_PORT.available() > 0) {
    String command = UART0_DEBUG_PORT.readStringUntil('\n');
    command.trim();

    if (command == "IDLE") {
      stayIDLE = !stayIDLE;
          UART0_DEBUG_PORT.print("режим stay-IDLE is ");
          UART0_DEBUG_PORT.println(stayIDLE);
    } else if (command == "IDL") {
      sendIDL();
    } else if (command == "DIS") {
      //sendDIS();
    } else if (command.startsWith("VRP")) {
      // Обработка обычной оплаты
      int spacePos = command.indexOf(' ');
      if (spacePos != -1) {
        String amountStr = command.substring(spacePos + 1);
        int amount = amountStr.toInt();
        if (amount > 0 && amount <= 1000000) {
          //sendVRP(amount);
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
              //startPayment(amount);
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
      sendHEX(hexString);
    } else if (command.startsWith("dcdHEX")) {
      // Извлекаем HEX-строку из команды
      String hexString = command.substring(10);
      // Проверяем, что строка не пустая
      if (hexString.length() > 0) {
        // Парсим сообщение
        dcodeHEX(hexString);
      } else {
        UART0_DEBUG_PORT.println("Ошибка: пустая HEX-строка");
      }
    } else if (command.startsWith("CRC")) {
      String hexString = command.substring(8);
      if (hexString.length() > 0) {
        calculateCRC(hexString);
      } else {
        UART0_DEBUG_PORT.println("Ошибка: пустая HEX-строка");
      }
    } else {
      UART0_DEBUG_PORT.println("Неизвестная команда");
    }
  }
}

void sendHEX(const String& hexString) {
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
      (hexCharToByte(high) << 4) | hexCharToByte(low);
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
  
  UART1_POS_PORT.write(messageBuffer, bufferIndex);
  
  UART0_DEBUG_PORT.print("Отправлено: ");
  for (size_t i = 0; i < bufferIndex; i++) {
    if (messageBuffer[i] < 0x10) UART0_DEBUG_PORT.print("0");
    UART0_DEBUG_PORT.print(messageBuffer[i], HEX);
    UART0_DEBUG_PORT.print(" ");
  }
  UART0_DEBUG_PORT.println();
}



void dcodeHEX(const String& hexString) {
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
      (hexCharToByte(high) << 4) | hexCharToByte(low);
    
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



void sendMessage(byte* message, int messageLength){
    
  UART1_POS_PORT.write(message, messageLength);

  UART0_DEBUG_PORT.print("Отправлено: ");
  for (int i = 0; i < messageLength; i++) {
    if (message[i] < 0x10) UART0_DEBUG_PORT.print("0");
    UART0_DEBUG_PORT.print(message[i], HEX);
    UART0_DEBUG_PORT.print(" ");
  }
  UART0_DEBUG_PORT.println("");
  UART0_DEBUG_PORT.println("-   -   --   ---  ---- ----- ------ ------- ---------");
}

