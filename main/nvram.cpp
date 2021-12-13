//=========================================================================================================
// nvram.cpp - Implements a structure in RAM that survives reboots
//=========================================================================================================
#include "nvram.h"
#include <string.h>
#include "common.h"

// We will look for this string in NVRAM to determine whether we have data there
#define MAGIC_KEY "**nvram**"

//=========================================================================================================
// Constructor() - Initializes our data, but only on the first boot after power-up
//=========================================================================================================
CNVRAM::CNVRAM()
{
    // If this object was initialized by a prior boot, do nothing
    if (strcmp(key, MAGIC_KEY) == 0) return;

    // Tell future boots that we've already initialized this object
    strcpy(key, MAGIC_KEY);

    // We aren't going to force Wi-Fi to start in access-point mode
    start_wifi_ap = false;
}
//=========================================================================================================


//=========================================================================================================
// Our NVRAM object is in a memory space that doesn't get re-initialized at boot.
//=========================================================================================================
RTC_NOINIT_ATTR CNVRAM NVRAM;
//=========================================================================================================
