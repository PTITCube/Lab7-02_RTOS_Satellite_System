#include "Camera.h"
#include "MUX.h"
#include "../EPS/EPS.h"
#include <SD.h>
#include <SPI.h>

namespace {
constexpr uint8_t SD_CS_PIN = 13;
constexpr uint8_t SD_SCK_PIN = 18;
constexpr uint8_t SD_MISO_PIN = 19;
constexpr uint8_t SD_MOSI_PIN = 23;
}

PTIT_Camera::PTIT_Camera() : gpsSelectPin(12), camSelectPin(14), sdReady(false) {
}

bool PTIT_Camera::initializeSd() {
    if (sdReady) return true;
    
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, SPI, 8000000)) {
        Serial.println("Camera: SD mount failed");
        sdReady = false;
        return false;
    }

    sdReady = true;
    return true;
}

bool PTIT_Camera::init(uint8_t gpsSelectPin, uint8_t camSelectPin) {
    this->gpsSelectPin = gpsSelectPin;
    this->camSelectPin = camSelectPin;

    PTIT_MUX::init(gpsSelectPin, camSelectPin);
    return initializeSd();
}

void PTIT_Camera::selectGPS() {
    PTIT_MUX::selectGPS();
}

void PTIT_Camera::selectCamera() {
    PTIT_MUX::selectCamera();
}

bool PTIT_Camera::isReady() const {
    return sdReady;
}

bool PTIT_Camera::capture() {
    return captureToFile("/photos/capture.jpg");
}

bool PTIT_Camera::captureToFile(const char* path) {
    if (!sdReady) {
        if (!initializeSd()) {
            Serial.println("Camera: SD not ready");
            return false;
        }
    }

    // Đảm bảo kênh MUX đang trỏ vào Camera và cổng Serial2 chuyển sang 115200
    selectCamera();
    delay(100);

    // Dọn dẹp bộ nhớ đệm UART2
    while(Serial2.available()) Serial2.read();

    // 1. Gửi lệnh chụp đến ESP32-CAM qua UART2
    Serial2.print("capture\n");
    Serial2.flush();

    // 2. Chờ ESP32-CAM phản hồi về SIZE
    String response = "";
    unsigned long startTime = millis();
    bool sizeReceived = false;
    long imageSize = 0;

    while (millis() - startTime < 5000) {
        if (Serial2.available()) {
            response = Serial2.readStringUntil('\n');
            response.trim();
            if (response.startsWith("SIZE:")) {
                imageSize = response.substring(5).toInt();
                sizeReceived = true;
                break;
            }
            if (response.startsWith("ERROR:")) {
                Serial.println("Camera: " + response);
                selectGPS(); // Trả lại kênh cho GPS khi có lỗi
                return false;
            }
        }
        delay(1);
    }

    if (!sizeReceived || imageSize <= 0) {
        Serial.println("Camera: Timeout: No response from ESP32-CAM");
        selectGPS(); // Trả lại kênh cho GPS khi timeout
        return false;
    }

    // 3. Tạo thư mục nếu chưa có
    String filePath = path;
    int lastSlash = filePath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dirPath = filePath.substring(0, lastSlash);
        if (!SD.exists(dirPath.c_str())) {
            if (!SD.mkdir(dirPath.c_str())) {
                Serial.println("Camera: Mkdir failed: " + dirPath);
                selectGPS();
                return false;
            }
        }
    }

    // 4. Mở file để ghi
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Camera: Open file failed: " + String(path));
        selectGPS();
        return false;
    }

    // 5. Báo cho ESP32-CAM biết OBC đã sẵn sàng nhận dữ liệu
    Serial2.print("OK\n");
    Serial2.flush();

    // 6. Nhận dữ liệu raw từ UART2 và ghi trực tiếp vào thẻ nhớ
    long receivedBytes = 0;
    startTime = millis();
    uint8_t buf[256];

    while (receivedBytes < imageSize && (millis() - startTime < 15000)) {
        if (Serial2.available()) {
            int bytesToRead = Serial2.available();
            if (bytesToRead > (int)sizeof(buf)) bytesToRead = sizeof(buf);
            
            int bytesRead = Serial2.readBytes(buf, bytesToRead);
            file.write(buf, bytesRead);
            receivedBytes += bytesRead;
            
            // Reset timeout mỗi khi nhận được dữ liệu
            startTime = millis();
        }
    }

    file.close();

    // 7. Hoàn tất và TRẢ LẠI KÊNH CHO GPS
    bool success = false;
    if (receivedBytes == imageSize) {
        Serial.printf("Camera: Saved %s (%ld bytes)\n", path, imageSize);
        Serial2.println("DONE"); // Báo ESP32-CAM kết thúc
        success = true;
    } else {
        Serial.printf("Camera: Incomplete transfer: %ld/%ld bytes\n", receivedBytes, imageSize);
        success = false;
    }

    // Luôn luôn trả lại đường truyền UART2 cho GPS sau khi chụp xong
    selectGPS();

    return success;
}
