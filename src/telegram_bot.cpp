#include "main.h"

// Dominion API Key
const char* apiKey = "oPhrCfp_UfMBKst2P3mM9n-Ti2tXnL9A0zeJzrMa9qE";

unsigned long previousMillis = 0;
const unsigned long interval = 1000; // 1 секунда



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