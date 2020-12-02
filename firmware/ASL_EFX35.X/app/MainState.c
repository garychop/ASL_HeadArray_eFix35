/*
	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
 */

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
#include "inc/eFix_Communication.h"
#include "inc/rtos_task_priorities.h"

//------------------------------------------------------------------------------
// Defines and Macros 
//------------------------------------------------------------------------------

#define MAIN_TASK_DELAY (23)        // Number of milliseconds for the main task.

//------------------------------------------------------------------------------
// Local Variables
//------------------------------------------------------------------------------

static void (*MainState)(void);
static int g_StartupDelayCounter;

//------------------------------------------------------------------------------
// Forward Declarations
//------------------------------------------------------------------------------

static void MainTaskInitialise(void);
static void MainTask (void);

// The following are the states
static void Idle_State (void);

// State: Startup_State
//      Stay here until 500 milliseconds lapses then switch to OONAPU_State.
static void Startup_State (void);

// State: OONAPU_State
//      Stay here until No Pads are active then change to Driving State
static void OONAPU_State (void);

// State: Driving_Setup_State
//      Stay here until User Port switch becomes inactive then
//          switch to Driving State.
static void Driving_Setup_State (void);

// State: Driving_State
//      Stay here while reading Pads and send pad info to eFix Task.
//      If user port switch is active then
//          - Send BT beeping sequence.
//          - switch to Bluetooth Setup state.
static void Driving_State (void);

//-------------------------------------------------------------------------
// Main task
//-------------------------------------------------------------------------

void MainTaskInitialise(void)
{
    MainState = Idle_State; // Startup_State;
    g_StartupDelayCounter = 1000 / MAIN_TASK_DELAY;
    
    (void)task_create(MainTask , NULL, MAIN_TASK_PRIO, NULL, 0, 0);
}

//-------------------------------------------------------------------------
// Function: MainTask
// Description: This is the main task that controls everything.
//-------------------------------------------------------------------------

static void MainTask (void)
{
   
    task_open();

    while (1)
	{
        MainState();

        task_wait(MILLISECONDS_TO_TICKS(MAIN_TASK_DELAY));

    }
    
    task_close();
}

//-------------------------------------------------------------------------
static void Idle_State (void)
{
    ++g_StartupDelayCounter;
}

//-------------------------------------------------------------------------
// State: Startup_State
//      Stay here until 500 milliseconds lapses then switch to OONAPU_State.
//-------------------------------------------------------------------------
static void Startup_State (void)
{
    // If we need to startup differently, this is where you can do it.
    MainState = OONAPU_State;       // Go check if any pads are active.
}

//-------------------------------------------------------------------------
// State: OONAPU_State
//      Stay here until No Pads are active then change to Driving State
//-------------------------------------------------------------------------
static void OONAPU_State (void)
{
    if (PadsInNeutralState())      // Nope we are not in neutral
    {
        // Guard against a painfully bug where we in Out-of-neutral state.
        if (g_StartupDelayCounter > 500 / MAIN_TASK_DELAY)
            g_StartupDelayCounter = 500 / MAIN_TASK_DELAY;
        
        if (--g_StartupDelayCounter == 0) // Have we waited long enough.
        {
            MainState = Driving_Setup_State;
        }
    }    
}

//-------------------------------------------------------------------------
// State: Driving_Setup_State
//      Stay here until User Port switch becomes inactive then
//          switch to Driving State.
//-------------------------------------------------------------------------
static void Driving_Setup_State (void)
{
    MainState = Driving_State;
}

//-------------------------------------------------------------------------
// State: Driving_State
//      Stay here while reading Pads and send pad info to eFix Task.
//      If user port switch is active then
//          - Send BT beeping sequence.
//          - switch to Bluetooth Setup state.
//-------------------------------------------------------------------------
static void Driving_State (void)
{
    ++g_StartupDelayCounter; // just having it do something.
    
}

