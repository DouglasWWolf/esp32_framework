//=========================================================================================================
// tcp_server.cpp() - Implements our TCP command server
//=========================================================================================================
#include "globals.h"
#include "history.h"

// Compares a token to a string constant.  The string constant can be in RAM or Flash
#define token_is(strcon) (strcmp(token,strcon) == 0)


//========================================================================================================= 
// handle_fwrev() - Reports the firmware revision to the user
//========================================================================================================= 
bool CTCPServer::handle_fwrev()
{
    char idf_version[50];

    // Fetch the version of ESP-IDF
    strcpy(idf_version, esp_get_idf_version());

    // Get a pointer to the 6th byte from the end of that string
    char* p = strchr(idf_version, 0) - 6;

    // If the last 6 characters are "-dirty", strip them off
    if (strcmp(p, "-dirty") == 0) *p = 0;

    return pass("%s %s", FW_VERSION, idf_version);
}
//========================================================================================================= 



//========================================================================================================= 
// handle_freeram() - Reports the amount of unallocated heap memory
//========================================================================================================= 
bool CTCPServer::handle_freeram()
{
    // Find out how much free RAM we have available
    int free_ram = xPortGetFreeHeapSize();

    // And report that number to the user
    return pass("%i", free_ram);
}
//========================================================================================================= 



//========================================================================================================= 
// handle_reboot() - Performs a soft-reset and reboots the firmware
//========================================================================================================= 
bool CTCPServer::handle_reboot()
{
    // Tell the user that we received his command
    pass();

    // Wait a half second to make sure that response gets sent
    msdelay(500);

    // Reboot the system
    System.reboot();

    // This is just here to keep the compiler happy
    return true;
}
//========================================================================================================= 





//========================================================================================================= 
// handle_time() - Reports the current time and optionally sets the current time
//
// If a parameter is supplied, it should be in HH:MM:SS format, in Coordinated Universal Time
//========================================================================================================= 
bool CTCPServer::handle_time()
{
    const char* token;
    char buffer[64];

    // Fetch the next token, if it exists, assume it's an ASCII representation of the time
    if (get_next_token(&token))
    {
        // Convert that token into hours, minutes, seconds
        if (!System.set_time(token)) return fail_syntax();
    }

    // Fetch the current system time in ASCII
    int64_t now = System.fetch_time(buffer);

    // Report current time in UTC
    return pass("%lld %s", now, buffer);
}
//========================================================================================================= 




//========================================================================================================= 
// handle__nvget() - Handles all of the commands that report values from the non-volatile storage
//                   data structure
//========================================================================================================= 
bool CTCPServer::handle_nvget()
{
    const char* token;

    // Fetch the token that tells us which non-volative parameter to fetch
    get_next_token(&token);

    // Are we being asked to re-read NVS into RAM?
    if token_is("read")
    {
        NVS.read_from_flash();
        return pass();
    }

    // Is the user asking for the NVS CRC?
    if token_is("crc")
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
        return pass("%i 0x%08X 0x%08X", ok, old_crc, new_crc);
    }

    // Is the user asking for the network SSID?
    if  token_is("ssid")
    {
        return pass("\"%s\"", NVS.data.network_ssid);
    }

    // Is the user asking for the network user-id?
    if token_is("netuser")
    {
        return pass("\"%s\"", NVS.data.network_user);
    }


    // Is the user asking for a general dump of everything in nv-storage?
    if token_is("")
    {
        replyf(" ssid:       \"%s\"", NVS.data.network_ssid);
        replyf(" netuser:    \"%s\"", NVS.data.network_user);
        return pass();
    }


    // If we get here, there was a syntax error
    return fail_syntax();
}
//========================================================================================================= 



//========================================================================================================= 
// handle_nvset() - Handles all of the commands that store values into the non-volatile storage
//                  data structure
//========================================================================================================= 
bool CTCPServer::handle_nvset()
{
    const char *token, *value;

    // Fetch the token that tells us which non-volative parameter to store into
    if (!get_next_token(&token)) return fail_syntax();

    // Fetch the token that is the value we're going to store in nv storage
    if (!get_next_token(&value)) return fail_syntax();

    // Is the user setting the network SSID?
    if token_is("ssid")
    {
        safe_copy(NVS.data.network_ssid, value);
        NVS.write_to_flash();
        return pass();
    }

    // Is the user setting the network user-id?
    if token_is("netuser")
    {
        safe_copy(NVS.data.network_user, value);
        NVS.write_to_flash();
        return pass();
    }

    // Is the user setting the network password?
    if token_is("netpw")
    {
        // Ensure that we don't exceed the maximum allowed length
        if (strlen(value) >= NET_PW_RAW_LEN) return fail_unsupp();

        safe_copy(NVS.data.network_pw, value);
        NVS.write_to_flash();
        return pass();
    }

    // If we get here, there was a syntax error
    return fail_syntax();
}
//========================================================================================================= 




//========================================================================================================= 
// handle_rssi() - Reports Wi-Fi RSSI (Received Signal Strength Indicator)
//========================================================================================================= 
bool CTCPServer::handle_rssi()
{
    return pass("%i", System.rssi());
}
//========================================================================================================= 




//========================================================================================================= 
// handle_wifi() - Handles Wi-Fi management commands
//========================================================================================================= 
bool CTCPServer::handle_wifi()
{
    const char* token;
    
    // Fetch the next token 
    get_next_token(&token);

    // An empty token or "rssi" reports the current RSSI from the WiFi radio
    if (token[0] == 0 || token_is("rssi"))
    {
        return pass("%i", System.rssi());
    }

    // If we get here, we didn't understand the sub-command
    return fail_syntax();
}
//========================================================================================================= 



//========================================================================================================= 
// handle_stack() - Displays the remaining free bytes on the task stacks
//========================================================================================================= 
bool CTCPServer::handle_stack()
{
    for (int i=0; i<TASK_IDX_COUNT; ++i)
    {
        task_idx_t idx = (task_idx_t)i;
        replyf(" %-10s %5i", StackMgr.name(idx), StackMgr.remaining(idx));
    }
    return pass();
}
//========================================================================================================= 





//=========================================================================================================
// on_command() - The top level dispatcher for commands
// 
// Passed:  token = Pointer to the command string
//=========================================================================================================
void CTCPServer::on_command(const char* token)
{
         if token_is("fwrev")    handle_fwrev();
    else if token_is("freeram")  handle_freeram();
    else if token_is("reboot")   handle_reboot();
    else if token_is("time")     handle_time();
    else if token_is("nvget")    handle_nvget();
    else if token_is("nv")       handle_nvget();
    else if token_is("nvset")    handle_nvset();
    else if token_is("rssi")     handle_rssi();
    else if token_is("wifi")     handle_wifi();
    else if token_is("stack")    handle_stack();

    else fail_syntax();
}
//=========================================================================================================


