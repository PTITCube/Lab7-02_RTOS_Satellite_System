#ifndef GPS_H
#define GPS_H

#include <Arduino.h>
#include <TinyGPS++.h>

class PTIT_GPS {
public:
    PTIT_GPS();

    // Khởi tạo GPS. Mặc định UART2 (GPIO 16 RX / GPIO 17 TX)
    // gpsSelectPin và camSelectPin dùng cho MUX chuyển mạch GPS/Camera
    void init(uint8_t gpsSelectPin = 12, uint8_t camSelectPin = 14);
    
    // Chọn kênh GPS trên MUX
    void selectGPS();
    
    // Chọn kênh Camera trên MUX (trả UART2 cho Camera)
    void selectCamera();
    
    // Đọc và xử lý dữ liệu GPS mới từ UART
    void update();

    // Kiểm tra GPS đã chốt vị trí chưa
    bool hasFix();

    float getLatitude();
    float getLongitude();
    float getLat();   // Alias cho getLatitude
    float getLng();   // Alias cho getLongitude
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
};

#endif
