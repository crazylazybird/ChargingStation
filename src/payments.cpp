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


//звменил  receivedTLV.isMesProcessed = true -> false
tlv receivedTLV {receivedTLV.mesName = "", receivedTLV.opNumber = 0, receivedTLV.amount = 0, receivedTLV.lastTime = millis(), receivedTLV.isMesProcessed = true};
tlv sentTLV{"", 0, 0, millis(), true};

transactions payment {payment.kWattPerHourAvailbale = 0, payment.paidMinor = 0, payment.isPaymentSucsess = NOT_PAID};

void start_payment(int amount) {
   
    UART0_DEBUG_PORT.print("Начата оплата на сумму: ");
    UART0_DEBUG_PORT.print(double(amount) / 100.0);
    UART0_DEBUG_PORT.println(" руб.");
    
    // Отправляем терминал в режим приема оплаты
    send_VRP(amount);
}


void processing_received_POS_message() {
  if (receivedTLV.isMesProcessed) return;      // одно сообщение — одно действие

  const unsigned long now = millis();

  // 1) STA -> стартуем оплату суммой из STA (в минорных единицах)
  if (receivedTLV.mesName == "STA") {
    receivedTLV.isMesProcessed = true;         // пометить до сайд-эффектов
    if (receivedTLV.amount > 0) {
      // фиксируем "контекст отправки" (для сравнения при ответе)
      sentTLV.amount   = receivedTLV.amount;
      sentTLV.mesName  = "VRP";
      sentTLV.lastTime = now;

      start_payment(receivedTLV.amount);                   // внутри сформирует и отправит VRP
    } else {
      UART0_DEBUG_PORT.println("STA без суммы — VRP не отправляем");
    }
    return;
  }

  // 2) Ответ по оплате: имя может быть VRP/RES/VRA (в зависимости от прошивки)
  if (receivedTLV.mesName == "VRP") {

    
    if (receivedTLV.amount == sentTLV.amount){
      UART0_DEBUG_PORT.print("Оплата подтверждена суммой: ");
      UART0_DEBUG_PORT.println(receivedTLV.amount);
      handle_successful_payment();                    // включает реле и т.п.
      send_IDL();                                     // при необходимости
    } else {
      UART0_DEBUG_PORT.print("Несовпадение суммы (ожидали ");
      UART0_DEBUG_PORT.print(sentTLV.amount);
      UART0_DEBUG_PORT.print(", получили ");
      UART0_DEBUG_PORT.print(receivedTLV.amount);
      UART0_DEBUG_PORT.println(") — отклоняем");
      handle_failed_payment();
    }
    receivedTLV.isMesProcessed = true;                 // пометить до сайд-эффектов
    // очистка контекста отправленного VRP
    sentTLV.amount   = 0;
    sentTLV.mesName  = "";
    sentTLV.lastTime = 0;
    sentTLV.opNumber = 0;
    return;
  }

  // 3) Прочие сообщения — пометить обработанными
  receivedTLV.isMesProcessed = true;

  // 4) Таймаут ожидания ответа на наш VRP
  if (sentTLV.amount > 0 && (now - sentTLV.lastTime) > PAYMENT_TIMEOUT) {
    UART0_DEBUG_PORT.println("Payment timeout");
    handle_payment_timeout();
    sentTLV.amount   = 0;
    sentTLV.mesName  = "";
    sentTLV.lastTime = 0;
    sentTLV.opNumber = 0;
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
  long operation_Number = -1;
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
        operation_Number = s.toInt();
        operationNumber = operation_Number;
      } break;

      case 0x04: { 
        String s=""; for (int i=0;i<len;i++) s += char(receiveBuffer[p+i]);
        amount = s.toInt();
      } break;


    }
    p += len;
  }


  if (receivedTLV.isMesProcessed){
    receivedTLV.amount = amount;
    receivedTLV.mesName = msgName;
    receivedTLV.isMesProcessed = false;
    receivedTLV.lastTime = millis();
  }

//  UART0_DEBUG_PORT.print("Имя: "); UART0_DEBUG_PORT.println(msgName);
  UART0_DEBUG_PORT.print("Имя_TLV: "); UART0_DEBUG_PORT.println(receivedTLV.mesName);
  if (amount >= 0) {
    UART0_DEBUG_PORT.print("Сумма: "); 
    UART0_DEBUG_PORT.print(amount/100); UART0_DEBUG_PORT.print(" руб "); 
    UART0_DEBUG_PORT.print(amount%100); UART0_DEBUG_PORT.println(" коп");
  }
  if (operation_Number >= 0) {
    UART0_DEBUG_PORT.print("номер операции: "); UART0_DEBUG_PORT.println(get_current_operation_number());
  }

  clear_buffer();
}


// Обработка успешного платежа
void handle_successful_payment() {
    UART0_DEBUG_PORT.println("Оплата успешно проведена");
  
    payment.isPaymentSucsess = PAID;
    payment.paidMinor = receivedTLV.amount;
    payment.kWattPerHourAvailbale = payment.paidMinor / PRICE_FOR_ONE_KWHOUR 
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