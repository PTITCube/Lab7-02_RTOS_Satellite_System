#include "EPS.h"

#define MAX17048_I2C_ADDR   0x36
#define MAX17048_REG_VCELL  0x02
#define MAX17048_REG_SOC    0x04
#define MAX17048_REG_MODE   0x06
#define MAX17048_REG_VER    0x08
#define MAX17048_REG_CRATE  0x16
#define MAX17048_REG_CMD    0xFE

PTIT_EPS::PTIT_EPS() : lastVoltage(0.0f), lastPercent(0.0f), lastRate(0.0f), sensorReady(false) {
}

bool PTIT_EPS::init() {
    Serial.println("[EPS] Initializing Power System (MAX17048 Fuel Gauge via I2C)...");
    Wire.begin();
    
    // Kiểm tra kết nối tới MAX17048
    Wire.beginTransmission(MAX17048_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        sensorReady = true;
        uint16_t ver = readRegister16(MAX17048_REG_VER);
        Serial.printf("[EPS] MAX17048 detected at 0x%02X (IC Version: 0x%04X).\n", MAX17048_I2C_ADDR, ver);
        update();
        return true;
    } else {
        sensorReady = false;
        Serial.println("[EPS] WARNING: MAX17048 not detected at 0x36! Check I2C wiring (SDA=21, SCL=22).");
        return false;
    }
}

void PTIT_EPS::update() {
    if (!sensorReady) {
        Wire.beginTransmission(MAX17048_I2C_ADDR);
        if (Wire.endTransmission() == 0) {
            sensorReady = true;
        } else {
            return;
        }
    }
    lastVoltage = getBatteryVoltage();
    lastPercent = getBatteryPercent();
    lastRate = getChargeRate();
}

float PTIT_EPS::getBatteryVoltage() {
    if (!sensorReady) return 0.0f;
    uint16_t raw = readRegister16(MAX17048_REG_VCELL);
    // VCELL resolution: 78.125 uV / LSB = 0.000078125 V
    float voltage = (float)raw * 0.000078125f;
    lastVoltage = voltage;
    return voltage;
}

float PTIT_EPS::getBatteryPercent() {
    if (!sensorReady) return 0.0f;
    uint16_t raw = readRegister16(MAX17048_REG_SOC);
    // SOC resolution: 1/256 %
    float percent = (float)raw / 256.0f;
    if (percent > 100.0f) percent = 100.0f;
    if (percent < 0.0f) percent = 0.0f;
    lastPercent = percent;
    return percent;
}

float PTIT_EPS::getBatteryPercentage() {
    return getBatteryPercent();
}

float PTIT_EPS::getChargeRate() {
    if (!sensorReady) return 0.0f;
    int16_t raw = (int16_t)readRegister16(MAX17048_REG_CRATE);
    // CRATE resolution: 0.208 %/hr (giá trị âm: xả pin, giá trị dương: đang sạc)
    float rate = (float)raw * 0.208f;
    lastRate = rate;
    return rate;
}

bool PTIT_EPS::isConnected() const {
    return sensorReady;
}

uint16_t PTIT_EPS::readRegister16(uint8_t reg) {
    Wire.beginTransmission(MAX17048_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        return 0;
    }
    if (Wire.requestFrom((uint8_t)MAX17048_I2C_ADDR, (uint8_t)2) == 2) {
        uint8_t msb = Wire.read();
        uint8_t lsb = Wire.read();
        return ((uint16_t)msb << 8) | lsb;
    }
    return 0;
}

bool PTIT_EPS::writeRegister16(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(MAX17048_I2C_ADDR);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xFF));
    return (Wire.endTransmission() == 0);
}
