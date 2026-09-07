# PTITCube Library

Thư viện lõi hỗ trợ lập trình cho dự án vệ tinh **PTITCube** (CubeSat). Thư viện được thiết kế theo mô hình hướng đối tượng (OOP) kết hợp với **FreeRTOS** nhằm giúp việc điều khiển đa luồng (multi-tasking) và giao tiếp với các hệ thống con (subsystems) trên vệ tinh trở nên dễ dàng, ổn định và có tính module hóa cao.

## Cấu trúc thư viện

Thư viện được chia thành các module (subsystems) chính:

- **Scheduler (FreeRTOS):** Trái tim của hệ thống. Bộ quản lý đa luồng giúp chạy độc lập các tác vụ đo lường, viễn thông và điều khiển với cơ chế khoá tài nguyên an toàn (Mutex lock).
- **CDH (Command and Data Handling):** Khối xử lý lệnh trung tâm, tự động kết nối WiFi và phát dữ liệu Telemetry chuẩn MQTT.
- **COM (Communication):** Khối giao tiếp vô tuyến LoRa E32, hỗ trợ truyền/nhận lệnh điều khiển ở khoảng cách xa với cơ chế chống nghẽn và cắt nhỏ gói tin (Chunking) thông minh.
- **EPS (Electrical Power System):** Khối quản lý năng lượng, giám sát điện áp và dung lượng pin.
- **Sensor:** Khối thu thập dữ liệu đa chiều từ các cảm biến: nhiệt độ, áp suất (BMP280), gia tốc, con quay hồi chuyển (MPU6050), la bàn từ trường (QMC5883L / GY271).
- **GPS:** Module định vị vệ tinh cung cấp tọa độ (Kinh độ, Vĩ độ) và cao độ.
- **Storage:** Module quản lý thẻ nhớ SD với các lệnh phân cấp cây thư mục tựa Linux (pwd, cd, ls, mkdir, rm, touch).

## Cơ chế Đa Luồng (FreeRTOS Scheduler)
Hệ thống sử dụng `PTIT_Scheduler` để chạy song song các luồng (Loop) riêng biệt:
- **Telemetry Loop:** Đóng gói JSON chia nhỏ và đẩy dữ liệu MQTT/LoRa.
- **COM Loop:** Lắng nghe lệnh từ Ground Station và phản hồi (Ping, Pause, Resume, Lệnh hệ điều hành Linux).
- **Sensor Loop / GPS Loop:** Cập nhật dữ liệu phần cứng theo chu kỳ ngắn.

Tất cả xung đột bus giao tiếp (như SPI cho thẻ nhớ, LORA UART, I2C cho cảm biến) đều được ngăn chặn triệt để qua `PTIT_Scheduler::lock()` và `unlock()`.

## Hướng dẫn sử dụng

Bạn chỉ cần `#include "PTITCube.h"` để truy cập mọi tính năng. Code ví dụ được cung cấp sẵn trong mục **examples**.

### Cài đặt
Cấu trúc thư mục chuẩn trên PlatformIO:
```
Project
├── lib
│   └── PTITCube
│       ├── src
│       ├── examples
│       └── README.md
├── src
│   └── main.cpp
└── platformio.ini
```

### Các lệnh điều khiển hỗ trợ từ xa (Ground Station)
Gửi các chuỗi sau qua giao thức LoRa để điều khiển vệ tinh:
- `ping`: Kiểm tra đường truyền 2 chiều.
- `pause` / `resume`: Chuyển hệ thống vào trạng thái ngủ/thức (Sleep/Standby mode) để tiết kiệm năng lượng pin.
- `status` / `uptime`: Tóm tắt sức khoẻ hệ thống.
- `get_gps` / `get_sensor` / `get_eps`: Lấy nhanh dữ liệu tức thời thay vì đợi Telemetry.
- `clear_log`: Xóa sạch nhật ký SD.
- `ls`, `cd`, `pwd`, `mkdir`, `rm`, `touch`: Quản lý không gian thẻ nhớ.

## Mã nguồn tham khảo
Mở thư mục `examples/` trong thư viện để xem các ví dụ:
- `Advanced_CubeSat.ino`: Triển khai đầy đủ tính năng đa luồng của hệ thống.
