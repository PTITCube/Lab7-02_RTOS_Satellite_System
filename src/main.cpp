/**
 * CUBESAT FIRMWARE - MAIN FILE
 * Đây là file chạy tổng thể kết hợp toàn bộ các phân hệ của vệ tinh:
 * - EPS (Quản lý pin)
 * - Cảm biến (Gia tốc, La bàn, Nhiệt độ, Áp suất)
 * - GPS (Định vị)
 * - Thẻ nhớ MicroSD (Lưu trữ log nội bộ)
 * - Viễn thông LoRa E32 (Gửi/Nhận tin nhắn vô tuyến)
 * - CDH (Xử lý trung tâm, WiFi & MQTT Telemetry)
 *
 * FreeRTOS được quản lý tự động bởi PTIT_Scheduler.
 * Người dùng chỉ cần viết logic nghiệp vụ trong các hàm thông thường.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include "PTITCube.h"

// ============================================================================
// 1. KHAI BÁO CÁC ĐỐI TƯỢNG PHÂN HỆ VỆ TINH
// ============================================================================
PTIT_Sensor sensor;
PTIT_GPS gps;
PTIT_COM lora;
PTIT_EPS eps;
PTIT_Storage storage;
PTIT_CDH cdh;

PTIT_Scheduler scheduler;

// Trạng thái hệ thống
bool isTelemetryPaused = false;

// ============================================================================
// 2. ĐỊNH NGHĨA CÁC HÀM TASK (logic nghiệp vụ thuần túy)
//    Mỗi hàm sẽ được Scheduler gọi lặp lại theo chu kỳ đã đăng ký.
// ============================================================================

// CDH: Duy trì kết nối WiFi & MQTT
void cdhLoop() {
    cdh.update();
}

// Sensor: Đọc cảm biến qua I2C (BMP280, MPU6050, QMC5883L)
void sensorLoop() {
    if (isTelemetryPaused) return;
    if (PTIT_Scheduler::lock(PTIT_BUS_I2C)) {
        sensor.update();
        PTIT_Scheduler::unlock(PTIT_BUS_I2C);
    }
}

// GPS: Feed dữ liệu NMEA từ UART2
void gpsLoop() {
    if (isTelemetryPaused) return;
    gps.update();
}

// COM: Nhận và xử lý lệnh từ trạm mặt đất qua LoRa
// Xử lý các lệnh hệ thống (trả về chuỗi rỗng nếu không phải lệnh hệ thống)
String processSystemCommand(String command) {
    if (command == "ping") {
        PTIT_Scheduler::println("[CMD] ping");
        if (PTIT_Scheduler::lock(PTIT_BUS_LORA, 500)) {
            for (int i = 0; i < 4; i++) {
                lora.sendMessage("ACK " + String(i + 1) + "/4");
                delay(200);
            }
            PTIT_Scheduler::unlock(PTIT_BUS_LORA);
        }
        return "PING_HANDLED"; // Cờ đặc biệt để không gửi response chung
    }
    if (command == "reboot") {
        PTIT_Scheduler::println("[CMD] reboot");
        if (PTIT_Scheduler::lock(PTIT_BUS_LORA, 500)) lora.sendMessage("Rebooting...");
        PTIT_Scheduler::unlock(PTIT_BUS_LORA);
        delay(500);
        ESP.restart();
    }
    if (command == "get_gps")    return "GPS: Lat=" + String(gps.getLat(), 6) + ", Lng=" + String(gps.getLng(), 6) + ", Alt=" + String(gps.getAltitude(), 2) + "m";
    if (command == "get_sensor") return "Sensor: Temp=" + String(sensor.getTemperature(), 2) + "C, Press=" + String(sensor.getPressure(), 2) + "hPa";
    if (command == "get_eps")    return "EPS: Battery=" + String(eps.getBatteryVoltage(), 2) + "V";
    if (command == "clear_log") {
        if (!PTIT_Scheduler::lock(PTIT_BUS_SPI, 200)) return "ERR: SD busy";
        String res = storage.removeFile("/datalog.txt") ? "Log cleared" : "ERR: rm /datalog.txt";
        PTIT_Scheduler::unlock(PTIT_BUS_SPI);
        return res;
    }
    if (command == "status") {
        return "STATUS: Up " + String(millis() / 1000) + "s, Bat=" + String(eps.getBatteryVoltage(), 1) + 
               "V, SD=" + (storage.isReady() ? "OK" : "ERR") + (isTelemetryPaused ? " (PAUSED)" : " (ACTIVE)");
    }
    if (command == "uptime") {
        unsigned long sec = millis() / 1000;
        return "Uptime: " + String(sec / 3600) + "h " + String((sec / 60) % 60) + "m " + String(sec % 60) + "s";
    }
    if (command == "pause") {
        isTelemetryPaused = true;
        return "Telemetry PAUSED. Satellite is in standby mode.";
    }
    if (command == "resume") {
        isTelemetryPaused = false;
        return "Telemetry RESUMED. Realtime data active.";
    }
    if (command == "help") {
        return "ping|reboot|uptime|get_gps|get_sensor|get_eps|status|clear_log|pause|resume|pwd|cd|ls|mkdir|rmdir|rm|touch|help";
    }
    return "";
}

// Hỗ trợ lệnh hệ thống + lệnh file/thư mục kiểu Linux
void comLoop() {
    String cmd = "";
    if (PTIT_Scheduler::lock(PTIT_BUS_LORA)) {
        cmd = lora.update();
        PTIT_Scheduler::unlock(PTIT_BUS_LORA);
    }

    if (cmd.length() > 0) {
        cmd.trim();
        String command = PTIT_Utils::extractCommand(cmd);
        String response = processSystemCommand(command);

        if (response == "PING_HANDLED") return; // Lệnh ping đã tự xử lý

        // Nếu không phải lệnh hệ thống, chuyển cho SD Card xử lý
        if (response == "") {
            if (PTIT_Scheduler::lock(PTIT_BUS_SPI, 200)) {
                response = storage.handleCommand(cmd);
                PTIT_Scheduler::unlock(PTIT_BUS_SPI);
            } else {
                response = "ERR: SD busy";
            }
            if (response == "") response = "ERR: Unknown cmd '" + command + "'. Send 'help'";
        }

        // Gửi phản hồi qua LoRa
        PTIT_Scheduler::println("[CMD] " + command + " → " + response);
        if (PTIT_Scheduler::lock(PTIT_BUS_LORA, 500)) {
            lora.sendMessage(response);
            PTIT_Scheduler::unlock(PTIT_BUS_LORA);
        }
    }
}

// Telemetry: Thu thập dữ liệu → gửi MQTT + LoRa + ghi SD
void telemetryLoop() {
    if (isTelemetryPaused) return;

    auto buildJson = [](const String& content) { return "{" + content + "}"; };

    float v = eps.getBatteryVoltage();
    String epsMqtt = buildJson("\"battery_v\":" + String(v, 2));
    String epsLora = buildJson("\"battery_v\":" + String(v, 3));

    float t = sensor.getTemperature(), p = sensor.getPressure();
    String envMqtt = buildJson("\"temp\":" + String(t, 2) + ",\"pressure\":" + String(p, 2));
    String envLora = buildJson("\"temp\":" + String(t, 3) + ",\"pressure\":" + String(p, 3));

    float ax = sensor.getAccX(), ay = sensor.getAccY(), az = sensor.getAccZ();
    String accMqtt = buildJson("\"acc_x\":" + String(ax, 2) + ",\"acc_y\":" + String(ay, 2) + ",\"acc_z\":" + String(az, 2));
    String accLora = buildJson("\"acc_x\":" + String(ax, 3) + ",\"acc_y\":" + String(ay, 3) + ",\"acc_z\":" + String(az, 3));

    float gx = sensor.getGyroX(), gy = sensor.getGyroY(), gz = sensor.getGyroZ();
    String gyroMqtt = buildJson("\"gyro_x\":" + String(gx, 2) + ",\"gyro_y\":" + String(gy, 2) + ",\"gyro_z\":" + String(gz, 2));
    String gyroLora = buildJson("\"gyro_x\":" + String(gx, 3) + ",\"gyro_y\":" + String(gy, 3) + ",\"gyro_z\":" + String(gz, 3));

    float mx = sensor.getMagX(), my = sensor.getMagY(), mz = sensor.getMagZ();
    String magMqtt = buildJson("\"mag_x\":" + String(mx, 2) + ",\"mag_y\":" + String(my, 2) + ",\"mag_z\":" + String(mz, 2));
    String magLora = buildJson("\"mag_x\":" + String(mx, 3) + ",\"mag_y\":" + String(my, 3) + ",\"mag_z\":" + String(mz, 3));

    float lat = gps.getLat(), lng = gps.getLng();
    String gpsMqtt = buildJson("\"lat\":" + String(lat, 6) + ",\"lng\":" + String(lng, 6));
    String gpsLora = buildJson("\"lat\":" + String(lat, 6) + ",\"lng\":" + String(lng, 6)); // GPS cần giữ độ phân giải 6 số

    PTIT_Scheduler::println("\n[TELEMETRY] EPS: " + epsMqtt);
    PTIT_Scheduler::println("[TELEMETRY] Env: " + envMqtt);
    PTIT_Scheduler::println("[TELEMETRY] Acc: " + accMqtt);
    PTIT_Scheduler::println("[TELEMETRY] Gyro: " + gyroMqtt);
    PTIT_Scheduler::println("[TELEMETRY] Mag: " + magMqtt);
    PTIT_Scheduler::println("[TELEMETRY] GPS: " + gpsMqtt);

    if (PTIT_Scheduler::lock(PTIT_BUS_SPI, 200)) {
        storage.logData(epsMqtt.c_str()); storage.logData(envMqtt.c_str());
        storage.logData(accMqtt.c_str()); storage.logData(gyroMqtt.c_str());
        storage.logData(magMqtt.c_str()); storage.logData(gpsMqtt.c_str());
        PTIT_Scheduler::unlock(PTIT_BUS_SPI);
    }

    cdh.publishData("cubesat/telemetry/eps", epsMqtt.c_str());
    cdh.publishData("cubesat/telemetry/env", envMqtt.c_str());
    cdh.publishData("cubesat/telemetry/acc", accMqtt.c_str());
    cdh.publishData("cubesat/telemetry/gyro", gyroMqtt.c_str());
    cdh.publishData("cubesat/telemetry/mag", magMqtt.c_str());
    cdh.publishData("cubesat/telemetry/gps", gpsMqtt.c_str());

    auto sendLoraChunk = [&](const String& payload) {
        if (isTelemetryPaused) return;
        if (PTIT_Scheduler::lock(PTIT_BUS_LORA, 200)) {
            lora.sendMessage(payload);
            PTIT_Scheduler::unlock(PTIT_BUS_LORA);
        }
        delay(150);
    };

    sendLoraChunk(epsLora);
    sendLoraChunk(envLora);
    sendLoraChunk(accLora);
    sendLoraChunk(gyroLora);
    sendLoraChunk(magLora);
    sendLoraChunk(gpsLora);
}

// ============================================================================
// 3. SETUP - Khởi tạo phân hệ + đăng ký task
// ============================================================================
void setup() {
    Serial.begin(115200);
    while (!Serial) { delay(10); }

    // Khởi tạo các phân hệ
    sensor.init();
    gps.init();
    lora.init();
    eps.init();
    storage.init();
    cdh.init();

    // Cấu hình LoRa
    lora.setChannel(23);
    lora.setTransmissionMode(0);

    // Đăng ký các task vào Scheduler
    //                      Tên            Hàm            Chu kỳ   Core  Priority  Stack
    scheduler.addTask("CDH",       cdhLoop,       500,    0,    2,        8192);
    scheduler.addTask("Sensor",    sensorLoop,    200,    1,    3,        4096);
    scheduler.addTask("GPS",       gpsLoop,       10,     1,    3,        4096);
    scheduler.addTask("COM",       comLoop,       100,    1,    2,        4096);
    scheduler.addTask("Telemetry", telemetryLoop, 5000,   1,    1,        8192);

    // Bật watchdog: nếu task bị treo > 30s → ESP32 tự reset
    scheduler.enableWatchdog(30);

    // Bắt đầu chạy đa nhiệm!
    scheduler.start();

    Serial.println("CubeSat Firmware v2.0 — Đang hoạt động!");
}

// ============================================================================
// 4. LOOP - Trống (logic đã phân phối vào các task riêng)
// ============================================================================
void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
