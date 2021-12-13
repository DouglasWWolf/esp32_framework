//=========================================================================================================
// button.cpp - Implements the interface to a button on a GPIO pin
//
// Buttons are assumed to be active low
//=========================================================================================================
#include "button.h"

typedef char qentry_t;

//=========================================================================================================
// dispatch_task() - Calls the task-handler for a specific CButton object
//=========================================================================================================
void CButton::dispatch_task(void* p_object)
{
    ((CButton*)p_object)->button_task();
}
//=========================================================================================================


//=========================================================================================================
// isr() - This routine gets call by the ISR service any time an interrupt occurs on a pin
//         that we have registered via gpio_isr_handler_add()
//=========================================================================================================
void IRAM_ATTR CButton::isr(void* p_object)
{
    // We just need something to push into the queue
    static qentry_t event = 1;

    // Create a pointer to the appropriate button object
    CButton* p_button = (CButton*)p_object;

    // Stuff this event into the queue of the specified button object
    xQueueSendFromISR(p_button->m_button_event_queue, &event, NULL);
}
//=========================================================================================================



//=========================================================================================================
// init() - Configures the specified pin as an input and adds an interrupt handler for it
//
// Passed: pin           = One of the GPIO_NUM_XX constants
//         trigger_state = The button state that is considered "triggered"  (usually 'true')
//=========================================================================================================
void CButton::init(gpio_num_t pin, bool trigger_state)
{
    gpio_config_t io_conf;

    // Set the GPIO configuration structure to known values
    memset(&io_conf, 0, sizeof io_conf);

    // Save the pin number for future use
    m_pin = pin;

    // Save the state (pressed / not pressed) that is considered "triggered"
    m_trigger_state = trigger_state;

    // We want an interrupt whenever the button is either pressed or released
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
       
    // Create the queue that the ISR will post event messages to when an interrupt occurs
    m_button_event_queue = xQueueCreate(20, sizeof(qentry_t));

    // When an interrupt occurs on this pin, call the interrupt-service routine
    gpio_isr_handler_add(pin, CButton::isr, (void*) this);

    // And start the task that is going to receive button press events
    xTaskCreatePinnedToCore(CButton::dispatch_task, "button_task", 2048, this, DEFAULT_TASK_PRI, NULL, TASK_CPU);

    // This is a bitmap of which GPIO pins this configuration applies to
    io_conf.pin_bit_mask = (1LL << (int)pin);

    // This pin is an input
    io_conf.mode = GPIO_MODE_INPUT;

    // Since we assume a button-press will short this pin to ground, enable the built-in pullup
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;

    // And configure this GPIO pin
    gpio_config(&io_conf);

    // Find out what the current state of the input pin is
    m_is_pressed = is_pin_active();

    // And right now is the last time we saw the state of the pin change
    m_state_change_time = esp_timer_get_time();
}
//=========================================================================================================



//=========================================================================================================
// button_task() - This runs as a separate task and handles (and debounces) button press messages from ISR.  
//=========================================================================================================
void CButton::button_task()
{
    qentry_t event;

     // Loop forever, waiting for event messages to arrive from the ISR
    while (xQueueReceive(m_button_event_queue, &event, portMAX_DELAY))
    {
        // Debounce for 50 milliseconds
        while (xQueueReceive(m_button_event_queue, &event, 50/portTICK_PERIOD_MS));

        // Now that it has settled, find out the physical state of the button
        bool settled_state = is_pin_active();

        // If it was a <really short> button press (so short that it was ignored as switch-bounce)
        // and we are now seeing the button be released, ignore that too.
        if (!settled_state && !m_is_pressed) continue;

        // Record the "is the button pressed?" state for posterity
        m_is_pressed = settled_state;

        // Find out when the previous state change for this button happened
        int64_t prior_change_time = m_state_change_time;

        // And right now is the last time we saw the state of the pin change
        m_state_change_time = esp_timer_get_time();

        // Find out how many milliseconds the button spent in the prior state
        uint32_t elapsed = (m_state_change_time - prior_change_time) / 1000;

        // If the button is being triggered right now, keep track of that
        if (m_is_pressed == m_trigger_state) m_is_triggered = true;

        // Call the handler for this button
        handler(m_is_pressed, elapsed);
    }
}
//=========================================================================================================


//=========================================================================================================
// is_pin_active() - Returns 'true' if the input pin is active (i.e., grounded)
//=========================================================================================================
bool CButton::is_pin_active()
{
    return (gpio_get_level(m_pin) == 0);
}
//=========================================================================================================


//=========================================================================================================
// is_pressed() - Returns 'true' if the button is pressed, else false
//
// Passed: p_how_long = Optional field that will be filled in with how long the button has been pressed
//=========================================================================================================
bool CButton::is_pressed(uint32_t* p_how_long)
{
    // Fetch the current time (in microseconds)
    int64_t now = esp_timer_get_time();

    // Compute the number of milliseconds that the button has been in this state
    if (p_how_long) *p_how_long = (now - m_state_change_time) / 1000;

    // And tell the caller whether the button is currently pressed
    return m_is_pressed;
}
//=========================================================================================================



//=========================================================================================================
// is_pressed_at_least() - Returns true if the button is currently pressed and has been pressed 
//                         continuously for the specified number of milliseconds
//=========================================================================================================
bool CButton::is_pressed_at_least(uint32_t milliseconds)
{
    // If the button isn't currently pressed, tell the caller
    if (!m_is_pressed) return false;

    // Compute the number of milliseconds that the button has been pressed
    uint32_t elapsed = (esp_timer_get_time() - m_state_change_time) / 1000;

    // Tell the caller whether the button has been pressed for long enough
    return elapsed >= milliseconds;
}
//=========================================================================================================
