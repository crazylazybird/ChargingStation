#pragma once



void init_wifi();
void send_POST_json(String occurred_at, int amount_paid, int refund_amount, float kwh_spent);
String getISO8601Time();