#pragma once

#define CHARGE_BOX_ID   "chargestation"
#define OCPP_SERVER_URL "ws://192.168.1.63:9000"   // БЕЗ /chargestation и без хвоста

void microOCPP_initialize();
void microOCPP_loop();