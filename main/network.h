//=========================================================================================================
// network.h - Defines the class/task that manages the connection to the WiFi network
//
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>  THIS CODE IS INTENDED TO WORK ACROSS FIRMWARE GENERATIONS   <><>
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
//
//=========================================================================================================
#pragma once
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "lwip/err.h"
#include "lwip/sys.h"

enum ap_mode_t
{
    AP_MODE_DEFAULT,   // We're in AP mode because we don't have an SSID
    AP_MODE_BUTTON,    // We're in AP mode because someone pressed the WiFi button
    AP_MODE_BAD_PW     // We're in AP mode due to a bad WiFi password
};

enum wifi_status_t
{
    WIFI_AP_MODE,       // We're not trying to connect to WiFi, we're in AP mode
    WIFI_CONNECTING,    // Trying to connect/reconnect to WiFi
    WIFI_CONNECTED,     // Connection attempt to WiFi was successful
    WIFI_STOPPED        // All Wi-Fi access has been stopped.  No longer in STA nor AP mode
};

// This is reported by scan_wifi_networks()
struct wifi_scan_rec_t
{
    const char* ssid;
    S16         rssi;
    S16         authmode;
};


class CNetwork
{
public:
 
    // Default constructor
    CNetwork()
    {
        m_ap_mode_reason = AP_MODE_DEFAULT;
        m_wifi_status = WIFI_CONNECTING;
    }

    // Start a connection to an existing network
    void    start();

    // Attempts to connect to Wi-Fi, then returns to AP mode
    bool    test_wifi_credentials();

    // If test_wifi_credentials() returned false, this is why
    unsigned char wifi_test_fail_code();

    // Configures the WiFi to be an access point
    void    start_as_ap(ap_mode_t reason);

    // Call this to get a list of wifi networks
    void    scan_wifi_networks(void (*callback)(const wifi_scan_rec_t*));

    // Stops all network access
    void    stop();
    
    // Register activity to reset the AP-mode expiration timer
    void    register_activity();

    // Call this to retrieve the current status of the Wi-Fi connection
    wifi_status_t wifi_status() {return m_wifi_status;}

    // Call this to find out why we're in AP mode
    ap_mode_t     ap_mode_reason() {return m_ap_mode_reason;}

    // Call this to find out if we've failed to connect to Wi-Fi due to bad password
    bool    is_bad_password();

public:

    // This is the event handler called by the system WiFi stack
    void event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data);

    // This is the event handler we use when testing the WiFi credentials
    void special_event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data);

    // This is the code that runs in it's own task and should not be called externally
    void    task();

protected:

    // true if we are trying to connected immediately after bootup.
    // false once we have succesfully connected at least once
    bool    m_is_connecting_at_boot;

    // Reflects the current status of the WiFi connection
    wifi_status_t m_wifi_status;

    // This is the reason for the most recent switch to AP mode
    ap_mode_t     m_ap_mode_reason;

    // The time (in "microseconds since boot") that we saw most recent 
    // network activity
    uint64_t      m_last_activity_time;
};
