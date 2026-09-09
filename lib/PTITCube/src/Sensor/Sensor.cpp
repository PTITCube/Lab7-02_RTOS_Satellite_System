#include "Sensor.h"
#include <Arduino.h>
#include <Wire.h>

void PTIT_Sensor::init() {
    Serial.println("[Sensor] Initializing BMP280, MPU (6050/6500), QMC5883L...");
    Wire.begin(21, 22);

    if (!bmp.begin(0x76) && !bmp.begin(0x77)) {
        Serial.println("BMP280 fail");
    }

    // Khởi tạo MPU (Adafruit_MPU6050 begin sẽ fail nếu là MPU6500 do khác WHO_AM_I)
    mpu.begin(0x68);
    
    // Đọc thủ công thanh ghi WHO_AM_I để phân biệt MPU6050 vs MPU6500
    Wire.beginTransmission(0x68);
    Wire.write(0x75);
    Wire.endTransmission(false);
    Wire.requestFrom((uint16_t)0x68, (uint8_t)1);
    uint8_t whoami = Wire.read();

    if (whoami == 0x68) {
        Serial.println("MPU6050 found!");
    } else if (whoami == 0x70) {
        Serial.println("MPU6500 found!");
    } else if (whoami == 0x71) {
        Serial.println("MPU9250 found!");
    } else {
        Serial.printf("MPU fail (WHO_AM_I = 0x%02X)\n", whoami);
    }

    // Bắt buộc đánh thức cảm biến (Ghi 0 vào thanh ghi PWR_MGMT_1)
    Wire.beginTransmission(0x68);
    Wire.write(0x6B); 
    Wire.write(0x00); 
    Wire.endTransmission();

    // Bắt buộc cấu hình dải đo
    mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
    if (!qmc.begin()) {
        Serial.println("QMC5883P fail");
    } else {
        Serial.println("QMC5883P found!");
        qmc.setMode(QMC5883P_MODE_CONTINUOUS);
        qmc.setRange(QMC5883P_RANGE_2G);
        qmc.setODR(QMC5883P_ODR_50HZ);
        qmc.setOSR(QMC5883P_OSR_8);
        qmc.setDSR(QMC5883P_DSR_1);
        qmc.setSetResetMode(QMC5883P_SETRESET_ON);
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
