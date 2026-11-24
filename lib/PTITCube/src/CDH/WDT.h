#ifndef CDH_WDT_H
#define CDH_WDT_H

#include <Arduino.h>
#include <esp_task_wdt.h>

/**
 * @class PTIT_WDT
 * @brief Quản lý Watchdog Timer (WDT) cho hệ thống CubeSat.
 * 
 * Cung cấp cơ chế giám sát treo task và tự động khởi động lại vi điều khiển khi có lỗi.
 */
class PTIT_WDT {
public:
    static bool init(uint32_t timeoutSec = 3, bool panic = true);
    static void add();
    static void feed();
    static void reset(); // Bí danh cho feed()
    static bool wasReset();
};

// ============================================================================
// Global Helper Functions (gọi trực tiếp không cần đối tượng)
// ============================================================================
inline bool wdtInit(uint32_t timeoutSec = 3, bool panic = true) {
    return PTIT_WDT::init(timeoutSec, panic);
}

inline void wdtAdd() {
    PTIT_WDT::add();
}

inline void wdtFeed() {
    PTIT_WDT::feed();
}

inline void wdtReset() {
    PTIT_WDT::reset();
}

inline bool wasWatchdogReset() {
    return PTIT_WDT::wasReset();
}

#endif // CDH_WDT_H
