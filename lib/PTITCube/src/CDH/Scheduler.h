#ifndef CDH_SCHEDULER_H
#define CDH_SCHEDULER_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "WDT.h"

// ===== Định danh Bus cho lock/unlock tài nguyên chia sẻ =====
#define PTIT_BUS_I2C    0
#define PTIT_BUS_SPI    1
#define PTIT_BUS_SERIAL 2
#define PTIT_BUS_UART2  2
#define PTIT_BUS_LORA   3
#define PTIT_NUM_BUSES  4

#define PTIT_MAX_TASKS  8

/**
 * @class PTIT_Scheduler
 * @brief Bộ lập lịch FreeRTOS cho CubeSat thuộc phân hệ C&DH.
 *
 * Giấu toàn bộ logic RTOS (tạo task, mutex, watchdog) đằng sau API đơn giản.
 * Người dùng viết logic nghiệp vụ trong các hàm void thông thường,
 * rồi đăng ký chúng bằng addTask().
 */
class PTIT_Scheduler {
public:
    /** Kiểu hàm task — hàm void không tham số. */
    typedef void (*TaskFunction)();

    /**
     * Đăng ký một task mới.
     * @param name       Tên task (hiển thị trên debug log)
     * @param fn         Hàm sẽ được gọi lặp lại theo chu kỳ
     * @param intervalMs Chu kỳ gọi (ms). VD: 200 = gọi mỗi 200ms
     * @param core       Core chạy task (0 hoặc 1). Mặc định: 1
     * @param priority   Ưu tiên (1-5, cao hơn = ưu tiên hơn). Mặc định: 2
     * @param stackSize  Kích thước stack (bytes). Mặc định: 4096
     */
    void addTask(const char* name, TaskFunction fn, uint32_t intervalMs,
                 uint8_t core = 1, uint8_t priority = 2, uint16_t stackSize = 4096);

    /**
     * Bật Watchdog Timer. Nếu bất kỳ task nào bị treo quá timeout → ESP32 tự reset.
     * @param timeoutSeconds Thời gian timeout (giây). Mặc định: 30
     */
    void enableWatchdog(uint32_t timeoutSeconds = 30);

    /**
     * Tạo và chạy tất cả task đã đăng ký. Gọi sau khi addTask() xong.
     */
    void start();

    // ===== TIỆN ÍCH THREAD-SAFE (static, gọi được từ mọi task) =====

    /** In ra Serial có bảo vệ mutex (không bị xen lẫn giữa các task). */
    static void print(const String& msg);
    static void println(const String& msg = "");

    /**
     * Khóa bus tài nguyên chia sẻ. Dùng khi nhiều task cùng truy cập một bus.
     * @param bus       Loại bus (PTIT_BUS_I2C, PTIT_BUS_SPI, PTIT_BUS_LORA...)
     * @param timeoutMs Thời gian chờ khóa tối đa (ms). Mặc định: 100
     * @return true nếu khóa thành công
     */
    static bool lock(uint8_t bus, uint32_t timeoutMs = 100);

    /** Mở khóa bus sau khi sử dụng xong. */
    static void unlock(uint8_t bus);

private:
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

#endif // CDH_SCHEDULER_H
