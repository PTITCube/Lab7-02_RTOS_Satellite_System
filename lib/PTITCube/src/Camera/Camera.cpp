#include "Camera.h"
#include <SD.h>
#include <SPI.h>

namespace {
constexpr uint8_t SD_CS_PIN = 13;
constexpr uint8_t SD_SCK_PIN = 18;
constexpr uint8_t SD_MISO_PIN = 19;
constexpr uint8_t SD_MOSI_PIN = 23;

// Baudrate giao tiếp với ESP32-CAM (UART2 của CAM ở 115200)
constexpr uint32_t CAM_BAUD = 115200;
// Baudrate giao tiếp với GPS (9600)
constexpr uint32_t GPS_BAUD = 9600;
}

PTIT_Camera::PTIT_Camera() : gpsSelectPin(12), camSelectPin(14), sdReady(false) {
}

bool PTIT_Camera::initializeSd() {
    // Nếu thẻ nhớ đã khởi tạo thành công trước đó, không cần khởi tạo lại
    if (sdReady) return true;
    
    Serial.println("[Camera] Khoi tao MicroSD qua SPI...");
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, SPI, 8000000)) {
        Serial.println("[Camera] MicroSD mount failed");
        sdReady = false;
        return false;
    }

    sdReady = true;
    Serial.println("[Camera] MicroSD ready for image storage");
    return true;
}

bool PTIT_Camera::init(uint8_t gpsSelectPin, uint8_t camSelectPin) {
    this->gpsSelectPin = gpsSelectPin;
    this->camSelectPin = camSelectPin;

    if (gpsSelectPin != 255 && camSelectPin != 255) {
        pinMode(this->gpsSelectPin, OUTPUT);
        pinMode(this->camSelectPin, OUTPUT);
        
        // Nguyên tắc Break-Before-Make: Tắt cả hai kênh trước
        digitalWrite(this->camSelectPin, LOW);
        digitalWrite(this->gpsSelectPin, LOW);
        delayMicroseconds(50);
        
        // Mặc định an toàn: Trả quyền ưu tiên bus UART2 cho GPS
        digitalWrite(this->gpsSelectPin, HIGH);
    }

    // Mặc định khởi động cổng UART2 ở baudrate GPS (9600)
    Serial2.begin(GPS_BAUD, SERIAL_8N1, 16, 17);

    return initializeSd();
}

void PTIT_Camera::selectGPS() {
    if (gpsSelectPin != 255 && camSelectPin != 255) {
        // Break-Before-Make: TẮT Camera trước
        digitalWrite(camSelectPin, LOW);
        delayMicroseconds(50);
        // BẬT GPS sau (chống chập 2 chân TX trên 74HC4066)
        digitalWrite(gpsSelectPin, HIGH);
        Serial2.begin(GPS_BAUD, SERIAL_8N1, 16, 17);
    }
}

void PTIT_Camera::selectCamera() {
    if (gpsSelectPin != 255 && camSelectPin != 255) {
        // Break-Before-Make: TẮT GPS trước
        digitalWrite(gpsSelectPin, LOW);
        delayMicroseconds(50);
        // BẬT Camera sau
        digitalWrite(camSelectPin, HIGH);
        Serial2.begin(CAM_BAUD, SERIAL_8N1, 16, 17);
    }
}

bool PTIT_Camera::isReady() const {
    return sdReady;
}

bool PTIT_Camera::capture() {
    return captureToFile("/photos/capture.jpg");
}

bool PTIT_Camera::captureToFile(const char* path) {
    if (!sdReady) {
        Serial.println("[Camera] Loi: The nho SD chua san sang.");
        return false;
    }

    // Đảm bảo kênh MUX đang trỏ vào Camera và cổng Serial2 chuyển sang 115200
    selectCamera();

    // Dọn dẹp bộ nhớ đệm UART2
    while(Serial2.available()) Serial2.read();

    // 1. Gửi lệnh CAPTURE đến ESP32-CAM qua UART2
    Serial.println("[Camera] Dang gui lenh chup anh toi ESP32-CAM...");
    Serial2.print("CAPTURE\n");
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
                Serial.println("[Camera] ESP32-CAM: " + response);
                selectGPS(); // Trả lại kênh cho GPS khi có lỗi
                return false;
            }
        }
        delay(1);
    }

    if (!sizeReceived || imageSize <= 0) {
        Serial.println("[Camera] Loi: Khong nhan duoc phan hoi kich thuoc tu ESP32-CAM.");
        selectGPS(); // Trả lại kênh cho GPS khi timeout
        return false;
    }

    Serial.printf("[Camera] ESP32-CAM phan hoi kich thuoc anh: %ld bytes\n", imageSize);

    // 3. Tạo thư mục nếu chưa có
    String filePath = path;
    int lastSlash = filePath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dirPath = filePath.substring(0, lastSlash);
        if (!SD.exists(dirPath.c_str())) {
            if (!SD.mkdir(dirPath.c_str())) {
                Serial.println("[Camera] Loi: Khong the tao thu muc " + dirPath);
                selectGPS();
                return false;
            }
        }
    }

    // 4. Mở file để ghi
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("[Camera] Loi: Khong the tao file " + String(path));
        selectGPS();
        return false;
    }

    // 5. Báo cho ESP32-CAM biết OBC đã sẵn sàng nhận dữ liệu
    Serial2.print("OK\n");
    Serial2.flush();
    Serial.println("[Camera] Dang tai du lieu anh tu ESP32-CAM...");

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
        Serial.println("[Camera] Luu anh thanh cong: " + String(path));
        Serial2.println("DONE"); // Báo ESP32-CAM kết thúc
        success = true;
    } else {
        Serial.printf("[Camera] Loi: Chi nhan duoc %ld/%ld bytes. Timeout!\n", receivedBytes, imageSize);
        success = false;
    }

    // Luôn luôn trả lại đường truyền UART2 cho GPS sau khi chụp xong
    selectGPS();

    return success;
}
