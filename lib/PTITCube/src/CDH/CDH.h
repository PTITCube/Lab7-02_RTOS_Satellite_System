#ifndef CDH_H
#define CDH_H

#include <Arduino.h>

class PTIT_CDH {
public:
    void init();
    void update();
    void publishData(const char* topic, const char* payload);
};

#endif
