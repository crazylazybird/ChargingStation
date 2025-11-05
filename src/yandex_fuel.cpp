// #include "main.h"

// // // ================== НАСТРОЙКИ ==================
// // const char* WIFI_SSID     = "YOUR_SSID";
// // const char* WIFI_PASSWORD = "YOUR_PASSWORD";

// // const char* API_BASE_URL  = "https://app.tst.tanker.yandex.net/api";  // тестовый адрес
// // //const char* API_BASE_URL  = "https://app.tanker.yandex.net/api";    // Боевой адрес

// // const char* API_KEY       = "YOUR_API_KEY_PARAM"; // ключ, который ждёт API в query ?apikey=
// // const char* STATION_ID    = "ALT-001";
// // const char* COLUMN_ID     = "N1";                 // колонка/пистолет (как требует интеграция)

// // const char* DISPENSER_ID  = "D1";                 // если нужно для start()
// // const uint32_t HTTP_TIMEOUT_MS = 10000;
// // const uint32_t POLL_PERIOD_MS  = 5000;

// // // Добавление сертификата
// // // static const char* rootCACertificate = R"EOF(
// // // -----BEGIN CERTIFICATE-----
// // // MIID... (обрезано для примера)
// // // ...==
// // // -----END CERTIFICATE-----
// // // )EOF";

// WiFiClientSecure tlsClient;
// Preferences prefs;



// void wifi_initialize(){
//   WiFi.mode(WIFI_STA);
//   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//   Serial.printf("WiFi connect to %s", WIFI_SSID);
//   while (WiFi.status() != WL_CONNECTED) {
//     Serial.print(".");
//     delay(500);
//   }
//   Serial.printf("\nWiFi ok, IP=%s\n", WiFi.localIP().toString().c_str());

//   init_time(); // желателен для TLS

//   api_ping();
// }

// bool apiPing() {
//   String resp;

//   int code = with_retry([&]() {
//     return api_request("GET", "/v1/ping", "", resp, "");
//   });

//   if (code == 200) {
//     Serial.println("[api] ping ok: " + resp);
//     return true;
//   }

//   Serial.printf("[api] ping fail (%d): %s\n", code, resp.c_str());
//   return false;
// }



// int api_request(const String& method,
//                const String& path,
//                const String& body,
//                String& responseOut,
//                const String& extraQuery = "")
// {
//   HTTPClient http;
//   String url = String(API_BASE_URL) + path;

//   // обязательные query
//   url += "?apikey="    + String(API_KEY)
//       +  "&stationId=" + String(STATION_ID)
//       +  "&columnId="  + String(COLUMN_ID);

//   if (extraQuery.length() > 0) {
//     url += "&" + extraQuery;
//   }

//   WiFiClientSecure tls;

//   // ===== ВЫБОР ВАРИАНТА TLS =====
//   // Продакшн: проверяем сертификат
//   //tls.setCACert(rootCACertificate);

//   // Тест: если нужно обойти проверку — просто раскомментируй строку ниже
//   tls.setInsecure();

//   http.setTimeout(HTTP_TIMEOUT_MS);

//   if (!http.begin(tls, url)) {
//     Serial.println("[http] begin() failed");
//     return -1;
//   }

//   http.addHeader("Accept", "application/json");
//   if (method == "POST" || method == "PATCH") {
//     http.addHeader("Content-Type", "application/json");
//     http.addHeader("Idempotency-Key", uuidV4());
//   }

//   int code = -1;
//   if (method == "GET")       code = http.GET();
//   else if (method == "POST") code = http.POST(body);
//   else if (method == "PATCH")code = http.sendRequest("PATCH", (uint8_t*)body.c_str(), body.length());
//   else {
//     http.end();
//     return -1;
//   }

//   if (code > 0) {
//     responseOut = http.getString();
//     Serial.printf("[http] %s %s -> %d\n", method.c_str(), url.c_str(), code);
//   } else {
//     Serial.printf("[http] failed: %s\n", http.errorToString(code).c_str());
//   }

//   http.end();
//   return code;
// }

// // Синхронизации времени
// bool time_is_set() {
//   time_t now = time(nullptr);
//   return now > 1609459200; // > 2021-01-01
// }


// // Инициализация NTP
// void init_time() {
//   configTzTime("UTC0", "pool.ntp.org", "time.nist.gov", nullptr);
//   Serial.print("Sync time");
//   for (int i = 0; i < 20 && !time_is_set(); ++i) {
//     Serial.print(".");
//     delay(500);
//   }
//   Serial.println(time_is_set() ? " OK" : " FAIL");
// }

// // UUID
// String uuidV4() {
//   uint8_t r[16];
//   for (int i = 0; i < 16; ++i) 
//     r[i] = (uint8_t)esp_random();

//   // Версия 4 + вариант RFC 4122
//   r[6] = (r[6] & 0x0F) | 0x40;
//   r[8] = (r[8] & 0x3F) | 0x80;

//   char buf[37];
//   snprintf(buf, sizeof(buf),
//            "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
//            r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7],
//            r[8], r[9], r[10], r[11], r[12], r[13], r[14], r[15]);
//   return String(buf);
// }

// // Простой экспоненциальный бэкофф в пределах попыток
// template<typename Fn>
// int with_retry(Fn fn, int maxAttempts, uint32_t baseDelayMs) {
//   for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
//     int code = fn();
//     if (code >= 200 && code < 300) return code;
//     if (attempt < maxAttempts) {
//       uint32_t sleepMs = baseDelayMs << (attempt - 1);
//       Serial.printf("[retry] attempt %d failed (code %d), sleep %ums\n", attempt, code, sleepMs);
//       delay(sleepMs);
//     } else {
//       return code;
//     }
//   }
//   return -1;
// }


// static const char* kOrderRespLog = "/order_responses.jsonl";

// // helper: дописать строку в файл
// static bool appendJsonLine(const char* path, const String& jsonLine) {
//   File f = SPIFFS.open(path, FILE_APPEND, true);
//   if (!f) return false;
//   bool ok = f.print(jsonLine) && f.print('\n');
//   f.close();
//   return ok;
// }

// bool postTankerOrder(const JsonDocument& orderDoc, JsonDocument& respDoc) {
//   // 1) сериализуем тело
//   String body; 
//   serializeJson(orderDoc, body);

//   // 2) отправляем
//   String resp; 
//   int code = with_retry([&]() {
//     // apiRequest добавит ?apikey=...&stationId=...&columnId=...
//     return api_request("POST", "/tanker/order", body, resp, "");
//   }, 3, 500);

//   // 3) логирование «сырое»
//   Serial.printf("[api] /tanker/order -> %d\n", code);
//   // на всякий — сохраняем «как есть» (для аудита/дебага)
//   (void)appendJsonLine(kOrderRespLog, resp);

//   // 4) успешный ответ → парсим/сохраняем
//   if (code == 200) {
//     DeserializationError err = deserializeJson(respDoc, resp);
//     if (err) {
//       Serial.printf("[api] parse error: %s\n", err.c_str());
//       return false;
//     }
//     // также положим последний ответ в NVS
//     Preferences prefs;
//     prefs.begin("tanker", false);
//     prefs.putString("last_order_resp", resp);
//     prefs.end();

//     Serial.println("[api] /tanker/order OK");
//     return true;
//   }

//   // 5) типовые ошибки по спецификации
//   if (code == 400) {
//     Serial.printf("[api] 400 BAD_REQUEST: %s\n", resp.c_str());
//   } else if (code == 402) {
//     Serial.printf("[api] 402 FuelId+PriceFuel mismatch: %s\n", resp.c_str());
//   } else if (code >= 500) {
//     Serial.printf("[api] %d SERVER ERROR: %s\n", code, resp.c_str());
//   } else {
//     Serial.printf("[api] FAIL %d: %s\n", code, resp.c_str());
//   }
//   return false;
// }

// bool postTankerAccept(const String& orderId, JsonDocument& respDoc) {
//   String resp;

//   int code = with_retry([&]() {
//     // extraQuery добавит orderId=... к URL
//     return api_request("POST", "/tanker/accept", /*body=*/"", resp, "orderId=" + orderId);
//   }, 3, 500);

//   Serial.printf("[api] /tanker/accept -> %d\n", code);

//   if (code == 200) {
//     DeserializationError err = deserializeJson(respDoc, resp);
//     if (err) {
//       Serial.printf("[api] /tanker/accept parse error: %s\n", err.c_str());
//       return false;
//     }
//     return true;
//   }

//   if (code == 400) Serial.printf("[api] /tanker/accept 400 BAD_REQUEST: %s\n", resp.c_str());
//   else if (code >= 500) Serial.printf("[api] /tanker/accept %d SERVER ERROR: %s\n", code, resp.c_str());
//   else Serial.printf("[api] /tanker/accept FAIL %d: %s\n", code, resp.c_str());
//   return false;
// }