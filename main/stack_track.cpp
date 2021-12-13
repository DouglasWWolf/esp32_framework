//=========================================================================================================
// stack_track.cpp - Implements a mechanism for keeping track of the number of free bytes remaining on
//                   the stack for a task at its high-water mark
//=========================================================================================================
#include "common.h"
#include "stack_track.h"

static int high_water_mark[TASK_IDX_COUNT];

//=========================================================================================================
// Constructor() - Initializes the high-water-mark values to a known dummy value
//=========================================================================================================
CStackTrack::CStackTrack()
{
    for (int i=0; i< TASK_IDX_COUNT; ++i) high_water_mark[i] = -9999;
}
//=========================================================================================================


//=========================================================================================================
// record_hwm() - Records the number of bytes remaining unused in this tasks stack
//=========================================================================================================
void CStackTrack::record_hwm(task_idx_t idx)
{
    high_water_mark[idx] = (int)uxTaskGetStackHighWaterMark(nullptr);
}
//=========================================================================================================


//=========================================================================================================
// remaining() - Returns the number of bytes free on the stack for the specified task
//=========================================================================================================
int CStackTrack::remaining(task_idx_t idx) {return high_water_mark[idx];}
//=========================================================================================================


//=========================================================================================================
// name() - Returns the displayable name of a task
//=========================================================================================================
const char* CStackTrack::name(task_idx_t idx)
{
    switch(idx)
    {
        case TASK_IDX_MAIN        : return "main";
        case TASK_IDX_PROV_BUTTON : return "prov";
        case TASK_IDX_TCP_SERVER  : return "tcp";
        default                   : break;
    }
    return "unknown";
}
//=========================================================================================================

