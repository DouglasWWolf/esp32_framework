//=========================================================================================================
// network.cpp - Implements the class/task that manages the connection to the WiFi network
//=========================================================================================================
#include <string.h>
#include "common.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/apps/sntp.h"
#include "globals.h"
#include "mdns.h"


// These are the three kinds of ways we can start the WiFi
enum wifi_start_t {WIFI_START_STA, WIFI_START_AP, WIFI_START_TEST};

// Enumerates which event handler to use
enum evt_handler_type_t {NO_HANDLER, MAIN_HANDLER, TEST_HANDLER};

// This is defined in "ble_server.cpp"
void ble_server_begin();

// This will be true after the WiFi connection test completes
static int wifi_test_status = 0;

// If the Wifi-credentials test fails, this is the "disconnect reason"
static int test_fail_reason = 0;

// This is the tag used for logging
static const char* WIFI_TAG = "Network";

// This is the number of failed connection attempts in a row
static int failed_connection_attempts = 0;

// This will be true when we're supposed to fall back to Wi-Fi AP mode
static bool fallback_to_ap_mode = false;

// At the moment, esp_wifi_start has not been called
static bool is_wifi_started = false;

// Keeps track of the number of consecutive times we've encountered a bad password
static int  bad_password_count = 0;

// This specifies which event handler to use
static evt_handler_type_t evt_handler_type = NO_HANDLER;


//=========================================================================================================
// safe_wifi_start() - performs an esp_wifi_start, stopping the wifi first if it's already running
//=========================================================================================================
void safe_wifi_start()
{
    if (is_wifi_started)
    {
        is_wifi_started = false;
        esp_wifi_stop();
        msdelay(1000);
    }

    is_wifi_started = true;
    esp_err_t rc = esp_wifi_start();
    if (rc) ESP_LOGE(WIFI_TAG, "esp_wifi_start() returned %s", esp_err_to_name(rc));
}
//=========================================================================================================


//=========================================================================================================
// safe_wifi_stop() - Performs an esp_wifi_stop(), but only if wifi is currently started
//=========================================================================================================
void safe_wifi_stop()
{
    // Make sure that the TCPServer is stopped
    TCPServer.stop();

    // If we have Wi-Fi running, stop it
    if (is_wifi_started)
    {
        is_wifi_started = false;
        esp_wifi_stop();
    }

}
//=========================================================================================================



//=========================================================================================================
// get_time_via_ntp() - Fetches the time from an NTP server on the internet
//=========================================================================================================
void get_time_via_ntp()
{
    time_t now = 0;
    struct tm timeinfo;

    ESP_LOGI(WIFI_TAG, "Initializing SNTP");
    sntp_stop();
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, (char*)"pool.ntp.org");
    
    // This runs in another thread, and will set up the system time when it obtains one via NTP
    sntp_init();

    // Initialize the timeinfo structure to all zeros
    memset(&timeinfo, 0, sizeof timeinfo);

    // We're going to wait up to 20 secondss to get fetch the time via NTP
    for (int i=0; i<20; ++i)
    {
        // Wait a second while NTP does its thing in the background
        msdelay(1000);

        // Fetch the current time (which we hope NTP has filled in for us)
        time(&now);

        // Break that out to the "timeinfo" structure
        localtime_r(&now, &timeinfo);

        // If NTP just updated our internal date/time structure, we know what time it is!
        if (timeinfo.tm_year >= 100)
        {
            System.has_current_time = true;
            printf("The current year is %i\n", timeinfo.tm_year + 1900);
            break;
        }
    }
}
//=========================================================================================================


//=========================================================================================================
// setup_mdns() - Start advertising our mDNS name over the network
//=========================================================================================================
static void setup_mdns()
{  
   mdns_init();
   mdns_hostname_set(System.ssid);
}
//=========================================================================================================




//=========================================================================================================
// event_handler() - This is the event handler that will be called by esp_wifi_start()
//=========================================================================================================
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    switch (evt_handler_type)
    {
        case MAIN_HANDLER:        
            Network.event_handler(event_base, event_id, event_data);
            break;

        case TEST_HANDLER:
            Network.special_event_handler(event_base, event_id, event_data);
            break;

        case NO_HANDLER:
            break;
    }
}
//=========================================================================================================

//=========================================================================================================
// event_handler() - The main event handler for network operations
//=========================================================================================================
void CNetwork::event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data)
{

    ip_event_got_ip_t*             got_ip_event;
    wifi_event_sta_disconnected_t* disco_event;

    int disconnect_reason = 0;

    // If the system is rebooting, do absolutely nothing
    if (System.is_rebooting) return;

    // The number of times we will retry a failed WPA/Enterprise connection
    int max_retries = 10;

    // Did we get a "Start connection" event?
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        return;
    }

    // Did we get a "We've been assigned an IP address" event?
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // We now have zero consecutive failed connection attempts
        failed_connection_attempts = 0;

        // Save our IP address for posterity
        got_ip_event = (ip_event_got_ip_t*) event_data;
        strcpy(System.ip_addr, ip4addr_ntoa((const ip4_addr_t*)&got_ip_event->ip_info.ip));
        
        // Log our IP address
        ESP_LOGI(WIFI_TAG, "got ip:%s", System.ip_addr);

        // Tell the outside world that we are connected to WiFi
        m_wifi_status = WIFI_CONNECTED;

        // At this point we know that we don't have a bad Wi-Fi password
        bad_password_count = 0;

        // We are no long attempting our first connection
        m_is_connecting_at_boot = false;

        // Show the appropriate status on the status LED
        //?WifiLED.show_new_status();
        
        // Initialize mDNS and broadcast our dns-name
        setup_mdns();

        // Fetch the current time via an NTP server on the internet. 
        printf("$$$>>>NTP\n");
        get_time_via_ntp();

        // Start the servers
        TCPServer.start();

        // Output the specially formatted message that software can use to determine our IP address
        printf("$$$>>>IP:%s\n", System.ip_addr);

        // We're connected to the outside world
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {

        #define CONFAIL_BADPW  15     // Connection failed due to bad password
        #define CONFAIL_WPA2   204    // Connection failed due to bad WPA2/Enterprise creds

        // Get a pointer to the disconnection event data
        disco_event = (wifi_event_sta_disconnected_t*) event_data;

        // Fetch a code that indicates why the connection failed
        disconnect_reason = disco_event->reason;

        // Log the reason that we got disconnected (potentially during our connection attempt)   
        printf(">>> SYSTEM_EVENT_STA_DISCONNECTED: %i\n", disconnect_reason);

        // Stop the servers
        TCPServer.stop();

        // If we're still in STA mode, go ahead and try to reconnect to the access point
        if (m_wifi_status != WIFI_AP_MODE && m_wifi_status != WIFI_STOPPED)
        {

            // Assume for the moment we will NOT be falling back to AP mode
            fallback_to_ap_mode = false;

            // If we failed a WPA2/Enterprise connection attempt too many times,
            // we're going to fall back to AP mode
            if (disconnect_reason == CONFAIL_WPA2 && ++failed_connection_attempts > max_retries)
                fallback_to_ap_mode = true;

            // If we had a bad password, we're going to fall back to AP mode
            if (disconnect_reason == CONFAIL_BADPW)
            {
                ++bad_password_count;
                fallback_to_ap_mode = true;
            }

            // If we've decided to fall back to Wi-Fi AP mode, make it so...
            if (AP_MODE_ON_BAD_PW && fallback_to_ap_mode)
            {
                start_as_ap(AP_MODE_BAD_PW);
            }

            // Otherwise, re-initialize and try to reconnect to the WiFi access point
            else
            {
                m_wifi_status = WIFI_CONNECTING;

                // Show the appropriate status on the status LED
                //?WifiLED.show_new_status();

                // Stop the TCP/IP stack
                safe_wifi_stop();

                // And restart the TCP/IP stack
                safe_wifi_start();

                // Log the fact that we are retrying a connection to the access point        
                ESP_LOGI(WIFI_TAG,"retry to connect to the AP");
            }

            return;
        }


        // We're done trying to handle the WiFi disconnect event
        return;
    }
}
//=========================================================================================================



//=========================================================================================================
// special_event_handler() - This is the event handler that will be called while testing WiFi credentials
//=========================================================================================================
void CNetwork::special_event_handler(esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    ip_event_got_ip_t*             got_ip_event;
    wifi_event_sta_disconnected_t* disco_event;

    // If the system is rebooting, do absolutely nothing
    if (System.is_rebooting) return;

    // Did we get a "Start connection" event?
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
        return;
    }

    // Did we get a "We've been assigned an IP address" event?
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // Save our IP address for posterity
        got_ip_event = (ip_event_got_ip_t*) event_data;
        strcpy(System.ip_addr, ip4addr_ntoa((const ip4_addr_t*)&got_ip_event->ip_info.ip));
        
        // Log our IP address
        ESP_LOGI(WIFI_TAG, "got ip:%s", System.ip_addr);

        // The Wi-Fi credentials test has passed
        wifi_test_status = 1;

        // We're connected to the outside world
        return;
    }


    // Did we just get a disconnection event?
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        #define CONFAIL_BADPW  15     // Connection failed due to bad password
        #define CONFAIL_WPA2   204    // Connection failed due to bad WPA2/Enterprise creds

        // Get a pointer to the disconnection event data
        disco_event = (wifi_event_sta_disconnected_t*) event_data;

        // And the connection attempt failed
        if (wifi_test_status == 0) 
        {
            wifi_test_status = -1;
            test_fail_reason = disco_event->reason;
        }

        // We're done trying to handle the WiFi disconnect event
        return;
    }
}
//=========================================================================================================




//=========================================================================================================
// initialize_wifi() - Performs common initialization for starting wifi
//
//     Required order of operations:
//
//     (1) esp_event_loop_create_default()
//     (2) esp_netif_create_default_wifi_ap()
//     (3) esp_wifi_init()
//     (4) esp_wifi_set_mode()
//     (5) esp_wifi_set_config()
//     (6) esp_wifi_start()
//=========================================================================================================
void initialize_wifi(wifi_start_t how, evt_handler_type_t handler_type)
{
    static esp_netif_t* interface = nullptr;
    static bool events_registered = false;

    // Don't handle any events we are about to cause
    evt_handler_type = NO_HANDLER;

    // Tear down anything that's already set up
    if (interface)
    {
        safe_wifi_stop();
        esp_netif_destroy(interface);
    }

    // If we haven't yet registered to receive events...
    if (!events_registered)
    {
        // Register for the events we want
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,    &::event_handler, nullptr, nullptr);
        esp_event_handler_instance_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, &::event_handler, nullptr, nullptr);

        // Keep track of the fact that we've registered for the events we want to handle
        events_registered = true;
    }
   
    // Create the interface
    if (how == WIFI_START_AP)
        interface = esp_netif_create_default_wifi_ap();
    else
        interface = esp_netif_create_default_wifi_sta();

    // Set the WiFi radio to default settings
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    // Set the Wi-Fi mode
    if (how == WIFI_START_AP)
        esp_wifi_set_mode(WIFI_MODE_AP);
    else
        esp_wifi_set_mode(WIFI_MODE_STA);

    // Save which event handler we should use
    evt_handler_type = handler_type;
}
//=========================================================================================================



//=========================================================================================================
// stop() -Stops all Wi-Fi network access.  (no longer in either STA mode or AP mode)
//=========================================================================================================
void CNetwork::stop()
{
    // We've stopped the Wi-Fi
    m_wifi_status = WIFI_STOPPED;
    
    // Show the appropriate status on the status LED
    //?WifiLED.show_new_status();

    // Stop the TCP/IP stack
    safe_wifi_stop();
}
//=========================================================================================================



//=========================================================================================================
// start() - Starts a connection to the local WiFi access-point (i.e., a WiFi router)
//=========================================================================================================
void CNetwork::start()
{
    // We are now trying to connect to the WiFi access point
    m_wifi_status = WIFI_CONNECTING;

    // Show the appropriate status on the status LED
    //?WifiLED.show_new_status();

    // We are connecting just after bootup
    m_is_connecting_at_boot = true;

    // Initalize basic wifi settings
    initialize_wifi(WIFI_START_STA, MAIN_HANDLER);

    // Tell the logger what credentials we're trying to connect with
    ESP_LOGI(WIFI_TAG, "Connecting to SSID %s\n", NVS.data.network_ssid);

    //  Fill in the WiFi configuration structure with our network SSID and password
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof wifi_config);
    strcpy((char*)wifi_config.sta.ssid,  (char*)NVS.data.network_ssid);
    strcpy((char*)wifi_config.sta.password, NVS.data.network_pw);
    

    // Create convenient pointers to the username and (decrypted) password to feed to the router
    const char* user = NVS.data.network_user;
    const char* pass = (char*)wifi_config.sta.password;

    // If we have a username stored in NVS, assume we're using WPA2/Enterprise. 
    if (*user)
    {
        esp_wifi_sta_wpa2_ent_set_username((uint8_t *)user, strlen(user));
        esp_wifi_sta_wpa2_ent_set_password((uint8_t *)pass, strlen(pass));
    }


    // Begin the connection attempt.  This will end up executing in a different thread    
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    safe_wifi_start();

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
}
//=========================================================================================================



//=========================================================================================================
// wifi_test_fail_reason() - Returns the reason for the router rejecting our connection attempt
//
//      15 = Bad Password
//     201 = Bad SSID
//     204 = Bad WPA2 credentials
//=========================================================================================================
U8 CNetwork::wifi_test_fail_code()
{
    return (U8) test_fail_reason;
}
//=========================================================================================================



//=========================================================================================================
// test_wifi_credentials() - Starts a connection to the local WiFi access-point (i.e., a WiFi router)
//=========================================================================================================
bool CNetwork::test_wifi_credentials()
{
    // We're about to test a Wi-Fi connection
    wifi_test_status = 0;

    // If we don't have an SSID, pretend this is a bad SSID
    if (NVS.data.network_ssid[0] == 0)
    {
        test_fail_reason = 201;        
        return false;
    }

    // We are now trying to connect to the WiFi access point
    m_wifi_status = WIFI_CONNECTING;

    // Initalize basic wifi settings
    initialize_wifi(WIFI_START_TEST, TEST_HANDLER);

    // Tell the logger what credentials we're trying to connect with
    ESP_LOGI(WIFI_TAG, "Testing WiFi connection to SSID %s\n", NVS.data.network_ssid);

    //  Fill in the WiFi configuration structure with our network SSID and password
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof wifi_config);
    strcpy((char*)wifi_config.sta.ssid,  (char*)NVS.data.network_ssid);
    strcpy((char*) wifi_config.sta.password, NVS.data.network_pw);
    
    // Create convenient pointers to the username and (decrypted) password to feed to the router
    const char* user = NVS.data.network_user;
    const char* pass = (char*)wifi_config.sta.password;

    // If we have a username stored in NVS, assume we're using WPA2/Enterprise. 
    if (*user)
    {
        esp_wifi_sta_wpa2_ent_set_username((uint8_t *)user, strlen(user));
        esp_wifi_sta_wpa2_ent_set_password((uint8_t *)pass, strlen(pass));
    }

    // Begin the connection attempt.  This will end up executing in a different thread    
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    safe_wifi_start();

    // Wait for the test to finish
    while (wifi_test_status == 0) msdelay(500);

    // Make sure we disconnect from the router
    safe_wifi_stop();

    // Wait for the disconnect to complete
    msdelay(250);

    // Tell the caller whether or not these wifi-credentials are good
    return (wifi_test_status == 1);
}
//=========================================================================================================





//=========================================================================================================
// start_as_ap() - Configures our micro-controller to be a wireless-access-point
//
//     Required order of operations:
//
//     (1) esp_event_loop_create_default()
//     (2) esp_netif_create_default_wifi_ap()
//     (3) esp_wifi_init()
//     (4) esp_wifi_set_mode()
//     (5) esp_wifi_set_config()
//     (6) esp_wifi_start()
//=========================================================================================================
void CNetwork::start_as_ap(ap_mode_t reason)
{
    esp_err_t rc;

    // Stop all existing WiFi access
    stop();

    // We're launching as an access point, not as a station on some other access point
    m_wifi_status = WIFI_AP_MODE;

    // Keep track of why we're in AP mode
    m_ap_mode_reason = reason;

    // Show the appropriate status on the status LED
    //?WifiLED.show_new_status();

    // Initalize basic wifi settings
    initialize_wifi(WIFI_START_AP, NO_HANDLER);

    // Tell the logger what credentials we're trying to connect with
    ESP_LOGI(WIFI_TAG, "Starting WiFi Access Point\n");

    //  Fill in the WiFi configuration structure
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof wifi_config);
    strcpy((char*)wifi_config.ap.ssid, System.ssid);
    wifi_config.ap.channel         = 1;
    wifi_config.ap.authmode        = WIFI_AUTH_OPEN;
    wifi_config.ap.ssid_hidden     = 0;
    wifi_config.ap.max_connection  = 1;
    wifi_config.ap.beacon_interval = 100;

    // Begin the connection attempt.  This will end up executing in a different thread    
    rc = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (rc)
    {
        printf(">>> esp_wifi_set_config() returned : %s", esp_err_to_name(rc));
    }
    safe_wifi_start();

    // Any time we are in "Access-Point" mode, we also want to have Wi-Fi provisioning enabled
    #if 0
    ble_server_begin();
    #endif

    // And start the servers
    TCPServer.start();

    // Keep track of what time (in microseconds since boot) that we launched AP mode
    m_last_activity_time = esp_timer_get_time();

    // Output the specially formatted message that software can use to determine our SSID
    printf("$$$>>>SSID:%s\n", wifi_config.ap.ssid);
}
//=========================================================================================================


//=========================================================================================================
// register_activity() - Notify this object that activity has taken place on the TCP port
//=========================================================================================================
void CNetwork::register_activity()
{
    m_last_activity_time = esp_timer_get_time();
}
//=========================================================================================================


//=========================================================================================================
// is_bad_password() - Returns 'true' if we can't connect to the WiFi due to a bad password
//=========================================================================================================
bool CNetwork::is_bad_password() {return (bad_password_count >= 2);}
//=========================================================================================================


//=========================================================================================================
// scan_wifi_networks() - Scans for Wi-Fi access points, and reports each one found via a callback
//=========================================================================================================
void CNetwork::scan_wifi_networks(void (*callback)(const wifi_scan_rec_t*))
{
    uint16_t count;
    wifi_scan_config_t scan_conf; 
    wifi_scan_rec_t ap;
    esp_err_t ret;

    // We're going to perform a very basic scan for WiFi networks
    memset(&scan_conf, 0, sizeof scan_conf);

    // We want to see hidden networks
    scan_conf.scan_type   = WIFI_SCAN_TYPE_ACTIVE;
    scan_conf.show_hidden = true;

    // Start up the Wi-Fi in STA mode
    initialize_wifi(WIFI_START_STA, NO_HANDLER);

    // Start wi-fi event handling
    safe_wifi_start();

    // Start the scan  (this call will block)
    ret = esp_wifi_scan_start(&scan_conf, true);
    if (ret)
    {
        ESP_LOGE(WIFI_TAG, "esp_wifi_scan_start() returned %d", (int) ret);
        return;
    }

    // How many wifi networks did we find?
    ret = esp_wifi_scan_get_ap_num(&count);
    if (ret)
    {
        ESP_LOGE(WIFI_TAG, "esp_wifi_scan_get_ap() returned %d", (int) ret);
        return;
    }

    // Allocate memory to hold all of the access-point records
    wifi_ap_record_t* ap_info = new wifi_ap_record_t[count];

    // Fetch all of the access-point records into the memory we just allocated
    ret = esp_wifi_scan_get_ap_records(&count, ap_info);
    if (ret)
    {
        ESP_LOGE(WIFI_TAG, "esp_wifi_scan_get_ap() returned %d", (int) ret);
    }

    // Loop through each Wi-Fi access point we just found...
    for (int i=0; i<count; ++i)
    {
        // Get a handy reference to the SSID of this Wi-Fi access point
        char* ssid = (char*) ap_info[i].ssid;
        
        // If the SSID is blank, ignore it
        if (ssid[0] == 0) continue;
        
        // If we have a callback, use it, otherwise just display the name
        if (callback)
        {
            ap.ssid     = ssid;
            ap.rssi     = ap_info[i].rssi;
            ap.authmode = ap_info[i].authmode;
            callback(&ap);
        }
        else
            ESP_LOGI(WIFI_TAG, ">>> Wifi Scan found \"%s\"", ssid);
    }

    // Free the memory we allocated
    delete[] ap_info;
}
//=========================================================================================================
