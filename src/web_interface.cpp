#include "web_interface.h"



/* Start Webserver */
AsyncWebServer server(80);

/* Attach ESP-DASH to AsyncWebServer */
ESPDash dashboard(server); 

//dash::CurrentCard currentDash(dashboard, "Current");
//dash::VoltageCard voltageDash(dashboard, "Voltage");

dash::GenericCard voltageCard(dashboard, "Voltage", "V");
dash::GenericCard currentCard(dashboard, "Current", "A");
dash::GenericCard powerCard(dashboard, "Power", "W");

unsigned long prevMillis = 0;
const unsigned long UPDATE_INTERVAL = 3000; // 3 секунды

void init_web_interface() {
  /* Start AsyncWebServer */
  server.begin();
}

void loop_web_interface() {
  unsigned long currentMillis = millis();

  // Проверяем, прошло ли 3 секунды
  if (currentMillis - prevMillis >= UPDATE_INTERVAL) {
    prevMillis = currentMillis;

    /* Update Card Values */
    currentCard.setValue(String(get_current()));
    voltageCard.setValue(String(get_voltage()));
    powerCard.setValue(String(get_power()));

    //currentCard.setValue("110");
    //voltageCard.setValue("1");
    //powerCard.setValue("110");

    dashboard.sendUpdates();
  }
}