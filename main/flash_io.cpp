//=========================================================================================================
// flash_io.cpp - Implements a task that manages reads/write to and from flash memory 
//=========================================================================================================
#include <nvs_flash.h>
#include "globals.h"

#define FLASH_READ  0
#define FLASH_WRITE 1

// This is the namespace that NVS stores our data structure under
static const char* NAMESPACE = "storage";

//=========================================================================================================
// launch_task() - Just calles the task() method of our FlashIO object
//=========================================================================================================
static void launch_task(void *pvParameters) {FlashIO.task();}
//=========================================================================================================


//=========================================================================================================
// write_flash() - Writes a blob of data to a named region of flash memory
//=========================================================================================================
static void write_flash(const char* nvs_key, char* buffer, size_t length)
{
    nvs_handle  handle;

    // Open a handle to non-volatile storage
    nvs_open(NAMESPACE, NVS_READWRITE, &handle);

    // Write this chunk of memory to flash
    nvs_set_blob(handle, nvs_key, buffer, length);

    // Commit those flash changes (i.e., make them permanent)
    nvs_commit(handle);

    // We're done with NVS storage for the moment
    nvs_close(handle);
}
//=========================================================================================================




//=========================================================================================================
// read_flash() - Reads a blob of data from named region of flash memory
//=========================================================================================================
static void read_flash(const char* nvs_key, char* buffer)
{
    nvs_handle handle;
    size_t     blob_size = 0;
    esp_err_t  status;

    // As a convenience to callers that are expecting ASCII data to be read, fill in the start 
    // of their buffer with nul-bytes, just in case the requested nvs_key doesn't exist yet
    *(U32*)buffer = 0;

    // Tell the engineer what we're up to
    printf("Reading flash memory key \"%s\"\n", nvs_key);

    // Open a handle to non-volatile storage
    status = nvs_open(NAMESPACE, NVS_READWRITE, &handle);
    if (status != ESP_OK) printf("*** nvs_open() failed!! 0x%X)\n", status);

    // Call this the first time to find out how big this data structure in flash is
    nvs_get_blob(handle, nvs_key, buffer, &blob_size);
    
    // If there is data in flash available to read, go read it
    if (blob_size > 0)
    {
        status = nvs_get_blob(handle, nvs_key, buffer, &blob_size);
        if (status != ESP_OK) printf("*** nvs_get_blob() failed!! (0x%X)\n", status);
    }

    // We're done with NVS storage for the moment
    nvs_close(handle);
}
//=========================================================================================================



//=========================================================================================================
// begin() - Creates the message queues and starts up the task thread
//=========================================================================================================
void CFlashIO::begin()
{
    // Other threads will write to this queue to start a read/write operation
    m_start_qh = xQueueCreate(4, 1);

    // Other threads will do a blocking read from this queue to know when that operation is done
    m_done_qh = xQueueCreate(4, 1);

    // This mutex will ensure one-thread-at-a-time access to reading/writing flash memory
    m_mutex = xSemaphoreCreateMutex();

    // And finally, launch the task that will perform flash memory read/writes for us
    xTaskCreatePinnedToCore(::launch_task, "flashio", 2048, nullptr, TASK_PRIO_FLASH, NULL, TASK_CPU);
}
//=========================================================================================================



//=========================================================================================================
// task() - Runs in an infinite loop waiting for flash read/write operations to be requested
//
// This task runs at the highest task priority, ensuring that other tasks are completely suspended while
// flash reads/writes happen.  This is to ensure that no thread tries to read from SPRAM while a flash
// write is taking place.  We do this to avoid the documented dangers of the SPRAM disappearing from the
// memory map (during flash writes) because SPRAM and flash memory share cache.
//=========================================================================================================
void CFlashIO::task()
{
    char    cmd;

    // We're going to sit in a loop forever listening for commands
    while (true)
    {
        // Wait for a command to arrive
        xQueueReceive(m_start_qh, &cmd, portMAX_DELAY);

        // Perform the requested operation
        if (cmd == FLASH_WRITE) write_flash(m_nvs_key, m_rw_buffer, m_rw_length);
        if (cmd == FLASH_READ ) read_flash(m_nvs_key, m_rw_buffer);

        // Tell the requesting task that the operation is complete
        xQueueSend(m_done_qh, &cmd, portMAX_DELAY);
    }
}
//=========================================================================================================


//=========================================================================================================
// read() - Reads from flash memory with a very high priorty task that blocks other tasks
//=========================================================================================================
void CFlashIO::read(const char* nvs_key, char* buffer)
{
    char cmd = FLASH_READ;

    // Only one thread a time is allowed to read/write flash memory
    xSemaphoreTake(m_mutex, portMAX_DELAY);

    // Fill in the paramaters required to read an object from flash
    m_nvs_key   = nvs_key;
    m_rw_buffer = buffer;

    // Tell the read/write task to commence the operation
    xQueueSend(m_start_qh, &cmd, portMAX_DELAY);

    // Wait for the operation to complete
    xQueueReceive(m_done_qh, &cmd, portMAX_DELAY);

    // Allow other threads to read/write flash memory
    xSemaphoreGive(m_mutex);
}
//=========================================================================================================




//=========================================================================================================
// write() - Writes to flash memory with a very high priorty task that blocks other tasks
//=========================================================================================================
void CFlashIO::write(const char* nvs_key, char* buffer, size_t length)
{
    char cmd = FLASH_WRITE;

    // Only one thread a time is allowed to read/write flash memory
    xSemaphoreTake(m_mutex, portMAX_DELAY);

    // Fill in the paramaters required to write an object to flash
    m_nvs_key   = nvs_key;
    m_rw_buffer = buffer;
    m_rw_length = length;

    // Tell the read/write task to commence the operation
    xQueueSend(m_start_qh, &cmd, portMAX_DELAY);

    // Wait for the operation to complete
    xQueueReceive(m_done_qh, &cmd, portMAX_DELAY);

    // Allow other threads to read/write flash memory
    xSemaphoreGive(m_mutex);
}
//=========================================================================================================


