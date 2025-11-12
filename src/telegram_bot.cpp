#include "main.h"

// WiFi credentials
//const char* ssid = "Anton";
//const char* password = "11223360";
const char* ssid = "VINcoder";
const char* password = "1234567890";

// Dominion API Key
const char* apiKey = "oPhrCfp_UfMBKst2P3mM9n-Ti2tXnL9A0zeJzrMa9qE";

unsigned long previousMillis = 0;
const unsigned long interval = 1000; // 1 секунда

extern transactions payment;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// Инициализация WiFi
void init_wifi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
    timeClient.begin();
}

String getISO8601Time() {
  timeClient.update();

  unsigned long epochTime = timeClient.getEpochTime();
  int hours = (epochTime  % 86400L) / 3600;
  int minutes = (epochTime % 3600) / 60;
  int seconds = epochTime % 60;
  
  // Время в формате YYYY-MM-DDTHH:mm:ss.sssZ
  // Миллисекунды получить из millis() % 1000
  unsigned int ms = millis() % 1000;

  // Преобразуем в необходимый формат
  char buffer[30];
  struct tm * timeinfo = gmtime((time_t *)&epochTime);
  snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ",
           timeinfo->tm_year + 1900,
           timeinfo->tm_mon + 1,
           timeinfo->tm_mday,
           hours,
           minutes,
           seconds,
           ms);

  return String(buffer);
}

// Отправка JSON POST на сервер с API Key
void send_POST_json(String occurred_at, int amount_paid, int refund_amount, float kwh_spent) {

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://188.124.37.211:8000/ingest/payments"); // адрес сервера

        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-Station-Key", apiKey); // добавляем ключ Dominion

        // Формируем JSON
        StaticJsonDocument<200> doc;
        doc["occurred_at"] = occurred_at;
        doc["amount_paid"] = String(amount_paid);
        doc["refund_amount"] = String(refund_amount);
        doc["kwh_spent"] = kwh_spent;
        String jsonStr;
        serializeJson(doc, jsonStr);

        int httpResponseCode = http.POST(jsonStr);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.printf("HTTP Response code: %d, response: %s\n", httpResponseCode, response.c_str());
        } else {
            Serial.printf("Error on sending POST: %s\n", http.errorToString(httpResponseCode).c_str());
        }

        http.end();
    } else {
        Serial.println("WiFi not connected");
    }


    }

    
}