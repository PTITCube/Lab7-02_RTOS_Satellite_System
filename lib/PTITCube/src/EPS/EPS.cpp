#include "EPS.h"

#define BATTERY_PIN 34 // Pin ADC dùng để đo điện áp pin
#define V_REF 3.3f
#define R1 1200.0 // Điện trở R1 (1.2k)
#define R2 1200.0 // Điện trở R2 (1.2k)

PTIT_EPS::PTIT_EPS() : lastVoltage(0.0f), lastPercent(0.0f) {
}

void PTIT_EPS::init() {
    Serial.println("[EPS] Initializing Power System...");
    pinMode(BATTERY_PIN, INPUT);
    Serial.println("[EPS] Battery voltage measurement using ADC (GPIO34).");
    update();
}

void PTIT_EPS::update() {
    lastVoltage = getBatteryVoltage();
    lastPercent = getBatteryPercent();
}

float PTIT_EPS::getBatteryVoltage() {
    int adc = analogRead(BATTERY_PIN);
    float v_pin = (adc / 4095.0) * V_REF;
    float v_bat = v_pin * ((R1 + R2) / R2);
    lastVoltage = v_bat;
    return v_bat;
}

float PTIT_EPS::getBatteryPercent() {
    float voltage = getBatteryVoltage();
    float percent = 0.0f;

    if (voltage >= 4.2f) percent = 100.0f;
    else if (voltage <= 3.0f) percent = 0.0f;
    else percent = ((voltage - 3.0f) / (4.2f - 3.0f)) * 100.0f;

    lastPercent = percent;
    return percent;
}
