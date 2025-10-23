#pragma once
#include "main.h"


#define SAMPLES 10  // Количество выборок для измерения
#define CURRENT_POS A0  // Вход тока +
#define CURRENT_NEG A1  // Вход тока -
#define VOLTAGE_POS A2  // Вход напряжения +
#define VOLTAGE_NEG A3  // Вход напряжения -

#define EEPROM_DATA_ADDR 2           // Адрес данных в EEPROM


float readCurrent();
float readVoltage();
void calibrateCurrent(float referenceValue);
void calibrateVoltage(float referenceValue);
void saveConfiguration();
uint16_t calculateDataChecksum();
uint16_t calculateCalibChecksum();


struct CalibrationData {
  int16_t currOffset;  // Смещение тока
  int16_t voltOffset;  // Смещение напряжения
  float currScale;     // Масштаб тока
  float voltScale;     // Масштаб напряжения
  uint16_t checksum;   // Контрольная сумма
};

struct SystemData {
  uint8_t version;           // Версия структуры
  CalibrationData calib;     // Данные калибровки
  double totalEnergy;        // Накопленная энергия
  unsigned long lastSave;    // Время последнего сохранения
  unsigned long lastUpdate;  // Время последнего обновления измерений
  uint16_t dataChecksum;     // Контрольная сумма
};

