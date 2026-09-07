/**
 * ADVANCED CUBESAT EXAMPLE
 * 
 * Ví dụ triển khai toàn diện mọi tính năng của thư viện PTITCube.
 * Khuyến nghị chạy trên board ESP32.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "PTITCube.h"

// ============================================================================
// 1. KHAI BÁO ĐỐI TƯỢNG
// ============================================================================
PTIT_Sensor sensor;
PTIT_GPS gps;
PTIT_COM lora;
PTIT_EPS eps;
PTIT_Storage storage;
PTIT_CDH cdh;

PTIT_Scheduler scheduler;
bool isTelemetryPaused = false;

// ============================================================================
// 2. CÁC LUỒNG XỬ LÝ (TASKS)
// ============================================================================
void sensorLoop() {
    if (isTelemetryPaused) return;
    if (PTIT_Scheduler::lock(PTIT_BUS_I2C)) {
        sensor.update();
        PTIT_Scheduler::unlock(PTIT_BUS_I2C);
    }
}

void gpsLoop() {
    if (isTelemetryPaused) return;
    gps.update();
}

void comLoop() {
    String cmd = "";
    if (PTIT_Scheduler::lock(PTIT_BUS_LORA)) {
        cmd = lora.update();
        PTIT_Scheduler::unlock(PTIT_BUS_LORA);
    }

    if (cmd.length() > 0) {
        cmd.trim();
        String command = PTIT_Utils::extractCommand(cmd);
        String response = "";

        if (command == "ping") response = "Pong!";
        else if (command == "pause") { isTelemetryPaused = true; response = "PAUSED"; }
        else if (command == "resume") { isTelemetryPaused = false; response = "RESUMED"; }
        else if (command == "status") response = "Bat: " + String(eps.getBatteryVoltage(), 1) + "V";
        
        if (response.length() > 0 && PTIT_Scheduler::lock(PTIT_BUS_LORA, 500)) {
            lora.sendMessage(response);
            PTIT_Scheduler::unlock(PTIT_BUS_LORA);
        }
    }
}

void telemetryLoop() {
    if (isTelemetryPaused) return;

    // Định dạng dữ liệu dạng chuỗi JSON nhẹ
    String epsJson = "{\"battery_v\":" + String(eps.getBatteryVoltage(), 2) + "}";
    
    // Ghi SD Card
    if (PTIT_Scheduler::lock(PTIT_BUS_SPI, 200)) {
        storage.logData(epsJson.c_str());
        PTIT_Scheduler::unlock(PTIT_BUS_SPI);
    }

    // MQTT
    cdh.publishData("cubesat/eps", epsJson.c_str());

    // LoRa RF (Gửi chia nhỏ để nhường bus)
    if (PTIT_Scheduler::lock(PTIT_BUS_LORA, 200)) {
        lora.sendMessage(epsJson);
        PTIT_Scheduler::unlock(PTIT_BUS_LORA);
    }
}

// ============================================================================
// 3. SETUP
// ============================================================================
void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    eps.init();
    sensor.init();
    gps.init();
    lora.init();
    storage.init();
    cdh.init();

    // Khởi tạo các task chạy ngầm song song
    scheduler.addTask("Sensor",    sensorLoop,    200,  1, 3);
    scheduler.addTask("GPS",       gpsLoop,       10,   1, 3);
    scheduler.addTask("COM",       comLoop,       100,  1, 2);
    scheduler.addTask("Telemetry", telemetryLoop, 5000, 1, 1);

    scheduler.enableWatchdog(30);
    scheduler.start();
}

void loop() {
    // Không làm gì cả! FreeRTOS đã xử lý mọi việc dưới nền.
}
