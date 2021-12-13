#include "esp_netif.h"
#include "esp_event.h"
#include "globals.h"
#include "history.h"

//=========================================================================================================
// Forward references
//=========================================================================================================
void periodic_task(void*);
void do_periodic();

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

    // Initialize the provisioning button
    ProvButton.init(PIN_PROV_BUTTON);

    // Configure the I2C bus.   This must be done before initializing I2C peripherals
    I2C.init(I2C_NUM_0, PIN_I2C_SDA, PIN_I2C_SCL);

    // Find out if we should start the Wi-Fi in "Access-Point" mode
    bool start_as_ap = ProvButton.is_pressed()       ||
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
    

    // Start the main periodic task
    xTaskCreatePinnedToCore(periodic_task, "main", 6 * 1024, nullptr, DEFAULT_TASK_PRI, NULL, TASK_CPU);

    // And we don't need this task anymore
    vTaskDelete(nullptr);
}
//=========================================================================================================


//=========================================================================================================
// periodic_task() - This task sits in a loop and calls "do_period" once a second
//=========================================================================================================
void periodic_task(void* arg)
{
    // Find out what the current tick is
    TickType_t last_wake_time  = xTaskGetTickCount();

    // We're going to run our periodic routine every second
    TickType_t period_in_ticks = 1000 / portTICK_PERIOD_MS;

    // We're going to periodically take measurements forever
    while (true)
    {
        // Fetch the microsecond counter so we can time the "do_periodic" routine
        S64 start_time = esp_timer_get_time();

        // Run our periodic routine that takes measurements
        do_periodic();

        // Find out how many milliseconds it took to run the "do_periodic" routine
        S64 elapsed = (esp_timer_get_time() - start_time) / 1000;

        // Change that 1 to a 0 to display the number of milliseconds that "do_periodic" takes
        #if 0
        printf("do_periodic() = %i ms\n", (int)elapsed);
        #endif

        // This is just here so that the variable is referenced, supressing the compile-time warning
        elapsed++;

        // Suspend this task until the period timer expires
        vTaskDelayUntil(&last_wake_time, period_in_ticks);

        // Keep track of the stack depth
        StackMgr.record_hwm(TASK_IDX_MAIN);
    }
}
//=========================================================================================================



//=========================================================================================================
// do_periodic() - This routine gets called once every second
//=========================================================================================================
void do_periodic()
{
    // If the provisioning button has been down for more than 4 seconds and our network is 
    // in STA mode, re-start the network in wireless-access-point mode
    if (ProvButton.is_pressed_at_least(4000) && Network.wifi_status() != WIFI_AP_MODE)
    {
        NVRAM.start_wifi_ap = true;
        System.reboot();
    }
}
//=========================================================================================================



//=========================================================================================================
// app_main() - The entry-point from the boot-loader
//=========================================================================================================
extern "C" {void app_main() {cpp_main();}}
//=========================================================================================================
