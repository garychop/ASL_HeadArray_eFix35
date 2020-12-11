//////////////////////////////////////////////////////////////////////////////
//
// Filename: beeper.c
//
// Description: Provides beeper control to the application.
//
// Author(s): Trevor Parsh (Embedded Wizardry, LLC)
//
// Modified for ASL on Date: 
//
//////////////////////////////////////////////////////////////////////////////


/* **************************   Header Files   *************************** */

// NOTE: This must ALWAYS be the first include in a file.
#include "device.h"

#include <stdint.h>
#include <stdbool.h>
#include "user_assert.h"

// from RTOS
#include "cocoos.h"

// from project
#include "rtos_task_priorities.h"
#include "common.h"
#include "stopwatch.h"
#include "app_common.h"

// from local
#include "beeper_bsp.h"
#include "beeper.h"

/* ******************************   Types   ******************************* */

// Defines the "beep" type
typedef struct
{
	TimerTick_t on_time_ms;     // On time in milliseconds OR pattern ID (BeepPattern_t)
	
	TimerTick_t off_time_ms;    // Off time in milliseconds or Usage Flag
} Beep_t;

#define BEEP_ALWAYS (0)
#define BEEP_SMART (1)

#define MAX_BEEPS_PER_PATTERN (4)
#define MAX_BEEP_PATTERNS (10)
#define BEEP_POOL_SIZE (2)
#define END_BEEP (0xff)


//-----------------------------------------------------------------------------
// NOTE: Using the following union cause the application to misbehave and
// go into the woods. I abandoned this just using the first entry in the
// beep sequence table as the pattern ID and Enable flag.
typedef struct
{
    union {
        struct {
            TimerTick_t on_time_ms;     // On time in milliseconds.
            TimerTick_t off_time_ms;    // Off time in milliseconds.
        };
        struct {
            //I'm using TimerTick_t to ensure proper alignment in beeper pattern
            // declarations.
            TimerTick_t m_PatternID;
            TimerTick_t m_Allowed;      // 0=always announce, 1=based upon Beeper Enabled.
        };
    };
} Beep_t_DONT_USE_CUZ_BAD_THINGS_HAPPEN;
//-----------------------------------------------------------------------------


/* ***********************   File Scope Variables   *********************** */

static Msg_t g_BeepMsgPool[BEEP_POOL_SIZE];
static uint8_t g_PatternIndex;
static uint8_t g_PatternStep;
static void (*BeepStateEngine)(void);

uint8_t g_BeeperTaskID = 0;
int IGotAMsg = 0;
static Msg_t g_LastBeepMsg;
static bool g_NewBeep = false;
static uint16_t g_Delay;

const Beep_t g_BeepPatterns[MAX_BEEP_PATTERNS][MAX_BEEPS_PER_PATTERN] = 
{
    {//[0]   // Pad Active beep pattern
        {BEEPER_PATTERN_PAD_ACTIVE,BEEP_SMART},
        {100, 50},
        //{END_BEEP,0},
        {END_BEEP,0}
    },
    {//[1]
        {BEEPER_PATTERN_USER_BUTTON_SHORT_PRESS,BEEP_ALWAYS},
        {500, 0},
        //{END_BEEP,0},
        {END_BEEP,0}
    },
    {//[2]
        {ANNOUNCE_BLUETOOTH,BEEP_ALWAYS},
        {100, 200},
        {100, 200},
        {END_BEEP,0}
    },
    {//[3]
        {ANNOUNCE_POWER_ON, BEEP_ALWAYS},
        //{END_BEEP,0},
        //{END_BEEP,0},
        {END_BEEP,0}
    },
    { {BEEPER_PATTERN_EOL, 0}},// {END_BEEP,0}, {END_BEEP,0},  {END_BEEP,0} },  // [4]
    { {BEEPER_PATTERN_EOL, 0}},// {END_BEEP,0}, {END_BEEP,0},  {END_BEEP,0} },  // [5]
    { {BEEPER_PATTERN_EOL, 0}},// {END_BEEP,0}, {END_BEEP,0},  {END_BEEP,0} },  // [6]
    { {BEEPER_PATTERN_EOL, 0}},// {END_BEEP,0}, {END_BEEP,0},  {END_BEEP,0} },  // [7]
    { {BEEPER_PATTERN_EOL, 0}},// {END_BEEP,0}, {END_BEEP,0},  {END_BEEP,0} },  // [8]
    { {BEEPER_PATTERN_EOL, 0}} // {END_BEEP,0}, {END_BEEP,0},  {END_BEEP,0} },  // [9]
};

static Evt_t os_event_start_beep_seq_id;
static Evt_t os_event_beep_seq_complete;
//static uint8_t beeper_task_id;
//static volatile bool signal_calling_task;

// Protects access to critical sections of code in this module
static volatile Sem_t data_lock_mutex;

/* ***********************   Function Prototypes   ************************ */

static void BeepPatternTask(void);

// State Engine
static void BeepReady (void);
static void StopBeeping (void);
static void WaitForStopping (void);
static void WeBeMakingNoise (void);

/* *******************   Public Function Definitions   ******************** */

static void BeepReady (void)
{
    if (g_NewBeep)
    {
        g_NewBeep = false;      // Ok, we can clear the request for a new beep sequence
        g_PatternStep = 0;      // Point to the first step in the Beep Sequence.
        beeperBspActiveSet (true); // Turn on beeping
        g_Delay = g_BeepPatterns[g_PatternIndex][g_PatternStep].on_time_ms / BEEPER_TASK_DELAY;  // 1/2 second delay
        BeepStateEngine = WeBeMakingNoise;
    }
}

//------------------------------------------------------------------------------

static void WeBeMakingNoise (void)
{
    if (g_Delay > 0)
        --g_Delay;

    if (g_Delay == 0)
        BeepStateEngine = StopBeeping;
}

//------------------------------------------------------------------------------

static void StopBeeping (void)
{
    beeperBspActiveSet (false); // Turn off beeping
    g_Delay = g_BeepPatterns[g_PatternIndex][g_PatternStep].off_time_ms / BEEPER_TASK_DELAY;
    BeepStateEngine = WaitForStopping;
}

//------------------------------------------------------------------------------

static void ForceStopBeeping (void)
{
    beeperBspActiveSet (false); // Turn off beeping
    g_Delay = 500 / BEEPER_TASK_DELAY;  // 1/2 second delay
    BeepStateEngine = WaitForStopping;
}

//------------------------------------------------------------------------------

static void WaitForStopping (void)
{
    if (g_Delay > 0)
        --g_Delay;

    if (g_Delay == 0)
        BeepStateEngine = BeepReady;
}

//-------------------------------
// Function: beeperInit
//
// Description: Initializes this module.
//
//-------------------------------
void beeperInit(void)
{
	beeperBspInit();
	
	//blocking_task_id = NO_TID;
	//session_pattern = BEEPER_PATTERN_EOL;
	//curr_session_pattern = BEEPER_PATTERN_EOL;
	//curr_session_cycle = 0;
	//beeper_is_running = false;
	//signal_calling_task = false;

    data_lock_mutex = sem_bin_create(1); // Set up so the first task to try and take the semaphore succeeds

	//os_event_start_beep_seq_id = event_create();
	//os_event_beep_seq_complete = event_create();
//    beeper_task_id = task_create(BeepPatternTask, NULL, BEEPER_MGMT_TASK_PRIO, NULL, 0, 0 );
    g_BeeperTaskID = task_create(BeepPatternTask, NULL, BEEPER_MGMT_TASK_PRIO, g_BeepMsgPool, BEEP_POOL_SIZE, sizeof (Msg_t)); // sizeof (BeepMsg_t));
//    g_BeeperTaskID = task_create(BeepPatternTask, NULL, BEEPER_MGMT_TASK_PRIO, NULL, 0, 0);
    
    BeepStateEngine = BeepReady;
}

//-------------------------------
// Function: beeperBeep
//
// Description: Beeps a pattern, in a non-blocking way.
//
// Note: If a beep session is running, new ones are ignored unless of higher priority
//
//-------------------------------
Evt_t beeperBeep(BeepPattern_t pattern)
{
//	if (appCommonSoundEnabled())
//	{
//		return BeepPatternStart(pattern);
//	}
//	else
//	{
		return NO_EVENT;
//	}
}


//-------------------------------
// Function: BeepPatternTick
//
// Description: Handles state control for a beep session.
//
//	Must be called from within a stable timing construct.
//
//-------------------------------

static void BeepPatternTask(void)
{
    task_open();

    while (1)
    {
        task_wait(MILLISECONDS_TO_TICKS(BEEPER_TASK_DELAY));

        g_LastBeepMsg.signal = BEEPER_PATTERN_EOL;
        //g_NewBeep = false;
        msg_receive_async (g_BeeperTaskID, &g_LastBeepMsg);
        if (g_LastBeepMsg.signal != NO_MSG_ID)
        {
            ++IGotAMsg;
            // locate the beep pattern
            for (uint8_t i = 0; i<MAX_BEEP_PATTERNS; ++i)
            {
                if (g_BeepPatterns[i][0].on_time_ms == g_LastBeepMsg.signal)
                {
                    // Check to see if we can (always) beep or if we
                    // have to be smart about it and look at the DIP switch.
                    g_NewBeep = true;
                    g_PatternIndex = i;
                    if (BeepStateEngine != BeepReady)
                    {
                        BeepStateEngine = ForceStopBeeping;
                    }
                    break;
                }
            }
        }
        

        BeepStateEngine();
	}
    task_close();
}

//-------------------------------
// Function: IsBeepEnabled
// Description: Get the status of the DIP switch for the Beep
// Returns: "true" if the Beeps should be making obnoxious noise or
//      "false" to operate in silence.
//-------------------------------

bool IsBeepEnabled(void)
{
    return (IsBeepFeatureEnable());
}

// end of file.
//-------------------------------------------------------------------------
