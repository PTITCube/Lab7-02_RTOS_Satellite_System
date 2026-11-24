#include "CDH.h"
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

// Global instances for WiFi & MQTT to simplify ISRs / Callbacks if any
static WiFiClientSecure espClient;
static PubSubClient mqttClient(espClient);

// ============================================================================
// STATIC MEMBERS FOR SCHEDULER & MUTEX
// ============================================================================
PTIT_CDH::TaskInfo PTIT_CDH::_tasks[PTIT_MAX_TASKS];
uint8_t PTIT_CDH::_taskCount = 0;
bool PTIT_CDH::_wdtEnabled = false;
uint32_t PTIT_CDH::_wdtTimeout = 30;
bool PTIT_CDH::_wdtActive = false;
SemaphoreHandle_t PTIT_CDH::_busMutex[PTIT_NUM_BUSES] = { NULL };

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
// FreeRTOS Multitasking Scheduler Implementation
// ==========================================
void PTIT_CDH::addTask(const char* name, TaskFunction fn, uint32_t intervalMs,
                       uint8_t core, uint8_t priority, uint16_t stackSize) {
    if (_taskCount >= PTIT_MAX_TASKS) {
        _serial.println("[CDH] Scheduler ERROR: Reached task limit (" + String(PTIT_MAX_TASKS) + ")!");
        return;
    }

    TaskInfo& t  = _tasks[_taskCount];
    t.name       = name;
    t.fn         = fn;
    t.intervalMs = intervalMs;
    t.core       = core;
    t.priority   = priority;
    t.stackSize  = stackSize;
    _taskCount++;

    _serial.println("[CDH] Registered task: " + String(name) +
                   " (" + String(intervalMs) + "ms, Core " + String(core) +
                   ", Priority " + String(priority) + ")");
}

void PTIT_CDH::enableWatchdog(uint32_t timeoutSeconds) {
    _wdtEnabled = true;
    _wdtTimeout = timeoutSeconds;
}

void PTIT_CDH::taskWrapper(void* pvParameters) {
    TaskInfo* info = (TaskInfo*)pvParameters;

    if (_wdtActive) {
        esp_task_wdt_add(NULL);
    }

    for (;;) {
        if (_wdtActive) {
            esp_task_wdt_reset();
        }

        info->fn();

        vTaskDelay(pdMS_TO_TICKS(info->intervalMs));
    }
}

void PTIT_CDH::start() {
    for (int i = 0; i < PTIT_NUM_BUSES; i++) {
        if (_busMutex[i] == NULL) {
            _busMutex[i] = xSemaphoreCreateMutex();
        }
    }

    if (_wdtEnabled) {
        esp_task_wdt_init(_wdtTimeout, true);
        _wdtActive = true;
    }

    for (uint8_t i = 0; i < _taskCount; i++) {
        TaskHandle_t handle = NULL;
        xTaskCreatePinnedToCore(
            taskWrapper,
            _tasks[i].name,
            _tasks[i].stackSize,
            &_tasks[i],
            _tasks[i].priority,
            &handle,
            _tasks[i].core
        );
    }
}

bool PTIT_CDH::lock(uint8_t bus, uint32_t timeoutMs) {
    if (bus >= PTIT_NUM_BUSES) return false;
    if (_busMutex[bus] == NULL) {
        _busMutex[bus] = xSemaphoreCreateMutex();
        if (_busMutex[bus] == NULL) return false;
    }
    return (xSemaphoreTake(_busMutex[bus], pdMS_TO_TICKS(timeoutMs)) == pdTRUE);
}

void PTIT_CDH::unlock(uint8_t bus) {
    if (bus >= PTIT_NUM_BUSES || _busMutex[bus] == NULL) return;
    xSemaphoreGive(_busMutex[bus]);
}

void PTIT_CDH::printSafe(const String& msg) {
    if (lock(PTIT_BUS_SERIAL, 50)) {
        Serial.print(msg);
        unlock(PTIT_BUS_SERIAL);
    } else {
        Serial.print(msg);
    }
}

void PTIT_CDH::printlnSafe(const String& msg) {
    if (lock(PTIT_BUS_SERIAL, 50)) {
        Serial.println(msg);
        unlock(PTIT_BUS_SERIAL);
    } else {
        Serial.println(msg);
    }
}

// ==========================================
// Watchdog Timer (WDT) Implementation
// ==========================================
bool PTIT_CDH::wdtInit(uint32_t timeoutSec, bool panic) {
    return (esp_task_wdt_init(timeoutSec, panic) == ESP_OK);
}

void PTIT_CDH::wdtAdd() {
    esp_task_wdt_add(NULL);
}

void PTIT_CDH::wdtFeed() {
    esp_task_wdt_reset();
}

void PTIT_CDH::wdtReset() {
    esp_task_wdt_reset();
}

bool PTIT_CDH::wasWatchdogReset() {
    return (esp_reset_reason() == ESP_RST_TASK_WDT);
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
