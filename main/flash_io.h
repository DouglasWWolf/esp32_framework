//=========================================================================================================
// flash_io.h - Defines a task that manages reads/write to and from flash memory 
//=========================================================================================================
#pragma once
#include "common.h"


class CFlashIO
{
public:

    // Called once at startup 
    void    begin();

    // This is the thread that waits for messages and performs IO
    void    task();

    // Call this to read an object from flash memory
    void    read(const char* nvs_key, char* buffer);

    // Call this to write an object to flash memory
    void    write(const char* nvs_key, char* buffer, size_t length);

protected:

    // Other tasks write to this queue to signal the start of a flash read/write
    QueueHandle_t   m_start_qh;
    
    // A blocking read on this queue will return when the requested flash read/write is complete
    QueueHandle_t   m_done_qh;

    // This is the handle to the mutex that ensures thread-safe reads/writes to/from flash
    SemaphoreHandle_t   m_mutex;

    // To perform a read/write we need a blob name, a buffer to read/write, and a buffer length
    const char* m_nvs_key;
    char*       m_rw_buffer;
    int         m_rw_length;
};
