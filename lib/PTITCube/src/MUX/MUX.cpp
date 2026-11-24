#include "MUX.h"
#include "../EPS/EPS.h"

uint8_t PTIT_MUX::_gpsPin = 12;
uint8_t PTIT_MUX::_camPin = 14;
bool PTIT_MUX::_isCameraActive = false;

void PTIT_MUX::init(uint8_t gpsSelectPin, uint8_t camSelectPin) {
    PTIT_EPS::setBuckState(true);

    _gpsPin = gpsSelectPin;
    _camPin = camSelectPin;

    if (_gpsPin != 255 && _camPin != 255) {
        pinMode(_gpsPin, OUTPUT);
        pinMode(_camPin, OUTPUT);

        // Break-Before-Make: Tắt cả hai kênh trước
        digitalWrite(_camPin, LOW);
        digitalWrite(_gpsPin, LOW);
        delayMicroseconds(50);

        // Mặc định an toàn: Trả quyền ưu tiên bus UART2 cho GPS
        digitalWrite(_gpsPin, HIGH);
    }

    _isCameraActive = false;
    Serial2.begin(9600, SERIAL_8N1, 16, 17);
}

void PTIT_MUX::selectGPS() {
    if (_gpsPin != 255 && _camPin != 255) {
        // Break-Before-Make: TẮT Camera trước
        digitalWrite(_camPin, LOW);
        delayMicroseconds(50);
        // BẬT GPS sau
        digitalWrite(_gpsPin, HIGH);
        Serial2.begin(9600, SERIAL_8N1, 16, 17);
    }
    _isCameraActive = false;
}

void PTIT_MUX::selectCamera() {
    if (_gpsPin != 255 && _camPin != 255) {
        // Break-Before-Make: TẮT GPS trước
        digitalWrite(_gpsPin, LOW);
        delayMicroseconds(50);
        // BẬT Camera sau
        digitalWrite(_camPin, HIGH);
        Serial2.begin(115200, SERIAL_8N1, 16, 17);
        delay(100);
    }
    _isCameraActive = true;
}

bool PTIT_MUX::isGPSSelected() {
    return !_isCameraActive;
}

bool PTIT_MUX::isCameraSelected() {
    return _isCameraActive;
}
