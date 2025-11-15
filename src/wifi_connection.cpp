#include "main.h"

// WiFi credentials
//const char* ssid = "Anton";
//const char* password = "11223360";
const char* ssid = "VINcoder";
const char* password = "1234567890";



// Инициализация WiFi
void init_wifi_connection() {
    WiFi.begin(ssid, password);
    UART0_DEBUG_PORT.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        UART0_DEBUG_PORT.print(".");
    }
    UART0_DEBUG_PORT.println("\nConnected to WiFi!");
    UART0_DEBUG_PORT.print("IP Address: ");
    UART0_DEBUG_PORT.println(WiFi.localIP());
}