#include "Sensor.h"
#include <Arduino.h>
#include <Wire.h>

void PTIT_Sensor::init(bool initBMP, bool initMPU, bool initQMC) {
    Wire.begin(21, 22);

    if (initBMP) {
        bmp.begin(0x76) || bmp.begin(0x77);
    }

    if (initMPU) {
        mpu.begin(0x68);

        // Bắt buộc đánh thức cảm biến (Ghi 0 vào thanh ghi PWR_MGMT_1)
        Wire.beginTransmission(0x68);
        Wire.write(0x6B); 
        Wire.write(0x00); 
        Wire.endTransmission();

        // Bắt buộc cấu hình dải đo
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    }

    if (initQMC) {
        if (qmc.begin()) {
            qmc.setMode(QMC5883P_MODE_CONTINUOUS);
            qmc.setRange(QMC5883P_RANGE_2G);
            qmc.setODR(QMC5883P_ODR_50HZ);
            qmc.setOSR(QMC5883P_OSR_8);
            qmc.setDSR(QMC5883P_DSR_1);
            qmc.setSetResetMode(QMC5883P_SETRESET_ON);
        }
    }
}

void PTIT_Sensor::update() {
    t = bmp.readTemperature();
    p = bmp.readPressure() / 100.0;
    
    sensors_event_t a, g, temp;
    if (mpu.getEvent(&a, &g, &temp)) {
        ax = a.acceleration.x;
        ay = a.acceleration.y;
        az = a.acceleration.z;
        gx = g.gyro.x;
        gy = g.gyro.y;
        gz = g.gyro.z;
    }

    float qx, qy, qz;
    if (qmc.getGaussField(&qx, &qy, &qz)) {
        mx = qx;
        my = qy;
        mz = qz;
    }
}

float PTIT_Sensor::getTemperature() { return t; }
float PTIT_Sensor::getPressure() { return p; }

float PTIT_Sensor::getAccX() { return ax; }
float PTIT_Sensor::getAccY() { return ay; }
float PTIT_Sensor::getAccZ() { return az; }

float PTIT_Sensor::getGyroX() { return gx; }
float PTIT_Sensor::getGyroY() { return gy; }
float PTIT_Sensor::getGyroZ() { return gz; }

float PTIT_Sensor::getMagX() { return mx; }
float PTIT_Sensor::getMagY() { return my; }
float PTIT_Sensor::getMagZ() { return mz; }
