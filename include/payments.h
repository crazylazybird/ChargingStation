#pragma once


#define PRICE_FOR_ONE_KWHOUR 15 /////////////////////// добавить умножение на 100

void start_payment(int amount);

void process_POS_received_data();
void processing_received_POS_message();

void handle_successful_payment();
void handle_failed_payment();
void handle_payment_timeout();

struct tlv {
    String          mesName;
    int             opNumber;
    int32_t         amount;
    unsigned long   lastTime;
    bool            isMesProcessed;
};

enum PaymentStatus : uint8_t {
    PAID = 0,
    NOT_PAID = 1
};


struct transactions{
    float   kWattPerHourAvailbale;        // текущее значение мощности (КВт.ч)
    int     paidMinor;                      // оплачено в минорных единицах (копейки)
    PaymentStatus    isPaymentSucsess;
};

