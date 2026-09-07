#ifndef SENSOR_H
#define SENSOR_H

#include <Adafruit_BMP280.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_QMC5883P.h>

class PTIT_Sensor {
public:
    void init();
    void update();
    float getTemperature();
    float getPressure();
    float getAccX();
    float getAccY();
    float getAccZ();
    float getGyroX();
    float getGyroY();
    float getGyroZ();
    float getMagX();
    float getMagY();
    float getMagZ();

private:
    Adafruit_BMP280 bmp;
    Adafruit_MPU6050 mpu;
    Adafruit_QMC5883P qmc;

    float t = 0, p = 0;
    float ax = 0, ay = 0, az = 0;
    float gx = 0, gy = 0, gz = 0;
    float mx = 0, my = 0, mz = 0;
};

#endif
