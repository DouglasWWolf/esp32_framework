//=========================================================================================================
// tcp_server.h - Defines a TCP socket server task
//
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>  THIS CODE IS INTENDED TO WORK ACROSS FIRMWARE GENERATIONS   <><>
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
//
//=========================================================================================================
#pragma once
#include "common.h"

class CTCPServer
{
public:

    CTCPServer() {m_task_handle = nullptr;}

    // Call this to start the server task
    void    start();

    // Call this to stop the server task
    void    stop();

    // Returns 'true' if the server has a client connected
    bool    has_client();

    // Turns Nagle's algorithm on or off
    void    set_nagling(bool flag);

    //----------------------------------------------
    // These are for use by command handlers
    //----------------------------------------------
    void    pass(const char* fmt , ...);
    void    pass();
    void    send(const char* fmt, ...);
    bool    read(unsigned char* p_data, int count);
    bool    write(unsigned char* p_data, int count);
    void    drain_input();
    bool    is_halted_by_user();
    void    close();
    //----------------------------------------------

public:
    
    // This is the task that serves as our TCP server.
    void    tcp_server_task();

protected:

    // This is the handle of the currently running server task
    TaskHandle_t m_task_handle;
};
