#include "main.h"

// Глобальные переменные для хранения данных
float voltage = 0.0; // Напряжение
float current = 0.0; // Ток
float power = 0.0; // Мощность
float energyTotal = 0.0; // Общее потребление энергии

extern SoftwareSerial SOFTSERIAL_ENERGY_PORT;


// ---------- CRC16 ----------
uint16_t calculate_CRC16(const uint8_t* data, int length) {
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < length; pos++) {
        crc ^= (uint16_t)data[pos];
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

bool check_CRC(const uint8_t* data, int length) {
    if (length < 6) return false; // слишком короткий пакет

    int dataLen = length - 3; // исключаем CRC (2 байта) и 0x0D
    uint16_t expectedCRC = (data[length - 3] << 8) | data[length - 2];
    uint16_t receivedCRC = calculate_CRC16(data, dataLen);

    // // === Отладка CRC ===
    // UART0_DEBUG_PORT.print("RX пакет (len=");
    // UART0_DEBUG_PORT.print(length);
    // UART0_DEBUG_PORT.print(") CRC_calc=");
    // UART0_DEBUG_PORT.print(receivedCRC, HEX);
    // UART0_DEBUG_PORT.print(" CRC_pkt=");
    // UART0_DEBUG_PORT.print(expectedCRC, HEX);
    // UART0_DEBUG_PORT.print(" : ");
    // for (int i = 0; i < length; i++) {
    //     if (data[i] < 0x10) UART0_DEBUG_PORT.print("0");
    //     UART0_DEBUG_PORT.print(data[i], HEX);
    //     UART0_DEBUG_PORT.print(" ");
    // }
    // UART0_DEBUG_PORT.println();

    return expectedCRC == receivedCRC;
}


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
    voltage     = bcd2dec(data[2], data[3]) * 0.1;
    current     = bcd2dec(data[4], data[5]) * 0.01;
    power       = bcd2dec(data[6], data[7]) * 0.1;
    energyTotal = bcd2dec(data[8], data[9], data[10], data[11]) * 0.001;

    UART0_DEBUG_PORT.println("------------------------");
    UART0_DEBUG_PORT.print("Напряжение: "); UART0_DEBUG_PORT.print(voltage); UART0_DEBUG_PORT.println(" В");
    UART0_DEBUG_PORT.print("Ток: "); UART0_DEBUG_PORT.print(current); UART0_DEBUG_PORT.println(" А");
    UART0_DEBUG_PORT.print("Мощность: "); UART0_DEBUG_PORT.print(power); UART0_DEBUG_PORT.println(" Вт");
    UART0_DEBUG_PORT.print("Энергия: "); UART0_DEBUG_PORT.print(energyTotal); UART0_DEBUG_PORT.println(" кВт·ч");
}

// ---------- ОБРАБОТКА ----------
void process_received_energy_data() {
    static byte buffer[BUFFER_SIZE];
    static int bufferIndex = 0;
    static bool inPacket = false;

    while (SOFTSERIAL_ENERGY_PORT.available() > 0) {
        byte incomingByte = SOFTSERIAL_ENERGY_PORT.read();

        if (!inPacket) {
            // ждём маркер начала 0xAA 0x55
            if (bufferIndex == 0 && incomingByte == 0xAA) {
                buffer[bufferIndex++] = incomingByte;
            } else if (bufferIndex == 1 && incomingByte == 0x55) {
                buffer[bufferIndex++] = incomingByte;
                inPacket = true;
            } else {
                bufferIndex = 0; // сброс, если начало не совпало
            }
        } else {
            // внутри пакета
            if (bufferIndex < BUFFER_SIZE) {
                buffer[bufferIndex++] = incomingByte;
            }

            if (incomingByte == 0x0D) { // конец пакета
                if (check_CRC(buffer, bufferIndex)) {
                    parse_energy_data(buffer, bufferIndex);
                } else {
                    UART0_DEBUG_PORT.println("Ошибка CRC!");
                }
                bufferIndex = 0;
                inPacket = false;
            }
        }
    }
    
}