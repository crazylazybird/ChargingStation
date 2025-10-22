#pragma once


#define PRICE_FOR_ONE_KWHOUR 15 //копеек. Указывать в копейках, !умножить на 100

void start_payment(long amount);

void process_POS_received_data();
void processing_received_POS_message();

void handle_successful_payment();
void handle_failed_payment();
void handle_payment_timeout();
void charging_managment();

struct tlv {
    String          mesName;
    int             opNumber;
    long            amount;     //копеек
    unsigned long   lastTime;
    bool            isMesProcessed;
};

enum PaymentStatus : uint8_t {
    //VRP 1000 -> IDL -> DIS -> FIN 560 = REFUND 440 -> IDL ->
    WAITING_PAYMENT = 0,
    PAID = 1,
    SPENDING = 2,
    REFUND = 3,
    INSUFFICIENT_FUNDS = 4
};
enum ChargingStatus : uint8_t {
    WAITING_TO_CHARGE = 0,
    START_TO_CHARGE = 1,
    RUNNING = 2,
    STOPPING = 3,
    doSTOP = 4
};


struct transactions{
    float   kWattPerHourAvailable;        // текущее значение мощности (КВт.ч)
    int     paidMinor;                      // оплачено в минорных единицах (копейки)
    PaymentStatus    paymentStatus;
    PaymentStatus    paymentStatusPrev;
    unsigned long   lastTime;
    ChargingStatus chargingStatus;
    ChargingStatus chargingStatusPrev;
};

