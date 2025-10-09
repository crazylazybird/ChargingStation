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

SoftwareSerial SOFTSERIAL_ENERGY_PORT(RX2_PIN, TX2_PIN); //для связи с энергосчетчика



void UART_Setup(){
    UART0_DEBUG_PORT.begin(UART0_DEBUG_PORT_BAUDRATE);
    UART1_VMC_PORT.begin(UART1_VMC_PORT_BAUDRATE, SERIAL_8N1, UART1_VMC_PORT_RX_PIN, UART1_VMC_PORT_TX_PIN); //настройка порта UART1 - для Vendotek
    SOFTSERIAL_ENERGY_PORT.begin(9600);
    SOFTSERIAL_ENERGY_PORT.print("R OFF");

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

void UART_POS_received_data(){
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
      process_POS_received_data();
      bufferIndex = 0;
  }
}

/*
----------------------------------------------------------------
----------------------------------------------------------------
*/



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

// ---------- ОБРАБОТКА ----------
void process_received_energy_data() {
    static byte buffer[BUFFER_SIZE];
    static int bufferIndex = 0;
    static bool inPacket = false;

    while (SOFTSERIAL_ENERGY_PORT.available() > 0) {
        byte incomingByte = SOFTSERIAL_ENERGY_PORT.read();

        if (!inPacket) {
            // ждём маркер начала 0xAA 0x55
            if (bufferIndex == 0 && incomingByte == 0xAA) {
                buffer[bufferIndex++] = incomingByte;
            } else if (bufferIndex == 1 && incomingByte == 0x55) {
                buffer[bufferIndex++] = incomingByte;
                inPacket = true;
            } else {
                bufferIndex = 0; // сброс, если начало не совпало
            }
        } else {
            // внутри пакета
            if (bufferIndex < BUFFER_SIZE) {
                buffer[bufferIndex++] = incomingByte;
            }

            if (incomingByte == 0x0D) { // конец пакета
                if (check_CRC(buffer, bufferIndex)) {
                    parse_energy_data(buffer, bufferIndex);
                } else {
                    UART0_DEBUG_PORT.println("Ошибка CRC!");
                }
                bufferIndex = 0;
                inPacket = false;
            }
        }
    }
    
}

#define STX1 0xAA
#define STX2 0x55
#define RX_MAX 128         // максимум полезной нагрузки

void softserial_energy_port_send_command(const String& cmd) {
  const uint16_t len = cmd.length();
  if (len == 0 || len > RX_MAX) return;

  // CRC по полезной нагрузке
  uint16_t crc = calculate_CRC16((const uint8_t*)cmd.c_str(), len);

  SOFTSERIAL_ENERGY_PORT.write(STX1);
  SOFTSERIAL_ENERGY_PORT.write(STX2);
  SOFTSERIAL_ENERGY_PORT.write((len >> 8) & 0xFF);
  SOFTSERIAL_ENERGY_PORT.write(len & 0xFF);
  SOFTSERIAL_ENERGY_PORT.print(cmd);                // PAYLOAD
  SOFTSERIAL_ENERGY_PORT.write((crc >> 8) & 0xFF);  // CRC_H
  SOFTSERIAL_ENERGY_PORT.write(crc & 0xFF);         // CRC_L
}


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






