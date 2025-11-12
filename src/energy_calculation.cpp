#include "main.h"

// Глобальные переменные для хранения данных
float voltage = 0.0; // Напряжение
float current = 0.0; // Ток
float power = 0.0; // Мощность
float energyTotal = 0.0; // Общее потребление энергии





// ---------- BCD -> DEC ----------
int bcd2dec(byte bcd) {
    return ((bcd >> 4) * 10) + (bcd & 0x0F);
}
int bcd2dec(byte high, byte low) {
    return (bcd2dec(high) * 100) + bcd2dec(low);
}
int bcd2dec(byte b1, byte b2, byte b3, byte b4) {
    return (bcd2dec(b1) * 1000000) +
           (bcd2dec(b2) * 10000) +
           (bcd2dec(b3) * 100) +
           bcd2dec(b4);
}

// ---------- ПАРСИНГ ----------
void parse_energy_data(byte* data, int length) {
    // data[0] = 0xAA, data[1] = 0x55
    voltage     = bcd2dec(data[2], data[3], data[4], data[5]) * 0.1;
    current     = bcd2dec(data[6], data[7], data[8], data[9]) * 0.1;
    //power       = bcd2dec(data[10], data[11], data[12], data[13]) * 0.1; 
    power = voltage*current;
    energyTotal = bcd2dec(data[14], data[15], data[16], data[17]) * 0.01;

    // UART0_DEBUG_PORT.println("------------------------");
    // UART0_DEBUG_PORT.print("Ток: "); UART0_DEBUG_PORT.print(current); UART0_DEBUG_PORT.println(" А");
    // UART0_DEBUG_PORT.print("Напряжение: "); UART0_DEBUG_PORT.print(voltage); UART0_DEBUG_PORT.println(" В");
    // UART0_DEBUG_PORT.print("Мощность: "); UART0_DEBUG_PORT.print(power); UART0_DEBUG_PORT.println(" Вт");
    // UART0_DEBUG_PORT.print("Энергия: "); UART0_DEBUG_PORT.print(energyTotal); UART0_DEBUG_PORT.println(" кВт·ч");
}

// float get_power(){
//     return power;
// }

// float get_energy_total(){
//     return energyTotal;
// }

// float get_voltage(){
//     return voltage;
// }

// float get_current(){
//     return current;
// }