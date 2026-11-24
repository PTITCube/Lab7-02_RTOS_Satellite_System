#include "EPS.h"

#define MAX17048_I2C_ADDR   0x36
#define MAX17048_REG_VCELL  0x02
#define MAX17048_REG_SOC    0x04
#define MAX17048_REG_MODE   0x06
#define MAX17048_REG_VER    0x08
#define MAX17048_REG_CRATE  0x16
#define MAX17048_REG_CMD    0xFE

PTIT_EPS::PTIT_EPS() 
    : lastVoltage(0.0f), lastPercent(0.0f), lastRate(0.0f), sensorReady(false), buckEnabled(true) {
}

static bool s_buckEnabled = false;
static bool s_buckInitialized = false;

bool PTIT_EPS::init() {
    // Mặc định cấp dòng cho buck khi khởi tạo EPS (Mức 0: LOW)
    setBuckState(true);

    Wire.begin(21, 22);
    Wire.setTimeOut(25);
    
    // Kiểm tra kết nối tới MAX17048
    Wire.beginTransmission(MAX17048_I2C_ADDR);
    if (Wire.endTransmission() == 0) {
        sensorReady = true;
        update();
        return true;
    } else {
        sensorReady = false;
        return false;
    }
}

void PTIT_EPS::update() {
    Wire.setTimeOut(25);

    // Luôn detect linh kiện MAX17048 và cố gắng khởi tạo lại (không in log serial)
    if (!sensorReady || lastVoltage <= 0.5f) {
        Wire.beginTransmission(MAX17048_I2C_ADDR);
        if (Wire.endTransmission() != 0) {
            // Nếu không có ACK, có thể do mạch Buck đang ngắt khiến bus I2C mất áp
            if (!getBuckState()) {
                setBuckState(true);
                delay(20);
                Wire.begin(21, 22);
            }
        }

        Wire.beginTransmission(MAX17048_I2C_ADDR);
        if (Wire.endTransmission() == 0) {
            sensorReady = true;
        } else {
            sensorReady = false;
            return;
        }
    }

    lastVoltage = getBatteryVoltage();
    lastPercent = getBatteryPercent();
    lastRate = getChargeRate();
    if (lastVoltage <= 0.5f) {
        sensorReady = false;
    }
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

void PTIT_EPS::setBuckState(bool enable) {
    if (s_buckInitialized && s_buckEnabled == enable) return;
    s_buckInitialized = true;
    s_buckEnabled = enable;
    pinMode(EPS_PIN_BUCK_CTRL, OUTPUT);
    digitalWrite(EPS_PIN_BUCK_CTRL, enable ? EPS_BUCK_POWER_ON : EPS_BUCK_POWER_OFF);
    if (enable) {
        delay(50); // Chờ điện áp hạ áp ổn định
    }
}

bool PTIT_EPS::getBuckState() {
    return s_buckEnabled;
}

uint16_t PTIT_EPS::readRegister16(uint8_t reg) {
    Wire.beginTransmission(MAX17048_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) {
        sensorReady = false;
        return 0;
    }
    
    Wire.requestFrom((uint8_t)MAX17048_I2C_ADDR, (uint8_t)2);
    if (Wire.available() >= 2) {
        uint8_t msb = Wire.read();
        uint8_t lsb = Wire.read();
        return ((uint16_t)msb << 8) | lsb;
    }
    sensorReady = false;
    return 0;
}

void PTIT_EPS::writeRegister16(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(MAX17048_I2C_ADDR);
    Wire.write(reg);
    Wire.write((uint8_t)(value >> 8));
    Wire.write((uint8_t)(value & 0xFF));
    Wire.endTransmission();
}
