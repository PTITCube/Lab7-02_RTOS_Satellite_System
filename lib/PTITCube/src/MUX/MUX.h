#ifndef MUX_H
#define MUX_H

#include <Arduino.h>

#ifndef MUX_GPS_PIN
#define MUX_GPS_PIN  12
#endif
#ifndef MUX_CAM_PIN
#define MUX_CAM_PIN  14
#endif
#ifndef UART2_RX_PIN
#define UART2_RX_PIN 16
#endif
#ifndef UART2_TX_PIN
#define UART2_TX_PIN 17
#endif

// Bao gồm các module cấu thành phân hệ MUX
#include "Camera.h"
#include "GPS.h"

/**
 * @class PTIT_MUX
 * @brief Quản lý chuyển mạch Multiplexer 74HC4066 trên bus UART2 giữa Camera và GPS.
 * 
 * Vệ tinh PTITCube sử dụng IC tương tự 74HC4066 để chia sẻ bus UART2 (GPIO 16 RX / GPIO 17 TX)
 * giữa module GPS và vi điều khiển phụ ESP32-CAM theo nguyên lý Break-Before-Make.
 */
class PTIT_MUX {
public:
    PTIT_Camera camera;
    PTIT_GPS    gps;

    static void init(uint8_t gpsSelectPin = MUX_GPS_PIN, uint8_t camSelectPin = MUX_CAM_PIN);
    static void selectGPS();
    static void selectCamera();
    static bool isGPSSelected();
    static bool isCameraSelected();

private:
    static uint8_t _gpsPin;
    static uint8_t _camPin;
    static bool    _isCameraActive;
};

#endif // MUX_H
