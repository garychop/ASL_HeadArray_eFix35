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

// from RTOS
#include "cocoos.h"

// from stdlib
#include <stdint.h>
#include <stdbool.h>
#include "user_assert.h"

// from project
//#include "common.h"
#include "bsp.h"
#include "test_gpio.h"
#include "eeprom_app.h"
#include "head_array.h"
#include "beeper.h"
#include "user_button_bsp.h"
#include "user_button.h"
#include "general_output_ctrl_app.h"
#include "ha_hhp_interface_app.h"
#include "app_common.h"

#include "beeper_bsp.h"
#include "bluetooth_simple_if_bsp.h"
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
static int g_SwitchDelay;
static uint8_t g_ExeternalSwitchStatus;
static BeepPattern_t g_BeepPattern = BEEPER_PATTERN_EOL;

//------------------------------------------------------------------------------
// Forward Declarations
//------------------------------------------------------------------------------

static void MainTaskInitialise(void);
static void MainTask (void);
static void MirrorDigitalInputOnBluetoothOutput(void);

// The following are the states
static void Idle_State (void);

// State: Startup_State
//      Stay here until 500 milliseconds lapses then switch to OONAPU_State.
static void Startup_State (void);

// State: OONAPU_State
//      Stay here until No Pads are active then change to Driving State
static void OONAPU_Setup_State (void);
static void OONAPU_State (void);

static void Annunciate_DrivingReady_State (void);
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
//static void Driving_UserSwitchActivated (void);
static void Driving_UserSwitch_State (void);
static void Driving_Idle_State (void);

// State: BluetoothSetup_State
//      Stay here and user port switch becomes inactive then
//          - Switch to DoBluetooth_State
static void BluetoothSetup_State (void);

// State: DoBluetooth_State
//      Stay here and send active pad info to Bluetooth module.
//      If user port switch is active then
//          - Beep to annunciate driving.
//          - Switch to Driving_Setup_State
static void DoBluetooth_State (void);

//-------------------------------------------------------------------------
// Main task
//-------------------------------------------------------------------------

void MainTaskInitialise(void)
{
    g_BeepPattern = BEEPER_PATTERN_EOL;
    g_StartupDelayCounter = 1000 / MAIN_TASK_DELAY;
    
    MainState = Startup_State;

    (void)task_create(MainTask , NULL, MAIN_TASK_PRIO, NULL, 0, 0);
}

//-------------------------------------------------------------------------
// Function: MainTask
// Description: This is the main task that controls everything.
//-------------------------------------------------------------------------

static void MainTask (void)
{
    Evt_t event_to_send_beeper_task;
   
    task_open();

    while (1)
	{
        // Get the User and Mode port switch status all of the time.
        g_ExeternalSwitchStatus = GetSwitchStatus();
        
        MainState();

        task_wait(MILLISECONDS_TO_TICKS(MAIN_TASK_DELAY));

        if (g_BeepPattern != BEEPER_PATTERN_EOL)
        {
            event_to_send_beeper_task = beeperBeep(g_BeepPattern);
            if (event_to_send_beeper_task != NO_EVENT)
            {
                event_signal(event_to_send_beeper_task);
            }
            g_BeepPattern = BEEPER_PATTERN_EOL;
         }
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
// Description: Stay here until 500 milliseconds lapses then switch to OONAPU_State.
//-------------------------------------------------------------------------
static void Startup_State (void)
{
    // If we need to startup differently, this is where you can do it.
    if (g_StartupDelayCounter > (500 / MAIN_TASK_DELAY))
        g_StartupDelayCounter = (500 / MAIN_TASK_DELAY);

    if (--g_StartupDelayCounter < 1) // Have we waited long enough.
    {
        g_StartupDelayCounter = (500 / MAIN_TASK_DELAY);
        MainState = OONAPU_State;
    }
}

static void OONAPU_Setup_State (void)
{
    g_StartupDelayCounter = (500 / MAIN_TASK_DELAY);
    
    MainState = OONAPU_State;
    
}
//-------------------------------------------------------------------------
// State: OONAPU_State (Out-Of-Neutral-At-Power-Up acronym)
// Description: Stay here until No Pads are active then change to Driving State
//-------------------------------------------------------------------------
static void OONAPU_State (void)
{
    if (PadsInNeutralState())      // Yep, we are in neutral
    {
        // Guard against a painfully long time out bug where we in Out-of-neutral state.
        if (g_StartupDelayCounter > (500 / MAIN_TASK_DELAY))
            g_StartupDelayCounter = (500 / MAIN_TASK_DELAY);
        
        if (--g_StartupDelayCounter < 1) // Have we waited long enough.
        {
            MainState = Annunciate_DrivingReady_State;
        }
    }
    else
    {
        g_StartupDelayCounter = (500 / MAIN_TASK_DELAY);  // reset the counter.
    }
}

static void Annunciate_DrivingReady_State (void)
{
    MainState = Driving_Setup_State;
}

//-------------------------------------------------------------------------
// State: Driving_Setup_State
// Description: Stay here until User Port switch becomes inactive then
//          switch to Driving State.
//-------------------------------------------------------------------------
static void Driving_Setup_State (void)
{
    // Check the user port for go inactive.
    // When it does, go to the Driving State.
    if ((g_ExeternalSwitchStatus & USER_SWITCH) == false)
    {
         g_BeepPattern = ANNOUNCE_POWER_ON;
        // Set to Blue tooth state
        MainState = Driving_State;
    }
}

//-------------------------------------------------------------------------
// State: Driving_State
// Description: 
//      Stay here while reading Pads and sending pad info to eFix Task.
//      If user port switch is active then
//          - Send BT beeping sequence.
//          - switch to Bluetooth Setup state.
//-------------------------------------------------------------------------
static void Driving_State (void)
{
    int speedPercentage = 0, directionPercentage = 0;

    if (!PadsInNeutralState())      // Nope we are not in neutral
    {
        // Determine which is active and set the output accordingly.
        // Note that the Left/Right override is performed at the lower level.
        if (headArrayDigitalInputValue(HEAD_ARRAY_SENSOR_LEFT)) // Is Left pad active?
        {
            directionPercentage = -100;
        }
        else if (headArrayDigitalInputValue(HEAD_ARRAY_SENSOR_RIGHT)) // Is right pad active?
        {
            directionPercentage = 100;
        }
        else if (headArrayDigitalInputValue(HEAD_ARRAY_SENSOR_CENTER)) // Is center pad active?
        {
            speedPercentage = 100;
        }
        else if (headArrayDigitalInputValue(HEAD_ARRAY_SENSOR_BACK)) // Is 4th back pad active?
        {
            speedPercentage = -100;
        }
    }
    SetSpeedAndDirection (speedPercentage, directionPercentage);
    
    // Check the user port for active... If so, change to Bluetooth state.
    if (g_ExeternalSwitchStatus & USER_SWITCH)
    {
        SetSpeedAndDirection (0, 0);
        
        g_SwitchDelay = 3000 / MAIN_TASK_DELAY;
        
        MainState = Driving_UserSwitch_State;
    }
}

//-------------------------------------------------------------------------
//static void Driving_UserSwitchActivated (void)
//{
//    g_SwitchDelay = 3000 / MAIN_TASK_DELAY;
//    
//    MainState = Driving_UserSwitch_State;
//}

//-------------------------------------------------------------------------
static void Driving_UserSwitch_State(void)
{
    if (g_ExeternalSwitchStatus & USER_SWITCH)
    {
        if (g_SwitchDelay != 0)      // Sanity check.
        {
            --g_SwitchDelay;
        }
        // Did we wait long enough to switch to Bluetooth
        if (g_SwitchDelay == 0)
        {
            g_BeepPattern = ANNOUNCE_BLUETOOTH;
            MainState = BluetoothSetup_State;
        }
    }
    else
    {
        MainState = Driving_Idle_State;
    }
}

//-------------------------------------------------------------------------
// State: Driving_Idle_State
//      Remain here until the User Switch goes active then we are going
//      enable driving, but first, we are doing a OON test.
//-------------------------------------------------------------------------
static void Driving_Idle_State (void)
{
    if (g_ExeternalSwitchStatus & USER_SWITCH)
    {
        MainState = OONAPU_Setup_State;
    }
}

//-------------------------------------------------------------------------
// State: BluetoothSetup_State
//      Remain here until the User Switch goes inactive.
//          - Beep to annunciate driving.
//          - Switch to Driving_Setup_State
//-------------------------------------------------------------------------
static void BluetoothSetup_State (void)
{
    
    // Check the user port for go inactive.
    // If so, change to doing the Bluetooth state
    if ((g_ExeternalSwitchStatus & USER_SWITCH) == false)
    {
        
        // Set to Blue tooth state
        MainState = DoBluetooth_State;
    }
}

//-------------------------------------------------------------------------
// State: DoBluetooth
//      Stay here and send active pad info to Bluetooth module.
//      If user port switch is active then
//          - Beep to annunciate driving.
//          - Switch to Driving_Setup_State
//-------------------------------------------------------------------------
static void DoBluetooth_State (void)
{
    
    // Check the user port for active... If so, change to Setting up the
    // driving state.
    if (g_ExeternalSwitchStatus & USER_SWITCH)
    {
        
        // Set to Blue tooth state
        g_StartupDelayCounter = (500 / MAIN_TASK_DELAY);
        MainState = OONAPU_Setup_State;
    }

    MirrorDigitalInputOnBluetoothOutput();

}

//------------------------------------------------------------------------------
// Function: MirrorDigitalInputOnBluetoothOutput
//
// Description: Mirrors digital pad inputs on Bluetooth digital output lines. No mapping, just a one-to-one map.
//
//------------------------------------------------------------------------------
static void MirrorDigitalInputOnBluetoothOutput(void)
{
	for (int i = 0; i < (int) HEAD_ARRAY_SENSOR_EOL; i++)
	{
		bluetoothSimpleIfBspPadMirrorStateSet((HeadArraySensor_t)i, headArrayDigitalInputValue(i));
	}
}



