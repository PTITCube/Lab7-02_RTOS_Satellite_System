#include "COM.h"
#include "../EPS/EPS.h"
#include "../Scheduler/Scheduler.h"

#ifndef LORA_AUX_PIN
#define LORA_AUX_PIN 27
#endif
#ifndef LORA_M0_PIN
#define LORA_M0_PIN  32
#endif
#ifndef LORA_M1_PIN
#define LORA_M1_PIN  33
#endif
#ifndef LORA_RX_PIN
#define LORA_RX_PIN  26
#endif
#ifndef LORA_TX_PIN
#define LORA_TX_PIN  25
#endif

PTIT_COM::PTIT_COM() {
    _serial = &Serial1;
    _auxPin = LORA_AUX_PIN;
    _m0Pin = LORA_M0_PIN;
    _m1Pin = LORA_M1_PIN;
    _rxPin = LORA_RX_PIN;
    _txPin = LORA_TX_PIN;
}

void PTIT_COM::init() {
    PTIT_EPS::setBuckState(true);

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
        config.CHAN = 0x17; // Kênh 23 (433 MHz)
        config.OPTION_bits.trsm_mode = TRSM_TT_MODE; // Chế độ Transparent (0x00) cho truyền thông 2 chiều thông suốt
        config.OPTION_bits.tsmt_pwr = TSMT_PWR_20DB;
        SleepModeCmd(W_CFG_PWR_DWN_SAVE, &config);
    }
}

String PTIT_COM::update() {
    bool locked = PTIT_Scheduler::lock(PTIT_BUS_LORA);
    SwitchMode(MODE_0_NORMAL);
    if (_serial->available() > 0) {
        String msg = "";
        while (_serial->available()) {
            msg += (char)_serial->read();
            delay(3);
        }
        if (locked) PTIT_Scheduler::unlock(PTIT_BUS_LORA);
        return msg;
    }
    if (locked) PTIT_Scheduler::unlock(PTIT_BUS_LORA);
    return "";
}

void PTIT_COM::sendPing() {
    sendMessage("Ping");
}

void PTIT_COM::sendMessage(String msg) {
    bool locked = PTIT_Scheduler::lock(PTIT_BUS_LORA);
    if (!msg.endsWith("\n")) {
        msg += "\n";
    }
    
    int maxLen = 50; 
    int len = msg.length();
    
    // Đảm bảo module ở chế độ hoạt động bình thường và đợi rảnh
    SwitchMode(MODE_0_NORMAL);
    WaitAUX_H();
    delay(20); // Thời gian chuyển mạch RX -> TX của transceiver bán song công
    
    if (len <= maxLen) {
        _serial->print(msg);
        _serial->flush();
        WaitAUX_H();
    } else {
        for (int i = 0; i < len; i += maxLen) {
            String chunk = msg.substring(i, i + maxLen);
            WaitAUX_H();
            _serial->print(chunk);
            _serial->flush();
            WaitAUX_H();
            delay(50);
        }
    }
    if (locked) PTIT_Scheduler::unlock(PTIT_BUS_LORA);
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
// Low-level AS32 / E32 Driver Implementation
// ---------------------------------------------------------

bool PTIT_COM::ReadAUX() {
    return digitalRead(_auxPin);
}

RET_STATUS PTIT_COM::WaitAUX_H() {
    RET_STATUS STATUS = RET_SUCCESS;
    uint8_t cnt = 0;
    while ((ReadAUX() == LOW) && (cnt++ < 100)) {
        delay(5);
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
    uint8_t CMD[3] = { (uint8_t)Tcmd, (uint8_t)Tcmd, (uint8_t)Tcmd };
    _serial->write(CMD, 3);
    delay(50);
}

RET_STATUS PTIT_COM::Module_info(uint8_t* pReadbuf, uint8_t buf_len) {
    RET_STATUS STATUS = RET_SUCCESS;
    uint8_t Readcnt, idx;

    Readcnt = _serial->available();
    if (Readcnt == buf_len) {
        for (idx = 0; idx < buf_len; idx++) {
            pReadbuf[idx] = _serial->read();
        }
    } else {
        STATUS = RET_DATA_SIZE_NOT_MATCH;
        cleanUARTBuf();
    }
    return STATUS;
}

RET_STATUS PTIT_COM::Write_CFG_PDS(struct CFGstruct* pCFG) {
    cleanUARTBuf();
    _serial->write((uint8_t*)pCFG, 6);
    WaitAUX_H();
    return RET_SUCCESS;
}

RET_STATUS PTIT_COM::Read_CFG(struct CFGstruct* pCFG) {
    RET_STATUS STATUS = RET_SUCCESS;
    cleanUARTBuf();
    triple_cmd(R_CFG);
    STATUS = Module_info((uint8_t*)pCFG, sizeof(CFGstruct));
    return STATUS;
}

RET_STATUS PTIT_COM::Read_module_version(struct MVerstruct* MVer) {
    RET_STATUS STATUS = RET_SUCCESS;
    cleanUARTBuf();
    triple_cmd(R_MODULE_VERSION);
    STATUS = Module_info((uint8_t*)MVer, sizeof(MVerstruct));
    return STATUS;
}

void PTIT_COM::Reset_module() {
    triple_cmd(W_RESET_MODULE);
    WaitAUX_H();
    delay(100);
}

RET_STATUS PTIT_COM::SleepModeCmd(uint8_t CMD, void* pBuff) {
    RET_STATUS STATUS = RET_SUCCESS;
    SwitchMode(MODE_3_SLEEP);
    switch (CMD) {
        case W_CFG_PWR_DWN_SAVE:
            STATUS = Write_CFG_PDS((struct CFGstruct*)pBuff);
            break;
        case R_CFG:
            STATUS = Read_CFG((struct CFGstruct*)pBuff);
            break;
        case W_CFG_PWR_DWN_LOSE:
            break;
        case R_MODULE_VERSION:
            STATUS = Read_module_version((struct MVerstruct*)pBuff);
            break;
        case W_RESET_MODULE:
            Reset_module();
            break;
        default:
            STATUS = RET_INVALID_PARAM;
            break;
    }
    SwitchMode(MODE_0_NORMAL);
    return STATUS;
}
