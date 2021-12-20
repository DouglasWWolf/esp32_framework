//=========================================================================================================
// common.h - Contains definitions that are system-wide and common to all files.
//
// Do not #include any files that are local to this project in this file.   This file is allowed
// to include only freertos header files, ESP-IDF header files,  and C/C++ standard header files
//=========================================================================================================
#pragma once

// We start our tasks in the APP core
#define TASK_CPU          1
#define DEFAULT_TASK_PRI  5
#define TASK_PRIO_TCP     6
#define TASK_PRIO_FLASH   9  // This has to be higher priority than all other tasks

#define USE_NTP 1

// This is a macro that can be used to check the size of structures at compile time
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

// Standard C/C++ includes
#include <stdint.h>
#include <string.h>

// FreeRTOS includes
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"


// ESP-IDF includes
#include <driver/gpio.h>
#include <driver/i2c.h>
#include <driver/uart.h>
#include <driver/adc.h>
#include <driver/ledc.h>
#include <soc/adc_channel.h>
#include <esp_log.h>

// Set this to true if we want to go to AP mode when a WiFi connection fails due to bad password
#define AP_MODE_ON_BAD_PW false

// Raw and encoded maximum length of WiFi network password
#define NET_PW_RAW_LEN  64
#define NET_PW_ENC_LEN (NET_PW_RAW_LEN * 2)


//=========================================================================================================
// GPIO pin definitions
//=========================================================================================================
#define PIN_PROV_BUTTON GPIO_NUM_5
#define PIN_I2C_SDA     GPIO_NUM_4
#define PIN_I2C_SCL     GPIO_NUM_15
//=========================================================================================================


//=========================================================================================================
// Convenient typedefs for common integer datatypes
//=========================================================================================================
typedef uint8_t     U8;
typedef  int8_t     S8;
typedef uint16_t    U16;
typedef  int16_t    S16;
typedef uint32_t    U32;
typedef  int32_t    S32;
typedef uint64_t    U64;
typedef  int64_t    S64;
//=========================================================================================================



//=========================================================================================================
// This is the structure that we're going to read and write to/from non-volatile storage
//
// If this changes, be sure to update CURRENT_STRUCT_VERSION and init_default_data() in nv_storage.cpp!!
//
// This structure should always be 1024 bytes long
//=========================================================================================================
struct nvsdata_t
{
    uint32_t  crc;
    uint32_t  present_flag;
    uint16_t  struct_version;
    uint8_t   network_ssid[32];
    char      network_pw[NET_PW_ENC_LEN];
    char      network_user[64];
    char      unused[788];
};
//=========================================================================================================



//=========================================================================================================
// Structure used to represent time in hours, minutes, seconds since midnight
//=========================================================================================================
struct hms_t {int year; int month; int day; int hour; int min; int sec;};
//=========================================================================================================


//=========================================================================================================
// arraycount() - Yields the number of elements in a fixed size array
//=========================================================================================================
#define array_count(x) (sizeof x / sizeof x[0])
