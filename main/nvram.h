//=========================================================================================================
// nvram.h - Defines a structure in RAM that survives reboots
//=========================================================================================================
#pragma once


class CNVRAM
{
public:
    
    // Constructor is responsible for initializing every variable in this class
    CNVRAM();
    
    // This will be true if Wi-Fi should start in access-point mode
    bool    start_wifi_ap;

protected:

    // This will contain a "magic string" if this object has already been initialized
    char    key[12];
};

extern CNVRAM NVRAM;


