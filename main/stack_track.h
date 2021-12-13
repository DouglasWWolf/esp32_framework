//=========================================================================================================
// stack_track.h - Defines a mechanism for keeping track of the number of free bytes remaining on
//                 the stack for a task at its high-water mark
//=========================================================================================================
#pragma once

enum task_idx_t
{
    TASK_IDX_MAIN = 0,
    TASK_IDX_PROV_BUTTON,
    TASK_IDX_TCP_SERVER,
    TASK_IDX_COUNT
};


class CStackTrack
{
public:
    
    // Constructor
    CStackTrack();

    // Call this to record the high-water mark for a task
    void        record_hwm(task_idx_t idx);
    
    // Call this to fetch a string that describes the specified task
    const char* name(task_idx_t idx);

    // Call this to find the number of free bytes remaining on a task's stack
    int         remaining(task_idx_t idx);

};

