#include "main.h"
#include <EEPROM.h>

// Глобальные переменные
SystemData sysData;
Measurements measurments;


void update_measurements(){
  measurments.current = read_current();
  measurments.voltage = read_voltage();
  measurments.power   = measurments.current * measurments.voltage;

  static float energySum = 0.0f;
  // Получаем текущую мощность
  float power = measurments.power;

  // Накопление энергии с учетом времени в кВт-ч
  energySum += power * (millis() - sysData.lastUpdate) / 3600000000.0f;

  // Обновление каждые ENERGY_UPDATE_INTERVAL мс
  if (millis() - sysData.lastUpdate >= ENERGY_UPDATE_INTERVAL) {
    sysData.totalEnergy += energySum;
    sysData.lastUpdate = millis();
    energySum = 0.0f;
    save_configuration();
  }
}


float read_current() {
  int32_t posSum = 0, negSum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    posSum += analogRead(CURRENT_POS);
    negSum += analogRead(CURRENT_NEG);
    delay(1);
  }
  // деление как float
  float raw = ((float)(posSum - negSum) / (float)SAMPLES) - sysData.calib.currOffset;

  if (raw >= 0.0f)
    return raw * sysData.calib.currScale;
  else
    return 0.0f;
}

// Измерение напряжения
float read_voltage() {
  int32_t posSum = 0, negSum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    posSum += analogRead(VOLTAGE_POS);
    negSum += analogRead(VOLTAGE_NEG);
    delay(1);
  }
  // деление как float
  float raw = ((float)(posSum - negSum) / (float)SAMPLES) - sysData.calib.voltOffset;

  if (raw >= 0.0f)
    return raw * sysData.calib.voltScale;
  else
    return 0.0f;
}

float get_voltage(){
  return measurments.voltage;
}

float get_current(){
  return measurments.current;
}

float get_power(){
  return measurments.power;
}

float get_total_energy(){
  return sysData.totalEnergy;
}



// Функция сброса счетчика энергии
void reset_energy_counter() {
  sysData.totalEnergy = 0.0;
  save_configuration();
  Serial.println("Счетчик энергии сброшен");
}



void calibrate_current(float referenceValue) {
  Serial.print("Калибровка тока на ");
  Serial.print(referenceValue);
  Serial.println(" А");

  int32_t sum = 0;
  for (int i = 0; i < SAMPLES * 20; i++) {
    sum += analogRead(CURRENT_POS) - analogRead(CURRENT_NEG);
    delayMicroseconds(100);
  }
  float avg = (float)sum / (float)(SAMPLES * 20); // среднее как float

  if (referenceValue == 0.0f) {
    sysData.calib.currOffset = avg;
    save_configuration();
    Serial.print("cмещение уcтановлено: ");
    Serial.println(sysData.calib.currOffset);
  } else {
    float rawValue = avg - sysData.calib.currOffset;
    if (rawValue != 0.0f) {
      sysData.calib.currScale = referenceValue / rawValue;
      save_configuration();
      Serial.print("Коэффициент: ");
      Serial.println(sysData.calib.currScale);
    } else {
      Serial.println("Ошибка калибровки тока: rawValue = 0");
    }
  }
}

// Калибровка напряжения
void calibrate_voltage(float referenceValue) {
  Serial.print("Калибровка напряжения на ");
  Serial.print(referenceValue);
  Serial.println(" В");

  int32_t sum = 0;
  for (int i = 0; i < SAMPLES * 20; i++) {
    sum += analogRead(VOLTAGE_POS) - analogRead(VOLTAGE_NEG);
    delayMicroseconds(100);
  }
  float avg = (float)sum / (float)(SAMPLES * 20); // среднее как float

  if (referenceValue == 0.0f) {
    sysData.calib.voltOffset = avg;
    save_configuration();
    Serial.print("cмещение уcтановлено: ");
    Serial.println(sysData.calib.voltOffset);
  } else {
    float rawValue = avg - sysData.calib.voltOffset;
    if (rawValue != 0.0f) {
      sysData.calib.voltScale = referenceValue / rawValue;
      save_configuration();
      Serial.print("Коэффициент: ");
      Serial.println(sysData.calib.voltScale);
    } else {
      Serial.println("Ошибка калибровки напряжения: rawValue = 0");
    }
  }
}

// Контрольная сумма всех данных (кроме поля dataChecksum)
uint16_t calculate_data_checksum() {
  uint16_t sum = 0;
  uint8_t* data = (uint8_t*)&sysData;

  for (size_t i = 0; i < sizeof(SystemData) - sizeof(uint16_t); i++) {
    sum += data[i];
    sum = (uint16_t)((sum << 1) | (sum >> 15));  // простой битовый сдвиг, как у тебя
  }

  return sum;
}

// Контрольная сумма калибровки (кроме своих последних 2 байт checksum)
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
  sysData.dataChecksum   = calculate_data_checksum();
  EEPROM.put(EEPROM_DATA_ADDR_CALIBRATION, sysData);
  EEPROM.commit(); // ОБЯЗАТЕЛЬНО на ESP32
}

void load_configuration() {
  EEPROM.get(EEPROM_DATA_ADDR_CALIBRATION, sysData);
  // (по максимуму близко к оригиналу — без авто-проверок и дефолтов)
}

void EEPROM_init_configuration() {
  EEPROM.begin(EEPROM_SIZE);
}