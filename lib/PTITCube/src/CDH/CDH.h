#ifndef CDH_H
#define CDH_H

#include <Arduino.h>
#include <Stream.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <type_traits>

// Bao gồm các module cấu thành phân hệ C&DH
#include "WDT.h"
#include "Scheduler.h"
#include "Storage.h"

using PTIT_ConsoleSerial = std::remove_reference<decltype(Serial)>::type;

/**
 * @class PTIT_CDH
 * @brief Phân hệ Xử lý Dữ liệu và Lệnh điều khiển (Command and Data Handling).
 * 
 * PTIT_CDH quản lý toàn bộ luồng dữ liệu trung tâm của vệ tinh PTITCube, bao gồm:
 * - Giao tiếp Serial / Stream kết nối OBC
 * - Bộ lập lịch đa nhiệm FreeRTOS (Scheduler) & Mutex bảo vệ Bus
 * - Hệ thống giám sát Watchdog Timer (WDT)
 * - Lưu trữ thẻ nhớ MicroSD và hệ thống tệp tin (Storage)
 * - Kết nối WiFi / MQTT Telemetry
 */
class PTIT_CDH : public Stream {
private:
    PTIT_ConsoleSerial& _serial;

public:
    // Submodules trực thuộc C&DH
    PTIT_Storage   storage;
    PTIT_Scheduler scheduler;
    PTIT_WDT       wdt;

    typedef PTIT_Scheduler::TaskFunction TaskFunction;

    PTIT_CDH(PTIT_ConsoleSerial& serial = Serial);

    // Khởi tạo WiFi, MQTT và vòng lặp phân hệ CDH
    void init();
    void update();
    void publishData(const char* topic, const char* payload);

    // Các phương thức quản lý cổng Serial của OBC
    void begin(unsigned long baud = 115200);
    void begin(unsigned long baud, uint32_t config, int8_t rxPin = -1, int8_t txPin = -1, 
               bool invert = false, unsigned long timeout_ms = 20000UL, uint8_t rxfifo_full_thrhd = 112);
    void end();
    operator bool() const;
    String extractCommand(const String& cmd);

    // Stream interface
    int available() override;
    int read() override;
    int peek() override;
    void flush() override;

    // Print interface
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    using Print::write;

    // ==========================================
    // FreeRTOS Scheduler APIs (Tích hợp trực tiếp)
    // ==========================================
    void addTask(const char* name, TaskFunction fn, uint32_t intervalMs,
                 uint8_t core = 1, uint8_t priority = 2, uint16_t stackSize = 4096) {
        scheduler.addTask(name, fn, intervalMs, core, priority, stackSize);
    }

    void enableWatchdog(uint32_t timeoutSeconds = 30) {
        scheduler.enableWatchdog(timeoutSeconds);
    }

    void start() {
        scheduler.start();
    }

    static bool lock(uint8_t bus, uint32_t timeoutMs = 100) {
        return PTIT_Scheduler::lock(bus, timeoutMs);
    }

    static void unlock(uint8_t bus) {
        PTIT_Scheduler::unlock(bus);
    }

    // ==========================================
    // Watchdog Timer (WDT) APIs
    // ==========================================
    bool wdtInit(uint32_t timeoutSec = 3, bool panic = true) {
        return PTIT_WDT::init(timeoutSec, panic);
    }

    void wdtAdd() {
        PTIT_WDT::add();
    }

    void wdtFeed() {
        PTIT_WDT::feed();
    }

    void wdtReset() {
        PTIT_WDT::reset();
    }

    bool wasWatchdogReset() {
        return PTIT_WDT::wasReset();
    }

    // ==========================================
    // Storage Helper APIs
    // ==========================================
    void logData(const char* data) {
        storage.logData(data);
    }

    String handleStorageCommand(const String& cmd) {
        return storage.handleCommand(cmd);
    }

    // ==========================================
    // Low-level FreeRTOS Task & Delay Helper APIs
    // ==========================================
    bool createTask(TaskFunction_t taskFn, const char* name, uint32_t stackSize = 2048, 
                    UBaseType_t priority = 1, BaseType_t core = 1, void* param = NULL);
    void delayMs(uint32_t ms);
};

// ============================================================================
// PTIT_Queue: Template bọc FreeRTOS Queue thành API hướng đối tượng tinh gọn
// ============================================================================
template <typename T>
class PTIT_Queue {
private:
    QueueHandle_t _queue;

public:
    PTIT_Queue(size_t length = 0) : _queue(NULL) {
        if (length > 0) {
            init(length);
        }
    }

    ~PTIT_Queue() {
        if (_queue != NULL) {
            vQueueDelete(_queue);
            _queue = NULL;
        }
    }

    bool init(size_t length = 10) {
        if (_queue != NULL) return true;
        _queue = xQueueCreate(length, sizeof(T));
        return (_queue != NULL);
    }

    bool send(const T* item, uint32_t waitMs = 0) {
        if (_queue == NULL) return false;
        TickType_t ticks = (waitMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(waitMs);
        return (xQueueSend(_queue, item, ticks) == pdTRUE);
    }

    bool send(const T& item, uint32_t waitMs = 0) {
        return send(&item, waitMs);
    }

    bool sendFromISR(const T* item) {
        if (_queue == NULL) return false;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        BaseType_t res = xQueueSendFromISR(_queue, item, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
        return (res == pdTRUE);
    }

    bool sendFromISR(const T& item) {
        return sendFromISR(&item);
    }

    bool receive(T* item, uint32_t waitMs = portMAX_DELAY) {
        if (_queue == NULL) return false;
        TickType_t ticks = (waitMs == portMAX_DELAY) ? portMAX_DELAY : pdMS_TO_TICKS(waitMs);
        return (xQueueReceive(_queue, item, ticks) == pdTRUE);
    }

    bool isEmpty() const {
        if (_queue == NULL) return true;
        return (uxQueueMessagesWaiting(_queue) == 0);
    }

    UBaseType_t available() const {
        if (_queue == NULL) return 0;
        return uxQueueMessagesWaiting(_queue);
    }

    QueueHandle_t handle() const {
        return _queue;
    }
};

inline void taskDelayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

inline bool createTask(TaskFunction_t taskFn, const char* name, uint32_t stackSize = 2048, 
                       UBaseType_t priority = 1, BaseType_t core = 1, void* param = NULL) {
    return (xTaskCreatePinnedToCore(taskFn, name, stackSize, param, priority, NULL, core) == pdPASS);
}

#endif // CDH_H
