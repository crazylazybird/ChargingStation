#include "main.h"

// WiFi credentials
const char* ssid = "Anton";
const char* password = "11223360";

// Dominion API Key
const char* apiKey = "dominion";

unsigned long previousMillis = 0;
const unsigned long interval = 1000; // 1 секунда

// Инициализация WiFi
void init_wifi() {
    WiFi.begin(ssid, password);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
}

// Отправка JSON POST на сервер с API Key
void send_POST_json(float voltage, float current) {

    unsigned long currentMillis = millis();

    if (currentMillis - previousMillis >= interval) {
        previousMillis = currentMillis;
        if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        http.begin("http://192.168.1.63:8000/measurements"); // адрес сервера

        http.addHeader("Content-Type", "application/json");
        http.addHeader("X-API-Key", apiKey); // добавляем ключ Dominion

        // Формируем JSON
        StaticJsonDocument<200> doc;
        doc["voltage"] = voltage;
        doc["current"] = current;
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