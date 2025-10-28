#pragma once

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_sntp.h>
#include <mbedtls/sha1.h>
#include <Preferences.h>
#include <rom/rtc.h>
#include <esp_system.h>

void wifi_initialize();
bool api_ping();
int api_request(const String& method, const String& path, const String& body, String& responseOut);
bool time_is_set();
void init_time();
String uuidV4();
template<typename Fn>
int with_retry(Fn fn, int maxAttempts = 3, uint32_t baseDelayMs = 500);