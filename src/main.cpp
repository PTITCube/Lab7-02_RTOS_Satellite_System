/**
 * ============================================================================
 * LAB 7 - VÍ DỤ 02: HỆ THỐNG PHẦN MỀM BAY ĐA NHIỆM FREERTOS (HEPTA-SAT PATTERN)
 * ============================================================================
 * Mô tả:
 *   - Sử dụng PTIT_Scheduler để quản lý đa nhiệm FreeRTOS trên 2 nhân ESP32.
 *   - Tích hợp đầy đủ phân hệ: C&DH, EPS, Sensor, GPS, Camera, Storage, COM.
 *   - Quản lý điện áp và chuyển chế độ tiết kiệm điện (Hysteresis) như HEPTA-SAT.
 *   - Phân chia gói tin viễn trắc (Housekeeping Chunks) gửi qua LoRa và ghi thẻ SD.
 *   - Nhận và thực thi lệnh điều khiển từ mặt đất (chuẩn lệnh HEPTA-SAT + System).
 * ============================================================================
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PTITCube.h>

// 1. Khai báo các đối tượng phân hệ vệ tinh
PTIT_CDH cdh;
PTIT_EPS eps;
PTIT_Sensor sensor;
PTIT_GPS gps;
PTIT_Camera camera;
PTIT_Storage storage;
PTIT_COM com;

PTIT_Scheduler scheduler;

// 2. Ngưỡng điện áp quản lý năng lượng (Hysteresis như HEPTA-SAT)
const float BUS_VOLTAGE_TURN_OFF = 3.5; // Dưới 3.5V: ngắt Buck tiết kiệm pin
const float BUS_VOLTAGE_TURN_ON  = 3.7; // Trên 3.7V: bật lại Buck
bool buck_is_on = true;
bool isTelemetryPaused = false;

// 3. Task 1: Thu thập dữ liệu cảm biến I2C (BMP280, MPU6500, QMC5883P - Core 1)
void sensorTask() {
    if (isTelemetryPaused) return;
    if (scheduler.lock(PTIT_BUS_I2C)) {
        sensor.update();
        scheduler.unlock(PTIT_BUS_I2C);
    }
}

// 4. Task 2: Cập nhật dòng NMEA từ GPS qua MUX UART2 (Core 1)
void gpsTask() {
    if (isTelemetryPaused) return;
    if (scheduler.lock(PTIT_BUS_UART2, 50)) {
        camera.selectGPS();
        gps.update();
        scheduler.unlock(PTIT_BUS_UART2);
    }
}

// 5. Hàm chụp ảnh và lưu thẻ nhớ SD (Đặc biệt có camera)
bool capturePhotoToSD() {
    if (!scheduler.lock(PTIT_BUS_UART2, 1000)) return false;
    if (!scheduler.lock(PTIT_BUS_SPI, 1000)) {
        scheduler.unlock(PTIT_BUS_UART2);
        return false;
    }

    camera.selectCamera();
    delay(200);

    static int photoCounter = 0;
    String photoPath = "/photos/rtos_" + String(photoCounter++) + ".jpg";
    bool success = camera.captureToFile(photoPath.c_str());

    camera.selectGPS();
    delay(100);

    scheduler.unlock(PTIT_BUS_SPI);
    scheduler.unlock(PTIT_BUS_UART2);

    if (success) {
        scheduler.println("Photo captured! Path: " + photoPath);
    } else {
        scheduler.println("Camera capture failed!");
    }
    return success;
}

// 6. Xử lý lệnh từ trạm mặt đất qua LoRa (HEPTA-SAT + Diagnostic Commands)
void processCommand(const String& cmdStr) {
    if (cmdStr.length() == 0) return;

    char cmdChar = cmdStr.charAt(0);
    scheduler.println("Received command: " + cmdStr);

    // Xử lý các lệnh ký tự đơn chuẩn HEPTA-SAT ('a', 'b', 'c', 'd', 'e', 'f')
    if (cmdStr.length() == 1) {
        switch (cmdChar) {
            case 'a': { // Echo / Beacon test
                for (uint8_t i = 0; i < 4; i++) {
                    if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                        com.sendMessage("ACK " + String(i + 1) + "/4");
                        scheduler.unlock(PTIT_BUS_LORA);
                    }
                    delay(200);
                }
                return;
            }

            case 'b': { // Lưu điện áp vào thẻ nhớ và đọc lại
                scheduler.println("Saving voltages to the SD card...");
                if (scheduler.lock(PTIT_BUS_SPI, 500)) {
                    storage.writeFile("/voltage.txt", "");
                    for (uint8_t i = 0; i < 5; i++) {
                        eps.update();
                        float bus_voltage = eps.getBatteryVoltage();
                        String logLine = "Bus voltage: " + String(bus_voltage, 2) + " V\r\n";
                        storage.appendFile("/voltage.txt", logLine.c_str());
                        delay(200);
                    }
                    scheduler.println("Done saving voltages to the SD card.");
                    scheduler.println("Reading voltages from the SD card...");
                    String content = storage.readFile("/voltage.txt");
                    scheduler.unlock(PTIT_BUS_SPI);

                    if (scheduler.lock(PTIT_BUS_LORA, 500)) {
                        com.sendMessage(content);
                        scheduler.unlock(PTIT_BUS_LORA);
                    }
                    scheduler.println("Done reading voltages from the SD card.");
                }
                return;
            }

            case 'c': { // Đọc gia tốc
                float ax = sensor.getAccX();
                float ay = sensor.getAccY();
                float az = sensor.getAccZ();
                String accMsg = "Acceleration: ax=" + String(ax, 2) + ", ay=" + String(ay, 2) + ", az=" + String(az, 2) + " m/s^2";
                scheduler.println(accMsg);
                if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                    com.sendMessage(accMsg);
                    scheduler.unlock(PTIT_BUS_LORA);
                }
                return;
            }

            case 'd': { // Đọc góc quay Gyroscope
                float gx = sensor.getGyroX();
                float gy = sensor.getGyroY();
                float gz = sensor.getGyroZ();
                String gyroMsg = "Gyroscope: gx=" + String(gx, 2) + ", gy=" + String(gy, 2) + ", gz=" + String(gz, 2) + " rad/s";
                scheduler.println(gyroMsg);
                if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                    com.sendMessage(gyroMsg);
                    scheduler.unlock(PTIT_BUS_LORA);
                }
                return;
            }

            case 'e': { // Đọc tọa độ GPS
                String gpsMsg;
                if (gps.hasFix()) {
                    gpsMsg = "GPS: Lat=" + String(gps.getLatitude(), 6) + ", Lng=" + String(gps.getLongitude(), 6);
                } else {
                    gpsMsg = "GPS: no valid fix";
                }
                scheduler.println(gpsMsg);
                if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                    com.sendMessage(gpsMsg);
                    scheduler.unlock(PTIT_BUS_LORA);
                }
                return;
            }

            case 'f': { // Chụp ảnh Camera ESP32-CAM
                scheduler.println("Capturing photo with ESP32-CAM...");
                bool captured = capturePhotoToSD();
                String camMsg = captured ? "Photo captured!" : "ERR: Capture failed";
                if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                    com.sendMessage(camMsg);
                    scheduler.unlock(PTIT_BUS_LORA);
                }
                return;
            }

            default:
                break;
        }
    }

    // Xử lý các lệnh chuỗi hệ thống (System & Diagnostic Commands)
    String response = "";
    if (cmdStr == "ping") {
        for (uint8_t i = 0; i < 4; i++) {
            if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                com.sendMessage("ACK " + String(i + 1) + "/4");
                scheduler.unlock(PTIT_BUS_LORA);
            }
            delay(200);
        }
        return;
    } else if (cmdStr == "reboot") {
        if (scheduler.lock(PTIT_BUS_LORA, 200)) {
            com.sendMessage("Rebooting...");
            scheduler.unlock(PTIT_BUS_LORA);
        }
        delay(500);
        ESP.restart();
    } else if (cmdStr == "status") {
        response = "STATUS: Up " + String(millis() / 1000) + "s, Bat=" + String(eps.getBatteryVoltage(), 1) +
                   "V, SD=" + (storage.isReady() ? "OK" : "ERR") + (isTelemetryPaused ? " (PAUSED)" : " (ACTIVE)");
    } else if (cmdStr == "uptime") {
        unsigned long sec = millis() / 1000;
        response = "Uptime: " + String(sec / 3600) + "h " + String((sec / 60) % 60) + "m " + String(sec % 60) + "s";
    } else if (cmdStr == "get_sensor") {
        response = "Sensor: Temp=" + String(sensor.getTemperature(), 2) + "C, Press=" + String(sensor.getPressure(), 2) + "hPa";
    } else if (cmdStr == "get_eps") {
        response = "EPS: Battery=" + String(eps.getBatteryVoltage(), 2) + "V";
    } else if (cmdStr == "get_gps") {
        if (gps.hasFix()) {
            response = "GPS: Lat=" + String(gps.getLatitude(), 6) + ", Lng=" + String(gps.getLongitude(), 6);
        } else {
            response = "GPS: no valid fix";
        }
    } else if (cmdStr == "capture") {
        bool captured = capturePhotoToSD();
        response = captured ? "Photo captured!" : "ERR: Capture failed";
    } else if (cmdStr == "pause") {
        isTelemetryPaused = true;
        response = "Telemetry PAUSED. Satellite is in standby mode.";
    } else if (cmdStr == "resume") {
        isTelemetryPaused = false;
        response = "Telemetry RESUMED. Realtime data active.";
    } else if (cmdStr == "help") {
        response = "ping|status|uptime|get_sensor|get_eps|get_gps|pause|resume|capture|reboot|help";
    } else {
        response = "Unknown command";
    }

    scheduler.println("[CMD] " + cmdStr + " -> " + response);
    if (scheduler.lock(PTIT_BUS_LORA, 200)) {
        com.sendMessage(response);
        scheduler.unlock(PTIT_BUS_LORA);
    }
}

// 7. Task 3: Nhận và xử lý lệnh từ trạm mặt đất qua LoRa (Core 0)
void comTask() {
    String cmd = "";
    if (scheduler.lock(PTIT_BUS_LORA)) {
        cmd = com.update();
        scheduler.unlock(PTIT_BUS_LORA);
    }

    if (cmd.length() > 0) {
        cmd.trim();
        processCommand(cmd);
    }
}

// 8. Task 4: Giám sát nguồn và truyền HK data theo từng chunk (Core 0)
void telemetryTask() {
    if (isTelemetryPaused) return;

    // A. Giám sát điện áp pin & chuyển chế độ tiết kiệm điện (Hysteresis như HEPTA-SAT)
    float bus_voltage = eps.getBatteryVoltage();
    if (buck_is_on && bus_voltage < BUS_VOLTAGE_TURN_OFF) {
        scheduler.println("Bus voltage is low! Switching off Buck converter to save power.");
        eps.setBuckState(false);
        buck_is_on = false;
    } else if (!buck_is_on && bus_voltage > BUS_VOLTAGE_TURN_ON) {
        scheduler.println("Bus voltage recovered. Switching on Buck converter.");
        eps.setBuckState(true);
        buck_is_on = true;
    }

    // B. Đóng gói HK Data thành từng Chunk chuẩn (< 58 bytes cho LoRa)
    String payloads[6];
    const char* labels[] = {"EPS", "Env", "Acc", "Gyro", "Mag", "GPS"};

    // Chunk 0: EPS
    payloads[0] = "{\"battery_v\":" + String(eps.getBatteryVoltage(), 2) + "}";

    // Chunk 1: Env
    payloads[1] = "{\"temp\":" + String(sensor.getTemperature(), 2) + 
                  ",\"pressure\":" + String(sensor.getPressure(), 2) + "}";

    // Chunk 2: Acc
    payloads[2] = "{\"acc_x\":" + String(sensor.getAccX(), 2) + 
                  ",\"acc_y\":" + String(sensor.getAccY(), 2) + 
                  ",\"acc_z\":" + String(sensor.getAccZ(), 2) + "}";

    // Chunk 3: Gyro
    payloads[3] = "{\"gyro_x\":" + String(sensor.getGyroX(), 2) + 
                  ",\"gyro_y\":" + String(sensor.getGyroY(), 2) + 
                  ",\"gyro_z\":" + String(sensor.getGyroZ(), 2) + "}";

    // Chunk 4: Mag
    payloads[4] = "{\"mag_x\":" + String(sensor.getMagX(), 2) + 
                  ",\"mag_y\":" + String(sensor.getMagY(), 2) + 
                  ",\"mag_z\":" + String(sensor.getMagZ(), 2) + "}";

    // Chunk 5: GPS
    if (gps.hasFix()) {
        payloads[5] = "{\"valid\":true,\"lat\":" + String(gps.getLatitude(), 4) + 
                      ",\"lng\":" + String(gps.getLongitude(), 4) + "}";
    } else {
        payloads[5] = "{\"valid\":false,\"lat\":null,\"lng\":null}";
    }

    // C. Xuất ra Serial chuẩn viễn trắc cho C&DH
    for (int i = 0; i < 6; i++) {
        scheduler.println(String("[TELEMETRY] ") + labels[i] + ": " + payloads[i]);
    }

    // D. Ghi lưu từng chunk vào thẻ nhớ SD
    if (scheduler.lock(PTIT_BUS_SPI, 200)) {
        String batch = "";
        for (int i = 0; i < 6; i++) {
            batch += payloads[i] + "\n";
        }
        bool saved = storage.appendFile("/datalog.txt", batch.c_str());
        scheduler.println(saved ? "[SD] telemetry saved" : "[SD] telemetry write failed");
        scheduler.unlock(PTIT_BUS_SPI);
    }

    // E. Truyền từng Chunk qua LoRa có khoảng giãn cách (Tránh tràn đệm LoRa)
    for (int i = 0; i < 6; i++) {
        if (isTelemetryPaused) break;
        if (scheduler.lock(PTIT_BUS_LORA, 200)) {
            com.sendMessage(payloads[i]);
            scheduler.unlock(PTIT_BUS_LORA);
        }
        delay(100);
    }
}

void setup() {
    cdh.begin();

    // Kích hoạt hạ áp cấp nguồn ngoại vi trên PCB vệ tinh qua EPS
    eps.setBuckState(true);

    eps.init();
    sensor.init();
    gps.init();
    camera.init();
    storage.init();
    com.init();

    com.setChannel(23);
    com.setTransmissionMode(0);

    camera.selectGPS(); // Mặc định chuyển UART2 tới GPS

    // Đăng ký các task vào Scheduler: (Tên, hàm, chu kỳ ms, Core, Priority)
    scheduler.addTask("Sensor",    sensorTask,    200,  1, 3);
    scheduler.addTask("GPS",       gpsTask,       50,   1, 3);
    scheduler.addTask("COM",       comTask,       100,  0, 2);
    scheduler.addTask("Telemetry", telemetryTask, 3000, 0, 1);

    scheduler.enableWatchdog(30);
    scheduler.start();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
