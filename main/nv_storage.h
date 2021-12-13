//=========================================================================================================
// nv_storage.h - Defines an interface to non-volatile storage in flash memory
//=========================================================================================================
#pragma once
#include "common.h"


//=========================================================================================================
// CNVS - Singleton class, manages non-volatile storage
//=========================================================================================================
class CNVS
{
public:

    // Call this once at startup to gain access to NVS
    void        init();

    // Read our data structure from flash memory
    void        read_from_flash();

    // Write our data structure to flash memory
    void        write_to_flash();

    // This structure contains the actual data fields that we read/write to/from NVS
    nvsdata_t   data;

protected:

    // This initializes our "data" structure to default values
    void        init_default_data();

};
//=========================================================================================================
