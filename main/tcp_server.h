#pragma once
#include "common.h"
#include "tcp_server_base.h"


//=========================================================================================================
// TCP Server - Handles incoming commands from a TCP socket
//=========================================================================================================
class CTCPServer : public CTCPServerBase
{
public:

    // Constructor - just calls the base class
    CTCPServer(int port) : CTCPServerBase(port) {}

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



