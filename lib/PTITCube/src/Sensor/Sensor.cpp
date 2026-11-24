#include "Sensor.h"
#include "../EPS/EPS.h"
#include "../CDH/CDH.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

bool PTIT_Sensor::initMPUDevice(uint8_t addr) {
    Wire.beginTransmission(addr);
    Wire.write(0x75); // WHO_AM_I
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((uint8_t)addr, (uint8_t)1) != 1) return false;
    uint8_t id = Wire.read();
    
    // MPU6500 (0x70, 0x71, 0x73)
    if (id != 0x70 && id != 0x71 && id != 0x73) {
        return false;
    }

    // Đánh thức cảm biến (Ghi 0x01 vào PWR_MGMT_1: chọn auto PLL clock)
    Wire.beginTransmission(addr);
    Wire.write(0x6B);
    Wire.write(0x01);
    if (Wire.endTransmission() != 0) return false;
    delay(10);

    // Cấu hình DLPF 42Hz
    Wire.beginTransmission(addr);
    Wire.write(0x1A);
    Wire.write(0x03);
    Wire.endTransmission();

    // Cấu hình Gyro dải đo ±500 deg/s (0x08)
    Wire.beginTransmission(addr);
    Wire.write(0x1B);
    Wire.write(0x08);
    Wire.endTransmission();

    // Cấu hình Accel dải đo ±8g (0x10)
    Wire.beginTransmission(addr);
    Wire.write(0x1C);
    Wire.write(0x10);
    Wire.endTransmission();

    mpuAddress = addr;
    return true;
}

void PTIT_Sensor::init(bool initBMP, bool initMPU, bool initQMC) {
    PTIT_EPS::setBuckState(true);

    Wire.begin(21, 22);
    Wire.setTimeOut(25);

    if (initBMP) {
        bmpReady = bmp.begin(0x76) || bmp.begin(0x77);
    }

    if (initMPU) {
        mpuReady = initMPUDevice(0x68) || initMPUDevice(0x69);
    }

    if (initQMC) {
        if (qmc.begin(0x2C) || qmc.begin(0x0D)) {
            qmc.setMode(QMC5883P_MODE_CONTINUOUS);
            qmc.setRange(QMC5883P_RANGE_2G);
            qmc.setODR(QMC5883P_ODR_50HZ);
            qmc.setOSR(QMC5883P_OSR_8);
            qmc.setDSR(QMC5883P_DSR_1);
            qmc.setSetResetMode(QMC5883P_SETRESET_ON);
            qmcReady = true;
        } else {
            qmcReady = false;
        }
    }
}

void PTIT_Sensor::update() {
    bool locked = PTIT_CDH::lock(PTIT_BUS_I2C);

    Wire.setTimeOut(25);

    // Luôn detect các linh kiện cảm biến trên I2C và cố gắng khởi tạo lại (không in log serial)
    if (!bmpReady) {
        Wire.beginTransmission(0x76);
        if (Wire.endTransmission() == 0 || (Wire.beginTransmission(0x77), Wire.endTransmission() == 0)) {
            bmpReady = bmp.begin(0x76) || bmp.begin(0x77);
        }
    }

    if (!mpuReady) {
        mpuReady = initMPUDevice(0x68) || initMPUDevice(0x69);
    }

    if (!qmcReady) {
        Wire.beginTransmission(0x2C);
        if (Wire.endTransmission() == 0 || (Wire.beginTransmission(0x0D), Wire.endTransmission() == 0)) {
            if (qmc.begin(0x2C) || qmc.begin(0x0D)) {
                qmc.setMode(QMC5883P_MODE_CONTINUOUS);
                qmc.setRange(QMC5883P_RANGE_2G);
                qmc.setODR(QMC5883P_ODR_50HZ);
                qmc.setOSR(QMC5883P_OSR_8);
                qmc.setDSR(QMC5883P_DSR_1);
                qmc.setSetResetMode(QMC5883P_SETRESET_ON);
                qmcReady = true;
            }
        }
    }

    if (bmpReady) {
        float newT = bmp.readTemperature();
        float newP = bmp.readPressure() / 100.0f;
        if (!isnan(newT) && newP > 0.0f) {
            t = newT;
            p = newP;
        } else {
            bmpReady = false;
        }
    }
    
    if (mpuReady) {
        Wire.beginTransmission(mpuAddress);
        Wire.write(0x3B);
        if (Wire.endTransmission(false) == 0 && Wire.requestFrom((uint8_t)mpuAddress, (uint8_t)14) == 14) {
            int16_t raw_ax = (Wire.read() << 8) | Wire.read();
            int16_t raw_ay = (Wire.read() << 8) | Wire.read();
            int16_t raw_az = (Wire.read() << 8) | Wire.read();
            Wire.read(); Wire.read(); // Bỏ qua temp nội bộ MPU (dùng BMP280 chính xác hơn)
            int16_t raw_gx = (Wire.read() << 8) | Wire.read();
            int16_t raw_gy = (Wire.read() << 8) | Wire.read();
            int16_t raw_gz = (Wire.read() << 8) | Wire.read();

            // ±8g range: 4096 LSB/g, gravity = 9.80665 m/s^2
            ax = (float)raw_ax * (9.80665f / 4096.0f);
            ay = (float)raw_ay * (9.80665f / 4096.0f);
            az = (float)raw_az * (9.80665f / 4096.0f);

            // ±500 dps range: 65.5 LSB/(deg/s), chuyển đổi sang rad/s
            gx = (float)raw_gx * 0.00026646248f;
            gy = (float)raw_gy * 0.00026646248f;
            gz = (float)raw_gz * 0.00026646248f;
        } else {
            mpuReady = false;
        }
    }

    if (qmcReady) {
        float qx, qy, qz;
        if (qmc.getGaussField(&qx, &qy, &qz)) {
            // 1 Gauss = 100 uT
            mx = qx * 100.0f;
            my = qy * 100.0f;
            mz = qz * 100.0f;
        } else {
            qmcReady = false;
        }
    }

    if (locked) PTIT_CDH::unlock(PTIT_BUS_I2C);
}

float PTIT_Sensor::getTemperature() { return t; }
float PTIT_Sensor::getPressure()    { return p; }

float PTIT_Sensor::getAltitude() {
    if (p <= 0.0f) return 0.0f;
    return 44330.0f * (1.0f - pow(p / 1013.25f, 0.1903f));
}

float PTIT_Sensor::getAccX() { return ax; }
float PTIT_Sensor::getAccY() { return ay; }
float PTIT_Sensor::getAccZ() { return az; }

float PTIT_Sensor::getGyroX() { return gx; }
float PTIT_Sensor::getGyroY() { return gy; }
float PTIT_Sensor::getGyroZ() { return gz; }

float PTIT_Sensor::getMagX() { return mx; }
float PTIT_Sensor::getMagY() { return my; }
float PTIT_Sensor::getMagZ() { return mz; }

float PTIT_Sensor::getRoll() {
    return atan2(ay, az) * 180.0f / (float)PI;
}

float PTIT_Sensor::getPitch() {
    return atan2(-ax, sqrt(ay * ay + az * az)) * 180.0f / (float)PI;
}

float PTIT_Sensor::getHeading() {
    float heading = atan2(my, mx) * 180.0f / (float)PI;
    if (heading < 0) heading += 360.0f;
    return heading;
}
