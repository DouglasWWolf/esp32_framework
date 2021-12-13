//=========================================================================================================
// tcp_handlers.cpp - Implements a TCP socket server task
//=========================================================================================================
#include "globals.h"
#include "history.h"
#include "parser.h"

// Parses an input string into tokens
static CParser parser;

//========================================================================================================= 
// fail_xxx() - Report to the user that their command failed and the reason why
//========================================================================================================= 
static void fail_syntax() {TCPServer.send("FAIL syntax\r\n");}
static void fail_unsupp() {TCPServer.send("FAIL unsupp\r\n");}
//========================================================================================================= 



//========================================================================================================= 
// handle_cmd_wifi() - Handles Wi-Fi management commands
//========================================================================================================= 
static void handle_cmd_wifi()
{
    char token[128];
    
    // Fetch the next token 
    parser.get_next_token(token, sizeof token);

    // An empty token or "rssi" reports the current RSSI from the WiFi radio
    if (token[0] == 0 || strcmp(token, "rssi") == 0)
    {
        TCPServer.pass("%i", System.rssi());
        return;
    }

    // If we get here, we didn't understand the sub-command
    fail_syntax();
}
//========================================================================================================= 


//========================================================================================================= 
// handle_cmd_rssi() - Reports Wi-Fi RSSI (Received Signal Strength Indicator)
//========================================================================================================= 
static void handle_cmd_rssi()
{
    TCPServer.pass("%i", System.rssi());
}
//========================================================================================================= 



//========================================================================================================= 
// handle_cmd_time() - Reports the current time and optionally sets the current time
//
// If a parameter is supplied, it should be in HH:MM:SS format, in Coordinated Universal Time
//========================================================================================================= 
static void handle_cmd_time()
{
    char token[128];

    // Fetch the next token, if it exists, assume it's an ASCII representation of the time
    if (parser.get_next_token(token, sizeof token))
    {
        // Convert that token into hours, minutes, seconds
        if (!System.set_time(token))
        {
            fail_syntax();
            return;
        }
    }

    // Fetch the current system time in ASCII
    int64_t now = System.fetch_time(token);

    // Report current time in UTC
    TCPServer.pass("%lld %s", now, token);
}
//========================================================================================================= 


//========================================================================================================= 
// handle_cmd_fwrev() - Reports the firmware revision to the user
//========================================================================================================= 
static void handle_cmd_fwrev()
{
    char idf_version[50];

    // Fetch the version of ESP-IDF
    strcpy(idf_version, esp_get_idf_version());

    // Get a pointer to the 6th byte from the end of that string
    char* p = strchr(idf_version, 0) - 6;

    // If the last 6 characters are "-dirty", strip them off
    if (strcmp(p, "-dirty") == 0) *p = 0;

    TCPServer.pass("%s %s", FW_VERSION, idf_version);
}
//========================================================================================================= 


//========================================================================================================= 
// handle_cmd_reboot() - Performs a soft-reset and reboots the firmware
//========================================================================================================= 
static void handle_cmd_reboot()
{
    // Tell the user that we received his command
    TCPServer.pass();

    // Wait a half second to make sure that response gets sent
    msdelay(500);

    // Reboot the system
    System.reboot();
}
//========================================================================================================= 

//========================================================================================================= 
// handle_cmd_freeram() - Reports the amount of unallocated heap memory
//========================================================================================================= 
static void handle_cmd_freeram()
{
    // Find out how much free RAM we have available
    int free_ram = xPortGetFreeHeapSize();

    // And report that number to the user
    TCPServer.pass("%i", free_ram);
}
//========================================================================================================= 




//========================================================================================================= 
// handle_cmd_nvget() - Handles all of the commands that report values from the non-volatile storage
//                      data structure
//========================================================================================================= 
static void handle_cmd_nvget()
{
    char token[128];

    // Fetch the token that tells us which non-volative parameter to fetch
    parser.get_next_token(token, sizeof token);

    // Are we being asked to re-read NVS into RAM?
    if (strcmp(token, "read") == 0)
    {
        NVS.read_from_flash();
        TCPServer.pass();
        return;
    }

    // Is the user asking for the NVS CRC?
    if (strcmp(token, "crc") == 0)
    {
        // Save the existing CRC
        U32 old_crc = NVS.data.crc;

        // Set the existing CRC to 0 so we can compute a new one
        NVS.data.crc = 0;

        // Compute a new CRC
        U32 new_crc = crc32(&NVS.data, sizeof NVS.data);

        // Restore the CRC in the NVS structure to its original value
        NVS.data.crc = old_crc;

        // Find out if the CRC's match
        int ok = (old_crc == new_crc) ? 1:0;

        // Tell the client whether the CRCs match and what the CRCs are
        TCPServer.pass("%i 0x%08X 0x%08X", ok, old_crc, new_crc);
        return;
    }

    // Is the user asking for the network SSID?
    if (strcmp(token, "ssid") == 0)
    {
        TCPServer.pass("\"%s\"", NVS.data.network_ssid);
        return;
    }


    // Is the user asking for the network user-id?
    if (strcmp(token, "netuser") == 0)
    {
        TCPServer.pass("\"%s\"", NVS.data.network_user);
        return;
    }


    // Is the user asking for a general dump of everything in nv-storage?
    if (strcmp(token, "") == 0)
    {
        TCPServer.send(" ssid:       \"%s\"\r\n", NVS.data.network_ssid);
        TCPServer.send(" netuser:    \"%s\"\r\n", NVS.data.network_user);
        TCPServer.pass();
        return;
    }


    // If we get here, there was a syntax error
    fail_syntax();
}
//========================================================================================================= 


//========================================================================================================= 
// handle_cmd_nvset() - Handles all of the commands that store values into the non-volatile storage
//                      data structure
//========================================================================================================= 
static void handle_cmd_nvset()
{
    char name[128];
    char value[128];

    // Fetch the token that tells us which non-volative parameter to store into
    parser.get_next_token(name, sizeof name);

    // Fetch the token that is the value we're going to store in nv storage
    if (!parser.get_next_token(value, sizeof value))
    {
        fail_syntax();
        return;
    }

    // Is the user setting the network SSID?
    if (strcmp(name, "ssid") == 0)
    {
        safe_copy(NVS.data.network_ssid, value);
        NVS.write_to_flash();
        TCPServer.pass();
        return;
    }

    // Is the user setting the network user-id?
    if (strcmp(name, "netuser") == 0)
    {
        safe_copy(NVS.data.network_user, value);
        NVS.write_to_flash();
        TCPServer.pass();
        return;
    }

    // Is the user setting the network password?
    if (strcmp(name, "netpw") == 0)
    {
        // Ensure that we don't exceed the maximum allowed length
        if (strlen(value) >= NET_PW_RAW_LEN)
        {
            fail_unsupp();
            return;
        }

        safe_copy(NVS.data.network_pw, value);
        NVS.write_to_flash();
        TCPServer.pass();
        return;
    }
    // If we get here, there was a syntax error
    fail_syntax();
}
//========================================================================================================= 



//========================================================================================================= 
// handle_cmd_stack() - Displays the remaining free bytes on the task stacks
//========================================================================================================= 
void handle_cmd_stack()
{
    for (int i=0; i<TASK_IDX_COUNT; ++i)
    {
        task_idx_t idx = (task_idx_t)i;
        TCPServer.send(" %-10s %5i\r\n", StackMgr.name(idx), StackMgr.remaining(idx));
    }
    TCPServer.pass();
}
//========================================================================================================= 



//========================================================================================================= 
// handle_tcp_command() - Parses an input line from the server and takes the appropriate action
//========================================================================================================= 
void handle_tcp_command(char* input)
{
    char token[64];

    // Point to the line of text that we just received from the TCP client
    parser.set_input(input);

    // Fetch the command token
    parser.get_next_token(token, sizeof token);

    if      (strcmp(token, "fwrev"  ) == 0) handle_cmd_fwrev();
    else if (strcmp(token, "freeram") == 0) handle_cmd_freeram();
    else if (strcmp(token, "reboot" ) == 0) handle_cmd_reboot();
    else if (strcmp(token, "time"   ) == 0) handle_cmd_time();
    else if (strcmp(token, "nvget"  ) == 0) handle_cmd_nvget();
    else if (strcmp(token, "nv"     ) == 0) handle_cmd_nvget();
    else if (strcmp(token, "nvset"  ) == 0) handle_cmd_nvset();
    else if (strcmp(token, "rssi"   ) == 0) handle_cmd_rssi();
    else if (strcmp(token, "wifi"   ) == 0) handle_cmd_wifi();
    else if (strcmp(token, "stack"  ) == 0) handle_cmd_stack();

    else fail_syntax();

    // Keep track of our stack usage
    StackMgr.record_hwm(TASK_IDX_TCP_SERVER);
}
//========================================================================================================= 

