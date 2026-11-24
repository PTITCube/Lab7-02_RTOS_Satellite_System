#include "CDH.h"
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Global instances for WiFi & MQTT to simplify ISRs / Callbacks if any
static WiFiClientSecure espClient;
static PubSubClient mqttClient(espClient);

PTIT_CDH::PTIT_CDH(PTIT_ConsoleSerial& serial) : _serial(serial) {
}

void PTIT_CDH::begin(unsigned long baud) {
    _serial.begin(baud);
}

#if ARDUINO_USB_CDC_ON_BOOT
void PTIT_CDH::begin(unsigned long baud, uint32_t, int8_t, int8_t, bool, unsigned long, uint8_t) {
    _serial.begin(baud); // USB CDC: no UART frame or pin configuration.
}
#else
void PTIT_CDH::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, 
                     bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrhd) {
    _serial.begin(baud, config, rxPin, txPin, invert, timeout_ms, rxfifo_full_thrhd);
}
#endif

void PTIT_CDH::end() {
    _serial.end();
}

PTIT_CDH::operator bool() const {
    return (bool)_serial;
}

int PTIT_CDH::available() {
    return _serial.available();
}

int PTIT_CDH::read() {
    return _serial.read();
}

int PTIT_CDH::peek() {
    return _serial.peek();
}

void PTIT_CDH::flush() {
    _serial.flush();
}

size_t PTIT_CDH::write(uint8_t c) {
    return _serial.write(c);
}

size_t PTIT_CDH::write(const uint8_t *buffer, size_t size) {
    return _serial.write(buffer, size);
}

void PTIT_CDH::init() {
    if (!_serial) {
        _serial.begin(115200);
    }
    _serial.println("[CDH] Initializing WiFi & MQTT...");

    WiFiManager wm;
    wm.setConnectTimeout(10);
    wm.setConfigPortalTimeout(30);
    // Set fallback AP if no WiFi
    if (!wm.autoConnect("CubeSat_AP", "12345678")) {
        _serial.println("[CDH] WiFi setup timed out; continuing offline");
    } else {
        _serial.println("[CDH] WiFi Connected!");
    }

    #ifdef MQTT_HOST
    espClient.setInsecure(); // Bỏ qua xác thực chứng chỉ TLS (phù hợp cho giai đoạn prototype)
    mqttClient.setBufferSize(1024); // Tăng buffer cho JSON telemetry lớn
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    #endif
}

void PTIT_CDH::update() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected()) {
            #ifdef MQTT_HOST
            _serial.println("[CDH] Connecting to MQTT...");
            if (mqttClient.connect("CubeSat_Client", MQTT_USER, MQTT_PASS)) {
                _serial.println("[CDH] MQTT Connected!");
            }
            #endif
        }
        mqttClient.loop();
    }
}

void PTIT_CDH::publishData(const char* topic, const char* payload) {
    if (mqttClient.connected()) {
        mqttClient.publish(topic, payload);
    }
}

String PTIT_CDH::extractCommand(const String& cmd) {
    String command = cmd;
    command.trim();
    int sp1 = command.indexOf(' ');
    if (sp1 > 0) {
        command = command.substring(0, sp1);
    }
    command.toLowerCase();
    return command;
}

// ==========================================
// FreeRTOS Task & Delay Implementation
// ==========================================
bool PTIT_CDH::createTask(TaskFunction_t taskFn, const char* name, uint32_t stackSize, 
                          UBaseType_t priority, BaseType_t core, void* param) {
    return (xTaskCreatePinnedToCore(taskFn, name, stackSize, param, priority, NULL, core) == pdPASS);
}

void PTIT_CDH::delayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
