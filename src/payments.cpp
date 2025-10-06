#include "main.h"

const unsigned long PAYMENT_TIMEOUT = 800000;  // Таймаут в миллисекундах
bool paymentInProgress = false;
unsigned long paymentStartTime = 0;
int requestedAmount = 0;

bool failedPaymentFlag = false;

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

extern SoftwareSerial SOFTSERIAL_ENERGY_PORT;

void start_payment(int amount) {
    if (paymentInProgress) {
        UART0_DEBUG_PORT.println("Ошибка: уже идет процесс оплаты");
        return;
    }
    
    if (amount <= 0 || amount > 1000000) {
        UART0_DEBUG_PORT.println("Ошибка: неверная сумма оплаты");
        return;
    }
    
    requestedAmount = amount;
    paymentInProgress = true;
    paymentStartTime = millis();
    
    UART0_DEBUG_PORT.print("Начата оплата на сумму: ");
    UART0_DEBUG_PORT.print(amount / 100);
    UART0_DEBUG_PORT.println(" руб.");
    
    // Отправляем терминал в режим приема оплаты
    send_VRP(amount);
}

void check_payment_status() {
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
    UART0_DEBUG_PORT.println("2");
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
    uint16_t calculatedCRC = calculate_CRC16(receiveBuffer, bufferIndex-2);
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
    UART0_DEBUG_PORT.println("Статус операции");

    // Проверяем статус операции
    if (receiveBuffer[payloadStart + amountLength + 3] == 0x05) {
        int statusLength = receiveBuffer[payloadStart + amountLength + 4];
        byte status = receiveBuffer[payloadStart + amountLength + 5];
        UART0_DEBUG_PORT.println("Тип операции");
        if (status == 0x00 && amount == requestedAmount) {
            // Успешная операция с правильной суммой            
            handleSuccessfulPayment();
        } else if (status == 0x00 && amount == 0) {
            // Платеж не выполнен (сумма 0)
            UART0_DEBUG_PORT.println("Ошибка: платеж не выполнен (сумма = 0)");
            handleFailedPayment();
        } else if (status >= 0x01 && status <= 0xFF) {
            // Ошибка с кодом
            UART0_DEBUG_PORT.print("Ошибка платежа, код: 0x");
            UART0_DEBUG_PORT.println(status, HEX);
            handleFailedPayment();
        } else {
            UART0_DEBUG_PORT.println("Неизвестный статус операции");
            handleFailedPayment();
        }
    }
}


void check_payment_status_(){
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
    uint16_t calculatedCRC = calculate_CRC16(receiveBuffer, bufferIndex-2);
    if (receivedCRC != calculatedCRC) return;

    // Парсим сумму из ответа
    int payloadStart = 7 + receiveBuffer[6];
    int amount = 0;
    int amountLength = 0;

    // ДИНАМИЧЕСКИЙ поиск тегов
    int pos = payloadStart;
    bool foundAmount = false;
    bool foundStatus = false;
    byte status = 0xFF;

    while (pos < bufferIndex - 2) {
        byte tag = receiveBuffer[pos];
        byte length = receiveBuffer[pos + 1];
        
        if (tag == 0x04) { // Тег суммы
            amountLength = length;
            String amountStr = "";
            for (int i = 0; i < amountLength; i++) {
                amountStr += char(receiveBuffer[pos + 2 + i]);
            }
            amount = amountStr.toInt();
            foundAmount = true;
        }
        else if (tag == 0x05) { // Тег статуса
            if (length >= 1) {
                status = receiveBuffer[pos + 2];
                foundStatus = true;
            }
        }
        
        pos += 2 + length; // Переход к следующему тегу
    }

    UART0_DEBUG_PORT.println("Статус операции");

    // Проверяем статус операции
    if (foundStatus) {
        UART0_DEBUG_PORT.println("Тип операции определена");
        
        if (status == 0x00) {
            if (foundAmount && amount == requestedAmount) {
                // Успешная операция с правильной суммой            
                UART0_DEBUG_PORT.println("Платеж успешен");
                handleSuccessfulPayment();
            } else if (foundAmount && amount == 0) {
                // Платеж не выполнен (сумма 0)
                UART0_DEBUG_PORT.println("Ошибка: платеж не выполнен (сумма = 0)");
                handleFailedPayment();
            } else {
                UART0_DEBUG_PORT.println("Несоответствие суммы");
                handleFailedPayment();
            }
        } else if (status >= 0x01 && status <= 0xFF) {
            // Ошибка с кодом
            UART0_DEBUG_PORT.print("Ошибка платежа, код: 0x");
            UART0_DEBUG_PORT.println(status, HEX);
            handleFailedPayment();
        } else {
            UART0_DEBUG_PORT.println("Неизвестный статус операции");
            handleFailedPayment();
        }
    } else {
        UART0_DEBUG_PORT.println("Тег статуса не найден в сообщении");
        // Если это VRP без тега статуса - возможно, это запрос от терминала
        if (foundAmount) {
            UART0_DEBUG_PORT.print("Получен запрос на оплату: ");
            UART0_DEBUG_PORT.print(amount / 100);
            UART0_DEBUG_PORT.println(" руб.");
            // Здесь можно автоматически начать оплату
            // startPayment(amount);
        }
    }
    clear_buffer();
}

bool isChargingStarted() {
    static unsigned long chargingStartTime = 0;  // время включения флага
    static bool wasCharging = false;             // предыдущее состояние

    if (power > 100) {
        if (!wasCharging) {
            // Флаг только что стал true — запоминаем время
            chargingStartTime = millis();
            wasCharging = true;
        }
        // Проверяем, прошло ли больше 3 секунд
        if (millis() - chargingStartTime >= 3000) {
            return true;
        }
    } else {
        // Если флаг сброшен — обнуляем контроль
        wasCharging = false;
    }

    return false;
}


void handle_charging(){
    static unsigned long failedPaymentRelayStartTime = 0;  // время включения флага
    if(isChargingStarted()){
        SOFTSERIAL_ENERGY_PORT.print("R ON");
        send_VRP(1);
    }
    if(failedPaymentFlag == 1){
        SOFTSERIAL_ENERGY_PORT.print("R OFF");
        if (millis() - failedPaymentRelayStartTime >= 200) {
            SOFTSERIAL_ENERGY_PORT.print("R ON");
        }
    }
    
}
// Обработка успешного платежа
void handleSuccessfulPayment() {
    UART0_DEBUG_PORT.println("Оплата успешно проведена");
    paymentInProgress = false;    
    requestedAmount = 0;
    send_FIN(requestedAmount);
    SOFTSERIAL_ENERGY_PORT.print("R ON");
}

// Обработка ошибки платежа
void handleFailedPayment() {
    UART0_DEBUG_PORT.println("Ошибка при проведении оплаты");
    paymentInProgress = false;
    requestedAmount = 0;
    failedPaymentFlag = 1;
    //Отправить разрыв реле
}

// Обработка таймаута
void handlePaymentTimeout() {
    UART0_DEBUG_PORT.println("Превышено время ожидания оплаты");
    paymentInProgress = false;
    requestedAmount = 0;  
    failedPaymentFlag = 1;  
    //Отправить разрыв реле
}