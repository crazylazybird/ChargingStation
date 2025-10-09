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
    UART_VMT_recieved_data();
    send_IDLE();
    process_received_energy_data();
}