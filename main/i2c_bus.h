//=========================================================================================================
// i2c_bus.h - Defines the interfaces to an I2C multi-drop serial bus
//=========================================================================================================
#pragma once
#include "common.h"


class CI2C
{
public:

    // Call this once at bootup to initialize this I2C bus
    void    init(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin);

    // These should be called before and after a set of "perform" and/or "read" operations to 
    // obtain thread-safe exclusive access to the bus.
    void    lock();
    void    unlock();

    // This is a convenience method that calls "perform" to do an I2C read for a specified number of bytes
    bool    read(int i2c_address, void* vp_data, int length);

    // This is a convenience method that calls "perform" to write one or two integer values to an I2C device
    bool    write(int i2c_address, int val1, int len1, int val2=0, int len2=0);

    // Call this to perform an arbitrary set of I2C read/write commands
    bool    perform(i2c_cmd_handle_t cmd);

protected:

    // This is the I2C port number of this I2C bus
    i2c_port_t          m_port;
        
    // This is the handle to the mutex that ensures thread-safe access to the I2C bus
    SemaphoreHandle_t   m_mutex;

};