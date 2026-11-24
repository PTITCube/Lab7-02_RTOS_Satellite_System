#ifndef MUX_CAMERA_H
#define MUX_CAMERA_H

#include <Arduino.h>

/**
 * @brief Lớp điều khiển module ESP32-CAM từ ESP32 chính (OBC) qua MUX.
 * 
 * ESP32-CAM giao tiếp qua UART2 (GPIO 14 TX / GPIO 15 RX).
 * ESP32 chính kết nối với ESP32-CAM qua UART2 (GPIO 16 RX / GPIO 17 TX),
 * chia sẻ đường UART2 với GPS qua Multiplexer 74HC4066.
 */
class PTIT_Camera {
public:
    PTIT_Camera();
    
    // Khởi tạo camera và MUX
    bool init(uint8_t gpsSelectPin = 12, uint8_t camSelectPin = 14);
    
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

#endif // MUX_CAMERA_H
