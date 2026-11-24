#ifndef CDH_H
#define CDH_H

#include <Arduino.h>
#include <Stream.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_task_wdt.h>
#include <type_traits>

using PTIT_ConsoleSerial = std::remove_reference<decltype(Serial)>::type;

// ===== Định danh Bus cho lock/unlock tài nguyên chia sẻ =====
#define PTIT_BUS_I2C    0
#define PTIT_BUS_SPI    1
#define PTIT_BUS_SERIAL 2
#define PTIT_BUS_UART2  2
#define PTIT_BUS_LORA   3
#define PTIT_NUM_BUSES  4

#define PTIT_MAX_TASKS  8

/**
 * @class PTIT_CDH
 * @brief Phân hệ Xử lý Dữ liệu, Lệnh điều khiển & Bộ lập lịch đa nhiệm (Command, Data Handling & Scheduler).
 * 
 * PTIT_CDH kế thừa từ Stream (và Print) để đóng gói toàn bộ các phương thức
 * giao tiếp dữ liệu nối tiếp của Serial, đồng thời tích hợp trực tiếp bộ lập lịch
 * đa nhiệm FreeRTOS (Scheduler), quản lý Task, Mutex bảo vệ Bus và Watchdog Timer.
 */
class PTIT_CDH : public Stream {
public:
    /** Kiểu hàm task — hàm void không tham số cho RTOS scheduler. */
    typedef void (*TaskFunction)();

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
    // FreeRTOS Multitasking Scheduler APIs
    // ==========================================
    void addTask(const char* name, TaskFunction fn, uint32_t intervalMs,
                 uint8_t core = 1, uint8_t priority = 2, uint16_t stackSize = 4096);
    void enableWatchdog(uint32_t timeoutSeconds = 30);
    void start();

    // ==========================================
    // Shared Resource Bus Lock / Unlock (Mutex)
    // ==========================================
    static bool lock(uint8_t bus, uint32_t timeoutMs = 100);
    static void unlock(uint8_t bus);
    static void printSafe(const String& msg);
    static void printlnSafe(const String& msg = "");

    // ==========================================
    // Watchdog Timer (WDT) APIs
    // ==========================================
    bool wdtInit(uint32_t timeoutSec = 3, bool panic = true);
    void wdtAdd();
    void wdtFeed();
    void wdtReset(); // Alias for wdtFeed
    bool wasWatchdogReset();

    // ==========================================
    // FreeRTOS Task & Delay Helper APIs
    // ==========================================
    bool createTask(TaskFunction_t taskFn, const char* name, uint32_t stackSize = 2048, 
                    UBaseType_t priority = 1, BaseType_t core = 1, void* param = NULL);
    void delayMs(uint32_t ms);

private:
    PTIT_ConsoleSerial& _serial;

    struct TaskInfo {
        const char*  name;
        TaskFunction fn;
        uint32_t     intervalMs;
        uint8_t      core;
        uint8_t      priority;
        uint16_t     stackSize;
    };

    static TaskInfo _tasks[PTIT_MAX_TASKS];
    static uint8_t  _taskCount;
    static bool     _wdtEnabled;
    static uint32_t _wdtTimeout;
    static bool     _wdtActive;
    static SemaphoreHandle_t _busMutex[PTIT_NUM_BUSES];

    static void taskWrapper(void* pvParameters);
};

// ============================================================================
// PTIT_Scheduler: Lớp kế thừa để tương thích hoàn toàn mã nguồn cũ
// ============================================================================
class PTIT_Scheduler : public PTIT_CDH {
public:
    using PTIT_CDH::PTIT_CDH;

    static void print(const String& msg) {
        PTIT_CDH::printSafe(msg);
    }

    static void println(const String& msg = "") {
        PTIT_CDH::printlnSafe(msg);
    }
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

// ============================================================================
// Global Helper Functions (gọi trực tiếp không cần đối tượng)
// ============================================================================
inline bool wdtInit(uint32_t timeoutSec = 3, bool panic = true) {
    return (esp_task_wdt_init(timeoutSec, panic) == ESP_OK);
}
inline void wdtAdd() {
    esp_task_wdt_add(NULL);
}
inline void wdtFeed() {
    esp_task_wdt_reset();
}
inline void wdtReset() {
    esp_task_wdt_reset();
}
inline bool wasWatchdogReset() {
    return (esp_reset_reason() == ESP_RST_TASK_WDT);
}
inline void taskDelayMs(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
inline bool createTask(TaskFunction_t taskFn, const char* name, uint32_t stackSize = 2048, 
                       UBaseType_t priority = 1, BaseType_t core = 1, void* param = NULL) {
    return (xTaskCreatePinnedToCore(taskFn, name, stackSize, param, priority, NULL, core) == pdPASS);
}

#endif
