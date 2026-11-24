#include "WDT.h"

bool PTIT_WDT::init(uint32_t timeoutSec, bool panic) {
    return (esp_task_wdt_init(timeoutSec, panic) == ESP_OK);
}

void PTIT_WDT::add() {
    esp_task_wdt_add(NULL);
}

void PTIT_WDT::feed() {
    esp_task_wdt_reset();
}

void PTIT_WDT::reset() {
    esp_task_wdt_reset();
}

bool PTIT_WDT::wasReset() {
    return (esp_reset_reason() == ESP_RST_TASK_WDT);
}
