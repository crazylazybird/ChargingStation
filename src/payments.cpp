#include "main.h"

const unsigned long PAYMENT_TIMEOUT = 800000;  // Таймаут в миллисекундах


extern volatile int operationNumber;
extern uint8_t receiveBuffer[BUFFER_SIZE];
extern int bufferIndex;


extern float power; // Мощность
extern float energyTotal; // Общее потребление энергии

const byte START_BYTE = 0x1F;                       // Стартовый байт
const byte PROTOCOL_DISCRIMINATOR_HIGH = 0x96;      // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_POS_HIGH = 0x97;  // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_LOW = 0xFB;       // Дискриминатор протокола (младшие биты)
const byte MESSAGE_ID_IDL = 0x01;                   // ID сообщения IDL




tlv recievedTLV;
tlv sentTLV;

void start_payment(int amount) {
   
    UART0_DEBUG_PORT.print("Начата оплата на сумму: ");
    UART0_DEBUG_PORT.print(amount / 100);
    UART0_DEBUG_PORT.println(" руб.");
    
    // Отправляем терминал в режим приема оплаты
    send_VRP(amount);
}


void processing_recieved_POS_message(){
    static int requestedAmount;

    if((!recievedTLV.isMesProcessed) && (recievedTLV.mesName == "STA")){
        start_payment(recievedTLV.amount);
        recievedTLV.lastTime = millis();
        recievedTLV.isMesProcessed = false;
    }
    if((!recievedTLV.isMesProcessed) && (recievedTLV.mesName == "VRP") && (millis() - recievedTLV.lastTime < 5000)){
        start_payment(recievedTLV.amount);
        requestedAmount = recievedTLV.amount;
        recievedTLV.lastTime = millis();
        recievedTLV.isMesProcessed = false;
    }
    if((!recievedTLV.isMesProcessed) && (recievedTLV.mesName == "IDL") && (millis() - recievedTLV.lastTime < 5000) && (requestedAmount == recievedTLV.amount)){
        start_payment(recievedTLV.amount);
        recievedTLV.isMesProcessed = false;
        recievedTLV.lastTime = millis();
        handle_successful_payment();
    } else {
        handle_failed_payment();
    }

}



void process_POS_received_data() {
  if (bufferIndex == 0) return;


  if (bufferIndex < 7) { clear_buffer(); return; }


  if (receiveBuffer[0] != START_BYTE) { clear_buffer(); return; }


  uint16_t msgLen = (uint16_t(receiveBuffer[1]) << 8) | receiveBuffer[2];
  uint16_t proto  = (uint16_t(receiveBuffer[3]) << 8) | receiveBuffer[4];


  if (1 + 2 + msgLen + 2 != bufferIndex) {
    UART0_DEBUG_PORT.println("Ошибка: длина кадра не совпала");
    clear_buffer(); return;
  }


  uint16_t rxCrc = (uint16_t(receiveBuffer[bufferIndex-2]) << 8) | receiveBuffer[bufferIndex-1];
  uint16_t calcCrc = calculate_CRC16_ccitt(receiveBuffer, bufferIndex - 2);
  if (rxCrc != calcCrc) { UART0_DEBUG_PORT.println("CRC error"); clear_buffer(); return; }


  if (proto != 0x97FB && proto != 0x96FB) {
    UART0_DEBUG_PORT.println("Ошибка: неверный протокол"); 
  }

  UART0_DEBUG_PORT.print("Сообщение в HEX: ");
  for (int i = 0; i < bufferIndex; i++) {
    if (receiveBuffer[i] < 0x10) UART0_DEBUG_PORT.print("0");
    UART0_DEBUG_PORT.print(receiveBuffer[i], HEX);
    UART0_DEBUG_PORT.print(" ");
  }
  UART0_DEBUG_PORT.println();

  // TLV-парсинг
  const int appStart = 5;                
  const int appEnd   = bufferIndex - 2;  // до CRC

  String msgName = "";
  long operationNumber = -1;
  long amount = -1;

  for (int p = appStart; p < appEnd; ) {
    uint8_t tag = receiveBuffer[p++];

   
    if (p >= appEnd) break;
    uint8_t len = receiveBuffer[p++];

    if (p + len > appEnd) { UART0_DEBUG_PORT.println("TLV выходит за границы"); break; }

    switch (tag) {
      case 0x01: { 
        msgName = "";
        for (int i=0;i<len;i++) msgName += char(receiveBuffer[p+i]);
      } break;

      case 0x03: { 
        String s=""; for (int i=0;i<len;i++) s += char(receiveBuffer[p+i]);
        operationNumber = s.toInt();
      } break;

      case 0x04: { 
        String s=""; for (int i=0;i<len;i++) s += char(receiveBuffer[p+i]);
        amount = s.toInt();
      } break;


    }
    p += len;
  }


  if (recievedTLV.isMesProcessed){
    recievedTLV.amount = amount;
    recievedTLV.mesName = msgName;
    recievedTLV.isMesProcessed = false;
    recievedTLV.lastTime = millis();
  }

  UART0_DEBUG_PORT.print("Имя: "); UART0_DEBUG_PORT.println(msgName);
  if (amount >= 0) {
    UART0_DEBUG_PORT.print("Сумма: "); 
    UART0_DEBUG_PORT.print(amount/100); UART0_DEBUG_PORT.print(" руб "); 
    UART0_DEBUG_PORT.print(amount%100); UART0_DEBUG_PORT.println(" коп");
  }
  if (operationNumber >= 0) {
    UART0_DEBUG_PORT.print("Operation: "); UART0_DEBUG_PORT.println(operationNumber);
  }

  clear_buffer();
}


// Обработка успешного платежа
void handle_successful_payment() {
    UART0_DEBUG_PORT.println("Оплата успешно проведена");
  
    softserial_energy_port_send_command("R ON");
    delay(1000);
    softserial_energy_port_send_command("R OFF");
}

// Обработка ошибки платежа
void handle_failed_payment() {
    UART0_DEBUG_PORT.println("Ошибка при проведении оплаты");

    softserial_energy_port_send_command("R ON");
    delay(1000);
    softserial_energy_port_send_command("R OFF");
}

// Обработка таймаута
void handle_payment_timeout() {
    UART0_DEBUG_PORT.println("Превышено время ожидания оплаты");

    softserial_energy_port_send_command("R ON");
    delay(1000);
    softserial_energy_port_send_command("R OFF");
}