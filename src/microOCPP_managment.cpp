// src/microOCPP_managment.cpp
#include "main.h"
#include <Arduino.h>

// ================== НАСТРОЙКИ ==================
const uint8_t  RELAY_PIN = 2;                 // пин реле/контактора
const float    CURRENT_THRESHOLD_A = 0.5f;    // ток, выше которого считаем "подключено"
const uint32_t METER_REPORT_INTERVAL = 5000;  // период лога в Serial (мс)
const uint32_t STATUS_LOG_INTERVAL   = 5000;  // период локального статуса в Serial (мс)

// ================== ГЛОБАЛЬНЫЕ ==================
static bool     relayState = false;
static bool     txStartedByAuto = false;
static uint32_t lastMeterReportMs = 0;
static uint32_t lastStatusLogMs   = 0;

// ================== ХЕЛПЕРЫ ==================
static const char* derive_local_status(bool ocppAllow, bool hasLoad, bool relay) {
  if (!ocppAllow && !hasLoad) return "Available";
  if ( ocppAllow && !hasLoad) return "Preparing";
  if ( ocppAllow &&  hasLoad && relay) return "Charging";
  if (!ocppAllow &&  hasLoad) return "SuspendedEV";
  return "Available";
}

// ================== ИНИЦИАЛИЗАЦИЯ ==================
void microOCPP_initialize() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // реле выключено по умолчанию

  // Инициализация MicroOCPP (без регистрации sampler'ов — их у твоей версии нет)
  mocpp_initialize(OCPP_SERVER_URL, CHARGE_BOX_ID, "ESP32 Charging Station", "MyCompany");

  lastMeterReportMs = millis();
  lastStatusLogMs   = millis();

  Serial.println(F("MicroOcpp initialized (no setPower*/setStatus* API used)"));
}

// ================== ОСНОВНОЙ ЦИКЛ ==================
void microOCPP_loop() {
  mocpp_loop(); // OCPP обработка

  const float powerW   = get_power();
  const float currentA = get_current();
  const float voltage  = get_voltage();

  const bool  ocppAllow = ocppPermitsCharge();            // разрешение CSMS
  const bool  hasLoad   = currentA > CURRENT_THRESHOLD_A; // авто подключено

  // ===== Управление реле =====
  const bool shouldEnable = ocppAllow && hasLoad;
  if (shouldEnable && !relayState) {
    digitalWrite(RELAY_PIN, HIGH);
    relayState = true;
    Serial.println(F("[RELAY] ENABLED"));
  } else if (!shouldEnable && relayState) {
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;
    Serial.println(F("[RELAY] DISABLED"));
  }

  // ===== Авто Start/Stop транзакции =====
  if (!getTransaction()) {
    if (hasLoad && ocppAllow && !txStartedByAuto) {
      Serial.println(F("[TX] Начало транзакции (auto)"));
      beginTransaction("auto-start");
      txStartedByAuto = true;
    }
  } else {
    if (!hasLoad && txStartedByAuto) {
      Serial.println(F("[TX] Окончание транзакции (auto)"));
      endTransaction(getTransactionIdTag());
      txStartedByAuto = false;
    }
  }

  // ===== Периодический лог измерений =====
  const uint32_t now = millis();
  if (now - lastMeterReportMs >= METER_REPORT_INTERVAL) {
    lastMeterReportMs = now;
    Serial.printf("[METER] V=%.2f V, I=%.2f A, P=%.2f W, Energy=%.4f Wh\n",
                  voltage, currentA, powerW, get_energy_total());
  }

  // ===== Локальный лог статуса (для наглядности) =====
  if (now - lastStatusLogMs >= STATUS_LOG_INTERVAL) {
    lastStatusLogMs = now;
    const char* st = derive_local_status(ocppAllow, hasLoad, relayState);
    Serial.printf("[STATUS] %s (allow=%d, load=%d, relay=%d)\n",
                  st, ocppAllow ? 1 : 0, hasLoad ? 1 : 0, relayState ? 1 : 0);
  }

  delay(30);
}
