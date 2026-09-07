#include "GPS.h"

PTIT_GPS::PTIT_GPS() : gpsSerial(2), gpsSelectPin(12), camSelectPin(14), selectedDevice(Uart2Device::GPS) {
}

void PTIT_GPS::init(uint8_t gpsSelectPin, uint8_t camSelectPin) {
    this->gpsSelectPin = gpsSelectPin;
    this->camSelectPin = camSelectPin;

    pinMode(this->gpsSelectPin, OUTPUT);
    pinMode(this->camSelectPin, OUTPUT);

    digitalWrite(this->gpsSelectPin, HIGH);
    digitalWrite(this->camSelectPin, LOW);
    selectedDevice = Uart2Device::GPS;

    Serial.println("[GPS] Initializing GPS NEO (UART2)...");
    gpsSerial.begin(9600, SERIAL_8N1, 16, 17); // RX2=16, TX2=17
}

void PTIT_GPS::selectGPS() {
    digitalWrite(gpsSelectPin, HIGH);
    digitalWrite(camSelectPin, LOW);
    selectedDevice = Uart2Device::GPS;
}

void PTIT_GPS::selectCamera() {
    digitalWrite(gpsSelectPin, LOW);
    digitalWrite(camSelectPin, HIGH);
    selectedDevice = Uart2Device::CAMERA;
}

void PTIT_GPS::selectDevice(Uart2Device device) {
    if (device == Uart2Device::GPS) {
        selectGPS();
    } else {
        selectCamera();
    }
}

void PTIT_GPS::update() {
    while (gpsSerial.available() > 0) {
        gps.encode(gpsSerial.read());
    }
}

float PTIT_GPS::getLat() {
    if (gps.location.isValid()) return gps.location.lat();
    return 0.0;
}

float PTIT_GPS::getLng() {
    if (gps.location.isValid()) return gps.location.lng();
    return 0.0;
}

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
