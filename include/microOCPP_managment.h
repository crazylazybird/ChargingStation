#pragma once

#define CHARGE_BOX_ID   "chargestation"
#define OCPP_SERVER_URL "ws://192.168.1.63:9000"   // БЕЗ /chargestation и без хвоста


void microOCPP_initialize();
void microOCPP_loop();
void debug_measurements_loop();

enum ChargeState {
  IDLE,             // ничего не делаем
  START_WAIT,       // дали "старт" (тап), ждём роста мощности
  CHARGING,         // зарядка идёт
  STOP_WAIT,        // дали "стоп" (тап), ждём падения мощности
  ERROR_STATE       // ошибка станции
};



enum StationError {
  NO_ERROR,
  START_TIMEOUT_ERROR,
  STOP_TIMEOUT_ERROR
};

