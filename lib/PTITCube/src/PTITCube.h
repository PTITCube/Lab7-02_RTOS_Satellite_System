#ifndef PTITCUBE_H
#define PTITCUBE_H

#include "EPS/EPS.h"
#include "CDH/CDH.h"
#include "COM/COM.h"
#include "Sensor/Sensor.h"
#include "GPS/GPS.h"
#include "Camera/Camera.h"
#include "Storage/Storage.h"
#include "Scheduler/Scheduler.h"

// File này đóng vai trò gom nhóm các header.
// Người dùng chỉ cần #include "PTITCube.h" để sử dụng toàn bộ thư viện.
// Sử dụng PTIT_Scheduler để quản lý đa nhiệm FreeRTOS.

class PTIT_Utils {
public:
    /**
     * Tách phần tên lệnh (từ đầu tiên) từ chuỗi lệnh đầy đủ
     * VD: "ls /logs" -> "ls"
     */
    static String extractCommand(const String& cmd);
};

#endif
