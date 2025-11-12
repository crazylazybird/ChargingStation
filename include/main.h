#pragma once
#include <Arduino.h>
#include <SoftwareSerial.h>
#include <map>
#include <vector>
#include <string>
#include <MicroOcpp.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESPDash.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

#include "uart.h"
#include "vtk_protocol.h"
#include "general_functions.h"
#include "payments.h"
#include "energy_calculation.h"
#include "AMC1200B_driver.h"
#include "yandex_fuel.h"
#include "microOCPP_managment.h"
#include "telegram_bot.h"
#include "relay.h"
#include "wifi_connection.h"
#include "web_interface.h"

