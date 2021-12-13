//=========================================================================================================
// tcp_server.cpp - Implements a TCP socket server task 
//
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>  THIS CODE IS INTENDED TO WORK ACROSS FIRMWARE GENERATIONS   <><>
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
//
//=========================================================================================================
#include <sys/param.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <tcp_server.h>
#include <globals.h>


// Defined in tcp_handlers.cpp
void handle_tcp_command(char* input);


#define CLOSED -1

#define SERVER_PORT 1000

static const char *TAG = "tcp_server";

// This is contains the information about the server socket we are about to create
static struct sockaddr_in sock_desc;

// This will be true when we have a fully constructed socket
static bool is_socket_created = false;

// This contains the socket descriptor of the socket when it's open
static int  sock = CLOSED;

// This contains the descriptor of the socket thst we listen for connections on
static int listen_sock = CLOSED;

// This is the input line with carriage-returns and linefeeds stripped from the end
static char input[128], *p_input;

//========================================================================================================= 
// is_socket_readable() - Will return 'true' if there is data on the socket ready to be read
//========================================================================================================= 
static bool is_socket_readable()
{
    fd_set read_set;

    // We won't want to wait for socket data, we're just polling
    struct timeval tv = { .tv_sec = 0, .tv_usec = 0};

    // Build an file-descriptor set that contains just our single socket descriptor
    FD_ZERO(&read_set);
    FD_SET(sock, &read_set);

	// Check to see if data is available on the socket
	int retval = select (sock+1, &read_set, NULL, NULL, &tv);

	// Tell the caller whether there is data to read
	return (retval > 0);
}
//========================================================================================================= 



//========================================================================================================= 
// read() - reads bytes from the socket
// 
// Return 'true' if all expected bytes are read from the socket
//========================================================================================================= 
bool CTCPServer::read(unsigned char* data, int length)
{
    // This is how many bytes we have remaining to receive
    int remaining = length;

    // So long as there is data remaining to be received
    while (remaining)
    {
        // Fetch data from the socket
        int len_recvd = ::recv(sock, data, remaining, 0);

        // If an error occurs, tell the caller
        if (len_recvd < 0) return false;

        // Prepare for the next ::recv() in case we didn't receive all the data
        remaining -= len_recvd;
        data += len_recvd;
    }

    // Tell the caller that all is well
    return true;
}
//========================================================================================================= 



//========================================================================================================= 
// write() - Writes bytes to the socket
//========================================================================================================= 
bool CTCPServer::write(unsigned char* p_data, int count)
{
    ::send(sock, p_data, count, 0);

    // Tell the caller that we read all of the requested bytes from the socket
    return true;
}
//========================================================================================================= 
  


//========================================================================================================= 
// SetNagling() - Turns on and off Nagle's Algorithm.  If Nagling is off, all packets will be sent to the
//                interface immediately upon  completion of a "::send()" operation
//========================================================================================================= 
void CTCPServer::set_nagling(bool flag)
{
    // Set up our flag that will be passed to setsockopt()
    char value = flag ? 1 : 0;

    // And set the Nagling option appropriately
    setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &value, sizeof value);
}
//========================================================================================================= 





//========================================================================================================= 
// is_halted_by_user() - Returns true the user has sent a carriage return or linefeed
//========================================================================================================= 
bool CTCPServer::is_halted_by_user()
{
    char c;

    // If there is data waiting to be read from the socket...
    while (is_socket_readable())
    {
        // Fetch a single character
        int len = recv(sock, &c, 1, 0);

        // If the user closed the socket, abandon ship!
        if (len < 0) return true;

        // If the user sent us a linefeed or carriage return, we're done
        if (c == 10 || c == 13) return true;
    }

    // If we get here, the user hasn't sent us a "stop what you're doing" signal
    return false;
}
//========================================================================================================= 


//========================================================================================================= 
// pass() - Report to the user that their command succeeded
//========================================================================================================= 
void CTCPServer::pass()
{
    ::send(sock, "OK\r\n", 4, 0);
}
//========================================================================================================= 


//========================================================================================================= 
// pass() - Report to the user that their command succeeded, using printf style parameters
//========================================================================================================= 
void CTCPServer::pass(const char* fmt, ...)
{
    static char buffer[128] = {'O', 'K', ' '};
    va_list args;

    // Get a pointer to the variable argument list
    va_start(args, fmt);
  
    // Print the caller's printf-style argument list to the buffer
    vsprintf(buffer+3, fmt, args);

    // Append a carriage-return and line-feed to or buffer
    strcat(buffer, "\r\n");    

    // Clean up the variable argument buffer
    va_end(args);

    // Send the "OK" report to the user
    ::send(sock, buffer, strlen(buffer), 0);
 }
//========================================================================================================= 


//========================================================================================================= 
// send() - Sends a printf-formatted string to the socket client
//========================================================================================================= 
void CTCPServer::send(const char* fmt, ...)
{
    char buffer[100];
    va_list args;

    // Get a pointer to the variable argument list
    va_start(args, fmt);
  
    // Print the caller's printf-style argument list to the buffer
    vsprintf(buffer, fmt, args);
    
    // Clean up the variable argument buffer
    va_end(args);

    // Send the "OK" report to the user
    ::send(sock, buffer, strlen(buffer), 0);
}
//========================================================================================================= 


//========================================================================================================= 
// drain_input() - Drains all incoming characters from our socket
//========================================================================================================= 
void CTCPServer::drain_input()
{
    char data[1];
    while (is_socket_readable())
    {
        recv(sock, data, 1, 0);
    }
}
//========================================================================================================= 


//========================================================================================================= 
// hard_shutdown() - Ensures that the listening socket and the server socket are closed
//========================================================================================================= 
static void hard_shutdown()
{
    // Shut down the server socket
    if (sock != CLOSED)
    {
        shutdown(sock, 0);
        close(sock);
        sock = CLOSED;
        is_socket_created = false;
    }

    // Shut down the listening socket
    if (listen_sock != CLOSED)
    {
        shutdown(listen_sock, 0);
        close(listen_sock);
        listen_sock = CLOSED;
    }

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
static bool wait_for_connection()
{
    int error;

    
    // If we already have a socket created, close it down
    if (sock != CLOSED)
    {
        shutdown(sock, 0);
        close(sock);
        sock = CLOSED;
    }    

    // We don't have a functioning set of sockets yet
    is_socket_created = false;

    // We can bind to any available IP address (though there will really only be one)
    sock_desc.sin_addr.s_addr = htonl(INADDR_ANY);

    // This is an IP socket
    sock_desc.sin_family = AF_INET;

    // Bind the the pre-defined port number
    sock_desc.sin_port = htons(SERVER_PORT);

    // If we haven't built a socket to listen on yet
    if (listen_sock == CLOSED)
    {
        // Create a socket and store its descriptor in "listen_sock"
        listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);

        // If we couldn't create a socket, it's a fatal error
        if (listen_sock == CLOSED)
        {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            return false;
        }

        // Bind the listen_socket to the port we selected at the top of this routine
        error = bind(listen_sock, (struct sockaddr *)&sock_desc, sizeof(sock_desc));
    
        // If that bind failed, it's a fatal error
        if (error)
        {
            ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
            return false;
        }

        // Begin listening for TCP connections on our predefined port
        error = listen(listen_sock, 1);
    
        // If that somehow failed, it's a fatal error
        if (error)
        {
            ESP_LOGE(TAG, "Error occured during listen: errno %d", errno);
            return false;
        }
    }

    struct sockaddr_in6 source_addr; 
    uint32_t addr_len = sizeof(source_addr);

    // Wait for a client connect and accept it when it arrives
    while (sock == CLOSED)
    {
        sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    }

    // The socket was created
    is_socket_created = true;

    // Tell the caller that all is well
    return true;
}
//========================================================================================================= 

//========================================================================================================= 
// fetch_line() - Fetches a line of text from the TCP server
//
// Returns: 'true' if a line of text was fetched, 'false' if the socket got closed by the client
//========================================================================================================= 
static bool fetch_line()
{
    char    c;

    // Point to the beginning of the input buffer
    p_input = input;

    // This is the number of characters that we can still stuff into the buffer
    int space_remaining = sizeof(input) - 1;

    while (true)
    {
        // Fetch a single character
        int len = recv(sock, &c, 1, 0);

        // If the other side of the socket is closed, we're done here
        if (len < 1) return false;

        // Convert tabs to spaces
        if (c == 9) c = 32;

        switch (c)
        {
        // If the recv'd character was a carriage-return or linefeed...
        case 10:
        case 13:

            // Write a nul terminating byte
            *p_input = 0;

            // If there is data in the input buffer, tell the caller
            if (p_input > input) return true;

            break;

        // If the incoming character was a backspace        
        case 8:

            // If we're not at the start of the buffer, backspace
            if (p_input > input)
            {
                --p_input;
                ++space_remaining;
            }
            
            break;
 
        // If it's any other character, it get stored into our input buffer
        default:
            if (space_remaining)
            {
                *p_input++ = c;
                --space_remaining;
            }
        }
    }
}
//========================================================================================================= 


//========================================================================================================= 
//  tcp_server_task - provides a continuously running TCP server that clients can connect to
//                    to perform various management tasks
//========================================================================================================= 
void CTCPServer::tcp_server_task()
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
        while (fetch_line())
        {
            Network.register_activity();
            handle_tcp_command(input);
        }
    }
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
    CTCPServer* p_object = (CTCPServer*) pvParameters;
    
    // And run the task for that object!
    p_object->tcp_server_task();
}
//=========================================================================================================


//=========================================================================================================
// start() - Starts the TCP server task
//=========================================================================================================
void CTCPServer::start()
{
    // If we're already started, do nothing
    if (m_task_handle) return;

    // Create the task
    xTaskCreatePinnedToCore(launch_task, "tcp_server", 4096, this, TASK_PRIO_TCP, &m_task_handle, TASK_CPU);
}
//=========================================================================================================


//=========================================================================================================
// stop() - Stops the TCP server task
//=========================================================================================================
void CTCPServer::stop()
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
// close() - Closes the socket
//=========================================================================================================
void CTCPServer::close()
{
    hard_shutdown();       
}
//=========================================================================================================



//=========================================================================================================
// has_client() - Returns 'true' if a client is connected to the TCP server, otherwise 'false'
//=========================================================================================================
bool CTCPServer::has_client()
{
    return sock != CLOSED;
}
//=========================================================================================================
