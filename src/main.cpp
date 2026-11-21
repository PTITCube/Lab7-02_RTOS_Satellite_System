/**
 * ============================================================================
 * LAB 7 - VÍ DỤ 02: HỆ THỐNG PHẦN MỀM BAY ĐA NHIỆM FREERTOS
 * ============================================================================
 * Mô tả:
 *   - Sử dụng PTIT_Scheduler để quản lý đa nhiệm FreeRTOS trên 2 nhân ESP32.
 *   - Core 1: Đọc cảm biến môi trường (I2C) và GPS (UART2).
 *   - Core 0: Thu phát LoRa COM, ghi thẻ nhớ SPI và đóng gói bản tin viễn trắc JSON.
 * ============================================================================
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PTITCube.h>

PTIT_CDH cdh;
PTIT_EPS eps;
PTIT_Sensor sensor;
PTIT_GPS gps;
PTIT_Camera camera;
PTIT_Storage storage;
PTIT_COM com;

PTIT_Scheduler scheduler;

// Task 1: Thu thập dữ liệu cảm biến I2C & GPS UART2 (Core 1)
void sensorTask() {
    sensor.update();
    gps.update();
}

// Task 2: Nhận và phản hồi lệnh từ trạm mặt đất qua LoRa (Core 0)
void comTask() {
    String cmd = com.update();
    if (cmd.length() > 0) {
        cmd.trim();
        if (cmd == "ping") {
            com.sendMessage("pong");
        } else if (cmd == "capture") {
            camera.capture();
        } else if (cmd == "reboot") {
            ESP.restart();
        }
    }
}

// Task 3: Đóng gói viễn trắc, ghi thẻ nhớ SPI và phát LoRa (Core 0)
void telemetryTask() {
    eps.update();

    JsonDocument doc;
    doc["v"]     = serialized(String(eps.getBatteryVoltage(), 2));
    doc["pct"]   = eps.getBatteryPercentage();
    doc["temp"]  = serialized(String(sensor.getTemperature(), 1));
    doc["press"] = serialized(String(sensor.getPressure(), 1));
    doc["roll"]  = serialized(String(sensor.getRoll(), 1));
    doc["pitch"] = serialized(String(sensor.getPitch(), 1));
    if (gps.hasFix()) {
        doc["lat"] = serialized(String(gps.getLatitude(), 5));
        doc["lng"] = serialized(String(gps.getLongitude(), 5));
    }

    String telem;
    serializeJson(doc, telem);

    cdh.printf("TELEM: %s\n", telem.c_str());

    storage.logData(telem.c_str());
    com.sendMessage(telem);
}

void setup() {
    cdh.begin();

    eps.init();
    sensor.init();
    gps.init();
    camera.init();
    storage.init();
    com.init();

    com.setChannel(23);
    com.setTransmissionMode(0);

    // Đăng ký các task vào Scheduler: (Tên, hàm, chu kỳ ms, Core, Priority)
    scheduler.addTask("Sensor",    sensorTask,    200,  1, 3);
    scheduler.addTask("COM",       comTask,       100,  0, 2);
    scheduler.addTask("Telemetry", telemetryTask, 5000, 0, 1);

    scheduler.enableWatchdog(30);
    scheduler.start();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
