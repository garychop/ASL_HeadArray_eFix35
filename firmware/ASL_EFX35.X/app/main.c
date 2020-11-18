//////////////////////////////////////////////////////////////////////////////
//
// Filename: main.c
//
// Description: Main point for the program.
//
// Author(s): Trevor Parsh (Embedded Wizardry, LLC)
//
// Modified for ASL on Date: 
//
//////////////////////////////////////////////////////////////////////////////

/* **************************   Header Files   *************************** */

#include <xc.h>

// NOTE: This must ALWAYS be the first include in a file.
#include "device.h"

// from stdlib
#include <stdint.h>
#include <stdbool.h>
#include "user_assert.h"

// from RTOS
#include "cocoos.h"

// from project
#include "bsp.h"
#include "test_gpio.h"
#include "eeprom_app.h"
#include "head_array.h"
#include "beeper.h"
#include "user_button.h"
#include "general_output_ctrl_app.h"
#include "ha_hhp_interface_app.h"
#include "app_common.h"

#include "beeper_bsp.h"
// Useful, but need all the space we can get.
#if 0
static void TestSetup(void);
#endif

/* *******************   Public Function Definitions   ******************** */

//------------------------------
// Function: main
//
// Description: Main entry point for the program.
//
//-------------------------------
void main(void)
{
    os_init();

    bspInitCore();
	testGpioInit();
	GenOutCtrlApp_Init();

	// Other high level modules depend on EEPROM being initialized, therefore it must be initialized here.
	bool eeprom_initialized_before = eepromAppInit();
	beeperInit();
	userButtonInit();
	headArrayinit();
//	haHhpApp_Init();
    
    // TODO:Take out
#if 0
    TRISEbits.TRISE0 = GPIO_BIT_OUTPUT;         // LED1 control
    TRISEbits.TRISE1 = GPIO_BIT_OUTPUT;         // LED2 control
    TRISEbits.TRISE2 = GPIO_BIT_OUTPUT;         // LED3 control
    TRISCbits.TRISC0 = GPIO_BIT_OUTPUT;         // LED4 control
    TRISAbits.TRISA1 = GPIO_BIT_OUTPUT;         // LED5 control
    while(1)
    {
        LATEbits.LATE0 = GPIO_HIGH;
        LATEbits.LATE1 = GPIO_HIGH;
        LATEbits.LATE2 = GPIO_HIGH;
        LATCbits.LATC0 = GPIO_HIGH;
        LATAbits.LATA1 = GPIO_HIGH;
        LATEbits.LATE0 = GPIO_LOW;
        LATEbits.LATE1 = GPIO_LOW;
        LATEbits.LATE2 = GPIO_LOW;
        LATCbits.LATC0 = GPIO_LOW;
        LATAbits.LATA1 = GPIO_LOW;;
    }
#endif

	// This must come after beeperInit() otherwise the pattern request will be ignored by the beeper module.
	if (!eeprom_initialized_before)
	{
		(void)beeperBeep(BEEPER_PATTERN_EEPROM_NOT_INIT_ON_BOOT);
	}

	AppCommonInit();
	
	// Enable to test system configurations without external control.
#if 0
	TestSetup();
#endif

    // Kick off the RTOS. This will never return.
	// NOTE: Interrupts are enabled by this function
    os_start();
}

// Useful, but need all the space we can get.
#if 0
static void TestSetup(void)
{
	eepromEnumSet(EEPROM_STORED_ITEM_LEFT_PAD_INPUT_TYPE, (EepromStoredEnumType_t)HEAD_ARR_INPUT_DIGITAL);
	eepromEnumSet(EEPROM_STORED_ITEM_RIGHT_PAD_INPUT_TYPE, (EepromStoredEnumType_t)HEAD_ARR_INPUT_DIGITAL);
	eepromEnumSet(EEPROM_STORED_ITEM_CTR_PAD_INPUT_TYPE, (EepromStoredEnumType_t)HEAD_ARR_INPUT_DIGITAL);
//	eepromEnumSet(EEPROM_STORED_ITEM_CURRENT_ACTIVE_FEATURE, (EepromStoredEnumType_t)FUNC_FEATURE_POWER_ON_OFF);
}
#endif

// end of file.
//-------------------------------------------------------------------------
