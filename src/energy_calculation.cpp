#include "main.h"

// Глобальные переменные для хранения данных
float voltage = 0.0; // Напряжение
float current = 0.0; // Ток
float power = 0.0; // Мощность
float energyTotal = 0.0; // Общее потребление энергии

SoftwareSerial SOFTSERIAL_ENERGY_PORT(RX2_PIN, TX2_PIN); //для связи с энергосчетчика

void process_received2_data() {
    //static const int BUFFER_SIZE = 256;
    static byte buffer[BUFFER_SIZE];
    static int bufferIndex = 0;
    
    while (SOFTSERIAL_ENERGY_PORT.available() > 0) {
        char incomingByte = SOFTSERIAL_ENERGY_PORT.read();
        
        // Собираем пакет данных
        if (bufferIndex < BUFFER_SIZE) {
            buffer[bufferIndex++] = incomingByte;
        }
        
        // Проверяем маркер конца пакета (например, 0x0D)
        if (incomingByte == 0x0D) {
            // Проверяем контрольную сумму пакета
            if (check_CRC(buffer, bufferIndex)) {
                // Парсим данные из буфера
                parse_energy_data(buffer, bufferIndex);
            }
            
            // Сбрасываем буфер
            bufferIndex = 0;
        }
    }
}

bool check_CRC(const uint8_t* data, int length) {
    // Рассчитываем CRC для полученных данных
    uint16_t receivedCRC = calculate_CRC16(data, length);
    
    // Извлекаем CRC из конца сообщения
    uint16_t expectedCRC = (data[length - 2] << 8) | data[length - 1];
    
    // Сравниваем рассчитанную и полученную CRC
    return receivedCRC == expectedCRC;
}

void parse_energy_data(byte* data, int length) {
    // Пример парсинга данных в формате BCD
    // Предполагаем, что данные приходят в формате BCD
    voltage = bcd2dec(data[2], data[3]) * 0.1; // Пример преобразования BCD в float
    current = bcd2dec(data[4], data[5]) * 0.01;
    power = bcd2dec(data[6], data[7]) * 0.1;
    energyTotal = bcd2dec(data[8], data[9], data[10], data[11]) * 0.001;
    
    // Вывод отладочной информации
    Serial.println("------------------------");
    Serial.print("Напряжение: "); Serial.print(voltage); Serial.println(" В");
    Serial.print("Ток: "); Serial.print(current); Serial.println(" А");
    Serial.print("Мощность: "); Serial.print(power); Serial.println(" Вт");
    Serial.print("Энергия: "); Serial.print(energyTotal); Serial.println(" кВт·ч");
}

// Функция преобразования BCD в десятичное число
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