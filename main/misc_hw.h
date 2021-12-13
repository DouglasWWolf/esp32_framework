//=========================================================================================================
// misc_hw.h - Defines the miscellaneous small hardware interfaces
//=========================================================================================================
#pragma once
#include "common.h"


//=========================================================================================================
// This class manages a single ESP32 digital output
//=========================================================================================================
class CDigitalOut
{
public:

    // Call this once at bootup to configured the pin-mux
    void    init(gpio_num_t pin);

    // Call this to turn the pin on or off
    void    set(bool state);

    // Fetches the current state of the digital output
    bool    state() {return m_state;}

protected:

    // The reflect the on/off state of the digital output (true = on)
    bool       m_state;

    gpio_num_t m_pin;
};
//=========================================================================================================




//=========================================================================================================
// This is just a collection of system-related information that doesn't fit anywhere else
//=========================================================================================================
class CSystem
{
public:

    CSystem()
    {
        has_current_time         = false;
        is_rebooting             = false;
    }

    // Create the SSID we'll broadcast in AP mode
    void    create_ssid();

    // Fetches the RSSI of the router we're connected to
    int     rssi();

    // Reboots the system
    void    reboot();

    // Sets the system time
    bool    set_time(const char* token);

    // Fetches the current system time
    int64_t fetch_time(char* output);

    // True if we know what time it is
    bool    has_current_time;
        
    // True if the system is about to do a reboot
    bool    is_rebooting;

    // The SSID that we broadcast in Wi-Fi AP mode
    char    ssid[32];

    // This is our IP address as an ASCII string
    char    ip_addr[16];

};
//=========================================================================================================
