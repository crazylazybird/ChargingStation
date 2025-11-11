#include "main.h"


void init_relay(){
    pinMode(RELAY_PIN, OUTPUT);   // задаём пин как выход
    digitalWrite(RELAY_PIN, LOW); // реле выключено при старте
}