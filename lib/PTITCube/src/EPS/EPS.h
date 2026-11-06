#ifndef EPS_H
#define EPS_H

#include <Arduino.h>
#include <Wire.h>

/**
 * @brief Phân hệ Năng lượng (EPS) sử dụng IC đo dung lượng pin chuyên dụng MAX17048 qua I2C.
 * Địa chỉ I2C mặc định: 0x36
 */
class PTIT_EPS {
public:
    PTIT_EPS();

    bool init();
    void update();
    
    // Đọc điện áp cell pin (V)
    float getBatteryVoltage();
    
    // Đọc phần trăm dung lượng pin (0.0% - 100.0%)
    float getBatteryPercent();
    float getBatteryPercentage(); // Alias
    
    // Đọc tốc độ sạc / xả (%/h)
    float getChargeRate();
    
    // Kiểm tra cảm biến MAX17048 có phản hồi trên I2C không
    bool isConnected() const;

private:
    float lastVoltage;
    float lastPercent;
    float lastRate;
    bool sensorReady;

    uint16_t readRegister16(uint8_t reg);
    bool writeRegister16(uint8_t reg, uint16_t value);
};

#endif
