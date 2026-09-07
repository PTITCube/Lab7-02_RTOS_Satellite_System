#include "CDH.h"
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Global instances for WiFi & MQTT to simplify ISRs / Callbacks if any
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

void PTIT_CDH::init() {
    Serial.println("[CDH] Initializing WiFi & MQTT...");

    WiFiManager wm;
    // Set fallback AP if no WiFi
    if (!wm.autoConnect("CubeSat_AP", "12345678")) {
        Serial.println("[CDH] Failed to connect WiFi, running AP mode");
    } else {
        Serial.println("[CDH] WiFi Connected!");
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
            Serial.println("[CDH] Connecting to MQTT...");
            if (mqttClient.connect("CubeSat_Client", MQTT_USER, MQTT_PASS)) {
                Serial.println("[CDH] MQTT Connected!");
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
