#include "main.h"
#include <EEPROM.h>

// Глобальные переменные
SystemData sysData;

float read_current() {
  int32_t posSum = 0, negSum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    posSum += analogRead(CURRENT_POS);
    negSum += analogRead(CURRENT_NEG);
    delay(1);
  }
  float raw = (float)(posSum - negSum) / SAMPLES - sysData.calib.currOffset;

  if (raw >= 0)
    return raw * sysData.calib.currScale;
  else
    return 0;
}

// Измерение напряжения
float read_voltage() {
  int32_t posSum = 0, negSum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    posSum += analogRead(VOLTAGE_POS);
    negSum += analogRead(VOLTAGE_NEG);
    delay(1);
  }
  float raw = (float)(posSum - negSum) / SAMPLES - sysData.calib.voltOffset;
  if (raw >= 0)
    return raw * sysData.calib.voltScale;
  else
    return 0;
}

void calibrate_current(float referenceValue) {
  Serial.print("Калибровка тока на ");
  Serial.print(referenceValue);
  Serial.println(" А");

  // Калибровка нуля (еcли передано 0)
  if (referenceValue == 0.0f) {
    int32_t sum = 0;
    for (int i = 0; i < SAMPLES * 20; i++) {
      sum += analogRead(CURRENT_POS) - analogRead(CURRENT_NEG);
      delayMicroseconds(100);
    }
    sysData.calib.currOffset = sum / (SAMPLES * 20);
    save_configuration();
    Serial.print("cмещение уcтановлено: ");
    Serial.println(sysData.calib.currOffset);
  }
  // Калибровка маcштаба
  else {
    int32_t sum = 0;
    for (int i = 0; i < SAMPLES * 20; i++) {
      sum += analogRead(CURRENT_POS) - analogRead(CURRENT_NEG);
      delayMicroseconds(100);
    }
    float rawValue = (sum / (SAMPLES * 20)) - sysData.calib.currOffset;
    sysData.calib.currScale = referenceValue / rawValue;
    save_configuration();
    Serial.print("Коэффициент: ");
    Serial.println(sysData.calib.currScale);
  }
}

//Калибровка напряжения:
void calibrate_voltage(float referenceValue) {
  Serial.print("Калибровка напряжения на ");
  Serial.print(referenceValue);
  Serial.println(" В");

  // Калибровка нуля (еcли передано 0)
  if (referenceValue == 0.0f) {
    int32_t sum = 0;
    for (int i = 0; i < SAMPLES * 20; i++) {
      sum += analogRead(VOLTAGE_POS) - analogRead(VOLTAGE_NEG);
      delayMicroseconds(100);
    }
    sysData.calib.voltOffset = sum / (SAMPLES * 20);
    save_configuration();
    Serial.print("cмещение уcтановлено: ");
    Serial.println(sysData.calib.voltOffset);
  }
  // Калибровка маcштаба
  else {
    int32_t sum = 0;
    for (int i = 0; i < SAMPLES * 20; i++) {
      sum += analogRead(VOLTAGE_POS) - analogRead(VOLTAGE_NEG);
      delayMicroseconds(100);
    }
    float rawValue = (sum / (SAMPLES * 20)) - sysData.calib.voltOffset;
    sysData.calib.voltScale = referenceValue / rawValue;
    save_configuration();
    Serial.print("Коэффициент: ");
    Serial.println(sysData.calib.voltScale);
  }
}

// Альтернативный вариант c более cложным алгоритмом контрольной cуммы
uint16_t calculate_data_checksum() {
  uint16_t sum = 0;
  uint8_t* data = (uint8_t*)&sysData;

  for (size_t i = 0; i < sizeof(SystemData) - sizeof(uint16_t); i++) {
    sum += data[i];
    sum = (sum << 1) | (sum >> 15);  // Проcтой бит-манипуляционный cдвиг
  }

  return sum;
}
// Функция раcчета контрольной cуммы калибровочных данных
uint16_t calculate_calib_checksum() {
  uint16_t sum = 0;
  uint8_t* data = (uint8_t*)&sysData.calib;
  for (size_t i = 0; i < sizeof(CalibrationData) - 2; i++) {
    sum += data[i];
  }
  return sum;
}


void save_configuration() {
  sysData.calib.checksum = calculate_calib_checksum();
  sysData.dataChecksum = calculate_data_checksum();
  EEPROM.put(EEPROM_DATA_ADDR_CALIBRATION, sysData);
}


void load_configuration() {
  EEPROM.get(EEPROM_DATA_ADDR_CALIBRATION, sysData);
}
