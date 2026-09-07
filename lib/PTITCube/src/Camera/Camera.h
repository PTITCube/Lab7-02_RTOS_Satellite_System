#ifndef CAMERA_H
#define CAMERA_H

#include <Arduino.h>

class PTIT_Camera {
public:
    PTIT_Camera();
    
    // Khởi tạo camera. 
    // gpsSelectPin và camSelectPin là các chân điều khiển Multiplexer (MUX).
    // Truyền 255 nếu nối trực tiếp không qua MUX.
    bool init(uint8_t gpsSelectPin = 255, uint8_t camSelectPin = 255);
    
    // Gửi tín hiệu điều khiển MUX để kết nối UART2 với mạch Camera
    void selectCamera();
    
    // Gửi tín hiệu điều khiển MUX để kết nối UART2 với mạch GPS
    void selectGPS();
    
    // Bắt đầu quá trình chụp ảnh và lưu trực tiếp vào thẻ nhớ SD trên OBC
    bool captureToFile(const char* path);
    
    // Gọi tắt của captureToFile với tên file mặc định
    bool capture();
    
    bool isReady() const;

private:
    uint8_t gpsSelectPin;
    uint8_t camSelectPin;
    bool sdReady;
    
    bool initializeSd();
};

#endif
