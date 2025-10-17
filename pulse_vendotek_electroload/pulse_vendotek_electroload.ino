//список команд управления калибровой, выводом в порты, настрокой доительностей
//смотри в конце кода в PrintHelp
#include <EEPROM.h>
#include <Arduino.h>
#include <SoftwareSerial.h>

// Создаем объект программного UART
SoftwareSerial mySerial(14, 16);  // RX-pin 14, TX-pin 16 - для связи с ESP32 по UART

// Константы системы
#define SAMPLES 10  // Количество выборок для измерения
// Константы для учета энергии
#define ENERGY_UPDATE_INTERVAL 1000  // интервал обновления (мс)
#define ENERGY_SCALE 3600000.0f      // коэффициент перевода в кВт*ч
#define DATA_VERSION 1               // Версия данных
#define EEPROM_VERSION_ADDR 0        // Адрес версии в EEPROM
#define EEPROM_DATA_ADDR 2           // Адрес данных в EEPROM

// Пины системы
#define CURRENT_POS A0  // Вход тока +
#define CURRENT_NEG A1  // Вход тока -
#define VOLTAGE_POS A2  // Вход напряжения +
#define VOLTAGE_NEG A3  // Вход напряжения -
#define ENERGY_PIN 2    // Вход импульсов энергии
#define RELAY_PIN 3        // Выход реле
#define BUZZER_PIN 10  // Звуковой сигнал
#define LED_PIN 17      // Индикатор

//назначаем UART_TX_PIN 8 - для связи с ESP32 по UART - только передача в одну сторону 
//см. что добалено изменение состояние этого пина в функции звука beep, 
//зачем? - для контроля работы пина? но уже было проаерено, что передача по UART передается
#define UART_TX_PIN 8  

// Структуры данных
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

// Глобальные переменные
SystemData sysData;
volatile bool soundActive = false;
unsigned long soundStartTime = 0;
unsigned long soundDuration = 0;
bool soundEnable = true;
unsigned long debugPeriod = 1000;       // период вывода отладочной информации (мс)
unsigned long energySendPeriod = 1000;  // период вывода Energy информации (мс)
unsigned long lastDebugTime = 0;        // время последнего вывода
unsigned long lastEnergySendTime = 0;   // время последнего вывода
bool debugMode = true;                  // флаг режима отладки
bool energySend = true;                 // флаг отправки данных на контроллер
bool energySendTo = true;               // флаг отправки данных на контроллер

// Инициализация системы
void setup() {
  // Настройка пинов
  pinMode(ENERGY_PIN, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(UART_TX_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Начальные состояния
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(UART_TX_PIN, HIGH);
  digitalWrite(LED_PIN, LOW);
  sysData.lastUpdate = millis();  // Инициализация времени обновления


  beep(3, 150);

  // Serial-порт
  Serial.begin(115200);
  mySerial.begin(9600); //- для связи с ESP32 по UART
  mySerial.println("Test Serial Ok");
  // Загрузка конфигурации
  loadConfiguration();
}

// Измерение тока
float readCurrent() {
  int32_t posSum = 0, negSum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    posSum += analogRead(CURRENT_POS);
    negSum += analogRead(CURRENT_NEG);
    delay(1);
  }
  float raw = (float)(posSum - negSum) / SAMPLES - sysData.calib.currOffset;

  if(raw >= 0)
    return raw * sysData.calib.currScale;
  else
    return 0;
}

// Измерение напряжения
float readVoltage() {
  int32_t posSum = 0, negSum = 0;
  for (int i = 0; i < SAMPLES; i++) {
    posSum += analogRead(VOLTAGE_POS);
    negSum += analogRead(VOLTAGE_NEG);
    delay(1);
  }
  float raw = (float)(posSum - negSum) / SAMPLES - sysData.calib.voltOffset;  
  if(raw >= 0)
    return raw * sysData.calib.voltScale;
  else
    return 0;
  
}

// Управление реле
void relayOn() {
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Реле включено");
}

void relayOff() {
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("Реле выключено");
}

// Неблокирующая Звуковая индикация: установка длительности и включение сигнала для неблокируюющего процесса
void startSound(unsigned long duration) {
  soundActive = true;
  soundStartTime = millis();
  soundDuration = duration;
  digitalWrite(BUZZER_PIN, LOW);
}

//отключить звуковой сигнал
void stopSound() {
  soundActive = false;
  digitalWrite(BUZZER_PIN, HIGH);
}

//проверка длительности звук.сигнала и отключение по таймеру millis()
void handleSound() {
  if (soundActive) {
    if (millis() - soundStartTime >= soundDuration) {
      stopSound();
    }
  }
}

//Калибровка тока:
void calibrateCurrent(float referenceValue) {
  Serial.print("Калибровка тока на ");
  Serial.print(referenceValue);
  Serial.println(" А");

  // Калибровка нуля (если передано 0)
  if (referenceValue == 0.0f) {
    int32_t sum = 0;
    for (int i = 0; i < SAMPLES * 20; i++) {
      sum += analogRead(CURRENT_POS) - analogRead(CURRENT_NEG);
      delayMicroseconds(100);
    }
    sysData.calib.currOffset = sum / (SAMPLES * 20);
    saveConfiguration();
    Serial.print("Смещение установлено: ");
    Serial.println(sysData.calib.currOffset);
  }
  // Калибровка масштаба
  else {
    int32_t sum = 0;
    for (int i = 0; i < SAMPLES * 20; i++) {
      sum += analogRead(CURRENT_POS) - analogRead(CURRENT_NEG);
      delayMicroseconds(100);
    }
    float rawValue = (sum / (SAMPLES * 20)) - sysData.calib.currOffset;
    sysData.calib.currScale = referenceValue / rawValue;
    saveConfiguration();
    Serial.print("Коэффициент: ");
    Serial.println(sysData.calib.currScale);
  }
}

//Калибровка напряжения:
void calibrateVoltage(float referenceValue) {
  Serial.print("Калибровка напряжения на ");
  Serial.print(referenceValue);
  Serial.println(" В");

  // Калибровка нуля (если передано 0)
  if (referenceValue == 0.0f) {
    int32_t sum = 0;
    for (int i = 0; i < SAMPLES * 20; i++) {
      sum += analogRead(VOLTAGE_POS) - analogRead(VOLTAGE_NEG);
      delayMicroseconds(100);
    }
    sysData.calib.voltOffset = sum / (SAMPLES * 20);
    saveConfiguration();
    Serial.print("Смещение установлено: ");
    Serial.println(sysData.calib.voltOffset);
  }
  // Калибровка масштаба
  else {
    int32_t sum = 0;
    for (int i = 0; i < SAMPLES * 20; i++) {
      sum += analogRead(VOLTAGE_POS) - analogRead(VOLTAGE_NEG);
      delayMicroseconds(100);
    }
    float rawValue = (sum / (SAMPLES * 20)) - sysData.calib.voltOffset;
    sysData.calib.voltScale = referenceValue / rawValue;
    saveConfiguration();
    Serial.print("Коэффициент: ");
    Serial.println(sysData.calib.voltScale);
  }
}

// Функция измерения мощности
float calculatePower() {
  return readCurrent() * readVoltage();
}

// Функция учета энергии
void updateEnergy() {
  static float energySum = 0.0f;
  // Получаем текущую мощность
  float power = calculatePower();

  // Накопление энергии с учетом времени в кВт-ч
  energySum += power * (millis() - sysData.lastUpdate) / 3600000000.0f;

  // Обновление каждые ENERGY_UPDATE_INTERVAL мс
  if (millis() - sysData.lastUpdate >= ENERGY_UPDATE_INTERVAL) {
    sysData.totalEnergy += energySum;
    sysData.lastUpdate = millis();
    energySum = 0.0f;
    saveConfiguration();
  }
}

// Основной цикл
void loop() {
  handleSound();
  updateEnergy();

  // Обработка команд
  while (Serial.available() > 0) {
    String cmd = Serial.readStringUntil('\n');
    processCommand(cmd);
  }

  // while (mySerial.available() > 0) {
  //   String cmd = mySerial.readStringUntil('\n');
  //   processCommandMySerial(cmd);
  // }
  processMySerial();
//вывод данных в порт с периодичностью energySendPeriod
  if (energySend) {
    if (millis() - lastEnergySendTime >= energySendPeriod) {
      lastEnergySendTime = millis();

  //выбор порта отправки данных по команде 'X' с изменением значения energySendTo

    //в порт на ESP32
      if (energySendTo) {
        sendEnergyData();
        // mySerial.print("energyData ");
        // mySerial.print(readCurrent(), 2);
        // mySerial.print(" ");
        // mySerial.print(readVoltage(), 2);
        // mySerial.print(" ");
        // mySerial.print(calculatePower(), 2);
        // mySerial.print(" ");
        // mySerial.println(sysData.totalEnergy, 4);
        // mySerial.print("I: ");
        // mySerial.println(readCurrent(), 2);
        // mySerial.print("U: ");
        // mySerial.println(readVoltage(), 2);
        // mySerial.print("P: ");
        // mySerial.println(calculatePower(), 2);
        // mySerial.print("E: ");
        // mySerial.println(sysData.totalEnergy, 4);
      } else {
      //в USB-порт
        // Serial.print("energyData ");
        // Serial.print(readCurrent(), 2);
        // Serial.print(" ");
        // Serial.print(readVoltage(), 2);
        // Serial.print(" ");
        // Serial.print(calculatePower(), 2);
        // Serial.print(" ");
        // Serial.println(sysData.totalEnergy, 4);
        // Serial.print("I: ");
        // Serial.println(readCurrent(), 2);
        // Serial.print("U: ");
        // Serial.println(readVoltage(), 2);
        // Serial.print("P: ");
        // Serial.println(calculatePower(), 2);
        // Serial.print("E: ");
        // Serial.println(sysData.totalEnergy, 4);
      }
    }
  }

  if (debugMode) {
    if (millis() - lastDebugTime >= debugPeriod) {
      lastDebugTime = millis();

      Serial.println("\n--- ОТЛАДОЧНЫЕ ДАННЫЕ ---");
      Serial.print("Ток (А): ");
      Serial.print(readCurrent(), 2);
      Serial.println();
      Serial.print("Напряжение (В): ");
      Serial.print(readVoltage(), 2);
      Serial.println();
      Serial.print("Мощность (Вт): ");
      Serial.print(calculatePower(), 2);
      Serial.println();
      Serial.print("Энергия (кВтч): ");
      Serial.print(sysData.totalEnergy, 4);
      Serial.println();

      Serial.print("Сырые данные тока: ");
      Serial.print(analogRead(CURRENT_POS));
      Serial.print(" - ");
      Serial.println(analogRead(CURRENT_NEG));

      Serial.print("Сырые данные напряжения: ");
      Serial.print(analogRead(VOLTAGE_POS));
      Serial.print(" - ");
      Serial.println(analogRead(VOLTAGE_NEG));

      Serial.print("Состояние реле: ");
      Serial.println(digitalRead(RELAY_PIN) ? "ВЫКЛ" : "ВКЛ");

      Serial.print("Звуковая индикация: ");
      Serial.println(soundEnable ? "ВКЛ" : "ВЫКЛ");
    }
  }
}


// ---------- CRC16 ----------
uint16_t calculate_CRC16(const uint8_t* data, int length) {
    uint16_t crc = 0xFFFF;
    for (int pos = 0; pos < length; pos++) {
        crc ^= (uint16_t)data[pos];
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

// ---------- DEC -> BCD ----------
byte dec2bcd(int val) {
    return ((val / 10) << 4) | (val % 10);
}

void put_bcd2(uint8_t* buf, int& index, int value) {
    buf[index++] = dec2bcd(value / 100);
    buf[index++] = dec2bcd(value % 100);
}

void put_bcd4(uint8_t* buf, int& index, long value) {
    buf[index++] = dec2bcd((value / 1000000) % 100);
    buf[index++] = dec2bcd((value / 10000) % 100);
    buf[index++] = dec2bcd((value / 100) % 100);
    buf[index++] = dec2bcd(value % 100);
}

// ---------- ОТПРАВКА ДАННЫХ ----------
void sendEnergyData() {
    uint8_t buffer[64];
    int index = 0;

    float I = readCurrent();
    float U = readVoltage();
    float P = calculatePower();
    float E = sysData.totalEnergy;

    // ==== Маркер начала пакета ====
    buffer[index++] = 0xAA;
    buffer[index++] = 0x55;

    // Напряжение (BCD, шаг 0.1 В)
    put_bcd2(buffer, index, (int)(U * 10));

    // Ток (BCD, шаг 0.01 А)
    put_bcd2(buffer, index, (int)(I * 100));

    // Мощность (BCD, шаг 0.1 Вт)
    put_bcd2(buffer, index, (int)(P * 10));

    // Энергия (BCD, шаг 0.001 кВт·ч)
    put_bcd4(buffer, index, (long)(E * 1000));

    // ==== CRC ====
    uint16_t crc = calculate_CRC16(buffer, index); // считаем по всем от 0xAA до конца данных
    buffer[index++] = (crc >> 8) & 0xFF;
    buffer[index++] = crc & 0xFF;

    // ==== Маркер конца ====
    buffer[index++] = 0x0D;


    // Отправляем пакет
    mySerial.write(buffer, index);

    // Serial.print("Отправлен пакет длиной ");
    // Serial.println(index);
    // // === Отладка HEX ===
    // Serial.print("TX пакет (len=");
    // Serial.print(index);
    // Serial.print(") CRC=");
    // Serial.print(crc, HEX);
    // Serial.print(" : ");
    // for (int i = 0; i < index; i++) {
    //     if (buffer[i] < 0x10) Serial.print("0");
    //     Serial.print(buffer[i], HEX);
    //     Serial.print(" ");
    // }
    // Serial.println();
}


#define STX1 0xAA
#define STX2 0x55
#define RX_MAX 128         // максимум полезной нагрузки
#define RX_TIMEOUT_MS 50   // тайм-аут «тишины» при приёме кадра

enum RxState { WAIT_STX1, WAIT_STX2, READ_LEN_H, READ_LEN_L, READ_PAYLOAD, READ_CRC_H, READ_CRC_L };
static RxState rxState = WAIT_STX1;

static uint16_t rxLen = 0;
static uint16_t rxIdx = 0;
static uint8_t  rxBuf[RX_MAX];   // только PAYLOAD
static uint16_t rxCRC_recv = 0;
static uint32_t lastByteMs = 0;

void resetRx() {
  rxState = WAIT_STX1;
  rxLen = rxIdx = 0;
  rxCRC_recv = 0;
}

void processMySerial() {
  // тайм-аут: если поток «завис», сбрасываемся
  if (rxState != WAIT_STX1 && (millis() - lastByteMs) > RX_TIMEOUT_MS) {
    resetRx();
  }

  while (mySerial.available() > 0) {
    uint8_t c = mySerial.read();
    lastByteMs = millis();

    switch (rxState) {
      case WAIT_STX1:
        if (c == STX1) rxState = WAIT_STX2;
        break;

      case WAIT_STX2:
        if (c == STX2) rxState = READ_LEN_H;
        else rxState = WAIT_STX1;
        break;

      case READ_LEN_H:
        rxLen = (uint16_t)c << 8;
        rxState = READ_LEN_L;
        break;

      case READ_LEN_L:
        rxLen |= c;
        if (rxLen == 0 || rxLen > RX_MAX) { // защита от переполнения/нONSENSE
          resetRx();
          break;
        }
        rxIdx = 0;
        rxState = READ_PAYLOAD;
        break;

      case READ_PAYLOAD:
        rxBuf[rxIdx++] = c;
        if (rxIdx >= rxLen) rxState = READ_CRC_H;
        break;

      case READ_CRC_H:
        rxCRC_recv = (uint16_t)c << 8;
        rxState = READ_CRC_L;
        break;

      case READ_CRC_L: {
        rxCRC_recv |= c;
        // проверяем CRC по PAYLOAD
        uint16_t rxCRC_calc = calculate_CRC16(rxBuf, rxLen);
        if (rxCRC_calc == rxCRC_recv) {
          // преобразуем PAYLOAD в строку команды (если ASCII)
          String cmd;
          cmd.reserve(rxLen);
          for (uint16_t i = 0; i < rxLen; ++i) cmd += (char)rxBuf[i];

          processCommandMySerial(cmd);
        } else {
          Serial.println(F("Ошибка CRC"));
          beep(3, 30);
        }
        resetRx();
        break;
      }
    }
  }
}




// Альтернативный вариант с более сложным алгоритмом контрольной суммы
uint16_t calculateDataChecksum() {
  uint16_t sum = 0;
  uint8_t* data = (uint8_t*)&sysData;

  for (size_t i = 0; i < sizeof(SystemData) - sizeof(uint16_t); i++) {
    sum += data[i];
    sum = (sum << 1) | (sum >> 15);  // Простой бит-манипуляционный сдвиг
  }

  return sum;
}
// Функция расчета контрольной суммы калибровочных данных
uint16_t calculateCalibChecksum() {
  uint16_t sum = 0;
  uint8_t* data = (uint8_t*)&sysData.calib;
  for (size_t i = 0; i < sizeof(CalibrationData) - 2; i++) {
    sum += data[i];
  }
  return sum;
}

// Функция сохранения конфигурации
void saveConfiguration() {
  sysData.calib.checksum = calculateCalibChecksum();
  sysData.dataChecksum = calculateDataChecksum();
  EEPROM.put(EEPROM_DATA_ADDR, sysData);
}

// Функция загрузки конфигурации
void loadConfiguration() {
  EEPROM.get(EEPROM_DATA_ADDR, sysData);
}

// Функция сброса счетчика энергии
void resetEnergyCounter() {
  sysData.totalEnergy = 0.0;
  saveConfiguration();
  Serial.println("Счетчик энергии сброшен");
}

// Функция вывода измерений
void printMeasurements() {
  Serial.println("--- Измерения ---");
  Serial.print("Ток: ");
  Serial.print(readCurrent(), 2);
  Serial.println(" А");
  Serial.print("Напряжение: ");
  Serial.print(readVoltage(), 2);
  Serial.println(" В");
  Serial.print("Мощность: ");
  Serial.print(calculatePower(), 2);
  Serial.println(" Вт");
  Serial.print("Энергия: ");
  Serial.print(sysData.totalEnergy, 4);
  Serial.println(" кВт*ч");
  Serial.print("Сырые данные тока: ");
  Serial.print(analogRead(CURRENT_POS));
  Serial.print(" - ");
  Serial.println(analogRead(CURRENT_NEG));

  Serial.print("Сырые данные напряжения: ");
  Serial.print(analogRead(VOLTAGE_POS));
  Serial.print(" - ");
  Serial.println(analogRead(VOLTAGE_NEG));
}

// Функция генерации звукового сигнала
void beep(int count, unsigned long duration) {
  if (!soundEnable) return;  // Проверка включения звука

  for (int i = 0; i < count; i++) {
    digitalWrite(BUZZER_PIN, LOW);  // Включение звука
    digitalWrite(UART_TX_PIN, LOW);  // Включение звука
    delay(duration);
    digitalWrite(BUZZER_PIN, HIGH);  // Выключение звука
    digitalWrite(UART_TX_PIN, HIGH);  // Выключение звука
    if (i < count - 1) {
      delay(duration / 2);  // Пауза между сигналами
    }
  }
}

void debugMeasurements() {
  if (debugMode) {
    Serial.println("Отладочные измерения:");
    Serial.print("Сырые данные тока: ");
    Serial.print(analogRead(CURRENT_POS));
    Serial.print(" - ");
    Serial.println(analogRead(CURRENT_NEG));

    Serial.print("Сырые данные напряжения: ");
    Serial.print(analogRead(VOLTAGE_POS));
    Serial.print(" - ");
    Serial.println(analogRead(VOLTAGE_NEG));
  }
}

//вывод калибровочных данных
void printCalibrationData() {
  Serial.println("=== КАЛИБРОВОЧНЫЕ ДАННЫЕ ==");
  Serial.print("Смещение тока: ");
  Serial.print(sysData.calib.currOffset);
  Serial.println(" ед.АЦП");

  Serial.print("Коэффициент тока: ");
  Serial.print(sysData.calib.currScale, 6);
  Serial.println(" А/ед.АЦП");

  Serial.print("Смещение напряжения: ");
  Serial.print(sysData.calib.voltOffset);
  Serial.println(" ед.АЦП");

  Serial.print("Коэффициент напряжения: ");
  Serial.print(sysData.calib.voltScale, 6);
  Serial.println(" В/ед.АЦП");

  Serial.print("Контрольная сумма: 0x");
  Serial.println(sysData.calib.checksum, HEX);
}

void processCommandMySerial(String cmd){
  if (cmd.length() == 0) return;

  char command = toupper(cmd[0]);
  String param = cmd.substring(1);
  param.trim();

  switch (command) {
    case 'R':
      if (param == "ON") {
        relayOn();
        beep(2, 50);
      } else if (param == "OFF") {
        relayOff();
        beep(1, 75);
      } else {
        Serial.println(F("Ошибка: используйте ON/OFF"));
        beep(3, 50);
      }
      break;
    case 'E':
      resetEnergyCounter();
      break;  
    default:
      Serial.println(F("Неизвестная команда! Введите '?' для справки"));
      beep(3, 30);
  }
}

// Функция обработки команд
void processCommand(String cmd) {
  if (cmd.length() == 0) return;

  char command = toupper(cmd[0]);
  String param = cmd.substring(1);
  param.trim();

  switch (command) {
    case '?':
      printHelp();
      // beep(1, 100);
      break;

    case 'M':
      printMeasurements();
      // beep(1, 50);
      break;

    case 'C':
      printCalibrationData();
      // beep(2, 50);
      break;

    case 'T':  // Калибровка тока
    case 'I':
      if (param.length() > 0) {
        float refValue = param.toFloat();
        if (refValue >= 0) {
          calibrateCurrent(refValue);
        } else {
          Serial.println(F("Ошибка: значение должно быть положительным"));
          beep(3, 50);
        }
      } else {
        Serial.println(F("Ошибка: укажите значение тока"));
        beep(3, 50);
      }
      break;

    case 'V':  // Калибровка напряжения
    case 'U':
      if (param.length() > 0) {
        float refValue = param.toFloat();
        if (refValue >= 0) {
          calibrateVoltage(refValue);
        } else {
          Serial.println(F("Ошибка: значение должно быть положительным"));
          beep(3, 50);
        }
      } else {
        Serial.println(F("Ошибка: укажите значение напряжения"));
        beep(3, 50);
      }
      break;

    case 'R':  // Управление реле
      if (param == "ON") {
        relayOn();
        beep(2, 50);
      } else if (param == "OFF") {
        relayOff();
        beep(1, 75);
      } else {
        Serial.println(F("Ошибка: используйте ON/OFF"));
        beep(3, 50);
      }
      break;

    case 'S':  // Звуковая индикация
      soundEnable = !soundEnable;
      Serial.print(F("Звуковая сигнализация "));
      Serial.println(soundEnable ? "ВКЛ" : "ВЫКЛ");
      beep(1, 50);
      break;

    case 'D':
      if (param.length() > 0) {
        // Установка нового периода отладки
        debugPeriod = param.toInt();
        if (debugPeriod < 100) debugPeriod = 100;  // защита от слишком частых выводимых данных
        Serial.print(F("Период отладки установлен: "));
        Serial.print(debugPeriod);
        Serial.println(F(" мс"));
        debugMode = true;
      } else {
        debugMode = !debugMode;
      }
      Serial.print(F("Режим отладки "));
      Serial.println(debugMode ? "ВКЛ" : "ВЫКЛ");
      beep(2, 50);
      break;

    case 'X':
      energySendTo = !energySendTo;
      beep(2, 50);
      Serial.print(F("вывод energyData to: "));
      Serial.println(energySendTo);
      break;

    case 'G':
      if (param.length() > 0) {
        // Установка нового периода отладки
        energySendPeriod = param.toInt();
        if (energySendPeriod < 100) energySendPeriod = 100;  // защита от слишком частых выводимых данных
        Serial.print(F("Период отправки EnergyData установлен: "));
        Serial.print(energySendPeriod);
        Serial.println(F(" мс"));
        energySend = true;
      } else {
        energySend = !energySend;
      }
      Serial.print(F("вывод EnergyDta "));
      Serial.println(energySend ? "ВКЛ" : "ВЫКЛ");
      beep(2, 50);
      break;

    case 'E':  // Сброс счетчика
      resetEnergyCounter();
      break;

      // case 'N': // Установка порога тока
      //     if(param.length() > 0) {
      //         float newThreshold = param.toFloat();
      //         if(newThreshold >= 0.1f && newThreshold <= 10.0f) {
      //             MIN_CURRENT_THRESHOLD = newThreshold;
      //             Serial.print(F("Минимальный порог тока установлен: "));
      //             Serial.print(newThreshold, 1);
      //             Serial.println(F(" А"));
      //             beep(2, 100);
      //         } else {
      //             Serial.println(F("Ошибка: порог должен быть от 0.1 до 10.0 А"));
      //             beep(3, 50);
      //         }
      //     }
      //     break;

    case 'B':  // Управление звуком
      if (param.length() > 0) {
        int duration = param.toInt();
        if (duration > 0) {
          startSound(duration);
        }
      } else {
        stopSound();
      }
      break;

    default:
      Serial.println(F("Неизвестная команда! Введите '?' для справки"));
      beep(3, 30);
  }
}

void printHelp() {
  Serial.println(F("\n=== СИСТЕМА КОМАНД ==="));
  Serial.println(F("? - показать эту справку"));
  Serial.println(F("M - показать текущие измерения"));
  Serial.println(F("C - показать калибровочные данные"));
  Serial.println(F("T <значение> - калибровка тока (в Амперах)"));
  Serial.println(F("   Пример: T 5.0 - калибровка на 5 Ампер"));
  Serial.println(F("   T 0 - калибровка нуля тока"));
  Serial.println(F("V <значение> - калибровка напряжения (в Вольтах)"));
  Serial.println(F("   Пример: V 220 - калибровка на 220 Вольт"));
  Serial.println(F("   V 0 - калибровка нуля напряжения"));
  Serial.println(F("R ON/OFF - управление реле"));
  Serial.println(F("S - включить/выключить звуковую индикацию"));
  Serial.println(F("D - включить/выключить режим отладки"));
  Serial.println(F("E - сбросить счетчик накопленной энергии"));
  Serial.println(F("N <значение> - установить минимальный порог тока (0.1-10.0 А)"));
  Serial.println(F("B <длительность> - включить звуковой сигнал"));
  Serial.println(F("B - выключить звуковой сигнал"));
  Serial.println(F("H - показать эту справку (альтернатива ?)"));
  Serial.println(F("\nПримеры использования:"));
  Serial.println(F("M - показать текущие измерения"));
  Serial.println(F("T 0 - калибровка нуля тока"));
  Serial.println(F("V 230 - калибровка напряжения на 230В"));
  Serial.println(F("R ON - включить реле"));
  Serial.println(F("S - переключить звук"));
  Serial.println(F("E - сбросить счетчик"));
  Serial.println(F("B 1000 - звуковой сигнал на 1 секунду"));
}
