// src/microOCPP_managment.cpp
#include "main.h"
#include <Arduino.h>

// ================== НАСТРОЙКИ ==================
const float    CURRENT_THRESHOLD_A   = 0.5f;   // ток, выше которого считаем "подключено"
const uint32_t METER_REPORT_INTERVAL = 5000;   // период лога измерений в Serial (мс)
const uint32_t STATUS_LOG_INTERVAL   = 5000;   // период лога статуса в Serial (мс)

// ================== ГЛОБАЛЬНЫЕ ==================
static bool     relayState        = false;     // текущее состояние реле
static bool     txStartedByAuto   = false;     // транзакция создана автоматикой станции
static uint32_t lastMeterReportMs = 0;
static uint32_t lastStatusLogMs   = 0;


// ================== ХЕЛПЕР СТАТУСА ДЛЯ ЛОГОВ ==================
static const char* derive_local_status(bool ocppAllow, bool hasLoad, bool relay) {
  if (!ocppAllow && !hasLoad)          return "Available";
  if ( ocppAllow && !hasLoad)          return "Preparing";
  if ( ocppAllow &&  hasLoad && relay) return "Charging";
  if (!ocppAllow &&  hasLoad)          return "SuspendedEV";
  return "Available";
}

// ================== ИНИЦИАЛИЗАЦИЯ ==================
void microOCPP_initialize() {

  // Инициализация MicroOCPP 1.6 (у твоей версии нет sampler'ов, поэтому их не трогаем)
  mocpp_initialize(
      OCPP_SERVER_URL,
      CHARGE_BOX_ID,
      "ESP32CS",     // об этом ниже
      "VinCoder"
  );

  lastMeterReportMs = millis();
  lastStatusLogMs   = millis();

  // Реле на старте выключаем
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  relayState      = false;
  txStartedByAuto = false;

  Serial.println(F("MicroOcpp initialized (using existing get_*() functions incl. get_total_energy)"));
}

// ================== ОСНОВНОЙ ЦИКЛ ==================
void microOCPP_loop() {
  // Обработка OCPP (WebSocket, входящие команды, таймеры и т.д.)
  mocpp_loop();

  // Берём актуальные измерения из твоих функций
  const float voltage   = get_voltage();
  const float currentA  = get_current();
  const float powerW    = get_power();
  const float energyVal = get_total_energy();  // твоя готовая энергия (например, Wh)

  const bool ocppAllow = ocppPermitsCharge();            // разрешение CSMS (сервер/эмулятор)
  const bool hasLoad   = currentA > CURRENT_THRESHOLD_A; // авто реально "висит" и берёт ток

  // ===== Управление реле =====
  const bool shouldEnable = ocppAllow && hasLoad;

  if (shouldEnable && !relayState) {
    digitalWrite(RELAY_PIN, HIGH);
    relayState = true;
    UART0_DEBUG_PORT.println(F("[RELAY] ENABLED (ocppPermitsCharge && hasLoad)"));
  } else if (!shouldEnable && relayState) {
    digitalWrite(RELAY_PIN, LOW);
    relayState = false;
    UART0_DEBUG_PORT.println(F("[RELAY] DISABLED (!ocppPermitsCharge || !hasLoad)"));
  }

  // ===== Авто Start/Stop транзакции =====
  if (!getTransaction()) {
    // Транзакции нет
    if (hasLoad && ocppAllow && !txStartedByAuto) {
      UART0_DEBUG_PORT.println(F("[TX] Начало транзакции (auto)"));
      // если хочешь сбрасывать свой счётчик энергии – вызывай здесь свою функцию reset
      // например: reset_energy_counter();  // если она у тебя есть
      beginTransaction("auto-start");  // idTag можно заменить на реальный
      txStartedByAuto = true;
    }
  } else {
    // Транзакция активна
    if (!hasLoad && txStartedByAuto) {
      UART0_DEBUG_PORT.printf("[TX] Окончание транзакции (auto), energy=%.3f\n", energyVal);
      endTransaction(getTransactionIdTag());
      txStartedByAuto = false;
    }
  }

  const uint32_t now = millis();

  // ===== Периодический лог измерений =====
  if (now - lastMeterReportMs >= METER_REPORT_INTERVAL) {
    lastMeterReportMs = now;
    UART0_DEBUG_PORT.printf("[METER] V=%.2f V, I=%.2f A, P=%.2f W, Energy=%.4f\n",
                  voltage, currentA, powerW, energyVal);
  }

  // ===== Локальный лог статуса =====
  if (now - lastStatusLogMs >= STATUS_LOG_INTERVAL) {
    lastStatusLogMs = now;
    const char* st = derive_local_status(ocppAllow, hasLoad, relayState);
    UART0_DEBUG_PORT.printf("[STATUS] %s (allow=%d, load=%d, relay=%d)\n",
                  st,
                  ocppAllow ? 1 : 0,
                  hasLoad   ? 1 : 0,
                  relayState? 1 : 0);
  }

  //delay(30);
}
