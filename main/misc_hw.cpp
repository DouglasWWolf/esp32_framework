//=========================================================================================================
// misc_hw.cpp - Implements miscellaneous small hardware interfaces
//=========================================================================================================
#include "globals.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                                     Generic Digital Output
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~



//=========================================================================================================
// init() - Initializes the hardware for a generic digital output
//
// Passed: The pin number to control
//=========================================================================================================
void CDigitalOut::init(gpio_num_t pin)
{
    // Save the pin number for future use
    m_pin = pin;
    
    // Tell the pinmux that pin should be treated as a GPIO
    gpio_pad_select_gpio(m_pin);
  
    // Make that pin an output
    gpio_set_direction(m_pin,  GPIO_MODE_OUTPUT);
 
    // Turn it off
    gpio_set_level(m_pin,  false);
 }
//=========================================================================================================


//=========================================================================================================
// set() - Sets the on/off state of a generic digital output
//
// Passed: 'true' = "Turn the output on",   'false' = "Turn the output off"
//=========================================================================================================
void CDigitalOut::set(bool state)
{
    m_state = state;
    gpio_set_level(m_pin,  state);
}
//=========================================================================================================



//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//                                       Misc System Functions
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~


//=========================================================================================================
// rssi() - Fetches and returns the received-signal-strength-indicator
//=========================================================================================================
int CSystem::rssi()
{
    wifi_ap_record_t wifidata;
    esp_wifi_sta_get_ap_info(&wifidata);
    return wifidata.rssi;
}
//=========================================================================================================



//=========================================================================================================
// set_time() - Sets the system clock time from a string in the form of "HH:MM:SS"
//
// Returns: 'true" if the string was formatted properly, else false
//=========================================================================================================
bool CSystem::set_time(const char* token)
{
    hms_t hms;
    struct tm timeinfo;

    // Convert that token into hours, minutes, seconds
    if (!parse_utc_string(token, &hms)) return false;
 
    // Find out the current time and date
    time_t now = time(NULL);

    // Break that out into year, month, day, hour, mininute, and second fields
    localtime_r(&now, &timeinfo);

    // If there was a date in that timestamp, fill in date related fields in the timeinfo structure
    if (hms.year)
    {
        timeinfo.tm_year = hms.year - 1900;
        timeinfo.tm_mon  = hms.month - 1;
        timeinfo.tm_mday = hms.day;
    }

    // Update the time portion of our structure
    timeinfo.tm_hour = hms.hour;
    timeinfo.tm_min  = hms.min;
    timeinfo.tm_sec  = hms.sec;

    // And update our internal date/time 
    now = mktime(&timeinfo);
    struct timeval tv = { .tv_sec = now, .tv_usec = 0};
    settimeofday(&tv, NULL);

    // Hey, we know what time it is now
    System.has_current_time = true;

    // And tell the caller that all is well
    return true;
}
//=========================================================================================================



//=========================================================================================================
// fetch_time() - Fetches the current time in HH:MM:SS format into the buffer specified
//
// On Exit: output contains current time in ASCII, nul terminated
//
// Returns: the current time
//=========================================================================================================
int64_t CSystem::fetch_time(char* output)
{
    struct tm timeinfo;

    // Find out the current time and date
    time_t now = time(NULL);

    // Break that out into hour, mininute, and second fields
    localtime_r(&now, &timeinfo);

    // Report current time in UTC
    sprintf(output, "%04i-%02i-%02i %02i:%02i:%02i", 
            timeinfo.tm_year + 1900, timeinfo.tm_mon+1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

    return (int64_t) now;
}
//=========================================================================================================


//=========================================================================================================
// reboot() - Reboot the system
//=========================================================================================================
void CSystem::reboot()
{
    // Tell the world that we're rebooting
    is_rebooting = true;

    // Disconnect from the router.  We have to do this or some routers won't let us
    // reconnect right away
    esp_wifi_disconnect();

    // And restart the microcontroller
    esp_restart();
}
//=========================================================================================================



//=========================================================================================================
// create_ssid() - Creates the SSID of the system for broadcasting in AP mode
//
// On Exit: System.ssid[]  = "proto_<cboard_id>"
//=========================================================================================================
void CSystem::create_ssid()
{
    unsigned char mac[6];
    char buffer[20];

    // Clear the MAC address to all nul-bytes on the *very* remote chance the next line fails
    memset(mac, 0, sizeof mac);

    // Fetch unique MAC address of this microcontroller
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    // The unit-ID is an ASCII representation of the MAC
    sprintf(buffer, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Create the system SSID
    sprintf(ssid, "proto_%s", buffer);
}
//=========================================================================================================
