//=========================================================================================================
// parser.h - Defines a parser that extracts tokens form a string, one at a time
//=========================================================================================================
#pragma once


// An array of this is passed into parse attributes()
struct attribute_t {const char* tag; int type; void* addr;};

// These are the possible values of the 'type' field in parse_attr_t
enum
{
    ATTR_STRING = 0,
    ATTR_INT    = 1
};


class CParser 
{
public:

    // Call this to point to the string to be parsed
    void    set_input(const char* input_ptr);

    // Call this repeatedly to fetch tokens
    bool    get_next_token(char* out, int buff_len = 1000);

    // Parses every token and parses the values of named attributes
    bool    get_attributes(attribute_t* p_array, int count);

    // Returns the pointer to the next token to be parsed
    const char*  get_ptr() {return m_ptr;}

protected:

    // This points to the next input character
    const char*  m_ptr;

};