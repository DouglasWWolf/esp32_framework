//=========================================================================================================
// i2c_bus.cpp - Implements the interfaces to an I2C multi-drop serial bus
//=========================================================================================================
#include "globals.h"


//=========================================================================================================
// init() - Call this once at bootup to initialize this I2C bus
//
// Passed: port    = I2C_NUM_0 or I2C_NUM_1
//         sda_pin = The name of the GPIO that will be the I2C data (SDA) pin.
//         scl_pin = The name of the GPIO that will be the I2C clock (SCL) pin 
//=========================================================================================================
void CI2C::init(i2c_port_t port, gpio_num_t sda_pin, gpio_num_t scl_pin)
{
    i2c_config_t conf;
    
    // Save the port number for future use
    m_port = port;

    // Initialize the I2C configuration structure to known values
    memset(&conf, 0, sizeof conf);

    // We're going to be the master of this I2C bus
    conf.mode = I2C_MODE_MASTER;

    // Keep track of which pins are going to serve as data (SDA) and clock (SDC)
    conf.sda_io_num = sda_pin;
    conf.scl_io_num = scl_pin;

    // Enable the pullup resistors for the clock and data pins
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;

    // Set the I2C bus clock to 100 kilohertz
    conf.master.clk_speed = 100000;

    // Configure this I2C serial bus
    i2c_param_config(port, &conf);

    // And install the I2C bus driver
    i2c_driver_install(port, conf.mode, 0, 0, 0);
    
    // Create the mutex that we will use to ensure thread-safe access to the I2C bus
    m_mutex = xSemaphoreCreateMutex();
}
//=========================================================================================================


//=========================================================================================================
// perform() - Performs an I2C read or write transaction
//=========================================================================================================
bool CI2C::perform(i2c_cmd_handle_t cmd)
{
    // Perform the read or write transaction
    esp_err_t status = i2c_master_cmd_begin(m_port, cmd, 0);

    // Tell the caller whether or not this read or write operation was successful
    return status == ESP_OK;
}
//=========================================================================================================


//=========================================================================================================
// read() - A convenience method that reads data from an I2C device
//=========================================================================================================
bool  CI2C::read(int i2c_address, void* vp_data, int length)
{
    // Allow 0 length reads
    if (length < 1) return true;
   
    // Turn the output-buffer void* into a U8*
    U8* p_data = (U8*) vp_data;

    // Allocate an I2C command buffers
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
 
    // Initialize the command buffer that will perform the I2C read operation
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, i2c_address << 1 | I2C_MASTER_READ, true);
    if (length > 1) i2c_master_read(cmd, p_data, length-1, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, p_data + length - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
 
    // Perform the I2C read operation
    bool status = I2C.perform(cmd);
 
    // Free the resources we allocated earlier
    i2c_cmd_link_delete(cmd);
 
    // Tell the caller whether or not this read operation was successful
    return status;
}
//=========================================================================================================


//=========================================================================================================
// write() - A conveience method that writes one or two integer values of arbitrary length to the 
//           an I2C device
//
// Passed: i2c_address = The I2C address of the device
//         val1        = The first value to be written to the device
//         len1        = Number of bytes of val1 to write
//         val2        = The second value to be written to the device
//         len2        = Number of byutes of val2 to write
//
// Returns: 'true' if the I2C write operation was successful, otherwise 'false'
//=========================================================================================================
bool  CI2C::write(int i2c_address, int val1, int len1, int val2, int len2)
{
    // Allocate an I2C command buffer for the write operation
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Initialize the write-operation buffer
    i2c_master_start(cmd);

    // Tell the I2C bus that this is going to be a write operation to the specified device
    i2c_master_write_byte(cmd, i2c_address << 1 | I2C_MASTER_WRITE, true);

    // Buffer up the first value to be written
    if (len1 >= 4) i2c_master_write_byte(cmd, val1 >> 24, true);
    if (len1 >= 3) i2c_master_write_byte(cmd, val1 >> 16, true);
    if (len1 >= 2) i2c_master_write_byte(cmd, val1 >>  8, true);
    if (len1 >= 1) i2c_master_write_byte(cmd, val1      , true);

    // Buffer up the second value to be written
    if (len2 >= 4) i2c_master_write_byte(cmd, val2 >> 24, true);
    if (len2 >= 3) i2c_master_write_byte(cmd, val2 >> 16, true);
    if (len2 >= 2) i2c_master_write_byte(cmd, val2 >>  8, true);
    if (len2 >= 1) i2c_master_write_byte(cmd, val2      , true);

    // Finalize the command buffer
    i2c_master_stop(cmd);
 
    // Perform the I2C write commands
    bool status = I2C.perform(cmd);

    // Free up the resources we allocated earlier
    i2c_cmd_link_delete(cmd);
 
    // Tell the caller whether this I2C write operation worked
    return status;
}
//=========================================================================================================


//=========================================================================================================
// lock() / unlock() - These are used to manage thread-safe exclusive access to the I2C bus.
//=========================================================================================================
void CI2C::lock()   {xSemaphoreTake(m_mutex, portMAX_DELAY);}
void CI2C::unlock() {xSemaphoreGive(m_mutex);}
//=========================================================================================================
