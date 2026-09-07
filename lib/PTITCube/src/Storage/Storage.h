#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>

class PTIT_Storage {
public:
    void init();

    // ===== GHI DỮ LIỆU =====
    /** Ghi thêm một dòng vào file log mặc định (/datalog.txt) */
    void logData(const char* data);

    /** Ghi nội dung vào file (ghi đè nếu đã tồn tại) */
    bool writeFile(const char* path, const char* content);

    /** Ghi thêm nội dung vào cuối file (tạo mới nếu chưa có) */
    bool appendFile(const char* path, const char* content);

    // ===== ĐỌC DỮ LIỆU =====
    /** Đọc toàn bộ nội dung file, trả về String (rỗng nếu lỗi) */
    String readFile(const char* path);

    // ===== QUẢN LÝ FILE =====
    /** Kiểm tra file hoặc thư mục có tồn tại không */
    bool exists(const char* path);

    /** Xóa file */
    bool removeFile(const char* path);

    /** Đổi tên / di chuyển file */
    bool renameFile(const char* from, const char* to);

    /** Lấy kích thước file (bytes). Trả về -1 nếu không tồn tại */
    long fileSize(const char* path);

    // ===== QUẢN LÝ THƯ MỤC =====
    /** Tạo thư mục (bao gồm thư mục cha nếu cần) */
    bool createDir(const char* path);

    /** Xóa thư mục (phải rỗng) */
    bool removeDir(const char* path);

    /** Liệt kê file/thư mục trong một thư mục, in ra Serial */
    void listDir(const char* dirname, uint8_t levels = 0);

    // ===== THÔNG TIN THẺ NHỚ =====
    /** Dung lượng tổng (bytes) */
    uint64_t totalBytes();

    /** Dung lượng đã dùng (bytes) */
    uint64_t usedBytes();

    /** Thẻ nhớ đã sẵn sàng chưa? */
    bool isReady();

    // ===== XỬ LÝ LỆNH SHELL =====
    /**
     * Xử lý lệnh file/thư mục kiểu Linux.
     * Trả về chuỗi kết quả để gửi lại trạm mặt đất.
     *
     * Lệnh hỗ trợ:
     *   pwd                 — In đường dẫn hiện tại
     *   cd <path>           — Đổi thư mục hiện tại
     *   ls [path]           — Liệt kê thư mục
     *   mkdir <path>        — Tạo thư mục
     *   rmdir <path>        — Xóa thư mục rỗng
     *   rm <path>           — Xóa file
     *   touch <path>        — Tạo file rỗng
     *
     * @param cmd Lệnh đầy đủ (VD: "ls /logs")
     * @return Chuỗi kết quả. Rỗng nếu lệnh không được nhận diện.
     */
    String handleCommand(const String& cmd);

private:
    bool sdReady = false;
    String _currentDir = "/"; // Thư mục hiện tại (hỗ trợ lệnh cd, pwd)
    String resolvePath(const String& path); // Chuyển đổi đường dẫn tương đối thành tuyệt đối
};

#endif
