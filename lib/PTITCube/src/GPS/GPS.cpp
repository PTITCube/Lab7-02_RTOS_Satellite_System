#include "GPS.h"
#include "../EPS/EPS.h"

PTIT_GPS::PTIT_GPS() : gpsSerial(Serial2), gpsSelectPin(12), camSelectPin(14), bufferIndex(0) {
    nmeaBuffer[0] = '\0';
}

void PTIT_GPS::init(uint8_t gpsSelectPin, uint8_t camSelectPin) {
    PTIT_EPS::setBuckState(true);

    this->gpsSelectPin = gpsSelectPin;
    this->camSelectPin = camSelectPin;

    if (this->gpsSelectPin != 255 && this->camSelectPin != 255) {
        pinMode(this->gpsSelectPin, OUTPUT);
        pinMode(this->camSelectPin, OUTPUT);

        // Break-Before-Make: Tắt CAM trước, ngắt hoàn toàn tiếp điểm 74HC4066
        digitalWrite(this->camSelectPin, LOW);
        delayMicroseconds(50);
        digitalWrite(this->gpsSelectPin, HIGH);
    }

    gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
}

void PTIT_GPS::selectGPS() {
    if (gpsSelectPin != 255 && camSelectPin != 255) {
        // Break-Before-Make: TẮT CAM trước
        digitalWrite(camSelectPin, LOW);
        delayMicroseconds(50);
        // BẬT GPS sau
        digitalWrite(gpsSelectPin, HIGH);
        // Đặt lại baudrate GPS (9600)
        gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
    }
}

void PTIT_GPS::selectCamera() {
    if (gpsSelectPin != 255 && camSelectPin != 255) {
        // Break-Before-Make: TẮT GPS trước
        digitalWrite(gpsSelectPin, LOW);
        delayMicroseconds(50);
        // BẬT CAM sau
        digitalWrite(camSelectPin, HIGH);
        // Đặt lại baudrate Camera (115200)
        gpsSerial.begin(115200, SERIAL_8N1, 16, 17);
    }
}

void PTIT_GPS::update() {
    while (gpsSerial.available() > 0) {
        gps.encode(gpsSerial.read());
    }
}

bool PTIT_GPS::readSentence(char* buffer, size_t maxLen) {
    if (!buffer || maxLen == 0) return false;

    while (gpsSerial.available() > 0) {
        char c = (char)gpsSerial.read();
        gps.encode(c);

        if (c == '$') {
            bufferIndex = 0;
            nmeaBuffer[bufferIndex++] = c;
        } else if (c == '\n' || c == '\r') {
            if (bufferIndex > 0 && nmeaBuffer[0] == '$') {
                nmeaBuffer[bufferIndex] = '\0';
                strncpy(buffer, nmeaBuffer, maxLen - 1);
                buffer[maxLen - 1] = '\0';
                bufferIndex = 0;
                return true;
            }
        } else {
            if (bufferIndex < sizeof(nmeaBuffer) - 1) {
                nmeaBuffer[bufferIndex++] = c;
            }
        }
    }
    return false;
}

bool PTIT_GPS::readSentence(String& sentence) {
    char buf[128];
    if (readSentence(buf, sizeof(buf))) {
        sentence = String(buf);
        return true;
    }
    return false;
}

String PTIT_GPS::readSentence() {
    String s = "";
    readSentence(s);
    return s;
}

bool PTIT_GPS::verifyChecksum(const char* sentence) {
    if (!sentence || sentence[0] != '$') return false;

    uint8_t calculatedChecksum = 0;
    int i = 1;

    while (sentence[i] != '*' && sentence[i] != '\0') {
        calculatedChecksum ^= (uint8_t)sentence[i];
        i++;
    }

    if (sentence[i] != '*') return false;

    char hexStr[3] = { sentence[i + 1], sentence[i + 2], '\0' };
    uint8_t receivedChecksum = (uint8_t)strtol(hexStr, NULL, 16);

    return (calculatedChecksum == receivedChecksum);
}

const char* PTIT_GPS::getNMEAType(const char* sentence) {
    if (!sentence) return "UNKNOWN";
    if (strstr(sentence, "RMC")) return "RMC";
    if (strstr(sentence, "GGA")) return "GGA";
    if (strstr(sentence, "VTG")) return "VTG";
    if (strstr(sentence, "GSA")) return "GSA";
    if (strstr(sentence, "GSV")) return "GSV";
    if (strstr(sentence, "GLL")) return "GLL";
    return "OTHER";
}

bool PTIT_GPS::hasFix() {
    return gps.location.isValid();
}

float PTIT_GPS::getLatitude() {
    if (gps.location.isValid()) return gps.location.lat();
    return 0.0;
}

float PTIT_GPS::getLongitude() {
    if (gps.location.isValid()) return gps.location.lng();
    return 0.0;
}

float PTIT_GPS::getLat() { return getLatitude(); }
float PTIT_GPS::getLng() { return getLongitude(); }

float PTIT_GPS::getAltitude() {
    if (gps.altitude.isValid()) return gps.altitude.meters();
    return 0.0;
}

int PTIT_GPS::getSatellites() {
    if (gps.satellites.isValid()) return gps.satellites.value();
    return 0;
}

float PTIT_GPS::getSpeed() {
    if (gps.speed.isValid()) return gps.speed.kmph();
    return 0.0;
}

float PTIT_GPS::getCourse() {
    if (gps.course.isValid()) return gps.course.deg();
    return 0.0;
}

float PTIT_GPS::getHDOP() {
    if (gps.hdop.isValid()) return gps.hdop.hdop();
    return 0.0;
}

String PTIT_GPS::getTimeString() {
    if (gps.date.isValid() && gps.time.isValid()) {
        char sz[25];
        snprintf(sz, sizeof(sz), "%04d-%02d-%02dT%02d:%02d:%02dZ",
                 gps.date.year(), gps.date.month(), gps.date.day(),
                 gps.time.hour(), gps.time.minute(), gps.time.second());
        return String(sz);
    }
    return "N/A";
}
