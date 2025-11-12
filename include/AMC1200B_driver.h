#pragma once
#include "main.h"


#define SAMPLES 10  // Количество выборок для измерения
#define CURRENT_POS 5  // Вход тока +
#define CURRENT_NEG 4  // Вход тока -
#define VOLTAGE_POS 6  // Вход напряжения +
#define VOLTAGE_NEG 7  // Вход напряжения -

#define EEPROM_DATA_ADDR_CALIBRATION 2           // Адрес данных в EEPROM
#define EEPROM_SIZE 512

#define ENERGY_UPDATE_INTERVAL 1000  // интервал обновления (мс)


float read_current();
float read_voltage();
void calibrate_current(float referenceValue);
void calibrate_voltage(float referenceValue);
uint16_t calculate_data_checksum();
uint16_t calculate_calib_checksum();
void save_configuration();
void load_configuration();
void EEPROM_init_configuration();
float read_power();
void update_energy();
float read_total_energy();
void reset_energy_counter();


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

