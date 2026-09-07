#include "COM.h"

// Constants for pins
#define LORA_AUX_PIN 27
#define LORA_M0_PIN 32
#define LORA_M1_PIN 33
#define LORA_RX_PIN 26
#define LORA_TX_PIN 25

PTIT_COM::PTIT_COM() {
    _serial = &Serial1;
    _auxPin = LORA_AUX_PIN;
    _m0Pin = LORA_M0_PIN;
    _m1Pin = LORA_M1_PIN;
    _rxPin = LORA_RX_PIN;
    _txPin = LORA_TX_PIN;
}

void PTIT_COM::init() {
    Serial.println("[COM] Initializing LoRa Module (UART1)...");
    
    pinMode(_m0Pin, OUTPUT);
    pinMode(_m1Pin, OUTPUT);
    pinMode(_auxPin, INPUT);
    _serial->begin(9600, SERIAL_8N1, _rxPin, _txPin);
    
    SwitchMode(MODE_0_NORMAL);
    WaitAUX_H();
    
    CFGstruct config;
    if (SleepModeCmd(R_CFG, &config) == RET_SUCCESS) {
        config.ADDH = 0x00;
        config.ADDL = 0x00;
        config.CHAN = 0x17;
        config.OPTION_bits.trsm_mode = TRSM_FP_MODE;
        config.OPTION_bits.tsmt_pwr = TSMT_PWR_20DB;
        SleepModeCmd(W_CFG_PWR_DWN_SAVE, &config);
        Serial.println("[COM] LoRa Module OK");
    } else {
        Serial.println("[COM] LoRa config failed");
    }
}

String PTIT_COM::update() {
    SwitchMode(MODE_0_NORMAL);
    if (_serial->available() > 1) {
        String msg = "";
        while (_serial->available()) {
            msg += (char)_serial->read();
            delay(2);
        }
        Serial.print("[COM] Nhận được tin nhắn LoRa: ");
        Serial.println(msg);
        return msg;
    }
    return "";
}

void PTIT_COM::sendPing() {
    sendMessage("Ping");
}

void PTIT_COM::sendMessage(String msg) {
    if (!msg.endsWith("\n")) {
        msg += "\n";
    }
    
    int maxLen = 50; 
    int len = msg.length();
    
    if (len <= maxLen) {
        SwitchMode(MODE_0_NORMAL);
        if (ReadAUX() != HIGH) return;
        _serial->print(msg);
        WaitAUX_H();
        
        Serial.print("[COM] Đã gửi tin nhắn: ");
        Serial.print(msg);
    } else {
        Serial.println("[COM] Tin nhắn quá dài (" + String(len) + " bytes), đang chia nhỏ...");
        for (int i = 0; i < len; i += maxLen) {
            String chunk = msg.substring(i, i + maxLen);
            SwitchMode(MODE_0_NORMAL);
            if (ReadAUX() != HIGH) continue;
            _serial->print(chunk);
            WaitAUX_H();
            
            Serial.print("[COM] Đã gửi chunk: ");
            Serial.print(chunk);
            delay(200);
        }
    }
}

bool PTIT_COM::setChannel(uint8_t channel) {
    CFGstruct config;
    if (SleepModeCmd(R_CFG, &config) != RET_SUCCESS) return false;
    config.CHAN = channel;
    return (SleepModeCmd(W_CFG_PWR_DWN_SAVE, &config) == RET_SUCCESS);
}

bool PTIT_COM::setAddress(uint8_t addh, uint8_t addl) {
    CFGstruct config;
    if (SleepModeCmd(R_CFG, &config) != RET_SUCCESS) return false;
    config.ADDH = addh;
    config.ADDL = addl;
    return (SleepModeCmd(W_CFG_PWR_DWN_SAVE, &config) == RET_SUCCESS);
}

bool PTIT_COM::setTransmissionMode(uint8_t mode) {
    CFGstruct config;
    if (SleepModeCmd(R_CFG, &config) != RET_SUCCESS) return false;
    config.OPTION_bits.trsm_mode = mode;
    return (SleepModeCmd(W_CFG_PWR_DWN_SAVE, &config) == RET_SUCCESS);
}

// ---------------------------------------------------------
// Low-level AS32 Driver Implementation
// ---------------------------------------------------------

bool PTIT_COM::ReadAUX() {
    return digitalRead(_auxPin);
}

RET_STATUS PTIT_COM::WaitAUX_H() {
    RET_STATUS STATUS = RET_SUCCESS;
    uint8_t cnt = 0;
    while ((ReadAUX() == LOW) && (cnt++ < 100)) {
        delay(10);
    }
    if (cnt >= 100) {
        STATUS = RET_TIMEOUT;
    }
    return STATUS;
}

bool PTIT_COM::chkModeSame(MODE_TYPE mode) {
    static MODE_TYPE pre_mode = MODE_INIT;
    if (pre_mode == mode) {
        return true;
    } else {
        pre_mode = mode;
        return false;
    }
}

void PTIT_COM::SwitchMode(MODE_TYPE mode) {
    if (!chkModeSame(mode)) {
        WaitAUX_H();
        switch (mode) {
            case MODE_0_NORMAL:
                digitalWrite(_m0Pin, LOW);
                digitalWrite(_m1Pin, LOW);
                break;
            case MODE_1_WAKE_UP:
                digitalWrite(_m0Pin, HIGH);
                digitalWrite(_m1Pin, LOW);
                break;
            case MODE_2_POWER_SAVIN:
                digitalWrite(_m0Pin, LOW);
                digitalWrite(_m1Pin, HIGH);
                break;
            case MODE_3_SLEEP:
                digitalWrite(_m0Pin, HIGH);
                digitalWrite(_m1Pin, HIGH);
                break;
            default:
                return;
        }
        WaitAUX_H();
        delay(10);
    }
}

void PTIT_COM::cleanUARTBuf() {
    while (_serial->available()) {
        _serial->read();
    }
}

void PTIT_COM::triple_cmd(SLEEP_MODE_CMD_TYPE Tcmd) {
    uint8_t CMD[3] = {(uint8_t)Tcmd, (uint8_t)Tcmd, (uint8_t)Tcmd};
    _serial->write(CMD, 3);
    delay(50);
}

RET_STATUS PTIT_COM::Module_info(uint8_t* pReadbuf, uint8_t buf_len) {
    RET_STATUS STATUS = RET_SUCCESS;
    uint8_t Readcnt = _serial->available();
    if (Readcnt >= buf_len) {
        for (uint8_t idx = 0; idx < buf_len; idx++) {
            *(pReadbuf + idx) = _serial->read();
        }
    } else {
        STATUS = RET_DATA_SIZE_NOT_MATCH;
        cleanUARTBuf();
    }
    return STATUS;
}

RET_STATUS PTIT_COM::Write_CFG_PDS(struct CFGstruct* pCFG) {
    _serial->write((uint8_t *)pCFG, sizeof(CFGstruct));
    WaitAUX_H();
    delay(100);
    return RET_SUCCESS;
}

RET_STATUS PTIT_COM::Read_CFG(struct CFGstruct* pCFG) {
    cleanUARTBuf();
    triple_cmd(R_CFG);
    return Module_info((uint8_t *)pCFG, sizeof(CFGstruct));
}

RET_STATUS PTIT_COM::Read_module_version(struct MVerstruct* MVer) {
    cleanUARTBuf();
    triple_cmd(R_MODULE_VERSION);
    return Module_info((uint8_t *)MVer, sizeof(MVerstruct));
}

void PTIT_COM::Reset_module() {
    triple_cmd(W_RESET_MODULE);
    WaitAUX_H();
    delay(1000);
}

RET_STATUS PTIT_COM::SleepModeCmd(uint8_t CMD, void* pBuff) {
    RET_STATUS STATUS = RET_SUCCESS;
    WaitAUX_H();
    SwitchMode(MODE_3_SLEEP);
    switch (CMD) {
        case W_CFG_PWR_DWN_SAVE:
            STATUS = Write_CFG_PDS((struct CFGstruct*)pBuff);
            break;
        case R_CFG:
            STATUS = Read_CFG((struct CFGstruct*)pBuff);
            break;
        case R_MODULE_VERSION:
            Read_module_version((struct MVerstruct*)pBuff);
            break;
        case W_RESET_MODULE:
            Reset_module();
            break;
        default:
            return RET_INVALID_PARAM;
    }
    WaitAUX_H();
    return STATUS;
}
