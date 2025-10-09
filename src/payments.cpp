#include "main.h"

const unsigned long PAYMENT_TIMEOUT = 800000;  // Таймаут в миллисекундах


extern volatile int operationNumber;
extern uint8_t receiveBuffer[BUFFER_SIZE];
extern int bufferIndex;

// Константы протокола связи VTK
extern const int KEEPALIVE_INTERVAL = 10000;   // Интервал keepalive в мс
extern const int OPERATION_NUMBER_LENGTH = 8;  // Длина номера операции
extern const int MAX_OPERATION_NUMBER = 65535;
extern const int MIN_MESSAGE_SIZE = 10;

extern float power; // Мощность
extern float energyTotal; // Общее потребление энергии

const byte START_BYTE = 0x1F;                       // Стартовый байт
const byte PROTOCOL_DISCRIMINATOR_HIGH = 0x96;      // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_POS_HIGH = 0x97;  // Дискриминатор протокола (старшие биты)
const byte PROTOCOL_DISCRIMINATOR_LOW = 0xFB;       // Дискриминатор протокола (младшие биты)
const byte MESSAGE_ID_IDL = 0x01;                   // ID сообщения IDL

int amountLength;

// Переменные состояния потребления энергии
bool chargingStartedFlag;
unsigned long chargingStartTime = 0;
const unsigned long PAYMENT_FOR_CHARGING_TIMEOUT = 3000; //Таймаут при котором нужно запросить оплату за зярядке


extern tlv recievedTLV;

void start_payment(int amount) {
   
    UART0_DEBUG_PORT.print("Начата оплата на сумму: ");
    UART0_DEBUG_PORT.print(amount / 100);
    UART0_DEBUG_PORT.println(" руб.");
    
    // Отправляем терминал в режим приема оплаты
    send_VRP(amount);
}


void processing_recieved_TLV_message(){
    if((!recievedTLV.isMesProcessed) && (recievedTLV.mesName == "STA")){
        start_payment(recievedTLV.amount);
        recievedTLV.lastTime = millis();
    }
    if(millis() - recievedTLV.lastTime > 5000)
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