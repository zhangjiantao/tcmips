#include "wl_def.h"

//*****************************************************************************
//
// The sequence of commands used to initialize the SSD0323 controller.  Each
// command is described as follows:  there is a byte specifying the number of
// bytes in the command sequence, followed by that many bytes of command data.
// Note:  This initialization sequence is derived from OSRAM App Note AN018.
//
//*****************************************************************************

#define OSRAM_INIT_REMAP    0x52
#define OSRAM_INIT_OFFSET   0x4C

const int oled_height = 64;

const uint8_t oled_init_strings[] =
{
    //
    // Column Address
    //
    4, 0x15, 0, 63, 0xe3,

    //
    // Row Address
    //
    4, 0x75, 0, 63, 0xe3,

    //
    // Contrast Control
    //
    3, 0x81, 50, 0xe3,

    //
    // Half Current Range
    //
    2, 0x85, 0xe3,

    //
    // Display Re-map
    //
    3, 0xA0, OSRAM_INIT_REMAP, 0xe3,

    //
    // Display Start Line
    //
    3, 0xA1, 0, 0xe3,

    //
    // Display Offset
    //
    3, 0xA2, OSRAM_INIT_OFFSET, 0xe3,

    //
    // Display Mode Normal
    //
    2, 0xA4, 0xe3,

    //
    // Multiplex Ratio
    //
    3, 0xA8, 63, 0xe3,

    //
    // Phase Length
    //
    3, 0xB1, 0x22, 0xe3,

    //
    // Row Period
    //
    3, 0xB2, 70, 0xe3,

    //
    // Display Clock Divide
    //
    3, 0xB3, 0xF1, 0xe3,

    //
    // VSL
    //
    3, 0xBF, 0x0D, 0xe3,

    //
    // VCOMH
    //
    3, 0xBE, 0x02, 0xe3,

    //
    // VP
    //
    3, 0xBC, 0x10, 0xe3,

    //
    // Gamma
    //
    10, 0xB8, 0x01, 0x11, 0x22, 0x32, 0x43, 0x54, 0x65, 0x76, 0xe3,

    //
    // Set DC-DC
    3, 0xAD, 0x03, 0xe3,

    //
    // Display ON/OFF
    //
    2, 0xAF, 0xe3,
    // Done.
    0
};


