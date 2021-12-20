//=========================================================================================================
// tcp_server_base.h - The base class for a TCP command server
//=========================================================================================================
#pragma once

class CTCPServerBase
{

    //--------------------------------------------------------------------------------
    //  Public API for the outside world to access
    //--------------------------------------------------------------------------------
public:

    // Constructor
    CTCPServerBase(int port);

    // Starts the thread that runs the server
    void    start();

    // Call this to delete the thread that is running the server
    void    stop();

    // Call this to turn Nagle's algorithm on or off.  "false" means "send packets immediately"
    void    set_nagling(bool flag);

    // Call this to find out if there is a client connected to our server
    bool    has_client() {return m_has_client;}

    //--------------------------------------------------------------------------------
    // Public only so that launch_thread() has access to it
    //--------------------------------------------------------------------------------
public:      

    // When the thread spawns, this is the routine that starts 
    void    task();


    //--------------------------------------------------------------------------------
    // Override this in your derived class
    //--------------------------------------------------------------------------------
protected:

    // This gets called whenever a new command is received. Over-ride this
    virtual void  on_command(const char* command) = 0;


    //--------------------------------------------------------------------------------
    // Tools for the command-handlers to use
    //--------------------------------------------------------------------------------
protected:      

    // Message handlers call this to fetch the next available token.  Returns false is none available
    bool    get_next_token(const char** p_token);

    // Message handlers call these to indicate pass or fail
    bool    pass();
    bool    pass(const char* fmt, ...);
    bool    fail(const char* fmt, ...);
    bool    fail_syntax();

    // Lowest level methods for replying to a command
    void    replyf(const char* fmt, ...);


    //--------------------------------------------------------------------------------
    // Functions and data that are private to the base class
    //--------------------------------------------------------------------------------
private:

    // Create a socket and listen for connections
    bool    wait_for_connection();

    // Once a connection is made, this executes the command handler
    void    execute();

    // This gets called when carriage-return or linefeed is received
    void    handle_new_message();

    // This is our incoming message
    char    m_message[128];

    // When "get_next_token()" is called, this points to the 1st char of the next token
    char*   m_next_token;

private:  /* TCP and ESP specific stuff */


    // Forces the socket closed if it's open
    void    hard_shutdown();

    const int CLOSED = -1;

    // This is the handle of the currently running server task
    TaskHandle_t    m_task_handle;

    // This is the socket descriptor of the TCP socket
    int             m_sock;

    // This will be true if there is a client connected
    bool            m_has_client;

    // This is the server port we listen on
    int             m_server_port;

};

