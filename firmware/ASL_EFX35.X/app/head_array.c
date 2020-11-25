//////////////////////////////////////////////////////////////////////////////
//
// Filename: head_array.c
//
// Description: Core head array control and feedback.
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
#include <stdint.h>
#include <stdbool.h>
#include "user_assert.h"

// from RTOS
#include "cocoos.h"

// from project
#include "rtos_task_priorities.h"
#include "config.h"
#include "common.h"
#include "bsp.h"
#include "stopwatch.h"
#include "bluetooth_simple_if_bsp.h"
#include "eeprom_app.h"
//#include "dac_bsp.h"
#include "general_output_ctrl_app.h"
#include "app_common.h"

// from local
#include "head_array_bsp.h"
#include "head_array.h"

/* ******************************   Macros   ****************************** */

// The values below are based on empherical data gathered from a single rev3 board head array.
// The spec is: Vref/Neutral must be 5V <= 6V <= 7V
// 				The mix/max must be +/- 1.2V from Vref/Neutral values.
// NOTE: Vref refers to the voltage reference supplied to the DB9 connector that goes to the IN500
// NOTE: module. Also, the Vref and DAC output for neutral state must be VERY close to the same voltage.
// NOTE: The exact specs for "how close" is not known.
//OBSOLETED in EEPROM Version 2
//#define DAC_NEUTRAL_DAC_VAL 					((uint16_t)2009)    // Without IC1 which reduces the offset.
//#define DAC_NEUTRAL_DAC_VAL 					((uint16_t)2270)    // With IC1 which increases the offset.
//#define DAC_UPPER_AND_BOTTOM_RAIL_DIFFERENTIAL	((uint16_t)410)

// This is the portion of output control that proportional has when the output control for a pad
// is set to proportional.
//#define DAC_UPPER_AND_BOTTOM_RAIL_DIFFERENTIAL_80_PERCENT	((DAC_UPPER_AND_BOTTOM_RAIL_DIFFERENTIAL * (uint16_t)8) / (uint16_t)10)
//#define DAC_UPPER_AND_BOTTOM_RAIL_DIFFERENTIAL_20_PERCENT	((DAC_UPPER_AND_BOTTOM_RAIL_DIFFERENTIAL * (uint16_t)2) / (uint16_t)10)

//#define DAC_LOWER_RAIL							(DAC_NEUTRAL_DAC_VAL - DAC_UPPER_AND_BOTTOM_RAIL_DIFFERENTIAL)
//#define DAC_UPPER_RAIL							(DAC_NEUTRAL_DAC_VAL + DAC_UPPER_AND_BOTTOM_RAIL_DIFFERENTIAL)

//#define DAC_LEFT_MAX_DAC_VAL 					DAC_LOWER_RAIL
//#define DAC_RIGHT_MAX_DAC_VAL 				DAC_UPPER_RAIL
//#define DAC_FWD_MAX_DAC_VAL 					DAC_UPPER_RAIL
//#define DAC_REV_MAX_DAC_VAL 					DAC_LOWER_RAIL

// Reducing these items their equivalent value. It's either a +1 or -1.
// Left = LOWER_RAIL =  -1
// Right = UPPER RAIL = 1
// Forward = UPPER RAIL = 1
// Reverse = LOWER RAIL = -1
//#define DAC_LEFT_DAC_VAL_MANIP_DIR (-1)     // ((DAC_LEFT_MAX_DAC_VAL > DAC_NEUTRAL_DAC_VAL) 	? ((int8_t)1) : ((int8_t)-1))
//#define DAC_RIGHT_DAC_VAL_MANIP_DIR (1)     // ((DAC_RIGHT_MAX_DAC_VAL > DAC_NEUTRAL_DAC_VAL) 	? ((int8_t)1) : ((int8_t)-1))
//#define DAC_FWD_DAC_VAL_MANIP_DIR (1)		//	((DAC_FWD_MAX_DAC_VAL > DAC_NEUTRAL_DAC_VAL) 	? ((int8_t)1) : ((int8_t)-1))
//#define DAC_REV_DAC_VAL_MANIP_DIR (-1)		//	((DAC_REV_MAX_DAC_VAL > DAC_NEUTRAL_DAC_VAL) 	? ((int8_t)1) : ((int8_t)-1))
//
//#define PROP_UNINITIALIZED_VAL					(0)

// Time to wait before claiming the system is in a neutral control state.
#define NEUTRAL_STATE_MIN_TIME_TO_CHECK_ms 		(750)   // 2 seconds is too long.

/* ***********************   File Scope Variables   *********************** */

// Type of input, or input disabled, function for each pad.
// Default values are set in eeprom_app.c
//static volatile HeadArrayInputType_t head_arr_input_type[(int)HEAD_ARRAY_SENSOR_EOL];

// Each index refers to the input pad, and the value at an index refers to the output pad.
// Default values are set in eeprom_app.c
//static volatile HeadArrayOutputFunction_t input_pad_to_output_pad_map[(int)HEAD_ARRAY_SENSOR_EOL];

static volatile FunctionalFeature_t curr_active_feature;

struct 
{
    bool m_CurrentPadStatus;
    bool m_PreviousPadStatus;
} g_PadInfo[HEAD_ARRAY_SENSOR_EOL];

// Last recorded values for digital and proportional input states.
//static volatile bool pad_dig_state[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t pad_prop_state[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t pad_raw_prop_state[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t pad_adc_min_thresh_val[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t pad_adc_max_thresh_val[(int)HEAD_ARRAY_SENSOR_EOL];
//
//static volatile uint16_t pad_min_adc_val[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t pad_max_adc_val[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t pad_min_thresh_perc[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t pad_max_thresh_perc[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t pad_MinDriveSpeed[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t DAC_Proportional_percent[(int)HEAD_ARRAY_SENSOR_EOL];
//static volatile uint16_t DAC_Minimum_percent[(int)HEAD_ARRAY_SENSOR_EOL];
//
//static volatile uint16_t neutral_DAC_counts;        // This holds the DAC counts constant, used as the center of the DAC output.
//static volatile uint16_t neutral_DAC_setting;       // The DAC counts used.
//static volatile uint16_t neutral_DAC_range;         // This is the allowable range of the voltage swing in counts.
//static volatile uint16_t DAC_lower_rail;
//static volatile uint16_t DAC_upper_rail;
//static volatile uint16_t DAC_max_forward_counts;
//static volatile uint16_t DAC_max_left_counts;
//static volatile uint16_t DAC_max_right_counts;
//static volatile uint16_t DAC_max_reverse_counts;
//static volatile uint16_t DAC_Proportional_percent;
//static volatile uint16_t DAC_Minimum_percent;

// Must match exactly with the ordering in HeadArrayOutputFunction_t.
//static const int8_t dac_output_manip_dir[(int)HEAD_ARRAY_OUT_FUNC_EOL] =
//{
//	DAC_LEFT_DAC_VAL_MANIP_DIR, DAC_RIGHT_DAC_VAL_MANIP_DIR, DAC_FWD_DAC_VAL_MANIP_DIR, DAC_REV_DAC_VAL_MANIP_DIR
//};

static volatile bool g_WaitForNeutral = false;
static volatile bool neutral_test_fail = false;

/* ***********************   Function Prototypes   ************************ */

static void HeadArrayInputControlTask(void);
static void CheckInputs(void);
static bool SetOutputs(void);

static bool SendStateRequestToLedControlModule(void);
//static void MirrorUpdateDigitalInputValues(void);
static void UpdatePadStatus(void);
//static void MirrorUpdateProportionalInputValues(void);
static void MirrorDigitalInputOnBluetoothOutput(void);
//static uint16_t ConvertPropInToOutValue(uint8_t sensor_id);

static bool InNeutralState(void);
static bool SyncWithEeprom(void);
//static void RefreshLimits(void);

#if defined(TEST_BASIC_DAC_CONTROL)
	static void TestBasicDacControl(void);
#endif

/* *******************   Public Function Definitions   ******************** */

//-------------------------------
// Function: headArrayinit
//
// Description: Initializes this module.
//
//-------------------------------
void headArrayinit(void)
{
	// Initialize other data
	for (int i = 0; i < (int)HEAD_ARRAY_SENSOR_EOL; i++)
	{
//		pad_dig_state[i] = false;
//		pad_prop_state[i] = PROP_UNINITIALIZED_VAL;
//		pad_raw_prop_state[i] = PROP_UNINITIALIZED_VAL;
        g_PadInfo[i].m_CurrentPadStatus = false;
        g_PadInfo[i].m_PreviousPadStatus = false;
	}

    
	// Initialize all submodules controlled by this module.
	headArrayBspInit();
	bluetoothSimpleIfBspInit();
	
	// Fetch initial values from the EEPROM
	(void)SyncWithEeprom();

    (void)task_create(HeadArrayInputControlTask, NULL, HEAD_ARR_MGMT_TASK_PRIO, NULL, 0, 0);
}

//-------------------------------
// Function: headArrayOutputValue
//
// Description: Returns the value (digital or ADC value of proportional depending on setting) of a proportional sensor input from the last reading.
//		For digital, the value will either be 0 or whatever the max ADC value is, depending on the active/inactive state.
//
//-------------------------------
//uint16_t headArrayOutputValue(HeadArrayOutputAxis_t axis_id)
//{
//	int16_t out_val = neutral_DAC_setting;

//	if (axis_id == HEAD_ARRAY_OUT_AXIS_LEFT_RIGHT)
//	{        
//		for (int sensor_id = 0; sensor_id < (int)HEAD_ARRAY_SENSOR_EOL; sensor_id++)
//		{
//			// If pad is not connected, don't bother taking it into account.
//			if (headArrayPadIsConnected((HeadArraySensor_t)sensor_id))
//			{
//				if ((input_pad_to_output_pad_map[sensor_id] == HEAD_ARRAY_OUT_FUNC_LEFT) ||
//					(input_pad_to_output_pad_map[sensor_id] == HEAD_ARRAY_OUT_FUNC_RIGHT))
//				{
//                    // Process RNet_SEATING feature here. If it's RNet_SEATING then
//                    // .. force a digital implementation ONLY.
//                    if (appCommonGetCurrentFeature() == FUNC_FEATURE_RNET_SEATING)
//                    {
//						// Only care about an input sensor affecting output if it is active.
//						if (headArrayDigitalInputValue((HeadArraySensor_t)sensor_id))
//						{
//							out_val += (int16_t)dac_output_manip_dir[(int)input_pad_to_output_pad_map[sensor_id]] * (int16_t)neutral_DAC_range;
//						}
//                    }
//					else if (head_arr_input_type[sensor_id] == HEAD_ARR_INPUT_DIGITAL)
//					{
//						// Only care about an input sensor affecting output if it is active.
//						if (headArrayDigitalInputValue((HeadArraySensor_t)sensor_id))
//						{
//							out_val += (int16_t)dac_output_manip_dir[(int)input_pad_to_output_pad_map[sensor_id]] * (int16_t)neutral_DAC_range;
//						}
//					}
//					else if (head_arr_input_type[sensor_id] == HEAD_ARR_INPUT_PROPORTIONAL)
//					{
//						// In order for proportional input to be considered, we first make sure that the corresponding digital sensor is active
//						// and also make sure that the minimum threshold required to have the prop signal active is met.
//						if (headArrayDigitalInputValue((HeadArraySensor_t)sensor_id))
//						{
//							out_val += (int16_t)dac_output_manip_dir[(int)input_pad_to_output_pad_map[sensor_id]] *
//									   (int16_t)ConvertPropInToOutValue(sensor_id);
//						}
//					}
//					else // Should never happen
//					{
//						// Input is of no care.
//						(void)0;
//					}
//				}
//				else
//				{
//					// It is either, none, forward, or backwards. Don't care.
//					(void)0;
//				}
//			}
//		}
//	}
//	else // HEAD_ARRAY_OUT_AXIS_FWD_REV
//	{
//		for (int sensor_id = 0; sensor_id < (int)HEAD_ARRAY_SENSOR_EOL; sensor_id++)
//		{
//			// If pad is not connected, don't bother taking it into account.
//			if (headArrayPadIsConnected((HeadArraySensor_t)sensor_id))
//			{
//				if ((input_pad_to_output_pad_map[sensor_id] == HEAD_ARRAY_OUT_FUNC_FWD) ||
//					(input_pad_to_output_pad_map[sensor_id] == HEAD_ARRAY_OUT_FUNC_REV))
//				{
//                    // Process RNet_SEATING feature here. If it's RNet_SEATING then
//                    // .. force the Forward/Reverse drive demand to be Neutral.
//                    if (appCommonGetCurrentFeature() == FUNC_FEATURE_RNET_SEATING)
//                    {
//						// out_val = neutral_DAC_setting;
//                    }
//					else if (head_arr_input_type[sensor_id] == HEAD_ARR_INPUT_DIGITAL)
//					{
//						// Only care about an input sensor affecting output if it is active.
//						if (headArrayDigitalInputValue((HeadArraySensor_t)sensor_id))
//						{
//							out_val += (int16_t)dac_output_manip_dir[(int)input_pad_to_output_pad_map[sensor_id]] * (int16_t)neutral_DAC_range;
//						}
//					}
//					else if (head_arr_input_type[sensor_id] == HEAD_ARR_INPUT_PROPORTIONAL)
//					{
//						// In order for proportional input to be considered, we first make sure that the corresponding digital sensor is active
//						// and also make sure that the minimum threshold required to have the prop signal active is met.
//						if (headArrayDigitalInputValue((HeadArraySensor_t)sensor_id))
//						{
//							out_val += (int16_t)dac_output_manip_dir[(int)input_pad_to_output_pad_map[sensor_id]] *
//									   (int16_t)ConvertPropInToOutValue(sensor_id);
//						}
//					}
//					else // Should never happen
//					{
//						// Input is of no care.
//						(void)0;
//					}
//				}
//				else
//				{
//					// It is either, none, left, or right. Don't care.
//					(void)0;
//				}
//			}
//		}
//        // Check to see if any FWD/REV pad is active. If not, see if the
//        // the Mode Reverse feature is active and if the mode switch is active.
//        // We will issue a neutral demand if both are active.
//        if (eeprom8bitGet(EEPROM_STORED_ITEM_ENABLED_FEATURES_2) & FUNC_FEATURE2_MODE_REVERSE_BIT_MASK)
//        {
//            if (out_val == neutral_DAC_setting) // Is a forward or reverse pad active?
//            {
//                if (IsModeSwitchActive())       // Is the Mode switch active
//                {
//					out_val += (int16_t)dac_output_manip_dir[HEAD_ARRAY_OUT_FUNC_REV] * (int16_t)neutral_DAC_range;
//                }
//            }
//            else // We have a forward or reverse demand
//            {
//                if (IsModeSwitchActive())       // Is the Mode switch active?
//                	out_val = neutral_DAC_setting; // Issue a neutral demand if multiple pads are active.
//            }
//        }
//	}
//
//	if (out_val < DAC_lower_rail)
//	{
//		out_val = DAC_lower_rail;
//	}
//	else if (out_val > DAC_upper_rail)
//	{
//		out_val = DAC_upper_rail;
//	}
//	else
//	{
//		// Nothing to do. The value is already in an acceptable range.
//		(void)0;
//	}
//	
//	return out_val;
//}

//-------------------------------
// Function: headArrayDigitalInputValue
//
// Description: Returns the value of a digital sensor input from the last reading.
//
// NOTE: Do not use this for control or reporting related features that require taking into account
// NOTE: input->output mapping.
//
//-------------------------------
bool headArrayDigitalInputValue(HeadArraySensor_t sensor)
{
	return g_PadInfo[sensor].m_CurrentPadStatus;
}

//-------------------------------
// Function: headArrayProportionalInputValueRaw
//
// Description: Returns the value of a raw proportional sensor input from the last reading.
//
// NOTE: Do not use this for control or reporting related features that require taking into account
// NOTE: input->output mapping.
//
//-------------------------------
//uint16_t headArrayProportionalInputValueRaw(HeadArraySensor_t sensor)
//{
//	return pad_raw_prop_state[(int)sensor];
//}

//-------------------------------
// Function: headArrayProportionalInputValue
//
// Description: Returns the value of a proportional sensor input from the last reading.
//
// NOTE: Do not use this for control or reporting related features that require taking into account
// NOTE: input->output mapping.
//
//-------------------------------
//uint16_t headArrayProportionalInputValue(HeadArraySensor_t sensor)
//{
//	return pad_prop_state[(int)sensor];
//}

//-------------------------------
// Function: headArrayPadIsConnected
//
// Description: Checks to see if a pad is connected.
//
//-------------------------------
bool headArrayPadIsConnected(HeadArraySensor_t sensor)
{
	// NOTE: There's currently no way to tell if a pad is connected or not given hardware specs.
	// TODO: If/when hardware supports detecting "pad disconnected", add code to handle the case as well.
	return true;
}

//-------------------------------
// Function: headArrayNeutralTestFail
//
// Description: Let's caller know whether or not the system is in a "neutral fail" state.
//
//-------------------------------
bool headArrayNeutralTestFail(void)
{
	return neutral_test_fail;
}

//-------------------------------
// Function: SetNeedForNeutralTest (void)
//
// Description: This sets a global flag that tells the Head Array Control task
//      to perform a neutral test.
//-------------------------------
void SetNeedForNeutralTest (void)
{
	g_WaitForNeutral = true;
}

/* ********************   Private Function Definitions   ****************** */
int myA = 0;
void doThis(void)
{
    ++myA;
}

//-------------------------------
// Function: HeadArrayInputControlTask
//
// Description: Does as the name suggests.
//
//-------------------------------
static void HeadArrayInputControlTask(void)
{
    task_open();
//	void (*myState)(void);
    
	bool outputs_are_off = false;
	StopWatch_t neutral_sw;

	// Start LED blinky sequence if needed.
	if (SendStateRequestToLedControlModule())
	{
		event_signal(genOutCtrlAppWakeEvent());
	}
	
    // Wait for head array to be in a neutral state on boot
//    if (appCommonIsPowerUpInIdleEnabled() == false)
//        g_WaitForNeutral = true;

//    myState = doThis;
    
	while (1)
	{
		CheckInputs();              // Read the Pad sensor into memory.
        
//        myState();

		if (g_WaitForNeutral)
		{
			if (!stopwatchIsActive(&neutral_sw))
			{
				stopwatchStart(&neutral_sw);
				
				// Shut down all outputs.
				//dacBspSet(DAC_SELECT_FORWARD_BACKWARD, neutral_DAC_setting);
				//dacBspSet(DAC_SELECT_LEFT_RIGHT, neutral_DAC_setting);
				bluetoothSimpleIfBspPadMirrorDisable();
			}

			// Need to wait for all pads to be inactive before controlling the wheelchair for safety reasons.
			if (InNeutralState())
			{
				if (stopwatchTimeElapsed(&neutral_sw, false) >= NEUTRAL_STATE_MIN_TIME_TO_CHECK_ms)
				{
					stopwatchStop(&neutral_sw);
					neutral_test_fail = false;
                    g_WaitForNeutral = false;
				}
			}
			else
			{
				stopwatchZero(&neutral_sw);
				neutral_test_fail = true;
			}
		}
		else
		{
			if (SyncWithEeprom())
			{
				event_signal(genOutCtrlAppWakeEvent());
			}
            
                if (g_PadInfo[HEAD_ARRAY_SENSOR_CENTER].m_CurrentPadStatus != g_PadInfo[HEAD_ARRAY_SENSOR_CENTER].m_PreviousPadStatus)
                {
                    if (g_PadInfo[HEAD_ARRAY_SENSOR_CENTER].m_CurrentPadStatus)
                        GenOutCtrlApp_SetStateAll(GEN_OUT_FORWARD_PAD_ACTIVE);
                    else
                        GenOutCtrlApp_SetStateAll(GEN_OUT_FORWARD_PAD_INACTIVE);
                    g_PadInfo[HEAD_ARRAY_SENSOR_CENTER].m_PreviousPadStatus = g_PadInfo[HEAD_ARRAY_SENSOR_CENTER].m_CurrentPadStatus;
                }
            
                if (g_PadInfo[HEAD_ARRAY_SENSOR_LEFT].m_CurrentPadStatus != g_PadInfo[HEAD_ARRAY_SENSOR_LEFT].m_PreviousPadStatus)
                {
                    if (g_PadInfo[HEAD_ARRAY_SENSOR_LEFT].m_CurrentPadStatus)
                        GenOutCtrlApp_SetStateAll(GEN_OUT_LEFT_PAD_ACTIVE);
                    else
                        GenOutCtrlApp_SetStateAll(GEN_OUT_LEFT_PAD_INACTIVE);
                    g_PadInfo[HEAD_ARRAY_SENSOR_LEFT].m_PreviousPadStatus = g_PadInfo[HEAD_ARRAY_SENSOR_LEFT].m_CurrentPadStatus;
                }

                if (g_PadInfo[HEAD_ARRAY_SENSOR_RIGHT].m_CurrentPadStatus != g_PadInfo[HEAD_ARRAY_SENSOR_RIGHT].m_PreviousPadStatus)
                {
                    if (g_PadInfo[HEAD_ARRAY_SENSOR_RIGHT].m_CurrentPadStatus)
                        GenOutCtrlApp_SetStateAll(GEN_OUT_RIGHT_PAD_ACTIVE);
                    else
                        GenOutCtrlApp_SetStateAll(GEN_OUT_RIGHT_PAD_INACTIVE);
                    g_PadInfo[HEAD_ARRAY_SENSOR_RIGHT].m_PreviousPadStatus = g_PadInfo[HEAD_ARRAY_SENSOR_RIGHT].m_CurrentPadStatus;
                }
//                            event_signal(genOutCtrlAppWakeEvent());
//            case 1:
//                GenOutCtrlApp_SetStateAll(GEN_OUT_REVERSE_PAD_INACTIVE);
//                break;
//            case 2:
//                GenOutCtrlApp_SetStateAll(GEN_OUT_LEFT_PAD_INACTIVE);
//                break;
//            case 3:
//                GenOutCtrlApp_SetStateAll(GEN_OUT_RIGHT_PAD_INACTIVE);
//                break;

			if (SetOutputs() && !outputs_are_off)
			{
				outputs_are_off = true;

				// Shut down all outputs.
				//dacBspSet(DAC_SELECT_FORWARD_BACKWARD, neutral_DAC_setting);
				//dacBspSet(DAC_SELECT_LEFT_RIGHT, neutral_DAC_setting);
				bluetoothSimpleIfBspPadMirrorDisable();
			}
			else if (outputs_are_off)
			{
				// We're back in an active output control state
				outputs_are_off = false;
			}
		}
        
        task_wait(MILLISECONDS_TO_TICKS(20));
	}
    task_close();
}

//-------------------------------
// Function: CheckInputs
//
// Description: Checks the input values from the head array.
//
//-------------------------------
static void CheckInputs(void)
{
#if defined(TEST_BASIC_DAC_CONTROL)
	// Never returns.
	TestBasicDacControl();  
#endif

	//MirrorUpdateProportionalInputValues();
	//MirrorUpdateDigitalInputValues();
    UpdatePadStatus();
}

//-------------------------------
// Function: SetOutputs
//
// Description: Sets outputs to their appropriate values, based on system state and input values.
//
//-------------------------------
static bool SetOutputs(void)
{
	bool turn_outputs_off = true;
    FunctionalFeature_t current_feature = FUNC_FEATURE_DRIVING;
    
	if (AppCommonDeviceActiveGet())
	{
		// "Power is on"
		if (!AppCommonCalibrationActiveGet())
		{
			// Not in calibration mode.
			switch (current_feature)
			{
//				case FUNC_FEATURE_POWER_ON_OFF:
//				case FUNC_FEATURE_OUT_NEXT_FUNCTION:
//				case FUNC_FEATURE_OUT_NEXT_PROFILE:
//                case FUNC_FEATURE_RNET_SEATING:
//					// Direct control of the wheelchair from this device
//					//dacBspSet(DAC_SELECT_FORWARD_BACKWARD, headArrayOutputValue(HEAD_ARRAY_OUT_AXIS_FWD_REV));
//					//dacBspSet(DAC_SELECT_LEFT_RIGHT, headArrayOutputValue(HEAD_ARRAY_OUT_AXIS_LEFT_RIGHT));
//					turn_outputs_off = false;
//					break;

                case FUNC_FEATURE_DRIVING:
                    break;
                    
				case FUNC_FEATURE_OUT_CTRL_TO_BT_MODULE:
					// Control sent to a Bluetooth module.
					MirrorDigitalInputOnBluetoothOutput();
					turn_outputs_off = false;
					break;
				
				default:
					// Nothing to do.
					break;
			}
		}
		else
		{
			// In calibration mode. Do not want to control the wheelchair right now!
			(void)0;
		}
	}
	else
	{
		// "Power is off". Better be in FUNC_FEATURE_POWER_ON_OFF state...
		// TODO: Put check here to ensure we're in FUNC_FEATURE_POWER_ON_OFF state
        // The following are required because the DAC output is sticky otherwise.
		//dacBspSet(DAC_SELECT_FORWARD_BACKWARD, neutral_DAC_setting);
        //dacBspSet(DAC_SELECT_LEFT_RIGHT, neutral_DAC_setting);
	}

	return turn_outputs_off;
}

//-------------------------------
// Function: SendStateRequestToLedControlModule
//
// Description: Determines what event needs to be sent to the LED state controller that reflects this
//		module's state and sends it on over.
//
//-------------------------------
static bool SendStateRequestToLedControlModule(void)
{
	GenOutState_t led_ctrl_state;

    // If the Head Array is allowed to issue drive commands, turn on Green LED.
//	if ( (curr_active_feature == FUNC_FEATURE_POWER_ON_OFF)
//        || (curr_active_feature == FUNC_FEATURE_OUT_NEXT_FUNCTION)
//        || (curr_active_feature == FUNC_FEATURE_OUT_NEXT_PROFILE))
//	{
//		led_ctrl_state = GEN_OUT_CTRL_STATE_HEAD_ARRAY_ACTIVE;
//	}
	if (curr_active_feature == FUNC_FEATURE_OUT_CTRL_TO_BT_MODULE)
	{
		led_ctrl_state = GEN_OUT_CTRL_STATE_BLUETOOTH_OUTPUT;
	}
	else
	{
		led_ctrl_state = GEN_OUT_CTRL_STATE_NO_OUTPUT;
	}
    
    GenOutCtrlApp_SetStateAll(led_ctrl_state);

	return genOutCtrlAppNeedSendEvent();
}

//-------------------------------
// Function: MirrorUpdateDigitalInputValues
//
// Description: Update digital input values in RAM that communicate to the rest of the system what
//		the value of each input is.
//
//-------------------------------
//static void MirrorUpdateDigitalInputValues(void)
//{
//	for (int sensor_id = 0; sensor_id < (int)HEAD_ARRAY_SENSOR_EOL; sensor_id++)
//	{
//		pad_dig_state[sensor_id] = headArrayBspDigitalState((HeadArraySensor_t)sensor_id);
//	}
//}

//-----------------------------------------------------------------------
// This gets the Pad's Digital status and stores them in the global structure.
//-----------------------------------------------------------------------
//DEBUG int g_MyCount = 0;

static void UpdatePadStatus (void)
{
	for (int sensor_id = 0; sensor_id < (int)HEAD_ARRAY_SENSOR_EOL; sensor_id++)
	{
		g_PadInfo[sensor_id].m_CurrentPadStatus = headArrayBspDigitalState((HeadArraySensor_t)sensor_id);
//DEBUG        if (g_PadInfo[sensor_id].m_CurrentPadStatus)
//DEBUG            ++g_MyCount;
	}
}
//-------------------------------
// Function: MirrorUpdateProportionalInputValues
//
// Description: Update proportional input values in RAM that communicate to the rest of the system what
//		the value of each input is.
//
// NOTE: ADC reads 0x02 when a pad is connected, and 0x00 when disconnected.
//		 It would be good to have a better distinction, but, well, there ya go.
//
//-------------------------------
//static void MirrorUpdateProportionalInputValues(void)
//{
//	for (int sensor_id = 0; sensor_id < (int)HEAD_ARRAY_SENSOR_EOL; sensor_id++)
//	{
//		//pad_raw_prop_state[sensor_id] = headArrayBspAnalogState((HeadArraySensor_t)sensor_id);
//
//		// For safety, make sure that min/max thresholds make sense before using them to control the output value.
//		if ((pad_raw_prop_state[sensor_id] < pad_adc_min_thresh_val[sensor_id]) ||
//			(pad_adc_max_thresh_val[sensor_id] <= pad_adc_min_thresh_val[sensor_id]))
//		{
//			pad_prop_state[sensor_id] = 0;
//		}
//		else if (pad_raw_prop_state[sensor_id] > pad_adc_max_thresh_val[sensor_id])
//		{
//			pad_prop_state[sensor_id] = 100;
//		}
//		else
//		{
//			uint16_t range = pad_adc_max_thresh_val[sensor_id] - pad_adc_min_thresh_val[sensor_id];
//
//			// In between min and max limits
//			pad_prop_state[sensor_id] = (uint16_t)(((uint32_t)100 * (uint32_t)(pad_raw_prop_state[sensor_id] - pad_adc_min_thresh_val[sensor_id])) / (uint32_t)range);
//        }
//	}
//}

//-------------------------------
// Function: MirrorDigitalInputOnBluetoothOutput
//
// Description: Mirrors digital pad inputs on Bluetooth digital output lines. No mapping, just a one-to-one map.
//
//-------------------------------
static void MirrorDigitalInputOnBluetoothOutput(void)
{
	for (int i = 0; i < (int) HEAD_ARRAY_SENSOR_EOL; i++)
	{
		//bluetoothSimpleIfBspPadMirrorStateSet((HeadArraySensor_t)i, pad_dig_state[i]);
		bluetoothSimpleIfBspPadMirrorStateSet((HeadArraySensor_t)i, g_PadInfo[i].m_CurrentPadStatus);
	}
}

//-------------------------------
// Function: ConvertPropInToOutValue
//
// Description: Converts input proportional value to DAC output value.
//
//-------------------------------
//static uint16_t ConvertPropInToOutValue(uint8_t sensor_id)
//{
//	uint16_t ret_val;
//	ret_val = DAC_Minimum_percent[sensor_id];
//
//	// The on-scale is 20%-100%. Where, the first 20% is always there if the digital input is active (and it
//	// MUST be active in order for the proportional value to be considered) and the other 80% of control
//	// comes for the proportional value.
//    // As of Feb 1, 2020, the percentage is programmable is considered in RefreshLimits()
//	ret_val += (pad_prop_state[sensor_id] * DAC_Proportional_percent[sensor_id]) / (uint16_t)100;
//
//	return ret_val;
//}

//-------------------------------
// Function: InNeutralState
//
// Description: Checks to see if the wheelchair is in a neutral wheelchair control state.
//
// NOTE: Must be called from a task/ISR.
//
//-------------------------------
static bool InNeutralState(void)
{
    bool active = true;
	for (int i = 0; i < (int) HEAD_ARRAY_SENSOR_EOL; i++)
	{
        if (!g_PadInfo[i].m_CurrentPadStatus)
            active = false;
	}
    return active;
}

//-------------------------------
// Function: SyncWithEeprom
//
// Description: Syncs all data that this module relies on that is stored in EEPROM with the EEPROM module.
//
// Return: Let's caller know whether or not the one and only event that can be sent needs to be sent.
//
// NOTE: Given that the task call rate is 20 ms and there are 7 items, in theory it will take a max of 140 ms for
// NOTE: a piece of data to be updated.
//
//-------------------------------
static bool SyncWithEeprom(void)
{
	uint16_t range;
    bool need_to_send_event = false;
	FunctionalFeature_t new_feature = FUNC_FEATURE_DRIVING;

	if (curr_active_feature != new_feature)
	{
		curr_active_feature = new_feature;

		if (curr_active_feature == FUNC_FEATURE_OUT_CTRL_TO_BT_MODULE)
		{
			// Shutdown output to control the system directly from this device
			//dacBspSet(DAC_SELECT_FORWARD_BACKWARD, neutral_DAC_setting);
			//dacBspSet(DAC_SELECT_LEFT_RIGHT, neutral_DAC_setting);
		}
		else
		{
			// Output to Bluetooth module is disabled.
			bluetoothSimpleIfBspPadMirrorDisable();
		}

		need_to_send_event = SendStateRequestToLedControlModule();
        
        // Must wait for head array to be in a neutral state. when switching to a new active feature.
        g_WaitForNeutral = true;
	}

//	head_arr_input_type[(int)HEAD_ARRAY_SENSOR_LEFT] = (HeadArrayInputType_t)eepromEnumGet(EEPROM_STORED_ITEM_LEFT_PAD_INPUT_TYPE);
//	head_arr_input_type[(int)HEAD_ARRAY_SENSOR_RIGHT] = (HeadArrayInputType_t)eepromEnumGet(EEPROM_STORED_ITEM_RIGHT_PAD_INPUT_TYPE);
//	head_arr_input_type[(int)HEAD_ARRAY_SENSOR_CENTER] = (HeadArrayInputType_t)eepromEnumGet(EEPROM_STORED_ITEM_CTR_PAD_INPUT_TYPE);

//	input_pad_to_output_pad_map[(int)HEAD_ARRAY_SENSOR_LEFT] = (HeadArrayOutputFunction_t)eepromEnumGet(EEPROM_STORED_ITEM_LEFT_PAD_OUTPUT_MAP);
//	input_pad_to_output_pad_map[(int)HEAD_ARRAY_SENSOR_RIGHT] = (HeadArrayOutputFunction_t)eepromEnumGet(EEPROM_STORED_ITEM_RIGHT_PAD_OUTPUT_MAP);
//	input_pad_to_output_pad_map[(int)HEAD_ARRAY_SENSOR_CENTER] = (HeadArrayOutputFunction_t)eepromEnumGet(EEPROM_STORED_ITEM_CTR_PAD_OUTPUT_MAP);
	
//	pad_min_adc_val[(int)HEAD_ARRAY_SENSOR_LEFT] = eeprom16bitGet(EEPROM_STORED_ITEM_LEFT_PAD_MIN_ADC_VAL);
//	pad_min_adc_val[(int)HEAD_ARRAY_SENSOR_RIGHT] = eeprom16bitGet(EEPROM_STORED_ITEM_RIGHT_PAD_MIN_ADC_VAL);
//	pad_min_adc_val[(int)HEAD_ARRAY_SENSOR_CENTER] = eeprom16bitGet(EEPROM_STORED_ITEM_CTR_PAD_MIN_ADC_VAL);
	
//	pad_max_adc_val[(int)HEAD_ARRAY_SENSOR_LEFT] = eeprom16bitGet(EEPROM_STORED_ITEM_LEFT_PAD_MAX_ADC_VAL);
//	pad_max_adc_val[(int)HEAD_ARRAY_SENSOR_RIGHT] = eeprom16bitGet(EEPROM_STORED_ITEM_RIGHT_PAD_MAX_ADC_VAL);
//	pad_max_adc_val[(int)HEAD_ARRAY_SENSOR_CENTER] = eeprom16bitGet(EEPROM_STORED_ITEM_CTR_PAD_MAX_ADC_VAL);
	
//	pad_min_thresh_perc[(int)HEAD_ARRAY_SENSOR_LEFT] = eeprom16bitGet(EEPROM_STORED_ITEM_LEFT_PAD_MIN_THRESH_PERC);
//	pad_min_thresh_perc[(int)HEAD_ARRAY_SENSOR_RIGHT] = eeprom16bitGet(EEPROM_STORED_ITEM_RIGHT_PAD_MIN_THRESH_PERC);
//	pad_min_thresh_perc[(int)HEAD_ARRAY_SENSOR_CENTER] = eeprom16bitGet(EEPROM_STORED_ITEM_CTR_PAD_MIN_THRESH_PERC);
	
//	pad_max_thresh_perc[(int)HEAD_ARRAY_SENSOR_LEFT] = eeprom16bitGet(EEPROM_STORED_ITEM_LEFT_PAD_MAX_THRESH_PERC);
//	pad_max_thresh_perc[(int)HEAD_ARRAY_SENSOR_RIGHT] = eeprom16bitGet(EEPROM_STORED_ITEM_RIGHT_PAD_MAX_THRESH_PERC);
//	pad_max_thresh_perc[(int)HEAD_ARRAY_SENSOR_CENTER] = eeprom16bitGet(EEPROM_STORED_ITEM_CTR_PAD_MAX_THRESH_PERC);

//    pad_MinDriveSpeed[(int)HEAD_ARRAY_SENSOR_CENTER] = (uint16_t) eeprom8bitGet(EEPROM_STORED_ITEM_MM_CENTER_PAD_MINIMUM_DRIVE_OFFSET);
//    pad_MinDriveSpeed[(int)HEAD_ARRAY_SENSOR_LEFT] = (uint16_t) eeprom8bitGet(EEPROM_STORED_ITEM_MM_LEFT_PAD_MINIMUM_DRIVE_OFFSET);
//    pad_MinDriveSpeed[(int)HEAD_ARRAY_SENSOR_RIGHT] = (uint16_t) eeprom8bitGet(EEPROM_STORED_ITEM_MM_RIGHT_PAD_MINIMUM_DRIVE_OFFSET);
    
//    neutral_DAC_counts = eeprom16bitGet(EEPROM_STORED_ITEM_MM_NEUTRAL_DAC_COUNTS);
//    neutral_DAC_setting = eeprom16bitGet(EEPROM_STORED_ITEM_MM_NEUTRAL_DAC_SETTING);
//    neutral_DAC_range = eeprom16bitGet(EEPROM_STORED_ITEM_MM_NEUTRAL_DAC_RANGE);

//	for (int sensor_id = 0; sensor_id < HEAD_ARRAY_SENSOR_EOL; sensor_id++)
//	{
//		range = pad_max_adc_val[sensor_id] - pad_min_adc_val[sensor_id];
//		pad_adc_min_thresh_val[sensor_id] = pad_min_adc_val[sensor_id] + (uint16_t)(((uint32_t)range * (uint32_t)pad_min_thresh_perc[sensor_id]) / (uint32_t)100);
//		pad_adc_max_thresh_val[sensor_id] = pad_min_adc_val[sensor_id] + (uint16_t)(((uint32_t)range * (uint32_t)pad_max_thresh_perc[sensor_id]) / (uint32_t)100);
//        DAC_Proportional_percent[sensor_id] = (neutral_DAC_range * (100 - pad_MinDriveSpeed[sensor_id])) / 100;
//        DAC_Minimum_percent[sensor_id] = (neutral_DAC_range * pad_MinDriveSpeed[sensor_id]) / 100;
//
//	}
	
	// Need to make sure that the limits are set using the latest variables pulled from the EEPROM
	// module from above.
//	RefreshLimits();

    return need_to_send_event;
}

//-------------------------------
// Function: RefreshLimits
//
// Description: Set the DAC variables and constants
//
//-------------------------------
//static void RefreshLimits(void)
//{
//    
//	DAC_lower_rail = neutral_DAC_setting - neutral_DAC_range;
//	DAC_upper_rail = neutral_DAC_setting + neutral_DAC_range;
//
//	DAC_max_forward_counts = DAC_upper_rail;
//	DAC_max_left_counts = DAC_lower_rail;
//	DAC_max_right_counts = DAC_upper_rail;
//	DAC_max_reverse_counts = DAC_lower_rail;
//}

#if defined(TEST_BASIC_DAC_CONTROL)
//-------------------------------
// Function: TestBasicDacControl
//
// Description: Makes sure that we are able to properly set the DAC output values.
//
//-------------------------------
static void TestBasicDacControl(void)
{
	#define NUM_DAC_STEPS ((uint16_t)10)
	#define DAC_MAX_VALUE (((uint16_t)1 << 12) - 1)
	#define DAC_STEP_SIZE (DAC_MAX_VALUE / NUM_DAC_STEPS)

	while (1)
	{
#if 0
		// Use this case to make sure the output values set are what are expected.
		// This case makes it easy to see the transitions and settling levels.
		
		for (uint16_t i = 0; i < NUM_DAC_STEPS; i++)
		{
			dacBspSet(DAC_SELECT_FORWARD_BACKWARD, i * DAC_STEP_SIZE);
			dacBspSet(DAC_SELECT_LEFT_RIGHT, i * DAC_STEP_SIZE);
			bspDelayMs(75);
		}
		
		// Set to absolute max value.
		dacBspSet(DAC_SELECT_FORWARD_BACKWARD, DAC_MAX_VALUE);
		dacBspSet(DAC_SELECT_LEFT_RIGHT, DAC_MAX_VALUE);
		bspDelayMs(75);
#elif 0
		// See min/max/neutral are setting properly.
		dacBspSet(DAC_SELECT_FORWARD_BACKWARD, DAC_LOWER_RAIL);
		dacBspSet(DAC_SELECT_LEFT_RIGHT, DAC_LOWER_RAIL);
		bspDelayMs(1000);
		dacBspSet(DAC_SELECT_FORWARD_BACKWARD, neutral_DAC_setting);
		dacBspSet(DAC_SELECT_LEFT_RIGHT, neutral_DAC_setting);
		bspDelayMs(1000);
		dacBspSet(DAC_SELECT_FORWARD_BACKWARD, DAC_UPPER_RAIL);
		dacBspSet(DAC_SELECT_LEFT_RIGHT, DAC_UPPER_RAIL);
		bspDelayMs(1000);
#else
		// use this case to make sure that the output can be changed at the fastest rate this program can
		// without issues.

//		for (uint16_t i = 0; i < NUM_DAC_STEPS; i++)
//		{
//			dacBspSet(DAC_SELECT_FORWARD_BACKWARD, i * DAC_STEP_SIZE);
//			dacBspSet(DAC_SELECT_LEFT_RIGHT, i * DAC_STEP_SIZE);
//		}
		
		// Set to absolute max value.
		dacBspSet(DAC_SELECT_FORWARD_BACKWARD, DAC_MAX_VALUE);
		dacBspSet(DAC_SELECT_LEFT_RIGHT, DAC_MAX_VALUE);
        
        
		dacBspSet(DAC_SELECT_FORWARD_BACKWARD, 2048);
		dacBspSet(DAC_SELECT_FORWARD_BACKWARD, 2048);
#endif
	}
}
#endif

// end of file.
//-------------------------------------------------------------------------
