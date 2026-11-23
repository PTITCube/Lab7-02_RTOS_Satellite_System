/**
 * LAB 7-02: HỆ THỐNG PHẦN MỀM BAY ĐA NHIỆM FREERTOS (HEPTA-SAT PATTERN)
 * Tích hợp: C&DH, EPS, Sensor, GPS, Camera, Storage, COM (FreeRTOS Multitasking).
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

const float BUS_VOLTAGE_TURN_OFF = 3.5;
const float BUS_VOLTAGE_TURN_ON  = 3.7;
bool buck_is_on = true;
bool isTelemetryPaused = false;

// Task 1: Cảm biến I2C (Core 1)
void sensorTask() {
    if (isTelemetryPaused) return;
    if (scheduler.lock(PTIT_BUS_I2C)) {
        sensor.update();
        scheduler.unlock(PTIT_BUS_I2C);
    }
}

// Task 2: Định vị GPS UART2 (Core 1)
void gpsTask() {
    if (isTelemetryPaused) return;
    if (scheduler.lock(PTIT_BUS_UART2, 50)) {
        camera.selectGPS();
        gps.update();
        scheduler.unlock(PTIT_BUS_UART2);
    }
}

// Chụp ảnh bằng ESP32-CAM và lưu vào thẻ SD
bool capturePhotoToSD() {
    if (!scheduler.lock(PTIT_BUS_UART2, 1000)) return false;
    if (!scheduler.lock(PTIT_BUS_SPI, 1000)) {
        scheduler.unlock(PTIT_BUS_UART2);
        return false;
    }

    camera.selectCamera();
    delay(200);
    static int photoCounter = 0;
    String path = "/photos/rtos_" + String(photoCounter++) + ".jpg";
    bool ok = camera.captureToFile(path.c_str());
    camera.selectGPS();
    delay(100);

    scheduler.unlock(PTIT_BUS_SPI);
    scheduler.unlock(PTIT_BUS_UART2);

    scheduler.println(ok ? ("Photo captured! Path: " + path) : "Camera capture failed!");
    return ok;
}

// Xử lý lệnh từ trạm mặt đất (HEPTA-SAT 'a'..'f' + LMS System Commands)
void processCommand(const String& cmdStr) {
    if (cmdStr.length() == 0) return;
    char c = cmdStr.charAt(0);
    scheduler.println("Received command: " + cmdStr);

    if (cmdStr.length() == 1) {
        switch (c) {
            case 'a':
                for (uint8_t i = 0; i < 4; i++) {
                    if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                        com.sendMessage("ACK " + String(i + 1) + "/4");
                        scheduler.unlock(PTIT_BUS_LORA);
                    }
                    delay(200);
                }
                return;
            case 'b':
                if (scheduler.lock(PTIT_BUS_SPI, 500)) {
                    storage.writeFile("/voltage.txt", "");
                    for (uint8_t i = 0; i < 5; i++) {
                        eps.update();
                        storage.appendFile("/voltage.txt", ("Bus voltage: " + String(eps.getBatteryVoltage(), 2) + " V\r\n").c_str());
                        delay(200);
                    }
                    String data = storage.readFile("/voltage.txt");
                    scheduler.unlock(PTIT_BUS_SPI);
                    if (scheduler.lock(PTIT_BUS_LORA, 500)) {
                        com.sendMessage(data);
                        scheduler.unlock(PTIT_BUS_LORA);
                    }
                }
                return;
            case 'c': {
                String msg = "Acceleration: ax=" + String(sensor.getAccX(), 2) + ", ay=" + String(sensor.getAccY(), 2) + ", az=" + String(sensor.getAccZ(), 2) + " m/s^2";
                scheduler.println(msg);
                if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(msg); scheduler.unlock(PTIT_BUS_LORA); }
                return;
            }
            case 'd': {
                String msg = "Gyroscope: gx=" + String(sensor.getGyroX(), 2) + ", gy=" + String(sensor.getGyroY(), 2) + ", gz=" + String(sensor.getGyroZ(), 2) + " rad/s";
                scheduler.println(msg);
                if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(msg); scheduler.unlock(PTIT_BUS_LORA); }
                return;
            }
            case 'e': {
                String msg = gps.hasFix() ? ("GPS: Lat=" + String(gps.getLatitude(), 6) + ", Lng=" + String(gps.getLongitude(), 6)) : "GPS: no valid fix";
                scheduler.println(msg);
                if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(msg); scheduler.unlock(PTIT_BUS_LORA); }
                return;
            }
            case 'f': {
                bool ok = capturePhotoToSD();
                String msg = ok ? "Photo captured!" : "ERR: Capture failed";
                if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(msg); scheduler.unlock(PTIT_BUS_LORA); }
                return;
            }
            default: break;
        }
    }

    String resp = "";
    if (cmdStr == "ping") {
        for (uint8_t i = 0; i < 4; i++) {
            if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage("ACK " + String(i + 1) + "/4"); scheduler.unlock(PTIT_BUS_LORA); }
            delay(200);
        }
        return;
    } else if (cmdStr == "reboot") {
        if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage("Rebooting..."); scheduler.unlock(PTIT_BUS_LORA); }
        delay(500);
        ESP.restart();
    } else if (cmdStr == "status") {
        resp = "STATUS: Up " + String(millis() / 1000) + "s, Bat=" + String(eps.getBatteryVoltage(), 1) + "V, SD=" + (storage.isReady() ? "OK" : "ERR") + (isTelemetryPaused ? " (PAUSED)" : " (ACTIVE)");
    } else if (cmdStr == "uptime") {
        unsigned long sec = millis() / 1000;
        resp = "Uptime: " + String(sec / 3600) + "h " + String((sec / 60) % 60) + "m " + String(sec % 60) + "s";
    } else if (cmdStr == "get_sensor") {
        resp = "Sensor: Temp=" + String(sensor.getTemperature(), 2) + "C, Press=" + String(sensor.getPressure(), 2) + "hPa";
    } else if (cmdStr == "get_eps") {
        resp = "EPS: Battery=" + String(eps.getBatteryVoltage(), 2) + "V";
    } else if (cmdStr == "get_gps") {
        resp = gps.hasFix() ? ("GPS: Lat=" + String(gps.getLatitude(), 6) + ", Lng=" + String(gps.getLongitude(), 6)) : "GPS: no valid fix";
    } else if (cmdStr == "capture") {
        resp = capturePhotoToSD() ? "Photo captured!" : "ERR: Capture failed";
    } else if (cmdStr == "pause") {
        isTelemetryPaused = true;
        resp = "Telemetry PAUSED. Satellite is in standby mode.";
    } else if (cmdStr == "resume") {
        isTelemetryPaused = false;
        resp = "Telemetry RESUMED. Realtime data active.";
    } else if (cmdStr == "help") {
        resp = "ping|status|uptime|get_sensor|get_eps|get_gps|pause|resume|capture|reboot|help";
    } else {
        resp = "Unknown command";
    }

    scheduler.println("[CMD] " + cmdStr + " -> " + resp);
    if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(resp); scheduler.unlock(PTIT_BUS_LORA); }
}

// Task 3: Nhận lệnh LoRa (Core 0)
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

// Task 4: Viễn trắc HK theo từng chunk (Core 0)
void telemetryTask() {
    if (isTelemetryPaused) return;

    // Giám sát nguồn Hysteresis
    float bus_v = eps.getBatteryVoltage();
    if (buck_is_on && bus_v < BUS_VOLTAGE_TURN_OFF) {
        scheduler.println("Bus voltage is low! Switching off Buck converter to save power.");
        eps.setBuckState(false);
        buck_is_on = false;
    } else if (!buck_is_on && bus_v > BUS_VOLTAGE_TURN_ON) {
        scheduler.println("Bus voltage recovered. Switching on Buck converter.");
        eps.setBuckState(true);
        buck_is_on = true;
    }

    // 6 Chunks chuẩn cho LMS và LoRa (< 58 bytes)
    String payloads[6];
    const char* labels[] = {"EPS", "Env", "Acc", "Gyro", "Mag", "GPS"};

    payloads[0] = "{\"battery_v\":" + String(eps.getBatteryVoltage(), 2) + "}";
    payloads[1] = "{\"temp\":" + String(sensor.getTemperature(), 2) + ",\"pressure\":" + String(sensor.getPressure(), 2) + "}";
    payloads[2] = "{\"acc_x\":" + String(sensor.getAccX(), 2) + ",\"acc_y\":" + String(sensor.getAccY(), 2) + ",\"acc_z\":" + String(sensor.getAccZ(), 2) + "}";
    payloads[3] = "{\"gyro_x\":" + String(sensor.getGyroX(), 2) + ",\"gyro_y\":" + String(sensor.getGyroY(), 2) + ",\"gyro_z\":" + String(sensor.getGyroZ(), 2) + "}";
    payloads[4] = "{\"mag_x\":" + String(sensor.getMagX(), 2) + ",\"mag_y\":" + String(sensor.getMagY(), 2) + ",\"mag_z\":" + String(sensor.getMagZ(), 2) + "}";
    payloads[5] = gps.hasFix() ? ("{\"valid\":true,\"lat\":" + String(gps.getLatitude(), 4) + ",\"lng\":" + String(gps.getLongitude(), 4) + "}") : "{\"valid\":false,\"lat\":null,\"lng\":null}";

    for (int i = 0; i < 6; i++) {
        scheduler.println(String("[TELEMETRY] ") + labels[i] + ": " + payloads[i]);
    }

    if (scheduler.lock(PTIT_BUS_SPI, 200)) {
        String batch = "";
        for (int i = 0; i < 6; i++) batch += payloads[i] + "\n";
        bool saved = storage.appendFile("/datalog.txt", batch.c_str());
        scheduler.println(saved ? "[SD] telemetry saved" : "[SD] telemetry write failed");
        scheduler.unlock(PTIT_BUS_SPI);
    }

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
    eps.setBuckState(true);

    eps.init();
    sensor.init();
    gps.init();
    camera.init();
    storage.init();
    com.init();

    com.setChannel(23);
    com.setTransmissionMode(0);
    camera.selectGPS();

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
