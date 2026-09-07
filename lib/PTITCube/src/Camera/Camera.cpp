#include "Camera.h"
#include <SD_MMC.h>

PTIT_Camera::PTIT_Camera() : gpsSelectPin(12), camSelectPin(14), sdReady(false) {
}

bool PTIT_Camera::initializeSd() {
    // Nếu thẻ nhớ đã khởi tạo thành công trước đó, không cần khởi tạo lại
    if (sdReady) return true;
    
    // Đảm bảo chân CS của mạch không xung đột (nếu dùng chung SPI, tuy nhiên SD_MMC dùng các chân cố định)
    if (!SD_MMC.begin("/sdcard", true)) {
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
        digitalWrite(this->gpsSelectPin, LOW);
        digitalWrite(this->camSelectPin, HIGH); // Mặc định mở kênh Camera
    }

    Serial.println("[Camera] Khởi tạo giao tiếp Master-Slave với ESP32-CAM (UART2)...");
    Serial2.begin(115200, SERIAL_8N1, 16, 17); // Mở cổng Serial2 để giao tiếp với ESP32-CAM

    return initializeSd();
}

void PTIT_Camera::selectGPS() {
    if (gpsSelectPin != 255 && camSelectPin != 255) {
        digitalWrite(gpsSelectPin, HIGH);
        digitalWrite(camSelectPin, LOW);
        // Trả lại cấu hình UART cho GPS
        Serial2.begin(9600, SERIAL_8N1, 16, 17);
    }
}

void PTIT_Camera::selectCamera() {
    if (gpsSelectPin != 255 && camSelectPin != 255) {
        digitalWrite(gpsSelectPin, LOW);
        digitalWrite(camSelectPin, HIGH);
        // Đặt lại cấu hình UART cho Camera
        Serial2.begin(115200, SERIAL_8N1, 16, 17);
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
        Serial.println("[Camera] Lỗi: Thẻ nhớ SD chưa sẵn sàng.");
        return false;
    }

    // Đảm bảo kênh MUX đang trỏ vào Camera và cổng Serial2 đang mở ở baudrate 115200
    selectCamera();
    delay(100);

    // Dọn dẹp bộ nhớ đệm
    while(Serial2.available()) Serial2.read();

    // 1. Gửi lệnh CAPTURE
    Serial.println("[Camera] Đang gửi lệnh chụp ảnh tới ESP32-CAM...");
    Serial2.println("CAPTURE");

    // 2. Chờ ESP32-CAM phản hồi về SIZE
    String response = "";
    long startTime = millis();
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
        }
        delay(10);
    }

    if (!sizeReceived || imageSize <= 0) {
        Serial.println("[Camera] Lỗi: Không nhận được phản hồi kích thước từ ESP32-CAM.");
        return false;
    }

    Serial.printf("[Camera] ESP32-CAM phản hồi kích thước ảnh: %ld bytes\n", imageSize);

    // 3. Tạo thư mục nếu chưa có
    String filePath = path;
    int lastSlash = filePath.lastIndexOf('/');
    if (lastSlash > 0) {
        String dirPath = filePath.substring(0, lastSlash);
        if (!SD_MMC.exists(dirPath.c_str())) {
            if (!SD_MMC.mkdir(dirPath.c_str())) {
                Serial.println("[Camera] Lỗi: Không thể tạo thư mục " + dirPath);
                return false;
            }
        }
    }

    // 4. Mở file để ghi
    File file = SD_MMC.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("[Camera] Lỗi: Không thể tạo file " + String(path));
        return false;
    }

    // 5. Báo cho ESP32-CAM biết OBC đã sẵn sàng nhận dữ liệu
    Serial2.println("OK");
    Serial.println("[Camera] Đang tải dữ liệu ảnh từ ESP32-CAM...");

    // 6. Nhận dữ liệu raw từ UART và ghi trực tiếp vào thẻ nhớ
    long receivedBytes = 0;
    startTime = millis();
    uint8_t buf[256];

    while (receivedBytes < imageSize && (millis() - startTime < 15000)) {
        if (Serial2.available()) {
            int bytesToRead = Serial2.available();
            if (bytesToRead > sizeof(buf)) bytesToRead = sizeof(buf);
            
            int bytesRead = Serial2.readBytes(buf, bytesToRead);
            file.write(buf, bytesRead);
            receivedBytes += bytesRead;
            
            // Reset timeout mỗi khi nhận được dữ liệu
            startTime = millis();
        }
    }

    file.close();

    // 7. Hoàn tất
    if (receivedBytes == imageSize) {
        Serial.println("[Camera] Lưu ảnh thành công: " + String(path));
        Serial2.println("DONE"); // Báo ESP32-CAM kết thúc
        return true;
    } else {
        Serial.printf("[Camera] Lỗi: Chỉ nhận được %ld/%ld bytes. Timeout!\n", receivedBytes, imageSize);
        return false;
    }
}
