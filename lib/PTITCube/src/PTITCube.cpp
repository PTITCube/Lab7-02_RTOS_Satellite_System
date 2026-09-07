#include "PTITCube.h"

// File này được giữ lại vì nền tảng thư viện C++ yêu cầu ít nhất một file .cpp,
// hoặc để sau này định nghĩa các biến tổng thể nếu cần.

String PTIT_Utils::extractCommand(const String& cmd) {
    String command = cmd;
    command.trim();
    int sp1 = command.indexOf(' ');
    if (sp1 > 0) {
        command = command.substring(0, sp1);
    }
    command.toLowerCase();
    return command;
}
