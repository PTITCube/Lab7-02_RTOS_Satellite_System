/**
 * LAB 7-02: HỆ THỐNG PHẦN MỀM BAY ĐA NHIỆM FREERTOS (PTITCUBE CUBESAT ARCHITECTURE)
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

// Ngưỡng bảo vệ nguồn & Chế độ tiết kiệm năng lượng
const float VOLTAGE_THRES = 3.5;  // V
const float PERCENT_THRES = 20.0; // %
bool isPowerSaving = false;
bool isTelemetryPaused = false;
unsigned long lastCmdTime = 0;

// Task 1: Cảm biến I2C (Core 1 - Vô hiệu hóa khi ở Power Saving)
void sensorTask() {
    if (isTelemetryPaused || isPowerSaving) return;
    if (scheduler.lock(PTIT_BUS_I2C)) {
        sensor.update();
        scheduler.unlock(PTIT_BUS_I2C);
    }
}

// Task 2: Định vị GPS UART2 (Core 1 - Luôn duy trì qua MUX)
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
    if (isPowerSaving) {
        scheduler.println("ERR: Camera disabled in power saving mode!");
        return false;
    }
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

// Xử lý lệnh từ trạm mặt đất (Lệnh 'a'..'f' và các ký tự điều khiển)
void processCommand(const String& cmdStr) {
    if (cmdStr.length() == 0) return;
    lastCmdTime = millis();
    delay(100); // Độ trễ ổn định RF thu nhận uplink và chuẩn bị phản hồi

    scheduler.println("Received command: " + cmdStr);

    if (cmdStr == "reboot") {
        if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage("Rebooting..."); scheduler.unlock(PTIT_BUS_LORA); }
        delay(500);
        ESP.restart();
    }

    char c = cmdStr.charAt(0);
    switch (c) {
        case 'a':
            for (uint8_t i = 0; i < 4; i++) {
                if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                    com.sendMessage("ACK " + String(i + 1) + "/4");
                    scheduler.unlock(PTIT_BUS_LORA);
                }
                delay(250);
            }
            break;
        case 'b': { // Lưu và đọc lại điện áp & dung lượng pin từ thẻ nhớ SD (nhận nguồn từ OBC, luôn hoạt động)
            scheduler.println("Saving battery data to SD card...");
            if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                com.sendMessage("Saving battery data to SD card...");
                scheduler.unlock(PTIT_BUS_LORA);
            }
            if (scheduler.lock(PTIT_BUS_SPI, 500)) {
                storage.writeFile("/voltage.txt", "");
                for (uint8_t i = 0; i < 5; i++) {
                    if (scheduler.lock(PTIT_BUS_I2C, 200)) {
                        eps.update();
                        scheduler.unlock(PTIT_BUS_I2C);
                    }
                    float v = eps.getBatteryVoltage();
                    float pct = eps.getBatteryPercentage();
                    String line = "[" + String(i + 1) + "/5] Bat: " + String(v, 2) + "V, " + String(pct, 1) + "%";
                    storage.appendFile("/voltage.txt", (line + "\r\n").c_str());
                    delay(200);
                }
                if (isPowerSaving) {
                    eps.setBuckState(false);
                }
                String data = storage.readFile("/voltage.txt");
                scheduler.unlock(PTIT_BUS_SPI);
                scheduler.print(data);

                if (scheduler.lock(PTIT_BUS_LORA, 500)) {
                    com.sendMessage("Done saving. Reading from SD card:");
                    delay(100);
                    int startIdx = 0;
                    while (startIdx < (int)data.length()) {
                        int endIdx = data.indexOf('\n', startIdx);
                        if (endIdx < 0) endIdx = data.length();
                        String singleLine = data.substring(startIdx, endIdx);
                        singleLine.trim();
                        if (singleLine.length() > 0) {
                            com.sendMessage(singleLine);
                            delay(120);
                        }
                        startIdx = endIdx + 1;
                    }
                    com.sendMessage("Done reading from SD card.");
                    scheduler.unlock(PTIT_BUS_LORA);
                }
                scheduler.println("Done reading from SD card.");
            }
            break;
        }
        case 'c': {
            if (isPowerSaving) {
                if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage("ERR: Sensors disabled in power saving mode"); scheduler.unlock(PTIT_BUS_LORA); }
                break;
            }
            String msg = "Acceleration: ax=" + String(sensor.getAccX(), 2) + ", ay=" + String(sensor.getAccY(), 2) + ", az=" + String(sensor.getAccZ(), 2) + " m/s^2";
            scheduler.println(msg);
            if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(msg); scheduler.unlock(PTIT_BUS_LORA); }
            break;
        }
        case 'd': {
            if (isPowerSaving) {
                if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage("ERR: Sensors disabled in power saving mode"); scheduler.unlock(PTIT_BUS_LORA); }
                break;
            }
            String msg = "Gyroscope: gx=" + String(sensor.getGyroX(), 2) + ", gy=" + String(sensor.getGyroY(), 2) + ", gz=" + String(sensor.getGyroZ(), 2) + " rad/s";
            scheduler.println(msg);
            if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(msg); scheduler.unlock(PTIT_BUS_LORA); }
            break;
        }
        case 'e': {
            String msg = gps.hasFix() ? ("GPS: Lat=" + String(gps.getLatitude(), 6) + ", Lng=" + String(gps.getLongitude(), 6)) : "GPS: no valid fix";
            scheduler.println(msg);
            if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(msg); scheduler.unlock(PTIT_BUS_LORA); }
            break;
        }
        case 'f': {
            bool ok = capturePhotoToSD();
            String msg = ok ? "Photo captured!" : "ERR: Capture failed";
            if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage(msg); scheduler.unlock(PTIT_BUS_LORA); }
            break;
        }
        case 'p': {
            isTelemetryPaused = true;
            scheduler.println("Telemetry PAUSED");
            if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage("Telemetry PAUSED"); scheduler.unlock(PTIT_BUS_LORA); }
            break;
        }
        case 'r': {
            isTelemetryPaused = false;
            scheduler.println("Telemetry RESUMED");
            if (scheduler.lock(PTIT_BUS_LORA, 200)) { com.sendMessage("Telemetry RESUMED"); scheduler.unlock(PTIT_BUS_LORA); }
            break;
        }
        default:
            break;
    }
}

// Task 3: Nhận lệnh LoRa (Core 0)
void comTask() {
    String raw = "";
    if (scheduler.lock(PTIT_BUS_LORA)) {
        raw = com.update();
        scheduler.unlock(PTIT_BUS_LORA);
    }
    if (raw.length() > 0) {
        // Lọc bỏ ký tự rác nhị phân
        String clean = "";
        for (unsigned int i = 0; i < raw.length(); i++) {
            char c = raw.charAt(i);
            if (c >= 32 && c <= 126) clean += c;
        }
        clean.trim();
        if (clean.length() > 0) {
            processCommand(clean);
        }
    }
}

// Task 4: Viễn trắc HK theo từng chunk (Core 0)
void telemetryTask() {
    if (isTelemetryPaused) return;

    // Nếu vừa có lệnh từ mặt đất trong vòng 3 giây trước, hoãn telemetry để nhường kênh vô tuyến
    if (millis() - lastCmdTime < 3000) return;

    if (scheduler.lock(PTIT_BUS_I2C, 200)) {
        eps.update();
        scheduler.unlock(PTIT_BUS_I2C);
    }

    // Giám sát ngưỡng pin - chỉ thông báo 1 lần khi chuyển trạng thái
    float vbat = eps.getBatteryVoltage();
    float pct  = eps.getBatteryPercentage();

    if (vbat > 0.5f) {
        if (!isPowerSaving && (vbat < VOLTAGE_THRES || pct < PERCENT_THRES)) {
            isPowerSaving = true;
            eps.setBuckState(false);
            scheduler.println("Power saving mode ENABLED (Low battery). Sensors and camera disabled.");
            com.sendMessage("ALERT: Power saving mode ENABLED");
        } else if (isPowerSaving && vbat >= VOLTAGE_THRES && pct >= PERCENT_THRES) {
            isPowerSaving = false;
            eps.setBuckState(true);
            scheduler.println("Power saving mode DISABLED (Battery recovered). All subsystems active.");
            com.sendMessage("ALERT: Power saving mode DISABLED");
        } else if (isPowerSaving) {
            eps.setBuckState(false);
        }
    }

    if (!isPowerSaving) {
        // Chế độ thông thường: 6 Chunks đầy đủ + ghi thẻ nhớ SD
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
            if (isTelemetryPaused || (millis() - lastCmdTime < 3000)) break;
            if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                com.sendMessage(payloads[i]);
                scheduler.unlock(PTIT_BUS_LORA);
            }
            delay(120);
        }
    } else {
        // Chế độ Power Saving: Cảm biến tắt, chỉ truyền EPS và GPS qua LoRa
        String epsPayload = "{\"battery_v\":" + String(eps.getBatteryVoltage(), 2) + ",\"pwr_save\":true}";
        scheduler.println("[TELEMETRY] EPS: " + epsPayload);
        if (scheduler.lock(PTIT_BUS_LORA, 200)) {
            com.sendMessage(epsPayload);
            scheduler.unlock(PTIT_BUS_LORA);
        }
        delay(120);

        if (gps.hasFix()) {
            String gpsPayload = "{\"valid\":true,\"lat\":" + String(gps.getLatitude(), 4) + ",\"lng\":" + String(gps.getLongitude(), 4) + "}";
            scheduler.println("[TELEMETRY] GPS: " + gpsPayload);
            if (scheduler.lock(PTIT_BUS_LORA, 200)) {
                com.sendMessage(gpsPayload);
                scheduler.unlock(PTIT_BUS_LORA);
            }
            delay(120);
        }
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
    scheduler.addTask("Telemetry", telemetryTask, 4000, 0, 1);

    scheduler.enableWatchdog(30);
    scheduler.start();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(10000));
}
