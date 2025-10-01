#include <Arduino.h>
#include <SoftwareSerial.h>
#include <map>
#include <vector>
#include <string>
#include "main.h"


void setup(){
    UART_Setup();
}


void loop() {
    UART_Commands_processing();
    check_payment_status_();
    UART_VMT_recieved_data();
    send_IDLE();
    //check_payment_status();
    check_payment_status_();
    process_received_energy_data();
}