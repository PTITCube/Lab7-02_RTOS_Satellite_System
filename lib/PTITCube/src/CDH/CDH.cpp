#include "CDH.h"
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Global instances for WiFi & MQTT to simplify ISRs / Callbacks if any
static WiFiClientSecure espClient;
static PubSubClient mqttClient(espClient);

PTIT_CDH::PTIT_CDH(HardwareSerial& serial) : _serial(serial) {
}

void PTIT_CDH::begin(unsigned long baud) {
    _serial.begin(baud);
}

void PTIT_CDH::begin(unsigned long baud, uint32_t config, int8_t rxPin, int8_t txPin, 
                     bool invert, unsigned long timeout_ms, uint8_t rxfifo_full_thrhd) {
    _serial.begin(baud, config, rxPin, txPin, invert, timeout_ms, rxfifo_full_thrhd);
}

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
    // Set fallback AP if no WiFi
    if (!wm.autoConnect("CubeSat_AP", "12345678")) {
        _serial.println("[CDH] Failed to connect WiFi, running AP mode");
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
