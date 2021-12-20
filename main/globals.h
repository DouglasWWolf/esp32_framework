//=========================================================================================================
// globals.h - Defines globally accessable objects and variables
//=========================================================================================================
#pragma once
#include "flash_io.h"
#include "nv_storage.h"
#include "network.h"
#include "misc_hw.h"
#include "nvram.h"
#include "stack_track.h"
#include "buttons.h"
#include "i2c_bus.h"
#include "serialserver.h"

extern CSystem     System;
extern CNVS        NVS;
extern CNetwork    Network;
extern CFlashIO    FlashIO;
extern CNVRAM      NVRAM;
extern CStackTrack StackMgr;
extern CProvButton ProvButton;
extern CI2C        I2C;
extern CSerialServer TCPServer;


uint32_t crc32(void *buf, size_t len);
void     msdelay(uint32_t milliseconds);
bool     parse_utc_string(const char* input, hms_t* p_hms);

#define safe_copy(d,s) safe_strcpy((char*)(d), (char*)(s), sizeof(d))
bool safe_strcpy(char* dest, char* source, int buf_size);

