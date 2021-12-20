#pragma once
#include "common.h"
#include "serialserver_base.h"


//=========================================================================================================
// Serial Server - Handles incoming commands from the serial port
//=========================================================================================================
class CSerialServer : public CSerialServerBase
{

protected:


    // ---------------  Handlers for specific commands ------------------
    // ------------  The bool return values are meaningless  ------------
    bool    handle_fwrev();
    bool    handle_freeram();
    bool    handle_reboot();
    bool    handle_time();
    bool    handle_nvget();
    bool    handle_nvset();
    bool    handle_rssi();
    bool    handle_wifi();
    bool    handle_stack();
    // ------------------------------------------------------------------


protected:  

    //  A custom failure code
    bool    fail_unsupp() {return fail("UNSUPP");}

    // Whenever a command comes in, this top-level handler gets called
    void    on_command(const char* command);

};
//=========================================================================================================



