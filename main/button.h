//=========================================================================================================
// button.h - Defines the interface to a button on a GPIO pin
//
// Buttons are assumed to be active low
//=========================================================================================================
#pragma once
#include "common.h"

//=========================================================================================================
// CButton base class - defines everything except the handler for the button in question
//=========================================================================================================
class CButton
{
public:

    // Call this once to initialize the GPIO registers
    void    init(gpio_num_t, bool trigger_state = true);

    // Call this to find out if the button is pressed, and how long it has been in that state (in msec)
    bool    is_pressed(uint32_t* p_how_long = nullptr);

    // Returns true if the button is pressed and has been for at least the specified number of msecs
    bool    is_pressed_at_least(uint32_t milliseconds);

    // This clears the current trigger flag
    void    clear_trigger() {m_is_triggered = false;}

    // This tells us if the button has been pressed since the last time the trigger flag was cleared
    bool    is_triggered() {return m_is_triggered;}
    
    // Returns true if the input pin is currently in the active (low) state
    bool    is_pin_active();

protected:

    // This is the button-response handler and must be over-ridden
    virtual void handler(bool state, uint32_t elapsed_milliseconds) = 0;

    // This is the global task handler that dispatches object-specific task handlers
    static void dispatch_task(void* p_object);

    // This is the global ISR that stuffs events into the button-specific event queues
    static void isr(void* p_object);

    // This is a task handler that is instantiated by init().
    void        button_task();

    // This is the pin number that our button is attached to
    gpio_num_t  m_pin;

    // This will be true if the button is currently pressed
    bool        m_is_pressed;

    // This is the pressed/not-pressed state this is considered "triggered". 
    bool        m_trigger_state;

    // This tells us whether the button has been pressed since the trigger was last cleared
    bool        m_is_triggered;

    // This is the time (in microseconds) when we last noticed a change of state
    int64_t     m_state_change_time;

    // This is the queue that the ISR will publish events into
    xQueueHandle m_button_event_queue;
};
//=========================================================================================================

