#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/param.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <stdint.h>
#include <stdarg.h>
#include "globals.h"

static const char* TAG = "tcp_server";

//=========================================================================================================
// Constructor() 
//=========================================================================================================
CTCPServerBase::CTCPServerBase(int port)
{
    m_task_handle = nullptr;
    m_sock = CLOSED;
    m_has_client = false;
    m_server_port = port;
}
//=========================================================================================================


//=========================================================================================================
// launch_task() - Calls the "task()" routine in the specified object
//
// Passed: *pvParameters points to the object that we want to use to run the task
//=========================================================================================================
static void launch_task(void *pvParameters)
{
    // Fetch a pointer to the object that is going to run out task
    CTCPServerBase* p_object = (CTCPServerBase*) pvParameters;
    
    // And run the task for that object!
    p_object->task();
}
//=========================================================================================================


//=========================================================================================================
// start() - Starts the TCP server task
//=========================================================================================================
void CTCPServerBase::start()
{
    // If we're already started, do nothing
    if (m_task_handle) return;

    // Create the task
    xTaskCreatePinnedToCore(launch_task, "tcp_server", 3000, this, TASK_PRIO_TCP, &m_task_handle, TASK_CPU);
}
//=========================================================================================================


//=========================================================================================================
// stop() - Stops the TCP server task
//=========================================================================================================
void CTCPServerBase::stop()
{
    static TaskHandle_t task_handle;

    // Kill the task if it's running
    if (m_task_handle)
    {
        task_handle = m_task_handle;
        m_task_handle = nullptr;
        vTaskDelete(task_handle);
    }

    // If this was called from a different thread, we'll get a chance to close the sockets
    // This has to come <<after>> the vTaskDelete() because trying to close a socket that is in
    // active use can either hang or panic the system.
    hard_shutdown();
}
//=========================================================================================================



//=========================================================================================================
// execute() - State machine: Call this often, or with blocking set to true
// 
// If blocking is true:
//     Virtual function read() should block until it has data to return
//     If the input stream closes, read() should return -1
//     If read() returns -1, this routine will return
//=========================================================================================================
void CTCPServerBase::execute()
{
    char c;

again:

    // This is how many bytes we have remaining free in buffer that holds the incoming message
    int free_remaining = sizeof(m_message) - 1;

    // This is where the next incoming character will be stored
    char* p_input = m_message;

     // For each character available
    while (true)
    {
        // Fetch a single character from the socket;
        if (recv(m_sock, &c, 1, 0) < 1) break;

        // Convert tabs to spaces
        if (c == 9) c = 32;

        // Handle backspace
        if (c == 8)
        {
            if (p_input > m_message)
            {
                --p_input;
                ++free_remaining;
            }
            continue;
        }

        // Handle both carriage-return and linefeed
        if (c == 13 || c == 10)
        {
            // If the message buffer is empty, ignore it
            if (p_input == m_message) continue;

            // Nul-terminate the message
            *p_input = 0;

            // Go see if the message needs to be handled
            handle_new_message();

            // Reset back to an empty message buffer
            goto again;
        }

        // If there's room to add this character to the input buffer, make it so
        if (free_remaining)
        {
            *p_input++ = c;
            --free_remaining;
        }
    }
}
//=========================================================================================================

  


//=========================================================================================================
// handle_new_message() - Parse out the first token from a newly arrives message and potentially
//                        call the message handler
//
// On Entry:  m_message = Null terminated character string
//=========================================================================================================
void CTCPServerBase::handle_new_message()
{
    // Tell the network that there is activity on this socket
    Network.register_activity();

    // Point to our input message
    char* in = m_message;

    // Skip over leading spaces
    while (*in == ' ') ++in;

    // If we've hit the end of the message, it was just spaces... ignore the message
    if (*in == 0) return;

    // This is where the first token begins
    const char* first_token = in;

    // Skip past the first token, converting to lowercase as we go
    --in;
    while (true)
    {
        // Fetch the next input character
        char c = *++in;

        // If we've hit the end of the token, break out of the loop
        if (c == ' ' || c == 0) break;

        // Convert uppercase to lowercase
        if (c >= 'A' && c < 'Z') *in = c + 32;
    }

    // If there are spaces after the first token, skip over them
    while (*in == ' ') *in++ = 0;

    // Make m_next_token point to the start of our first command parameter
    m_next_token = in;

    // Call the top level command handler
    on_command(first_token);

    // Keep track of the high-water mark on the stack for this thread
    StackMgr.record_hwm(TASK_IDX_TCP_SERVER);
}
//=========================================================================================================



//=========================================================================================================
// get_next_token() - Provides a pointer to the next token if there is one
// 
// Passed:   A pointer to the user's char* that he wants pointed to the token
// 
// Returns:  true if there was a token available, otherwise false
// 
// On Exit:  The caller's char* points to the next token (or to a null byte if no token available).
//           If the original token was in quotation marks, they have been stripped.
//           A quoted token has letter-case and internal spaces preserved.
//           An unquoted token is always converted to lowercase.
//
// Notes: This routine always leaves m_token pointing to the first byte of the following token
//=========================================================================================================
bool CTCPServerBase::get_next_token(const char** p_retval)
{
    char c;

    // Get a pointer to the start of the token
    char* in = m_next_token;

    // If there isn't a next token available, tell the caller
    if (*in == 0)
    {
        *p_retval = in;
        return false;
    }

    // Does this token begin with a quote-mark?
    bool in_quotes = (*in == 34);

    // If the token begins with a quote-mark, skip over it
    if (in_quotes) ++in;

    // One way or another, this is the start of the token we'll hand to the caller
    *p_retval = in;

    // If it was just a lone quote-mark, there's no token
    if (*in == 0)
    {
        m_next_token = in;
        return false;
    }

    // Bump back by 1 so that the scanning loops below can always point to the character being examined
    --in;

    // If we're in quotation marks, search for the closing quote-mark
    if (in_quotes) while (true)
    {
        // Fetch the next character
        c = *++in;

        // If this was the closing quote-mark, convert to a space, and we're at the end of the token
        if (c == 34)
        {
            *in = ' ';
            break;
        }

        // A nul-byte always terminates the token
        if (c == 0) break;
    }

    // Otherwise, we're not in quotes, so search for a space at the end of the token
    else while (true)
    {
        // Fetch the next character
        c = *++in;

        // If we hit a space or a null, it's the end of the token
        if (c == ' ' || c == 0) break;

        // Convert to lower case as neccessary
        if (c >= 'A' && c <= 'Z') *in += 32;
    }

    // We're at the end of the token.  Scan past any spaces
    while (*in == ' ') *in++ = 0;

    // And this is where the next scan for tokens will begin
    m_next_token = in;

    // Tell the caller he has a token available
    return true;
}
//=========================================================================================================



//=========================================================================================================
// pass() - Reports OK
//=========================================================================================================
bool CTCPServerBase::pass()
{
    ::send(m_sock, "OK\r\n", 4, 0);
    return true;
}
//=========================================================================================================


//=========================================================================================================
// pass() - A printf-style function that allows a derives class to report success
//=========================================================================================================
bool CTCPServerBase::pass(const char* fmt, ...)
{
    char buffer[200] = "OK ";
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer+3, sizeof(buffer)-3, fmt, args);
    va_end(args);
    strcat(buffer, "\r\n");    
    ::send(m_sock, buffer, strlen(buffer), 0);
    return true;
}
//=========================================================================================================



//=========================================================================================================
// fail() - A printf-style function that allows a derives class to report success
//=========================================================================================================
bool CTCPServerBase::fail(const char* fmt, ...)
{
    char buffer[200] = "FAIL ";
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer+5, sizeof(buffer)-5, fmt, args);
    va_end(args);
    strcat(buffer, "\r\n");    
    ::send(m_sock, buffer, strlen(buffer), 0);
    return true;
}
//=========================================================================================================


//=========================================================================================================
// fail_syntax() - A convenience method for reporting a syntax error
//=========================================================================================================
bool CTCPServerBase::fail_syntax()
{
    fail("SYNTAX");
    return false;
}
//=========================================================================================================



//=========================================================================================================
// replyf() - A printf-style function that outputs formatted strings
//=========================================================================================================
void CTCPServerBase::replyf(const char* fmt, ...)
{
    char buffer[200];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer)-2, fmt, args);
    va_end(args);
    strcat(buffer, "\r\n");    
    ::send(m_sock, buffer, strlen(buffer), 0);
}
//=========================================================================================================


//========================================================================================================= 
// wait_for_connection() - Closes down any existing socket, creates a new one, and starts listening for 
//                         a TCP connection on our server port
//
// Returns:  'true' if a socket is succesfully created and a client connected
//           'false' if something went awry in the socket-creation process.
//
// On Entry: is_socket_created = true if a socket already exists
//
// On Exit:  sock = socket descriptor of a socket that has a client connected to it
//========================================================================================================= 
bool CTCPServerBase::wait_for_connection()
{
    int error, True = 1;
    struct sockaddr_in sock_desc;

    // We have no client connected
    m_has_client = false;

    // If we already have a socket created, close it down
    hard_shutdown();

    // We can bind to any available IP address (though there will really only be one)
    sock_desc.sin_addr.s_addr = htonl(INADDR_ANY);

    // This is an IP socket
    sock_desc.sin_family = AF_INET;

    // Bind the the pre-defined port number
    sock_desc.sin_port = htons(m_server_port);

    // Create our socket
    m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

    // This socket is allowed to re-use a previous bound port number
    setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (void*)&True, sizeof(True));

    // Bind the socket to the TCP port we specified
    error = bind(m_sock, (struct sockaddr *)&sock_desc, sizeof(sock_desc));
    
    // If that bind failed, it's a fatal error
    if (error)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        return false;
    }

    // Begin listening for TCP connections on our predefined port
    error = listen(m_sock, 1);
    
    // If that somehow failed, it's a fatal error
    if (error)
    {
        ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
        return false;
    }

    // The IP address of the client will be stored here
    struct sockaddr_in6 source_addr; 
    uint32_t addr_len = sizeof(source_addr);

    // Wait for a client connect and accept it when it arrives
    int new_socket = accept(m_sock, (struct sockaddr *)&source_addr, &addr_len);

    // Close the original socket we were listening on
    close(m_sock);

    // And start using the new socket
    m_sock = new_socket;

    // We now have a client connected
    m_has_client = true;

    // Tell the caller that all is well
    return true;
}
//========================================================================================================= 






//========================================================================================================= 
// hard_shutdown() - Forces the socket closed
//========================================================================================================= 
void CTCPServerBase::hard_shutdown()
{
    if (m_sock != CLOSED)
    {
        shutdown(m_sock, 0);
        close(m_sock);
        m_sock = CLOSED;
    }
}
//========================================================================================================= 






//========================================================================================================= 
// task() - When the thread is spawned, this is the routine that starts in its own thread
//========================================================================================================= 
void CTCPServerBase::task()
{
    // Ensure that sockets are closed
    hard_shutdown();

    // We're going to do this forever
    while (true)
    {
        // Build our listening socket and wait for a client to connect.
        // If something goes awry, there's no way to recover, so we halt this task
        if (!wait_for_connection()) stop();

        // Fetch and handle incoming messages
        execute();
    }
}
//========================================================================================================= 




//========================================================================================================= 
// set_nagling() - Turns on and off Nagle's Algorithm.  If Nagling is off, all packets will be sent to the
//                 interface immediately upon  completion of a "::send()" operation
//========================================================================================================= 
void CTCPServerBase::set_nagling(bool flag)
{
    // Set up our flag that will be passed to setsockopt()
    char value = flag ? 1 : 0;

    // And set the Nagling option appropriately
    setsockopt(m_sock, IPPROTO_TCP, TCP_NODELAY, &value, sizeof value);
}
//========================================================================================================= 
