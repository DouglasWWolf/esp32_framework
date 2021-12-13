//=========================================================================================================
// parser.cpp - Implements a parser that extracts tokens form a string, one at a time
//
// Extracted strings will be converted to lower case unless surrounded by single or double quotes.
//
// <space> is considered a token separator unless the string is surrounded by single or double quotes
//=========================================================================================================
#include "parser.h"
#include <string.h>
#include <stdlib.h>


//=========================================================================================================
// set_input() - Records the pointer to the string to be parsed
//=========================================================================================================
void CParser::set_input(const char* input_ptr)
{
    // Save the pointer to the input buffer for future reference
    m_ptr = input_ptr;
}
//=========================================================================================================


//=========================================================================================================
// get_next_word() - Fetches the next token from the input string
//
// Passed: Pointer to buffer where the token should be stored
//
// Returns: 'true' if a token was available, otherwise false
//=========================================================================================================
bool  CParser::get_next_token(char* out, int buff_len)
{
    int end_of_token = ' ';

    // Leave room in the buffer for the terminating null-byte
    --buff_len;

    // nul terminate the output buffer
    *out = 0;

    // Skip over leading spaces in the input
    while (*m_ptr == ' ') ++m_ptr;

    // If there is no more input, tell the caller
    if (*m_ptr == 0 || *m_ptr == 10 || *m_ptr == 13) return false;

    // If we are pointed at the start of a quoted string, 
    // keep track of what character ends the string
    if (*m_ptr == '\'' || *m_ptr == '\"') end_of_token = *m_ptr++;

    while (true)
    {
        // Fetch the next character
        int c = *m_ptr++;

        // Have we just hit the end of this token?
        if (c == end_of_token) break;
 
        // Have we just hit the terminating nul-byte of the input string?
        // If so, make sure that m_ptr points to the terminting nul so that
        // we don't run off the end of the string the next time this routine
        // gets called.
        if (c == 0 || c== 10 || c == 13) 
        {
            --m_ptr;      
            break;
        }

        // If we're not in a quoted string, force this character to lower case
        if (end_of_token == ' ' && c >= 'A' && c <= 'Z') c += 32;

        // Output this character to the token buffer
        if (buff_len)
        {
            *out++ = c;
            --buff_len;
        }
    }

    // nul-terminate the caller's output buffer
    *out = 0;

    // And tell the caller that he has a token to process
    return true;
}
//=========================================================================================================



//=========================================================================================================
// get_attributes - Parses a series of attributes into values.  An attribute looks like: "name:value"
//=========================================================================================================
bool CParser::get_attributes(attribute_t* p_array, int count)
{
    char token[256];
    const char* value;

    // Presume for the moment that every token will contain a recognized attribute
    bool result = true;

    // Parse every token...
    while (get_next_token(token))
    {
        // Assume for a moment that this token doesn't contain a recognized attribute
        attribute_t *p_attr = nullptr;

        // Loop through each possible attribute...
        for (int i=0; i<count; ++i)
        {
            // Fetch the name of this attribute
            const char* tag = p_array[i].tag;
         
            // Find out how long the the name of this attribute is
            int attr_len = strlen(tag);

            // If this token contains this attribute, make note of it
            if (strncmp(token, tag, attr_len) == 0) 
            {
                p_attr = p_array + i;
                value  = token + attr_len;
                break;
            }
        }

        // If this token contained an unrecognized attribute, ignore it
        if (p_attr == nullptr)
        {
            result = false;
            continue;
        }

        // Where are we supposed to store the value of this attribute?
        void* destination = p_attr->addr;

        // If the destination is nullptr, it means the caller wants to ignore this attribute
        if (destination == nullptr) continue;

        // Store the value of this attribute into the place specified by the caller
        if (p_attr->type == ATTR_STRING)
            strcpy((char*)destination, value);
        else
            *(int*)destination = atoi(value);
    }

    // When we get here, we've processed all of the tokens.  Tell the caller whether or
    // not every token we encountered contained a recognized attribute
    return result;
}
//=========================================================================================================
