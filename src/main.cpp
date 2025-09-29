#include "main.h"




void setup(){
    UART_Setup();
}


void loop() {
    UART_Commands_processing();
}