//////////////////////////////////////////////////////////////////////////////
//
// Filename: eFix_Communication.c
//
// Description: Implements application specific needs for the ASL110 related to
//		communicating with the eFix Model 35.
//  
//  Reference the following web page to create this driver:
//  https://openlabpro.com/guide/uart-interfacing-with-pic-microcontroller/
//
// Author(s): G. Chopcinski (Kg Solutions, LLC)
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
#include "rtos_task_priorities.h"

// from project
#include "config.h"
#include "common.h"
#include "user_button.h"
#include "head_array_bsp.h" // TODO: Expose MIN/MAX values in head_array driver module
#include "head_array.h"
#include "app_common.h"

#include "inc/eFix_Communication.h"
#include "RS232.h"

/* **************************   Local Macro Declarations   *************************** */

#define EFIX_COMM_TASK_DELAY (15)
#define DELAYS_BETWEEN_XMT_CHAR (2)

#define ASL110_SOT (0xeb)       // Start Of Transmission Character

/* **************************   Forward Declarations   *************************** */

static void eFix_Communication_Task (void);
static void Create_eFix_Setup_Msg(unsigned char *buffer);
static void Create_eFix_MaxSpeed_Message(unsigned char *buffer);
static void Create_eFix_Steering_Message (unsigned char *buffer, int direction);
static void Create_eFix_Speed_Message(unsigned char *buffer, int speed);
static void SendMessageToEFIX (unsigned char *buffer);

// State Engine
static void SendMaxSpeedMessage_State (void);
static void SendSpeedDirection_State (void);

/* **************************    Local Variables   *************************** */

unsigned char g_XmtBuffer[8];
void (*gpState)(void);
int g_Direction;
int g_Speed;

int g_ReadyCounter = 0;
int g_NotReadyCounter = 0;
int g_ReceivedCounter = 0;
int g_Received55Counter = 0;
int g_ReceiveTimeout = 0;
int g_SendCounter = 0;
char myChar = 0xff;
char myBadChar = 0x41;
unsigned char g_XmtChar = 0;


//------------------------------------------------------------------------------
// Function: eFix_Communincation_Initialize
//
// Description: Initialize the RS-232 Hardware and buffers.
//
//------------------------------------------------------------------------------

void eFix_Communincation_Initialize(void)
{
    RS232_Initialize();     // Initialize the RS-232 PORT on the CPU.
    
    gpState = SendMaxSpeedMessage_State;
    
    // Create the state update and control task
    (void)task_create(eFix_Communication_Task, NULL, EFIX_COMM_TASK_PRIO, NULL, 0, 0);
    
}

//------------------------------------------------------------------------------

static void SendMaxSpeedMessage_State (void) 
{
    Create_eFix_MaxSpeed_Message (g_XmtBuffer);
    SendMessageToEFIX (g_XmtBuffer);

    gpState = SendSpeedDirection_State;
}

//------------------------------------------------------------------------------

static void SendSpeedDirection_State (void)
{
    Create_eFix_Steering_Message (g_XmtBuffer, g_Direction);
    Create_eFix_Speed_Message (g_XmtBuffer, g_Speed);
}

//------------------------------------------------------------------------------
// Function: eFix_Communication_Task
//
// Description: This is the eFix Communication Task.
//
//------------------------------------------------------------------------------

static void eFix_Communication_Task (void)
{
   
    task_open();

    while (1)
	{
        task_wait(MILLISECONDS_TO_TICKS(EFIX_COMM_TASK_DELAY));
        
        gpState();
        
    }
    
    task_close();
}

//------------------------------------------------------------------------------
// Send message this item in the buffer to the eFix controller via RS-232
// Assumption is that the message is 6 character in length.
//------------------------------------------------------------------------------
static void SendMessageToEFIX (unsigned char *buffer)
{
    int i;

    for (int counter = 0; counter < 6; ++counter)
    {
        for (i=0; i<20; ++i)    // A little pause between each character
            NOP();
        
        while (RS232_TransmitReady() == false)       // Wait for transmit buffer to be ready to accept new character
        {
            ; // ++g_NotReadyCounter;
        }
        RS232_TransmitChar(buffer[counter]);
    }
}
//------------------------------------------------------------------------------
// Function: CalcChecksum()
// Description: This function calculates the checksum and puts in the 
//      data to be sent to the eFix controller.
//------------------------------------------------------------------------------
void CalcChecksum(unsigned char *buffer)
{
    int checksum = 0;

    for (int i = 0; i < 4; ++i)
        checksum += buffer[i];
    checksum = ~checksum;
    checksum++;
    buffer[4] = (unsigned char)(checksum >> 8);
    buffer[5] = (unsigned char)(checksum & 0xff);
}

//------------------------------------------------------------------------------
// This creates the 1st startup message.
//------------------------------------------------------------------------------
static void Create_eFix_Setup_Msg(unsigned char *buffer)
{
    buffer[0] = ASL110_SOT; // 0xEB;     // Start of Transmission (SOT)
    buffer[1] = 0x04;     // Message ID
    buffer[2] = 0x00;
    buffer[3] = 0xd0; //  0xB0;
    CalcChecksum (buffer);
}

//------------------------------------------------------------------------------
// This creates the Maximum Speed message
//------------------------------------------------------------------------------
static void Create_eFix_MaxSpeed_Message(unsigned char *buffer)
{
    buffer[0] = ASL110_SOT;     // Start of Transmission (SOT)
    buffer[1] = 0x08;     // Message ID
    buffer[2] = 0x64;     // Only high byte
    buffer[3] = 0x00;     // Only low byte
    CalcChecksum(buffer);
}

//------------------------------------------------------------------------------
// This creates the Direction (steering) message
//------------------------------------------------------------------------------
static void Create_eFix_Steering_Message (unsigned char *buffer, int direction)
{
    buffer[0] = ASL110_SOT;     // Start of Transmission (SOT)
    buffer[1] = 0x01;     // Message ID
    buffer[2] = (direction >> 8);    // Only high byte
    buffer[3] = (direction & 0xff);  // Only low byte
    CalcChecksum(buffer);
}

//------------------------------------------------------------------------------
// This creates the Speed message
//------------------------------------------------------------------------------
static void Create_eFix_Speed_Message(unsigned char *buffer, int speed)
{
    buffer[0] = ASL110_SOT;     // Start of Transmission (SOT)
    buffer[1] = 0x02;     // Message ID
    buffer[2] = (speed >> 8);    // Only high byte
    buffer[3] = (speed & 0xff);  // Only low byte
    CalcChecksum(buffer);
}
