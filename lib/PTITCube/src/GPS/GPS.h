#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include <TinyGPS++.h>

class PTIT_GPS {
public:
    enum class Uart2Device {
        GPS,
        CAMERA
    };

    PTIT_GPS();

    void init(uint8_t gpsSelectPin = 12, uint8_t camSelectPin = 14);
    void selectGPS();
    void selectCamera();
    void selectDevice(Uart2Device device);
    void update();
    float getLat();
    float getLng();
    float getAltitude();
    int getSatellites();
    float getSpeed();
    float getCourse();
    float getHDOP();
    String getTimeString();

private:
    TinyGPSPlus gps;
    HardwareSerial gpsSerial;
    uint8_t gpsSelectPin;
    uint8_t camSelectPin;
    Uart2Device selectedDevice;
};

#endif
