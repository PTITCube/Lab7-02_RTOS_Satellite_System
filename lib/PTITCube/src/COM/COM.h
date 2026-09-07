#ifndef COM_H
#define COM_H

#include <Arduino.h>

// Enums and definitions for AS32 / E32 LoRa modules
typedef enum {
  RET_SUCCESS = 0,
  RET_ERROR_UNKNOWN,
  RET_NOT_SUPPORT,
  RET_NOT_IMPLEMENT,
  RET_NOT_INITIAL,
  RET_INVALID_PARAM,
  RET_DATA_SIZE_NOT_MATCH,
  RET_BUF_TOO_SMALL,
  RET_TIMEOUT,
  RET_HW_ERROR,
} RET_STATUS;

enum MODE_TYPE {
  MODE_0_NORMAL = 0,
  MODE_1_WAKE_UP,
  MODE_2_POWER_SAVIN,
  MODE_3_SLEEP,
  MODE_INIT = 0xFF
};

enum SLEEP_MODE_CMD_TYPE {
  W_CFG_PWR_DWN_SAVE = 0xC0,
  R_CFG              = 0xC1,
  W_CFG_PWR_DWN_LOSE = 0xC2,
  R_MODULE_VERSION   = 0xC3,
  W_RESET_MODULE     = 0xC4
};

#define TRSM_TT_MODE		0x00
#define TRSM_FP_MODE		0x01
#define OD_DRIVE_MODE		0x00
#define PP_DRIVE_MODE		0x01

#define DISABLE_FEC			0x00
#define ENABLE_FEC			0x01

enum TSMT_PWR_TYPE {
  TSMT_PWR_20DB = 0x00,
  TSMT_PWR_17DB = 0x01,
  TSMT_PWR_14DB = 0x02,
  TSMT_PWR_10DB = 0x03
};

#pragma pack(push, 1)
struct SPEDstruct {
  uint8_t air_bps : 3;
  uint8_t uart_bps: 3;
  uint8_t uart_fmt: 2;
};

struct OPTIONstruct {
  uint8_t tsmt_pwr    : 2;
  uint8_t enFWC       : 1;
  uint8_t wakeup_time : 3;
  uint8_t drive_mode  : 1;
  uint8_t trsm_mode   : 1;
};

struct CFGstruct {
  uint8_t HEAD;
  uint8_t ADDH;
  uint8_t ADDL;
  struct SPEDstruct   SPED_bits;
  uint8_t CHAN;
  struct OPTIONstruct OPTION_bits;
};

struct MVerstruct {
  uint8_t HEAD;
  uint8_t Model;
  uint8_t Version;
  uint8_t features;
};
#pragma pack(pop)

class PTIT_COM {
public:
    PTIT_COM();

    void init();
    String update();
    void sendPing();
    void sendMessage(String msg);
    
    // API thay đổi tham số vật lý
    bool setChannel(uint8_t channel);
    bool setAddress(uint8_t addh, uint8_t addl);
    bool setTransmissionMode(uint8_t mode);

private:
    HardwareSerial* _serial;
    int _auxPin, _m0Pin, _m1Pin;
    int _rxPin, _txPin;
    
    // Các hàm giao tiếp cấp thấp với module
    bool ReadAUX();
    RET_STATUS WaitAUX_H();
    bool chkModeSame(MODE_TYPE mode);
    void SwitchMode(MODE_TYPE mode);
    void cleanUARTBuf();
    void triple_cmd(SLEEP_MODE_CMD_TYPE Tcmd);
    RET_STATUS Module_info(uint8_t* pReadbuf, uint8_t buf_len);
    RET_STATUS Write_CFG_PDS(struct CFGstruct* pCFG);
    RET_STATUS Read_CFG(struct CFGstruct* pCFG);
    RET_STATUS Read_module_version(struct MVerstruct* MVer);
    void Reset_module();
    RET_STATUS SleepModeCmd(uint8_t CMD, void* pBuff);
};

#endif
