#include "Sensor.h"
#include <Arduino.h>
#include <Wire.h>
#include <math.h>

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

        // Cấu hình dải đo
        mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
        mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    }

    if (initQMC) {
        if (qmc.begin(0x2C) || qmc.begin(0x0D)) {
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
    p = bmp.readPressure() / 100.0f;
    
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
        // 1 Gauss = 100 uT
        mx = qx * 100.0f;
        my = qy * 100.0f;
        mz = qz * 100.0f;
    }
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
    if (heading < 0.0f) heading += 360.0f;
    return heading;
}
