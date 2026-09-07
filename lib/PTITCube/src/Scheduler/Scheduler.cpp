#include "Scheduler.h"
#include <esp_task_wdt.h>

// ============================================================================
// STATIC MEMBERS
// ============================================================================
bool PTIT_Scheduler::_wdtActive = false;

// Mutex cho từng bus (khởi tạo trong start())
static SemaphoreHandle_t _busMutex[PTIT_NUM_BUSES] = { NULL };

// ============================================================================
// API: Đăng ký task
// ============================================================================
void PTIT_Scheduler::addTask(const char* name, TaskFunction fn, uint32_t intervalMs,
                              uint8_t core, uint8_t priority, uint16_t stackSize) {
    if (_taskCount >= PTIT_MAX_TASKS) {
        Serial.println("[Scheduler] LỖI: Đã đạt giới hạn " + String(PTIT_MAX_TASKS) + " task!");
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

    Serial.println("[Scheduler] Đã đăng ký task: " + String(name) +
                   " (chu kỳ " + String(intervalMs) + "ms, Core " + String(core) +
                   ", Priority " + String(priority) + ")");
}

// ============================================================================
// API: Bật Watchdog
// ============================================================================
void PTIT_Scheduler::enableWatchdog(uint32_t timeoutSeconds) {
    _wdtEnabled = true;
    _wdtTimeout = timeoutSeconds;
}

// ============================================================================
// TASK WRAPPER: Bọc hàm người dùng với watchdog + vTaskDelay
// ============================================================================
void PTIT_Scheduler::taskWrapper(void* pvParameters) {
    TaskInfo* info = (TaskInfo*)pvParameters;

    // Đăng ký task vào watchdog nếu được bật
    if (_wdtActive) {
        esp_task_wdt_add(NULL);
    }

    for (;;) {
        // Feed watchdog
        if (_wdtActive) {
            esp_task_wdt_reset();
        }

        // Gọi hàm nghiệp vụ của người dùng
        info->fn();

        // Nghỉ theo chu kỳ đã đăng ký
        vTaskDelay(pdMS_TO_TICKS(info->intervalMs));
    }
}

// ============================================================================
// API: Tạo và chạy tất cả task
// ============================================================================
void PTIT_Scheduler::start() {
    Serial.println("========================================");
    Serial.println(" PTIT_Scheduler — FreeRTOS Task Manager");
    Serial.println("========================================");

    // Tạo mutex cho các bus
    for (int i = 0; i < PTIT_NUM_BUSES; i++) {
        _busMutex[i] = xSemaphoreCreateMutex();
    }

    // Bật watchdog nếu được yêu cầu
    if (_wdtEnabled) {
        esp_task_wdt_init(_wdtTimeout, true); // true = reset ESP32 khi timeout
        _wdtActive = true;
        Serial.println("[WDT] Watchdog Timer đã bật (timeout: " + String(_wdtTimeout) + "s)");
    }

    // Tạo từng task đã đăng ký
    Serial.println("[Scheduler] Đang tạo " + String(_taskCount) + " task...");

    for (uint8_t i = 0; i < _taskCount; i++) {
        TaskHandle_t handle = NULL;
        BaseType_t result = xTaskCreatePinnedToCore(
            taskWrapper,             // Hàm wrapper
            _tasks[i].name,          // Tên task
            _tasks[i].stackSize,     // Stack size
            &_tasks[i],              // pvParameters = con trỏ tới TaskInfo
            _tasks[i].priority,      // Priority
            &handle,                 // Task handle
            _tasks[i].core           // Core
        );

        if (result == pdPASS) {
            Serial.println("[Scheduler] ✓ " + String(_tasks[i].name) +
                           " → Core " + String(_tasks[i].core));
        } else {
            Serial.println("[Scheduler] ✗ THẤT BẠI: " + String(_tasks[i].name));
        }
    }

    Serial.println("[Scheduler] Hoàn tất! " + String(_taskCount) + " task đang chạy.");
    Serial.println("========================================");
}

// ============================================================================
// TIỆN ÍCH: Thread-safe Serial print
// ============================================================================
void PTIT_Scheduler::print(const String& msg) {
    if (_busMutex[PTIT_BUS_SERIAL] != NULL &&
        xSemaphoreTake(_busMutex[PTIT_BUS_SERIAL], pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.print(msg);
        xSemaphoreGive(_busMutex[PTIT_BUS_SERIAL]);
    } else {
        Serial.print(msg);
    }
}

void PTIT_Scheduler::println(const String& msg) {
    if (_busMutex[PTIT_BUS_SERIAL] != NULL &&
        xSemaphoreTake(_busMutex[PTIT_BUS_SERIAL], pdMS_TO_TICKS(50)) == pdTRUE) {
        Serial.println(msg);
        xSemaphoreGive(_busMutex[PTIT_BUS_SERIAL]);
    } else {
        Serial.println(msg);
    }
}

// ============================================================================
// TIỆN ÍCH: Lock/Unlock bus tài nguyên chia sẻ
// ============================================================================
bool PTIT_Scheduler::lock(uint8_t bus, uint32_t timeoutMs) {
    if (bus >= PTIT_NUM_BUSES || _busMutex[bus] == NULL) return false;
    return xSemaphoreTake(_busMutex[bus], pdMS_TO_TICKS(timeoutMs)) == pdTRUE;
}

void PTIT_Scheduler::unlock(uint8_t bus) {
    if (bus >= PTIT_NUM_BUSES || _busMutex[bus] == NULL) return;
    xSemaphoreGive(_busMutex[bus]);
}
