//=========================================================================================================
// nv_storage.cpp - Implements an interface to non-volatile storage in flash memory
//=========================================================================================================
#include <nvs_flash.h>
#include "esp_log.h"
#include "nv_storage.h"
#include "common.h"
#include "globals.h"

// This is the key-name (within the namespace) that the IDF NVS system stores our data structure under
static const char* KEY_NAME = "data";

// If this value is in the "data_present" field, we know our structure contains data
const U32 DATA_PRESENT_MARKER = 0xDEEDBAAF;

//=========================================================================================================
// This should be incremented any time a field gets added to the nvsdata_t structure
//=========================================================================================================
const int CURRENT_STRUCT_VERSION = 1;
//--------------------------------------------------------------------------------------------------------
// Ver  FW_REV  Description
//--------------------------------------------------------------------------------------------------------
//   1   1000   Initial creation
//--------------------------------------------------------------------------------------------------------
//=========================================================================================================


//=========================================================================================================
// init() - Called once at startup to gain access too nvs in flash
//=========================================================================================================
void CNVS::init()
{
    // Initialize non-volatile storage in flash memory
    int status = nvs_flash_init();
    
    // If the relevant region in flash memory hasn't been initialized, do so
    if (status == ESP_ERR_NVS_NO_FREE_PAGES || status == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        status = nvs_flash_init();
    }

    // If an error occured during the operations above, log it
    ESP_ERROR_CHECK(status);

    // Read the structure that holds our NV data into RAM
    read_from_flash();
}
//=========================================================================================================


//=========================================================================================================
// read_from_flash() - Reads the structure that holds our NV data into RAM
//=========================================================================================================
void CNVS::read_from_flash()
{
    // Just for safety, clear out the existing data structure
    memset(&data, 0, sizeof data);

    // Read in our NVS data structure from flash 
    FlashIO.read(KEY_NAME, (char*)&data);

    // Initialize any uninitialized fields in our data structure
    init_default_data();
}
//=========================================================================================================



//=========================================================================================================
// init_default_data() - Initializes fields to appropriate default values
//=========================================================================================================
void CNVS::init_default_data()
{
    // If we have no data present at all, clear it to zeros, and write the marker into the structure
    if (data.present_flag != DATA_PRESENT_MARKER)
    {
        memset(&data, 0, sizeof data);
        data.present_flag = DATA_PRESENT_MARKER;
    }


    //------------------------------------------------------------------------------------------------
    // TODO: As the data structure grows due to new fields being added, there should be a series of
    // initializers here that look like:
    //
    //    if (data.struct_version < SOME_CONSTANT)
    //    {
    //        ... initialize fields that were first added at structure version SOME_CONSTANT
    //    }
    //
    //-----------------------------------------------------------------------------------------------

    // Indicate that the data structure is of the most recent format
    data.struct_version = CURRENT_STRUCT_VERSION;
}
//=========================================================================================================



//=========================================================================================================
// write_to_flash() - Writes the RAM structure that holds our NV data into flash memory
//=========================================================================================================
void CNVS::write_to_flash()
{
    // Compute a new CRC for the data
    data.crc = 0;
    data.crc = crc32(&data, sizeof data);

    // And write our NVS structure to flash memory
    FlashIO.write(KEY_NAME, (char*)&data, sizeof data);
}
//=========================================================================================================


