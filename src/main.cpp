#include "main.h"


bool stayIDLE = true;

void setup(){
    UART_Setup();
    softserial_energy_port_send_command("E");                                                           //сброс счетчика энергии
    softserial_energy_port_send_command("R OFF");                                                       //Отключаем NFC карту, ожидание оплаты    
}


void loop() {
    UART_Commands_processing();  // Обработка команд меню из терминала
    UART_POS_received_data();
    send_IDLE();
    process_received_energy_data();
    processing_received_POS_message();
    charging_managment();
}