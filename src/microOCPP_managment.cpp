// src/microOCPP_managment.cpp
#include "main.h"
#include <Arduino.h>
#include <MicroOcpp.h>

// ================== НАСТРОЙКИ ==================

constexpr float POWER_THRESHOLD_W         = 5000.0f;   // порог "есть зарядка"
const unsigned long START_STOP_TIMEOUT_MS = 5000;     // 5 секунд ожидания старта / останова

// idTag по умолчанию для локально инициированных сессий
// (при RemoteStartTransaction библиотека сама заводит idTag,
// мы его только читаем через getTransactionIdTag())
static const char *DEFAULT_ID_TAG = "REMOTE_USER";

// ================== ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ==================

bool relayState             = false;
unsigned long stateChangeTime = 0;
bool lastOcppAllow          = false;   // предыдущее значение ocppPermitsCharge()

static const char* moErrorCode = nullptr;

StationError stationError = NO_ERROR;
ChargeState  chargeState  = IDLE;

// ================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ==================

static void relayOn() {
  if (!relayState) {
    relayState = true;
    digitalWrite(RELAY_PIN, HIGH);
    UART0_DEBUG_PORT.println(F("[RELAY] ON (эмуляция карты)"));
  }
}

static void relayOff() {
  if (relayState) {
    relayState = false;
    digitalWrite(RELAY_PIN, LOW);
    UART0_DEBUG_PORT.println(F("[RELAY] OFF"));
  }
}

/**
 * Старт транзакции на стороне OCPP, если её ещё нет.
 * Работает и для кейса, когда сервер делает RemoteStartTransaction:
 * - если MicroOcpp уже создал транзакцию, getTransaction() != nullptr,
 *   мы ничего не делаем и не лезем повторно.
 */
static void beginOcppTxIfNeeded(const char *idTag = DEFAULT_ID_TAG) {
  if (!getTransaction()) {
    if (beginTransaction(idTag)) {
      UART0_DEBUG_PORT.printf("[OCPP] beginTransaction(\"%s\") OK\n", idTag);
    } else {
      UART0_DEBUG_PORT.printf("[OCPP][WARN] beginTransaction(\"%s\") не запустилась (tx в процессе?)\n", idTag);
    }
  } else {
    // Уже есть транзакция — скорее всего, RemoteStartTransaction от сервера
    UART0_DEBUG_PORT.println(F("[OCPP] beginOcppTxIfNeeded: транзакция уже активна, пропускаем"));
  }
}

/**
 * Корректное завершение активной транзакции.
 * Если транзакции нет (getTransaction() == nullptr), ничего не делает.
 */
static void endOcppTxIfNeeded(const char *reason) {
  auto tx = getTransaction();
  if (!tx) {
    UART0_DEBUG_PORT.println(F("[OCPP] endOcppTxIfNeeded: активной транзакции нет"));
    return;
  }

  String idTag = getTransactionIdTag();   // фактический idTag (в т.ч. при RemoteStartTransaction)
  UART0_DEBUG_PORT.printf(
      "[OCPP] endTransaction(\"%s\"), reason=%s\n",
      idTag.c_str(),
      reason ? reason : "-"
  );

  // В MicroOcpp v1.x endTransaction(idTag) сам сформирует StopTransaction
  // и отправит на сервер.
  endTransaction(idTag.c_str());
}

// ================== ИНИЦИАЛИЗАЦИЯ ==================

void microOCPP_initialize() {

  // Инициализация OCPP-клиента
  mocpp_initialize(
      OCPP_SERVER_URL,
      CHARGE_BOX_ID,
      "ESP32CS",
      "VinCoder"
  );

  // Реле (эмуляция NFC-ридера / "карты")
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  relayState = false;

  // Вход для кодов ошибок — MicroOcpp сам отправит StatusNotification с errorCode
  addErrorCodeInput([]() -> const char* {
      return moErrorCode;   // "InternalError", "OtherError", "PowerMeterFailure", ...
  }, 1);

  // Энергия: общий, монотонно растущий счётчик в Wh
  setEnergyMeterInput([]() {
    return static_cast<int>(get_total_energy());
  });

  Serial.println(F("MicroOcpp initialized"));
}

// ================== ОСНОВНОЙ ЦИКЛ ==================

void microOCPP_loop() {
  mocpp_loop(); // обязательно вызывать как можно чаще

  const float powerW    = get_power();
  const bool  ocppAllow = ocppPermitsCharge(); // true = сервер разрешает зарядку (Tx разрешена)

  switch (chargeState) {

    case IDLE:
      stationError = NO_ERROR;
      moErrorCode  = nullptr;

      // Переход в START: сервер дал разрешение (фронт ocppAllow)
      if (ocppAllow && !lastOcppAllow) {
        UART0_DEBUG_PORT.println(F("[LOGIC] OCPP: START -> включаем реле, ждём роста мощности"));
        relayOn();
        stateChangeTime = millis();
        chargeState = START_WAIT;

        // Стартуем транзакцию, если её ещё нет (локальный start или RemoteStartTransaction)
        beginOcppTxIfNeeded();
      }
      break;

    case START_WAIT:
      // Успешный старт: мощность > порога
      if (powerW > POWER_THRESHOLD_W) {
        UART0_DEBUG_PORT.printf(
            "[LOGIC] Зарядка началась: power=%.1fW -> CHARGING, выключаем реле\n",
            powerW
        );
        relayOff();
        chargeState = CHARGING;

      } else if (millis() - stateChangeTime > START_STOP_TIMEOUT_MS) {
        // 5 сек прошло, мощность не выросла — ошибка старта
        UART0_DEBUG_PORT.printf(
            "[ERROR] Старт не удался за %lu ms, power=%.1fW -> ERROR_STATE\n",
            START_STOP_TIMEOUT_MS, powerW
        );
        relayOff();
        stationError = START_TIMEOUT_ERROR;
        moErrorCode  = "InternalError";   // отдадим в StatusNotification
        // Попробуем аккуратно завершить транзакцию, если успели её начать
        endOcppTxIfNeeded("start timeout");
        chargeState = ERROR_STATE;
      }
      break;

    case CHARGING:
      // Нормальная работа — зарядка идёт

      // Сервер хочет остановить зарядку (RemoteStopTransaction)
      if (!ocppAllow && lastOcppAllow) {
        UART0_DEBUG_PORT.println(F("[LOGIC] OCPP: STOP -> включаем реле, ждём падения мощности"));
        relayOn();                        // эмуляция "второго тапа" карты
        stateChangeTime = millis();
        chargeState = STOP_WAIT;
      }

      // (по желанию можно добавить локальную остановку,
      //  если powerW внезапно падает < порога)
      break;

    case STOP_WAIT:
      // Условие успешной остановки: мощность < порога за 5 секунд
      if (powerW < POWER_THRESHOLD_W) {
        UART0_DEBUG_PORT.printf(
            "[LOGIC] Зарядка остановилась: power=%.1fW -> IDLE, выключаем реле\n",
            powerW
        );
        relayOff();
        // Нормальное завершение транзакции
        endOcppTxIfNeeded("normal stop");
        chargeState = IDLE;

      } else if (millis() - stateChangeTime > START_STOP_TIMEOUT_MS) {
        // 5 секунд прошло, а мощность не упала — ошибка останова
        UART0_DEBUG_PORT.printf(
            "[ERROR] Остановка не удалась за %lu ms, power=%.1fW -> ERROR_STATE\n",
            START_STOP_TIMEOUT_MS, powerW
        );
        relayOff();
        stationError = STOP_TIMEOUT_ERROR;
        moErrorCode  = "InternalError";
        // Принудительно завершить транзакцию
        endOcppTxIfNeeded("stop timeout");
        chargeState = ERROR_STATE;
      }
      break;

    case ERROR_STATE:
      // Гарантируем, что реле точно выключено
      relayOff();
      // moErrorCode уже установлен выше (START_TIMEOUT_ERROR / STOP_TIMEOUT_ERROR)
      // MicroOcpp через addErrorCodeInput сам отправит Faulted-статус.
      // Здесь можно ждать manual reset / команду сброса из другого модуля
      ESP.restart();
      break;
  }

  lastOcppAllow = ocppAllow;
  debug_measurements_loop();
}


void debug_measurements_loop() {
    static unsigned long lastPrint = 0;
    const unsigned long interval = 3000;   // каждые 3 секунды

    unsigned long now = millis();
    if (now - lastPrint < interval) return;
    lastPrint = now;

    float voltage = get_voltage();
    float current = get_current();
    float power   = get_power();
    float energy  = get_total_energy();   // твоя готовая энергия

    UART0_DEBUG_PORT.print(F("[MEASURE] U="));
    UART0_DEBUG_PORT.print(voltage);
    UART0_DEBUG_PORT.print(F(" V | I="));
    UART0_DEBUG_PORT.print(current);
    UART0_DEBUG_PORT.print(F(" A | P="));
    UART0_DEBUG_PORT.print(power);
    UART0_DEBUG_PORT.print(F(" W | E="));
    UART0_DEBUG_PORT.print(energy, 4);
    UART0_DEBUG_PORT.println(F(" kWh"));
}