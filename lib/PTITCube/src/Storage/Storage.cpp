#include "Storage.h"
#include <SD.h>
#include <SPI.h>

#define SD_CS_PIN 13
#define SD_SCK_PIN 18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23

// ============================================================================
// Khởi tạo
// ============================================================================
void PTIT_Storage::init() {
  Serial.println("[Storage] Initializing MicroSD (SPI)...");
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, SPI, 8000000)) {
    Serial.println("[Storage] MicroSD mount failed");
    sdReady = false;
  } else {
    Serial.println("[Storage] MicroSD mount OK");
    sdReady = true;

    // In thông tin thẻ nhớ
    Serial.print("[Storage] Tổng: ");
    Serial.print(SD.totalBytes() / (1024 * 1024));
    Serial.print(" MB, Đã dùng: ");
    Serial.print(SD.usedBytes() / (1024 * 1024));
    Serial.println(" MB");
  }
}

bool PTIT_Storage::isReady() { return sdReady; }

// ============================================================================
// Ghi dữ liệu
// ============================================================================
void PTIT_Storage::logData(const char *data) {
  if (!sdReady)
    return;
  File file = SD.open("/datalog.txt", FILE_APPEND);
  if (file) {
    file.println(data);
    file.close();
  }
}

bool PTIT_Storage::writeFile(const char *path, const char *content) {
  if (!sdReady)
    return false;
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
    Serial.println("[Storage] Không thể mở file để ghi: " + String(path));
    return false;
  }
  bool ok = file.print(content);
  file.close();
  return ok;
}

bool PTIT_Storage::appendFile(const char *path, const char *content) {
  if (!sdReady)
    return false;
  File file = SD.open(path, FILE_APPEND);
  if (!file) {
    Serial.println("[Storage] Không thể mở file để ghi thêm: " + String(path));
    return false;
  }
  bool ok = file.print(content);
  file.close();
  return ok;
}

// ============================================================================
// Đọc dữ liệu
// ============================================================================
String PTIT_Storage::readFile(const char *path) {
  if (!sdReady)
    return "";
  File file = SD.open(path);
  if (!file) {
    Serial.println("[Storage] Không thể mở file để đọc: " + String(path));
    return "";
  }
  String content = "";
  while (file.available()) {
    content += (char)file.read();
  }
  file.close();
  return content;
}

// ============================================================================
// Quản lý file
// ============================================================================
bool PTIT_Storage::exists(const char *path) {
  if (!sdReady)
    return false;
  return SD.exists(path);
}

bool PTIT_Storage::removeFile(const char *path) {
  if (!sdReady)
    return false;
  if (!SD.exists(path)) {
    Serial.println("[Storage] File không tồn tại: " + String(path));
    return false;
  }
  bool ok = SD.remove(path);
  if (ok) {
    Serial.println("[Storage] Đã xóa: " + String(path));
  }
  return ok;
}

bool PTIT_Storage::renameFile(const char *from, const char *to) {
  if (!sdReady)
    return false;
  if (!SD.exists(from)) {
    Serial.println("[Storage] File nguồn không tồn tại: " + String(from));
    return false;
  }
  bool ok = SD.rename(from, to);
  if (ok) {
    Serial.println("[Storage] Đã đổi tên: " + String(from) + " → " +
                   String(to));
  }
  return ok;
}

long PTIT_Storage::fileSize(const char *path) {
  if (!sdReady)
    return -1;
  File file = SD.open(path);
  if (!file)
    return -1;
  long size = file.size();
  file.close();
  return size;
}

// ============================================================================
// Quản lý thư mục
// ============================================================================
bool PTIT_Storage::createDir(const char *path) {
  if (!sdReady)
    return false;
  if (SD.exists(path)) {
    return true; // Đã tồn tại
  }
  bool ok = SD.mkdir(path);
  if (ok) {
    Serial.println("[Storage] Đã tạo thư mục: " + String(path));
  } else {
    Serial.println("[Storage] Không thể tạo thư mục: " + String(path));
  }
  return ok;
}

bool PTIT_Storage::removeDir(const char *path) {
  if (!sdReady)
    return false;
  bool ok = SD.rmdir(path);
  if (ok) {
    Serial.println("[Storage] Đã xóa thư mục: " + String(path));
  } else {
    Serial.println("[Storage] Không thể xóa thư mục (có thể chưa rỗng): " +
                   String(path));
  }
  return ok;
}

void PTIT_Storage::listDir(const char *dirname, uint8_t levels) {
  if (!sdReady) {
    Serial.println("[Storage] Thẻ nhớ chưa sẵn sàng");
    return;
  }

  File root = SD.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("[Storage] Không thể mở thư mục: " + String(dirname));
    return;
  }

  Serial.println("[Storage] Nội dung thư mục: " + String(dirname));

  File file = root.openNextFile();
  while (file) {
    // Thụt lề theo cấp
    for (uint8_t i = 0; i < levels; i++) {
      Serial.print("  ");
    }

    if (file.isDirectory()) {
      Serial.print("  📁 ");
      Serial.println(file.name());
      if (levels > 0) {
        listDir(file.path(), levels - 1);
      }
    } else {
      Serial.print("  📄 ");
      Serial.print(file.name());
      Serial.print("  (");
      Serial.print(file.size());
      Serial.println(" bytes)");
    }
    file = root.openNextFile();
  }
}

// ============================================================================
// Thông tin thẻ nhớ
// ============================================================================
uint64_t PTIT_Storage::totalBytes() {
  if (!sdReady)
    return 0;
  return SD.totalBytes();
}

uint64_t PTIT_Storage::usedBytes() {
  if (!sdReady)
    return 0;
  return SD.usedBytes();
}

// ============================================================================
// Hỗ trợ đường dẫn
// ============================================================================
String PTIT_Storage::resolvePath(const String &path) {
  if (path.startsWith("/")) {
    return path;
  }
  if (path == ".") {
    return _currentDir;
  }
  if (_currentDir.endsWith("/")) {
    return _currentDir + path;
  }
  return _currentDir + "/" + path;
}

// ============================================================================
// Xử lý lệnh shell
// ============================================================================
String PTIT_Storage::handleCommand(const String &fullCmd) {
  if (!sdReady)
    return "ERR: SD not ready";

  String cmd = fullCmd;
  cmd.trim();

  // Tách lệnh và tham số
  String command = cmd;
  String arg1 = "";

  int sp1 = cmd.indexOf(' ');
  if (sp1 > 0) {
    command = cmd.substring(0, sp1);
    arg1 = cmd.substring(sp1 + 1);
    arg1.trim();
  }
  command.toLowerCase();

  // pwd
  if (command == "pwd") {
    return _currentDir;
  }

  // cd <path>
  if (command == "cd") {
    if (arg1.length() == 0) {
      _currentDir = "/";
      return "OK: cd /";
    }

    String newPath = resolvePath(arg1);

    // Basic dot-dot resolution (only 1 level supported for simplicity in this
    // context, or just direct assignment)
    if (arg1 == "..") {
      if (_currentDir != "/") {
        int lastSlash = _currentDir.lastIndexOf('/');
        if (lastSlash <= 0) {
          _currentDir = "/";
        } else {
          _currentDir = _currentDir.substring(0, lastSlash);
        }
      }
      return "OK: cd " + _currentDir;
    }

    if (exists(newPath.c_str())) {
      _currentDir = newPath;
      return "OK: cd " + _currentDir;
    } else {
      return "ERR: Directory not found";
    }
  }

  // ls [path]
  if (command == "ls") {
    String path = (arg1.length() > 0) ? resolvePath(arg1) : _currentDir;
    listDir(path.c_str(), 1);
    return "OK: ls " + path;
  }

  // mkdir <path>
  if (command == "mkdir") {
    if (arg1.length() == 0)
      return "ERR: mkdir <path>";
    String path = resolvePath(arg1);
    return createDir(path.c_str()) ? "OK: mkdir " + path : "ERR: mkdir " + path;
  }

  // rmdir <path>
  if (command == "rmdir") {
    if (arg1.length() == 0)
      return "ERR: rmdir <path>";
    String path = resolvePath(arg1);
    return removeDir(path.c_str()) ? "OK: rmdir " + path : "ERR: rmdir " + path;
  }

  // rm <path>
  if (command == "rm") {
    if (arg1.length() == 0)
      return "ERR: rm <path>";
    String path = resolvePath(arg1);
    return removeFile(path.c_str()) ? "OK: rm " + path : "ERR: rm " + path;
  }

  // touch <path>
  if (command == "touch") {
    if (arg1.length() == 0)
      return "ERR: touch <path>";
    String path = resolvePath(arg1);
    if (!exists(path.c_str())) {
      writeFile(path.c_str(), "");
    }
    return "OK: touch " + path;
  }

  // Lệnh không nhận diện → trả về chuỗi rỗng
  return "";
}
