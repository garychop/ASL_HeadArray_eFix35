//////////////////////////////////////////////////////////////////////////////
//
// Filename: user_button_bsp.c
//
// Description: Handles bsp level user button access.
//
// Author(s): Trevor Parsh (Embedded Wizardry, LLC)
//
// Modified for ASL on Date: 
//
//////////////////////////////////////////////////////////////////////////////


/* **************************   Header Files   *************************** */

// NOTE: This must ALWAYS be the first include in a file.
#include "device.h"

// from stdlib
#include <stdbool.h>

// from project
#include "common.h"
#include "bsp.h"

// from local
#include "user_button_bsp.h"

/* ******************************   Macros   ****************************** */

void SW3_Init (void);
void SW6_Init (void);

void USER_BTN_INIT()
{
//    TRISBbits.TRISB0 = GPIO_BIT_INPUT;  // This must be 
    //ANSELBbits.ANSELB1 = 0;
}

#define MODE_BTN_IS_ACTIVE()	(PORTBbits.RB0 == GPIO_LOW)
void MODE_BTN_INIT()
{
//    TRISCbits.TRISC5 = GPIO_BIT_INPUT;
//    //ANSELCbits.ANSELC5 = 0;
}

/* *******************   Public Function Definitions   ******************** */

//-------------------------------
// Function: ButtonBspInit
//
// Description: Initializes this module.
//
//-------------------------------
void ButtonBspInit(void)
{
    USER_BTN_INIT();
    MODE_BTN_INIT();        // Set up the Port for MODE Button input.
    SW6_Init();
}

//-------------------------------
// Function: userButtonBspIsActive
//
// Description: Reads the digital input state of the user button
// Returns: TRUE if the button is pushed, else false.
//-------------------------------
bool userButtonBspIsActive(void)
{
//    if (PORTBbits.RB0 == GPIO_LOW)
//        return true;
//    else
        return false;
//	return USER_BTN_IS_ACTIVE();
}

//-------------------------------
// Function: ModeButtonBspIsActive
//
// Description: Reads the digital input state of the MODE button
// Returns: TRUE if the button is pushed, else false.
//-------------------------------
bool ModeButtonBspIsActive(void)
{
	return MODE_BTN_IS_ACTIVE();
}

//-------------------------------------------------------------------------
// DIP Switch #6 is on D3
void SW6_Init()
{
    TRISDbits.TRISD3 = GPIO_BIT_INPUT;
}

bool Is_SW6_ON(void)
{
    return (PORTDbits.RD3 == GPIO_LOW);
}

//-------------------------------------------------------------------------
// DIP Switch #3 is on C4
void SW3_Init (void)
{
    // This pin is always an Input. The following code will not comile.
    //TRISCbits.TRISC4 = GPIO_BIT_INPUT;
}

bool Is_SW3_ON (void)
{
    return (PORTCbits.RC4 == GPIO_LOW);
}

// end of file.
//-------------------------------------------------------------------------
