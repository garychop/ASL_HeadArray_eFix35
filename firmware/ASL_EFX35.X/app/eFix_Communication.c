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

/* **************************   Forward Declarations   *************************** */

static void eFix_Communication_Task (void);

/* **************************    Local Variables   *************************** */

int g_Index = 0;
unsigned char g_CharArray[0x100];


//------------------------------------------------------------------------------
// Function: eFix_Communincation_Initialize
//
// Description: Initialize the RS-232 Hardware and buffers.
//
//------------------------------------------------------------------------------

void eFix_Communincation_Initialize(void)
{
    for (g_Index = 0; g_Index < sizeof(g_CharArray); ++g_Index)
        g_CharArray[g_Index] = 0xff;
    g_Index = 0;
    
    RS232_Initialize();     // Initialize the RS-232 PORT on the CPU.
    
    // Create the state update and control task
    (void)task_create(eFix_Communication_Task, NULL, EFIX_COMM_TASK_PRIO, NULL, 0, 0);
    
}

//------------------------------------------------------------------------------
// Function: eFix_Communication_Task
//
// Description: This is the eFix Communication Task.
//
//------------------------------------------------------------------------------

int g_ReadyCounter = 0;
int g_NotReadyCounter = 0;
int g_ReceivedCounter = 0;
int g_Received55Counter = 0;
int g_ReceiveTimeout = 0;
int g_SendCounter = 0;
char myChar = 0xff;
char myBadChar = 0x41;
unsigned char g_XmtChar = 0;

static void eFix_Communication_Task (void)
{
    bool receivedChar;
    int i;
    
    task_open();

    while (1)
	{
        task_wait(MILLISECONDS_TO_TICKS(EFIX_COMM_TASK_DELAY));
        
        while (RS232_TransmitReady() == false)       // Wait for transmit buffer to be ready to accept new character
        {
            ; // ++g_NotReadyCounter;
        }
        //TXREG = 0x55;
        //g_XmtChar = 0x55;
        RS232_TransmitChar(g_XmtChar);
        g_CharArray[g_Index] = g_XmtChar;
        if (++g_Index == sizeof (g_CharArray))
            g_Index = 0;
        
        ++g_SendCounter;
        
        receivedChar = false;
        i = 0;
        while ((!receivedChar) && (i < 100))
        {
            ++i;
            if (PIR1bits.RCIF != 0) // non0 = we got a character
            {
                receivedChar = true;
//                if (RCSTAbits.OERR)
//                {
//                    CREN = 0;
//                    NOP();
//                    CREN = 1;
//                }
                myChar = RCREG;
                if (myChar == g_XmtChar) // Did we get the right character
                    ++g_Received55Counter;
                else
                {
                    ++g_ReceivedCounter;
                    myBadChar = myChar;
                }
            }
        }
        if (!receivedChar)          // Let's keep track of missed characters.
            ++g_ReceiveTimeout;

        ++g_XmtChar;
    }
    
    task_close();
}
