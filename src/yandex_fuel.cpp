#include "main.h"

const char* WIFI_SSID     = "YOUR_SSID";
const char* WIFI_PASSWORD = "YOUR_PASSWORD";

const char* API_BASE_URL  = "https://app.tst.tanker.yandex.net/api"; // тест
// const char* API_BASE_URL  = "https://app.tanker.yandex.net/api";   // прод

const char* API_KEY       = "YOUR_API_KEY"; // Bearer токен

// Станции/оборудование
const char* STATION_ID    = "ALT-001";
const char* NOZZLE_ID     = "N1";
const char* DISPENSER_ID  = "D1";

const uint32_t POLL_PERIOD_MS = 5000;   // опрос новых заказов каждые 5с
const uint32_t HTTP_TIMEOUT_MS = 10000; // таймаут HTTP

// Если хотите pinning/verify, добавьте корневой сертификат сервера сюда
// static const char* ROOT_CA = R"EOF(
// -----BEGIN CERTIFICATE-----
// ...
// -----END CERTIFICATE-----
// )EOF";

WiFiClientSecure tlsClient;
Preferences prefs;



void wifi_initialize(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WiFi connect to %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.printf("\nWiFi ok, IP=%s\n", WiFi.localIP().toString().c_str());

  init_time(); // желателен для TLS

  api_ping();
}

bool api_ping() {
  String resp;
  int code = with_retry([&]() {
    return api_request("GET", "/v1/ping", "", resp);
  });
  if (code == 200) {
    Serial.println("[api] ping ok: " + resp);
    return true;
  }
  Serial.printf("[api] ping fail (%d)\n", code);
  return false;
}

int api_request(const String& method, const String& path, const String& body, String& responseOut) {
  HTTPClient http;
  String url = String(API_BASE_URL) + path;

  // HTTPS клиент
  // tlsClient.setCACert(ROOT_CA);         // если используете корневой сертификат
  tlsClient.setInsecure();                 // !!! для тестов
  http.setConnectTimeout(HTTP_TIMEOUT_MS);
  http.setTimeout(HTTP_TIMEOUT_MS);

  if (!http.begin(tlsClient, url)) {
    Serial.println("[http] begin() failed");
    return -1;
  }

  // Заголовки
  http.addHeader("Authorization", String("Bearer ") + API_KEY);
  http.addHeader("Accept", "application/json");
  if (method == "POST" || method == "PATCH") {
    http.addHeader("Content-Type", "application/json");
    // Идемпотентный ключ для безопасных повторов
    http.addHeader("Idempotency-Key", uuidV4());
  }

  // Отправка
  int httpCode = -1;
  if (method == "GET") {
    httpCode = http.GET();
  } else if (method == "POST") {
    httpCode = http.POST(body);
  } else if (method == "PATCH") {
    httpCode = http.sendRequest("PATCH", (uint8_t*)body.c_str(), body.length());
  } else {
    http.end();
    Serial.println("[http] unsupported method");
    return -1;
  }

  // Ответ
  if (httpCode > 0) {
    responseOut = http.getString();
    Serial.printf("[http] %s %s -> %d\n", method.c_str(), url.c_str(), httpCode);
    // Serial.println(responseOut);
  } else {
    Serial.printf("[http] request failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
  return httpCode;
}


// Синхронизации времени
bool time_is_set() {
  time_t now = time(nullptr);
  return now > 1609459200; // > 2021-01-01
}


// Инициализация NTP
void init_time() {
  configTzTime("UTC0", "pool.ntp.org", "time.nist.gov", nullptr);
  Serial.print("Sync time");
  for (int i = 0; i < 20 && !time_is_set(); ++i) {
    Serial.print(".");
    delay(500);
  }
  Serial.println(time_is_set() ? " OK" : " FAIL");
}

// UUID
String uuidV4() {
  uint8_t r[16];
  for (int i = 0; i < 16; ++i) 
    r[i] = (uint8_t)esp_random();

  // Версия 4 + вариант RFC 4122
  r[6] = (r[6] & 0x0F) | 0x40;
  r[8] = (r[8] & 0x3F) | 0x80;

  char buf[37];
  snprintf(buf, sizeof(buf),
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
           r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
  return String(buf);
}

// Простой экспоненциальный бэкофф в пределах попыток
template<typename Fn>
int with_retry(Fn fn, int maxAttempts, uint32_t baseDelayMs) {
  for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
    int code = fn();
    if (code >= 200 && code < 300) return code;
    if (attempt < maxAttempts) {
      uint32_t sleepMs = baseDelayMs << (attempt - 1);
      Serial.printf("[retry] attempt %d failed (code %d), sleep %ums\n", attempt, code, sleepMs);
      delay(sleepMs);
    } else {
      return code;
    }
  }
  return -1;
}