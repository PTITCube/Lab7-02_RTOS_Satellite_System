#ifndef EPS_H
#define EPS_H

#include <Arduino.h>

class PTIT_EPS {
public:
    PTIT_EPS();

    void init();
    void update();
    float getBatteryVoltage();
    float getBatteryPercent();

private:
    float lastVoltage;
    float lastPercent;
};

#endif
