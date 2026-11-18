/**
 * PTITCube Core Library v1.1.0
 * Định nghĩa cấu hình phần cứng và bao gồm các phân hệ vệ tinh PTITCube
 */

#ifndef PTITCUBE_H
#define PTITCUBE_H

// =========================================================
// SƠ ĐỒ CHÂN PHẦN CỨNG CHUẨN PTITCUBE V1.1.0
// =========================================================

// --- Phân hệ Truyền thông COM (LoRa E32 / E220 - UART1) ---
#define LORA_RX_PIN   26  // ESP32 RX1 nối TXD của LoRa
#define LORA_TX_PIN   25  // ESP32 TX1 nối RXD của LoRa
#define LORA_AUX_PIN  27  // Chân AUX báo trạng thái bận/rảnh
#define LORA_M0_PIN   32  // Chân điều khiển Mode M0
#define LORA_M1_PIN   33  // Chân điều khiển Mode M1

// Bí danh ngắn gọn tương thích mã nguồn cũ
#define LORA_RX       LORA_RX_PIN
#define LORA_TX       LORA_TX_PIN
#define LORA_AUX      LORA_AUX_PIN
#define LORA_M0       LORA_M0_PIN
#define LORA_M1       LORA_M1_PIN

// --- Phân hệ Nguồn EPS ---
#define EPS_SDA_PIN   21
#define EPS_SCL_PIN   22
#define EPS_PWR_GATE  4   // Điều khiển mạch hạ áp (Power Gating)

// --- Phân hệ Lưu trữ Storage & CDH (MicroSD - VSPI) ---
#define SD_CS_PIN     13
#define SD_SCK_PIN    18
#define SD_MISO_PIN   19
#define SD_MOSI_PIN   23

// --- Phân hệ Cảm biến & Định vị & Camera (UART2 & MUX 74HC4066) ---
#define UART2_RX_PIN  16
#define UART2_TX_PIN  17
#define MUX_GPS_PIN   12
#define MUX_CAM_PIN   14

// Bao gồm các header phân hệ
#include "EPS/EPS.h"
#include "CDH/CDH.h"
#include "COM/COM.h"
#include "Sensor/Sensor.h"
#include "GPS/GPS.h"
#include "Camera/Camera.h"
#include "Storage/Storage.h"
#include "Scheduler/Scheduler.h"

class PTIT_Utils {
public:
    static String extractCommand(const String& cmd);
};

#endif
