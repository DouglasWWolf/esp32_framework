#include "esp_netif.h"
#include "esp_event.h"
#include "globals.h"
#include "history.h"

//=========================================================================================================
// This is used by the 'exeversion' utility to extract our version number from the executable file
//=========================================================================================================
static const char *EXE_TAG = "$$$>>>EXE_VERSION:" FW_VERSION;
//=========================================================================================================


//=========================================================================================================
// cpp_main() - Execution begins here, with the FreeRTOS kernel already running
//=========================================================================================================
void cpp_main()
{
    // This is a compile-time check to ensure that the size of the NVS data structure hasn't
    // been inadvertently changed
    BUILD_BUG_ON(sizeof(NVS.data) != 1024);

    // Initialze the TCP/IP stack.  This has to be done first because NVS.init() needs our MAC address    
    esp_netif_init();

    // We need a system event loop
    esp_event_loop_create_default();

    // This is a high priorty task that manages flash read/writes for us
    FlashIO.begin();

     // Initialize non-volatile storage in flash memory
    NVS.init();

    // Start the GPIO ISR service that will handle all GPIO interrupts
    gpio_install_isr_service(0);

    // Create the SSID we'll use in Wi-Fi AP mode
    System.create_ssid();

    // Tell any software watching the serial port what firmware version we are
    printf("%s\n", EXE_TAG);

    // Find out if we should start the Wi-Fi in "Access-Point" mode
    bool start_as_ap = /* ProvButton.is_pressed()  */ false   ||
                       NVS.data.network_ssid[0] == 0 ||
                       NVRAM.start_wifi_ap;

    // At next boot, we won't be forcing Wi-Fi to start in AP mode
    NVRAM.start_wifi_ap = false;

    // If the provisioning button is pressed or there is no SSID to connect to,
    // start the network in "access point" mode, otherwise start trying to
    // connect to the local WiFi network.
    if (start_as_ap)  
        Network.start_as_ap(AP_MODE_DEFAULT);
    else
        Network.start();
    
    while(true) msdelay(30000);
}
//=========================================================================================================



//=========================================================================================================
// app_main() - The entry-point from the boot-loader
//=========================================================================================================
extern "C" {void app_main() {cpp_main();}}
//=========================================================================================================
