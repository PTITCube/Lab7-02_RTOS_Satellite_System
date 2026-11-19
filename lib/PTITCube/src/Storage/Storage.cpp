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
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, SPI, 8000000)) {
    Serial.println("Storage: MicroSD mount failed");
    sdReady = false;
  } else {
    sdReady = true;
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
    Serial.println("Storage: Không thể mở file để ghi: " + String(path));
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
    Serial.println("Storage: Không thể mở file để ghi thêm: " + String(path));
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
    Serial.println("Storage: Không thể mở file để đọc: " + String(path));
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
    Serial.println("Storage: File không tồn tại: " + String(path));
    return false;
  }
  bool ok = SD.remove(path);
  if (ok) {
    Serial.println("Storage: Đã xóa: " + String(path));
  }
  return ok;
}

bool PTIT_Storage::renameFile(const char *from, const char *to) {
  if (!sdReady)
    return false;
  if (!SD.exists(from)) {
    Serial.println("Storage: File nguồn không tồn tại: " + String(from));
    return false;
  }
  bool ok = SD.rename(from, to);
  if (ok) {
    Serial.println("Storage: Đã đổi tên: " + String(from) + " → " +
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
    Serial.println("Storage: Đã tạo thư mục: " + String(path));
  } else {
    Serial.println("Storage: Không thể tạo thư mục: " + String(path));
  }
  return ok;
}

bool PTIT_Storage::removeDir(const char *path) {
  if (!sdReady)
    return false;
  bool ok = SD.rmdir(path);
  if (ok) {
    Serial.println("Storage: Đã xóa thư mục: " + String(path));
  } else {
    Serial.println("Storage: Không thể xóa thư mục (có thể chưa rỗng): " +
                   String(path));
  }
  return ok;
}

void PTIT_Storage::listDir(const char *dirname, uint8_t levels) {
  if (!sdReady) {
    Serial.println("Storage: Thẻ nhớ chưa sẵn sàng");
    return;
  }

  File root = SD.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("Storage: Không thể mở thư mục: " + String(dirname));
    return;
  }

  Serial.println("Storage: Danh sách: " + String(dirname));

  File file = root.openNextFile();
  int count = 0;
  while (file) {
    count++;
    for (uint8_t i = 0; i < levels; i++) {
      Serial.print("  ");
    }

    if (file.isDirectory()) {
      Serial.print("  DIR:  ");
      Serial.println(file.name());
      if (levels > 0) {
        listDir(file.path(), levels - 1);
      }
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  (");
      Serial.print(file.size());
      Serial.println(" bytes)");
    }
    file = root.openNextFile();
  }
  if (count == 0) {
    Serial.println("  (Thu muc rong)");
  }
  root.close();
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
  String p = path;
  p.trim();

  if (p == "~" || p.length() == 0) {
    return "/";
  }

  if (p == ".") {
    return _currentDir;
  }

  if (p == "..") {
    if (_currentDir == "/") return "/";
    int lastSlash = _currentDir.lastIndexOf('/');
    if (lastSlash <= 0) return "/";
    return _currentDir.substring(0, lastSlash);
  }

  if (p.startsWith("../")) {
    String parentDir = "/";
    if (_currentDir != "/") {
      int lastSlash = _currentDir.lastIndexOf('/');
      if (lastSlash > 0) parentDir = _currentDir.substring(0, lastSlash);
    }
    String sub = p.substring(3);
    return (parentDir == "/") ? ("/" + sub) : (parentDir + "/" + sub);
  }

  if (p.startsWith("./")) {
    p = p.substring(2);
  }

  if (p.startsWith("/")) {
    return p;
  }

  if (_currentDir == "/") {
    return "/" + p;
  }
  return _currentDir + "/" + p;
}

// ============================================================================
// Xử lý lệnh shell
// ============================================================================
String PTIT_Storage::handleCommand(const String &fullCmd) {
  if (!sdReady)
    return "ERR: SD not ready";

  String cmd = fullCmd;
  cmd.trim();
  if (cmd.length() == 0) return "";

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
    if (arg1.length() == 0 || arg1 == "/" || arg1 == "~") {
      _currentDir = "/";
      return "OK: cd /";
    }

    String newPath = resolvePath(arg1);
    if (newPath == "/") {
      _currentDir = "/";
      return "OK: cd /";
    }

    File f = SD.open(newPath.c_str());
    if (f && f.isDirectory()) {
      f.close();
      _currentDir = newPath;
      return "OK: cd " + _currentDir;
    }
    if (f) f.close();
    return "ERR: Directory not found: " + newPath;
  }

  // ls [path]
  if (command == "ls") {
    String path = (arg1.length() > 0) ? resolvePath(arg1) : _currentDir;
    if (path != "/") {
      File f = SD.open(path.c_str());
      if (!f) {
        return "ERR: Path not found: " + path;
      }
      if (!f.isDirectory()) {
        Serial.printf("  FILE: %s (%ld bytes)\n", path.c_str(), f.size());
        f.close();
        return "OK: ls " + path;
      }
      f.close();
    }
    listDir(path.c_str(), 0);
    return "OK: ls " + path;
  }

  // cat <path>
  if (command == "cat") {
    if (arg1.length() == 0)
      return "ERR: cat <path>";
    String path = resolvePath(arg1);
    File f = SD.open(path.c_str());
    if (!f) {
      return "ERR: File not found: " + path;
    }
    if (f.isDirectory()) {
      f.close();
      return "ERR: " + path + " is a directory";
    }
    f.close();
    String content = readFile(path.c_str());
    return content;
  }

  // touch <path>
  if (command == "touch") {
    if (arg1.length() == 0)
      return "ERR: touch <path>";
    String path = resolvePath(arg1);
    if (!exists(path.c_str())) {
      if (!writeFile(path.c_str(), "")) {
        return "ERR: Cannot create file: " + path;
      }
    }
    return "OK: touch " + path;
  }

  // echo <text> [> | >> <file>]
  if (command == "echo") {
    int appendPos = arg1.indexOf(">>");
    int writePos  = arg1.indexOf('>');
    if (appendPos > 0) {
      String text = arg1.substring(0, appendPos);
      text.trim();
      if ((text.startsWith("\"") && text.endsWith("\"")) || 
          (text.startsWith("'") && text.endsWith("'"))) {
        text = text.substring(1, text.length() - 1);
      }
      String target = arg1.substring(appendPos + 2);
      target.trim();
      String path = resolvePath(target);
      return appendFile(path.c_str(), (text + "\n").c_str()) ? "OK: append " + path : "ERR: append " + path;
    } else if (writePos > 0) {
      String text = arg1.substring(0, writePos);
      text.trim();
      if ((text.startsWith("\"") && text.endsWith("\"")) || 
          (text.startsWith("'") && text.endsWith("'"))) {
        text = text.substring(1, text.length() - 1);
      }
      String target = arg1.substring(writePos + 1);
      target.trim();
      String path = resolvePath(target);
      return writeFile(path.c_str(), (text + "\n").c_str()) ? "OK: write " + path : "ERR: write " + path;
    }
    return arg1;
  }

  // mkdir <path>
  if (command == "mkdir") {
    if (arg1.length() == 0)
      return "ERR: mkdir <path>";
    String path = resolvePath(arg1);
    File f = SD.open(path.c_str());
    if (f) {
      bool isDir = f.isDirectory();
      f.close();
      if (isDir) return "ERR: Directory already exists: " + path;
      return "ERR: File with same name exists: " + path;
    }
    return createDir(path.c_str()) ? "OK: mkdir " + path : "ERR: mkdir " + path;
  }

  // rmdir <path>
  if (command == "rmdir") {
    if (arg1.length() == 0)
      return "ERR: rmdir <path>";
    String path = resolvePath(arg1);
    if (path == "/")
      return "ERR: Cannot remove root directory";
    return removeDir(path.c_str()) ? "OK: rmdir " + path : "ERR: rmdir " + path;
  }

  // rm <path>
  if (command == "rm") {
    if (arg1.length() == 0)
      return "ERR: rm <path>";
    String path = resolvePath(arg1);
    File f = SD.open(path.c_str());
    if (f && f.isDirectory()) {
      f.close();
      return "ERR: " + path + " is a directory (use rmdir)";
    }
    if (f) f.close();
    return removeFile(path.c_str()) ? "OK: rm " + path : "ERR: rm " + path;
  }

  // df
  if (command == "df") {
    uint64_t totalMB = totalBytes() / (1024 * 1024);
    uint64_t usedMB  = usedBytes() / (1024 * 1024);
    uint64_t freeMB  = (totalMB > usedMB) ? (totalMB - usedMB) : 0;
    char buf[100];
    snprintf(buf, sizeof(buf), "SD Card: Total: %llu MB | Used: %llu MB | Free: %llu MB",
             totalMB, usedMB, freeMB);
    return String(buf);
  }

  // help
  if (command == "help" || command == "?") {
    return "Commands: pwd, cd <dir>, ls [dir], cat <file>, touch <file>, echo <text> [> file], mkdir <dir>, rmdir <dir>, rm <file>, df, help";
  }

  // Lệnh không nhận diện → trả về chuỗi rỗng
  return "";
}
