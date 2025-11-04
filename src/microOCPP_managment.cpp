#include "main.h"

const uint8_t RELAY_PIN = 2; // пин реле/контактора
const uint32_t METER_REPORT_INTERVAL = 5000;
const float CURRENT_THRESHOLD_A = 0.5f; // минимальный ток, чтобы считать авто подключено

uint32_t lastMeterReportMs = 0;
static bool relayState = false;
static bool txStartedByAuto = false;

// ====== Инициализация MicroOcpp ======
void microOCPP_initialize() {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW); // реле выключено по умолчанию

    // Инициализация MicroOcpp
    mocpp_initialize(OCPP_SERVER_URL, CHARGE_BOX_ID, "ESP32 Charging Station", "MyCompany");

    // Передача энергии на сервер
    setEnergyMeterInput([]() -> float {
        return get_energy_total(); // Wh
    });

    lastMeterReportMs = millis();
    Serial.println(F("MicroOcpp initialized"));
}

// ====== Основной цикл ======
void microOCPP_loop() {
    mocpp_loop(); // обработка сообщений OCPP

    float powerW = get_power();
    float currentA = get_current();

    bool ocppAllow = ocppPermitsCharge();
    bool hasLoad = currentA > CURRENT_THRESHOLD_A;

    // ===== управление реле =====
    bool shouldEnable = ocppAllow && hasLoad;
    if (shouldEnable && !relayState) {
        digitalWrite(RELAY_PIN, HIGH);
        relayState = true;
        Serial.println(F("[RELAY] ENABLED"));
    } else if (!shouldEnable && relayState) {
        digitalWrite(RELAY_PIN, LOW);
        relayState = false;
        Serial.println(F("[RELAY] DISABLED"));
    }

    // ===== автоматический старт/стоп транзакции =====
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

    // ===== периодический лог MeterValues =====
    uint32_t now = millis();
    if (now - lastMeterReportMs >= METER_REPORT_INTERVAL) {
        lastMeterReportMs = now;
        Serial.printf("[METER] V=%.2f V, I=%.2f A, P=%.2f W, Energy=%.4f Wh\n",
                      get_voltage(), currentA, powerW, get_energy_total());
    }

    delay(50); // лёгкая задержка
}
