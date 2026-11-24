#include "Storage.h"
#include "../EPS/EPS.h"
#include "../CDH/CDH.h"
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
  PTIT_EPS::setBuckState(true);
  SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);

  if (!SD.begin(SD_CS_PIN, SPI, 8000000)) {
    Serial.println("Storage: SD mount failed");
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
  bool locked = PTIT_CDH::lock(PTIT_BUS_SPI);
  File file = SD.open("/datalog.txt", FILE_APPEND);
  if (file) {
    file.println(data);
    file.close();
  }
  if (locked) PTIT_CDH::unlock(PTIT_BUS_SPI);
}

bool PTIT_Storage::writeFile(const char *path, const char *content) {
  if (!sdReady)
    return false;
  File file = SD.open(path, FILE_WRITE);
  if (!file) {
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
    return false;
  }
  return SD.remove(path);
}

bool PTIT_Storage::renameFile(const char *from, const char *to) {
  if (!sdReady)
    return false;
  if (!SD.exists(from)) {
    return false;
  }
  return SD.rename(from, to);
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
  return SD.mkdir(path);
}

bool PTIT_Storage::removeDir(const char *path) {
  if (!sdReady)
    return false;
  return SD.rmdir(path);
}

void PTIT_Storage::listDir(const char *dirname, uint8_t levels) {
  if (!sdReady) {
    Serial.println("Storage: SD not ready");
    return;
  }

  File root = SD.open(dirname);
  if (!root || !root.isDirectory()) {
    Serial.println("Storage: Open dir failed: " + String(dirname));
    return;
  }

  Serial.println("Storage: List: " + String(dirname));

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
    Serial.println("  (Empty)");
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

  String result;
  if (p.startsWith("../")) {
    String parentDir = "/";
    if (_currentDir != "/") {
      int lastSlash = _currentDir.lastIndexOf('/');
      if (lastSlash > 0) parentDir = _currentDir.substring(0, lastSlash);
    }
    String sub = p.substring(3);
    result = (parentDir == "/") ? ("/" + sub) : (parentDir + "/" + sub);
  } else if (p.startsWith("./")) {
    String sub = p.substring(2);
    result = (_currentDir == "/") ? ("/" + sub) : (_currentDir + "/" + sub);
  } else if (p.startsWith("/")) {
    result = p;
  } else {
    result = (_currentDir == "/") ? ("/" + p) : (_currentDir + "/" + p);
  }

  // Chuẩn hóa: loại bỏ dấu / ở cuối nếu có (trừ root "/")
  while (result.length() > 1 && result.endsWith("/")) {
    result = result.substring(0, result.length() - 1);
  }

  return result;
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
      return "";
    }

    String newPath = resolvePath(arg1);
    if (newPath == "/") {
      _currentDir = "/";
      return "";
    }

    if (!SD.exists(newPath.c_str())) {
      return "ERR: Directory not found: " + newPath;
    }

    File f = SD.open(newPath.c_str());
    if (f && f.isDirectory()) {
      f.close();
      _currentDir = newPath;
      return "";
    }
    if (f) f.close();
    return "ERR: Not a directory: " + newPath;
  }

  // ls [path]
  if (command == "ls") {
    String path = (arg1.length() > 0) ? resolvePath(arg1) : _currentDir;
    if (path != "/") {
      if (!SD.exists(path.c_str())) {
        return "ERR: Path not found: " + path;
      }
      File f = SD.open(path.c_str());
      if (f) {
        if (!f.isDirectory()) {
          Serial.printf("  FILE: %s (%ld bytes)\n", path.c_str(), f.size());
          f.close();
          return "";
        }
        f.close();
      }
    }
    listDir(path.c_str(), 0);
    return "";
  }

  // cat <path>
  if (command == "cat") {
    if (arg1.length() == 0)
      return "ERR: cat <path>";
    String path = resolvePath(arg1);
    if (!SD.exists(path.c_str())) {
      return "ERR: File not found: " + path;
    }
    File f = SD.open(path.c_str());
    if (f) {
      if (f.isDirectory()) {
        f.close();
        return "ERR: " + path + " is a directory";
      }
      f.close();
    }
    return readFile(path.c_str());
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
    return "";
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
      return appendFile(path.c_str(), (text + "\n").c_str()) ? "" : "ERR: append " + path;
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
      return writeFile(path.c_str(), (text + "\n").c_str()) ? "" : "ERR: write " + path;
    }
    return arg1;
  }

  // mkdir <path>
  if (command == "mkdir") {
    if (arg1.length() == 0)
      return "ERR: mkdir <path>";
    String path = resolvePath(arg1);
    if (SD.exists(path.c_str())) {
      return "ERR: Directory already exists: " + path;
    }
    return createDir(path.c_str()) ? "" : "ERR: Cannot create directory: " + path;
  }

  // rmdir <path>
  if (command == "rmdir") {
    if (arg1.length() == 0)
      return "ERR: rmdir <path>";
    String path = resolvePath(arg1);
    if (path == "/")
      return "ERR: Cannot remove root directory";
    if (!SD.exists(path.c_str()))
      return "ERR: Directory not found: " + path;
    return removeDir(path.c_str()) ? "" : "ERR: Cannot remove directory (may not be empty): " + path;
  }

  // rm <path>
  if (command == "rm") {
    if (arg1.length() == 0)
      return "ERR: rm <path>";
    String path = resolvePath(arg1);
    if (!SD.exists(path.c_str()))
      return "ERR: File not found: " + path;
    File f = SD.open(path.c_str());
    if (f) {
      if (f.isDirectory()) {
        f.close();
        return "ERR: " + path + " is a directory (use rmdir)";
      }
      f.close();
    }
    return removeFile(path.c_str()) ? "" : "ERR: Cannot remove file: " + path;
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

  // Lệnh không nhận diện
  return "Command not recognized. Type 'help' for commands.";
}
