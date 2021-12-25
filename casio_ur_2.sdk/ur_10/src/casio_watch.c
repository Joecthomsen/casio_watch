/*
 * interrupt_counter_tut_2B.c
 *
 *  Version 1.2 Author : Edward Todirica
 *
 *  Created on: 	Unknown
 *      Author: 	Ross Elliot
 *     Version:		1.1
 */

/********************************************************************************************

* VERSION HISTORY
********************************************************************************************
*   v1.2 - 10.11.2016
*		Fixed some bugs regarding Timer Interrupts and adding some
*       debug messages for the Timer Interrupt Handler
*
* 	v1.1 - 01/05/2015
* 		Updated for Zybo ~ DN
*
*	v1.0 - Unknown
*		First version created.
*******************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include "xparameters.h"
#include "xgpio.h"
#include "xtmrctr.h"
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xtime_l.h"

// Parameter definitions
#define INTC_DEVICE_ID 		XPAR_PS7_SCUGIC_0_DEVICE_ID
#define TMR_DEVICE_ID		XPAR_TMRCTR_0_DEVICE_ID
#define STP_WTC_DEVICE_ID   XPAR_TMRCTR_1_DEVICE_ID
#define BTNS_DEVICE_ID		XPAR_AXI_GPIO_0_DEVICE_ID
#define SW_DEVICE_ID		XPAR_AXI_GPIO_1_DEVICE_ID
#define LEDS_DEVICE_ID		XPAR_AXI_GPIO_2_DEVICE_ID
#define BUZZER_DEVICE_ID	XPAR_AXI_GPIO_3_DEVICE_ID

//Interrupt controller definitions
#define INTC_GPIO_INTERRUPT_BTNS XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define INTC_TMR_INTERRUPT_ID XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR
#define INTC_GPIO_INTERRUPT_SW XPAR_FABRIC_AXI_GPIO_1_IP2INTC_IRPT_INTR
#define INTC_STP_WTC_INTERRUPT_ID XPAR_FABRIC_AXI_TIMER_1_INTERRUPT_INTR

#define BTN_INT 			XGPIO_IR_CH1_MASK
#define SW_INT 				XGPIO_IR_CH1_MASK

int timerLoad = 100000000;

//----------------------------------------------------
// CREATE INSTANCES OF THE DEVICES
//----------------------------------------------------
XGpio LEDInst, BTNInst, SWInst, BUZInst; 		// General Purpose I/O (XGpio) device driver.
XScuGic INTCInst;								//Generic Interrupt controller driver component.
XTmrCtr TMRInst, StpWtcTMRInst; 				//Timer counter component

int led_data;
int btn_value;
int sw_value;

int alarmTimeCounter = 0;

//Set the initial alarm status to OFF. If the alarm status is on, the alarm will sound at the selected time.
enum alarmStatus {OFF, DAILY_TIME_AND_ALARM_ON, ALARM_ON, DAILY_TIME_ON,};
int alarmStatus = ALARM_ON;

//Time modes
enum timeModes {TWELVE_HOUR_TIME_FORMAT , TWENTYFOUR_HOUR_TIME_FORMATE};
int timemode = TWENTYFOUR_HOUR_TIME_FORMATE;

//The states for the watch. initially the watch is in clock mode. (TIME mode)
enum states {CLOCK, ALARM_SETTING, SET_ALARM_HOUR, SET_ALARM_MINUTES, ALARM_IS_ACTIVE ,STOPWATCH ,
			STOPWATCH_RUNNING , SETTINGS , SET_CLOCK_HOUR , SET_CLOCK_MINUTE , SET_MONTH};	//Watch states
int state = CLOCK;

enum ledStates {LED_CLOCK_MODE = 0x01 , LED_ALARM_MODE = 0x02 , LED_STOP_WATCH_MODE = 0x04, LED_SETTING_MODE = 0x08};

// Values for the ZYBO button vector
enum buttons {A = 0x01, C = 0x04, L = 0x08};

//month values
enum month {Jan , Feb , Mar, Apr, May , Jun, Jul, Aug, Sep, Oct, Nov , Dec};
int month = Jan;

int counter = 0;	//Counter for the alarm

// The clock 24 hour format
	int sec0 = 0;
	int sec10 = 0;
	int minut0 = 0;
	int minut10 = 0;
	int hour0 = 0;
	int hour10 = 0;

// The clock 12 hour format
	int hour12format;

//Alarm time
	int alarmSec0 = 0;
	int alarmSec10 = 0;
	int alarmMinut0 = 1;
	int alarmMinut10 = 0;
	int alarmHour0 = 0;
	int alarmHour10 = 0;
	int alarmMode = 0;

//Stop Watch time
	int swMinut10 = 0;
	int swMinut0 = 0;
	int swSec10 = 0;
	int swSec0 = 0;
	int swCenSec10 = 0;
	int swCenSec0 = 0;

//These instances provides access to the global time counter.
XTime tStart, tEnd;

//----------------------------------------------------
// PROTOTYPE FUNCTIONS
//----------------------------------------------------

int InterruptSystemSetup(XScuGic *XScuGicInstancePtr);
int IntcInitFunction(u16 DeviceId, XTmrCtr *TmrInstancePtr, XTmrCtr *StpWtcInstancePtr , XGpio *GpioInstanceBtnsPtr,  XGpio *GpioInstanceSwPtr);
void XTmrCtr_ClearInterruptFlag(XTmrCtr * InstancePtr, u8 TmrCtrNumber);
void alarmState();
void setAlarmModes();
void setAlarm();
void clearScreen();
void printTime();
void setAlarmMinutes();
void setAlarmHour();
void setAlarmMinut();
void printAlarm();
void printAlarmStatus();
void printState();
void printStopWatch();
void reInitTimer();
void incrementStopWatch();
void resetStopWatch();
void setClockHour();
void setClockMinute();
char *getMonth(int month);
void setMonth();
void printToDisplay();
void dailyTimeSignal();
void alarmSound();

//----------------------------------------------------
// PROTOTYPE INTERRUPT HANDLERS
//----------------------------------------------------

void BTN_Intr_Handler(void *baseaddr_p);
void SW_Intr_Handler(void *baseaddr_p);
void TMR_Intr_Handler(void *InstancePtr, u8 TmrCtrNumber);
void STP_WCH_TMR_Intr_Handler(void *InstancePtr, u8 TmrCtrNumber);

/*****************************************************************************/
/**
* This function should be part of the device driver for the timer device
* Clears the interrupt flag of the specified timer counter of the device.
* This is necessary to do in the interrupt routine after the interrupt was handled.
*
* @param	InstancePtr is a pointer to the XTmrCtr instance.
* @param	TmrCtrNumber is the timer counter of the device to operate on.
*		Each device may contain multiple timer counters. The timer
*		number is a zero based number  with a range of
*		0 - (XTC_DEVICE_TIMER_COUNT - 1).
*
* @return	None.
*
* @note		None.
*
******************************************************************************/


//-----------------------------------------------------------------------
//							CLEAR INTERRUPTFLAG FUNCTION
//-----------------------------------------------------------------------

void XTmrCtr_ClearInterruptFlag(XTmrCtr * InstancePtr, u8 TmrCtrNumber){

	u32 CounterControlReg;

	Xil_AssertVoid(InstancePtr != NULL);
	Xil_AssertVoid(TmrCtrNumber < XTC_DEVICE_TIMER_COUNT);
	Xil_AssertVoid(InstancePtr->IsReady == XIL_COMPONENT_IS_READY);

	/*
	 * Read current contents of the CSR register so it won't be destroyed
	 */
	CounterControlReg = XTmrCtr_ReadReg(InstancePtr->BaseAddress,
					       TmrCtrNumber, XTC_TCSR_OFFSET);
	/*
	 * Reset the interrupt flag
	 */
	XTmrCtr_WriteReg(InstancePtr->BaseAddress, TmrCtrNumber,
			  XTC_TCSR_OFFSET,
			  CounterControlReg | XTC_CSR_INT_OCCURED_MASK);
}


//-----------------------------------------------------------------------
//							MAIN FUNCTION
//-----------------------------------------------------------------------
int main (void)
{
  int status;
  //XTmrCtr TMRInst;
  //----------------------------------------------------
  // INITIALIZE THE PERIPHERALS & SET DIRECTIONS OF GPIO
  //----------------------------------------------------
  // Initialise LEDs
  status = XGpio_Initialize(&LEDInst, LEDS_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;

  // Initialise Push Buttons
  status = XGpio_Initialize(&BTNInst, BTNS_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;

  // Initialize Switches
  status = XGpio_Initialize(&SWInst, SW_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;

  // Initialize buzzer
  status = XGpio_Initialize(&BUZInst, BUZZER_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;

  // Set LEDs direction to outputs
  XGpio_SetDataDirection(&LEDInst, 1, 0x00);
  // Set all buttons direction to inputs
  XGpio_SetDataDirection(&BTNInst, 1, 0xFF);
  // Set Switches to input
  XGpio_SetDataDirection(&SWInst, 1, 0xFF);
  // Set Buzzer direction to output
  XGpio_SetDataDirection(&BUZInst, 1, 0x00);


  //----------------------------------------------------
  // SETUP THE CLOCK TIMER
  //----------------------------------------------------
  status = XTmrCtr_Initialize(&TMRInst, TMR_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;
  XTmrCtr_SetHandler(&TMRInst, TMR_Intr_Handler, &TMRInst);
  XTmrCtr_SetResetValue(&TMRInst, 0, timerLoad);
  XTmrCtr_SetOptions(&TMRInst, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);

  //----------------------------------------------------
  // SETUP THE STOP WATCH TIMER
  //----------------------------------------------------
  status = XTmrCtr_Initialize(&StpWtcTMRInst, STP_WTC_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;
  XTmrCtr_SetHandler(&StpWtcTMRInst, STP_WCH_TMR_Intr_Handler, &StpWtcTMRInst);
  XTmrCtr_SetResetValue(&StpWtcTMRInst, 0, timerLoad/100);
  XTmrCtr_SetOptions(&StpWtcTMRInst, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);

  // Initialize interrupt controller
  status = IntcInitFunction(INTC_DEVICE_ID, &TMRInst, &StpWtcTMRInst ,&BTNInst, &SWInst);
  if(status != XST_SUCCESS) return XST_FAILURE;

  XTmrCtr_Start(&TMRInst, 0);
  XTmrCtr_Start(&StpWtcTMRInst, 0);
  //Here we get the time when the timer first started
  XTime_GetTime(&tStart);
  XGpio_DiscreteWrite(&LEDInst, 1, LED_CLOCK_MODE);

  while(1){}
  return 0;
}

//----------------------------------------------------
// INTERRUPT HANDLER FUNCTIONS
// - called by the timer, button and switches
//----------------------------------------------------


void SW_Intr_Handler(void *InstancePtr){


	//XGpio* pSWInst = (XGpio *) InstancePtr;
	 //Disable GPIO interrupts
	XGpio_InterruptDisable(&SWInst, SW_INT);
	 //Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&SWInst) & SW_INT) != SW_INT)
	{
		return;
	}


	sw_value = XGpio_DiscreteRead(&SWInst, 1);

	if(sw_value == 0x00)
	{
		XGpio_InterruptClear(&SWInst, SW_INT);
		timerLoad = 100000000;
		reInitTimer();
	}

	else if(sw_value == 0x01)
	{
		XGpio_InterruptClear(&SWInst, SW_INT);
		timerLoad = 100000000/10;
		reInitTimer();
	}

	else if(sw_value == 0x02)
	{
		XGpio_InterruptClear(&SWInst, SW_INT);
		timerLoad = 100000000/20;
		reInitTimer();
	}

	else if(sw_value == 0x04)
	{
		XGpio_InterruptClear(&SWInst, SW_INT);
		timerLoad = 100000000/30;
		reInitTimer();
	}

	else if(sw_value == 0x08)
	{
		XGpio_InterruptClear(&SWInst, SW_INT);
		timerLoad = 100000000/60;
		reInitTimer();
	}


	//XGpio_InterruptClear(&SWInst, 1); //Kig på denne
	XGpio_InterruptEnable(&SWInst, SW_INT);
    XGpio_InterruptEnable(&BTNInst, BTN_INT);
}



void BTN_Intr_Handler(void *InstancePtr)
{
	// Disable GPIO interrupts
	XGpio_InterruptDisable(&BTNInst, BTN_INT);
	// Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&BTNInst) & BTN_INT) !=
			BTN_INT) {
			return;
		}
	btn_value = XGpio_DiscreteRead(&BTNInst, 1);
	// Increment counter based on button value
	// Reset if centre button pressed
	//led_data = led_data + btn_value;

	switch(state){

		case CLOCK:
		{
			if(btn_value == C)
			{
				XGpio_DiscreteWrite(&LEDInst, 1, LED_ALARM_MODE);
				state = ALARM_SETTING;
				printToDisplay();
				while(btn_value == C){btn_value = XGpio_DiscreteRead(&BTNInst, 1);};
				XGpio_InterruptEnable(&BTNInst, BTN_INT);

			}

			else if(btn_value == A)
			{
				if(timemode == TWENTYFOUR_HOUR_TIME_FORMATE)
				{
					timemode = TWELVE_HOUR_TIME_FORMAT;
				}
				else
				{
					timemode = TWENTYFOUR_HOUR_TIME_FORMATE;
				}
			}
			else if(btn_value == L)
			{
				XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
			}
			break;
		}

		case ALARM_SETTING:
		{
			if(btn_value == L)
			{
				printToDisplay();
				state = SET_ALARM_HOUR;
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);};
				XGpio_InterruptEnable(&BTNInst, BTN_INT);
			}

			else if(btn_value == C)
			{
				XGpio_DiscreteWrite(&LEDInst, 1, LED_STOP_WATCH_MODE);
				state = STOPWATCH;
				printToDisplay();
				while(btn_value == C){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}

			else if(btn_value == A)	//If A is hold for more than 2 seconds, the alarm-test will sound for 2 seconds.
			{
				double duration = 0.0;
				XTime_GetTime(&tStart);

				while(btn_value == A && duration < 2.0){
					XTime_GetTime(&tEnd);
					duration = ((double)(tEnd-tStart))/COUNTS_PER_SECOND;
					btn_value = XGpio_DiscreteRead(&BTNInst, 1);
				};


				if(duration < 2.0)
				{
					xil_printf("%f",duration);
					alarmStatus++;
					if(alarmStatus > DAILY_TIME_ON){alarmStatus = OFF;}
					while(btn_value == A){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
					printToDisplay();
				}
				else
				{
					duration = 0.0;
					XGpio_DiscreteWrite(&BUZInst, 1, 0x01);
					XTime_GetTime(&tStart);

					while(duration < 2.0){
						XTime_GetTime(&tEnd);
						duration = ((double)(tEnd-tStart))/COUNTS_PER_SECOND;
					}
					XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
				}
			}
			break;
		}

		case ALARM_IS_ACTIVE:
		{
			if(btn_value == L)
			{
				XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
				state = CLOCK;
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
				printToDisplay();
			}
			break;
		}

		case SET_ALARM_HOUR:
		{
			if(btn_value == A){
				setAlarmHour();
			}
			else if(btn_value == L)
			{
				printToDisplay();
				state = SET_ALARM_MINUTES;
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);};
			}
			break;
		}

		case SET_ALARM_MINUTES:
		{
			if(btn_value == A)
			{
				setAlarmMinutes();
			}
			else if(btn_value == L)
			{
				state = ALARM_SETTING;
				printToDisplay();
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);};
			}
			break;
		}

		case STOPWATCH:
		{
			printToDisplay();

			if(btn_value == A)
			{
				XTmrCtr_Start(&TMRInst, 0);
				XScuGic_Enable(&INTCInst, INTC_STP_WTC_INTERRUPT_ID);
				while(btn_value == A){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
				state = STOPWATCH_RUNNING;
			}

			else if(btn_value == L)
			{
				resetStopWatch();
				printToDisplay();
			}

			else if(btn_value == C)
			{
				XGpio_DiscreteWrite(&LEDInst, 1, LED_SETTING_MODE);
				state = SETTINGS;
				printToDisplay();
				while(btn_value == C){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}
			break;
		}

		case STOPWATCH_RUNNING:
		{
			if(btn_value == A)
			{
				printToDisplay();
				XScuGic_Disable(&INTCInst, INTC_STP_WTC_INTERRUPT_ID);
				while(btn_value == A){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
				state = STOPWATCH;
			}
			break;
		}

		case SETTINGS:
		{
			printToDisplay();

			if(btn_value == A)
			{
				sec0 = 0;
				sec10 = 0;
				printToDisplay();
				while(btn_value == A){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}

			else if(btn_value == L)
			{
				state = SET_CLOCK_HOUR;
				printToDisplay();
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}

			else if(btn_value == C)
			{
				XGpio_DiscreteWrite(&LEDInst, 1, LED_CLOCK_MODE);
				state = CLOCK;
				printToDisplay();
				while(btn_value == C){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}
			break;
		}

		case SET_CLOCK_HOUR:
		{
			printToDisplay();
			if(btn_value == A)
			{
				setClockHour();
				printToDisplay();
				while(btn_value == A){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}

			else if(btn_value == L)
			{
				state = SET_CLOCK_MINUTE;
				printToDisplay();
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}
			break;
		}

		case SET_CLOCK_MINUTE:
		{
			if(btn_value == A)
			{
				setClockMinute();
				printToDisplay();
				while(btn_value == A){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}

			else if(btn_value == L)
			{
				state = SET_MONTH;
				printToDisplay();
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}
			break;
		}

		case SET_MONTH:
		{
			if(btn_value == L)
			{
				state = SETTINGS;
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}
			else if(btn_value == A)
			{
				setMonth();
				printToDisplay();
				while(btn_value == A){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}
			break;
		}
	}

    (void)XGpio_InterruptClear(&BTNInst, BTN_INT);
    // Enable GPIO interrupts
    XGpio_InterruptEnable(&BTNInst, BTN_INT);
}

void TMR_Intr_Handler(void *InstancePtr, u8 TmrCtrNumber)
{

	XTime_GetTime(&tEnd);
	XTmrCtr* pTMRInst = (XTmrCtr *) InstancePtr;


	if (TmrCtrNumber==0) { //Handle interrupts generated by timer 0

		tStart=tEnd;

		if (XTmrCtr_IsExpired(pTMRInst,0)){

				XTmrCtr_Stop(pTMRInst,0);
				XTmrCtr_Reset(pTMRInst,0);
				XTmrCtr_Start(pTMRInst,0);

				if(sec0 > 9){
					sec0 = 0;
					sec10++;
				}
				if(sec10 > 5){
					sec10 = 0;
					minut0++;
				}
				if(minut0 > 9){
					minut0 = 0;
					minut10++;
				}
				if(minut10 > 5){
					if((state == CLOCK && alarmStatus == DAILY_TIME_AND_ALARM_ON) || (state == CLOCK && alarmStatus == DAILY_TIME_ON))
					{
						dailyTimeSignal();
					}
					minut10 = 0;
					hour0++;
				}
				if(hour0 > 9 && hour10 < 2){
					hour0 = 0;
					hour10++;
				}
				if (hour10 == 2 && hour0 > 4){
					hour10 = 0;
					hour0 = 0;
				}
			}
		printToDisplay();
		sec0++;

		if(state == ALARM_IS_ACTIVE){
			if(alarmTimeCounter < 19)
			{
				alarmTimeCounter++;
			}
			else
			{
				alarmTimeCounter = 0;
				XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
				state = CLOCK;
			}
		}
	}
}

void STP_WCH_TMR_Intr_Handler(void *InstancePtr, u8 TmrCtrNumber){


		XTime_GetTime(&tEnd);
		XTmrCtr* pTMRInst = (XTmrCtr *) InstancePtr;

		if (TmrCtrNumber==0) { //Handle interrupts generated by timer 0

			tStart=tEnd;

			if (XTmrCtr_IsExpired(pTMRInst,0)){

					XTmrCtr_Stop(pTMRInst,0);
					led_data++;
					XTmrCtr_Reset(pTMRInst,0);
					XTmrCtr_Start(pTMRInst,0);
					clearScreen();
					printAlarmStatus();
					printState();
					xil_printf("\n\r");
					incrementStopWatch();
					printStopWatch();
				}

			}
}

//----------------------------------------------------
// INITIAL SETUP FUNCTIONS
//----------------------------------------------------

int InterruptSystemSetup(XScuGic *XScuGicInstancePtr)
{
	// Enable interrupt
	XGpio_InterruptEnable(&BTNInst, BTN_INT);
	XGpio_InterruptGlobalEnable(&BTNInst);
	XGpio_InterruptEnable(&SWInst, SW_INT);
	XGpio_InterruptGlobalEnable(&SWInst);

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
	 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
	 XScuGicInstancePtr);
	Xil_ExceptionEnable();


	return XST_SUCCESS;

}



int IntcInitFunction(u16 DeviceId, XTmrCtr *TmrInstancePtr, XTmrCtr *StpWtcInstancePtr ,XGpio *GpioInstanceBtnsPtr, XGpio *GpioInstanceSwPtr)
{
	XScuGic_Config *IntcConfig;
	int status;
	u8 pri, trig;

	// Interrupt controller initialisation
	IntcConfig = XScuGic_LookupConfig(DeviceId);
	status = XScuGic_CfgInitialize(&INTCInst, IntcConfig, IntcConfig->CpuBaseAddress);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Call to interrupt setup
	status = InterruptSystemSetup(&INTCInst);
	if(status != XST_SUCCESS) return XST_FAILURE;
	
	// Connect GPIO BTNS interrupt to handler
	status = XScuGic_Connect(&INTCInst,
					  	  	 INTC_GPIO_INTERRUPT_BTNS,
					  	  	 (Xil_ExceptionHandler)BTN_Intr_Handler,
					  	  	 (void *)GpioInstanceBtnsPtr);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Connect GPIO SW interrupt to handler
	status = XScuGic_Connect(&INTCInst,
							 INTC_GPIO_INTERRUPT_SW,
							 (Xil_ExceptionHandler)SW_Intr_Handler,
							 (void *)GpioInstanceSwPtr);
	if(status != XST_SUCCESS) return XST_FAILURE;


	// Connect timer interrupt to handler
	status = XScuGic_Connect(&INTCInst,
							 INTC_TMR_INTERRUPT_ID,
							// (Xil_ExceptionHandler)TMR_Intr_Handler,
							 (Xil_ExceptionHandler) XTmrCtr_InterruptHandler,
							 (void *)TmrInstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Connect stop watch interrupt to handler
	status = XScuGic_Connect(&INTCInst,
							INTC_STP_WTC_INTERRUPT_ID,
							// (Xil_ExceptionHandler)TMR_Intr_Handler,
							 (Xil_ExceptionHandler) XTmrCtr_InterruptHandler,
							 (void *)StpWtcInstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Enable GPIO interrupts interrupt
	XGpio_InterruptEnable(GpioInstanceBtnsPtr, 1);
	XGpio_InterruptGlobalEnable(GpioInstanceBtnsPtr);

	XGpio_InterruptEnable(GpioInstanceSwPtr, 1);
	XGpio_InterruptGlobalEnable(GpioInstanceSwPtr);

	// Enable GPIO, switches and timer interrupts in the controller
	XScuGic_Enable(&INTCInst, INTC_GPIO_INTERRUPT_BTNS);
	XScuGic_Enable(&INTCInst, INTC_GPIO_INTERRUPT_SW);
	XScuGic_Enable(&INTCInst, INTC_TMR_INTERRUPT_ID);

	//XScuGic_Enable(&INTCInst, INTC_STP_WTC_INTERRUPT_ID);

	//xil_printf("Getting the Timer interrupt info\n\r");
	XScuGic_GetPriTrigTypeByDistAddr(INTCInst.Config->DistBaseAddress, INTC_TMR_INTERRUPT_ID, &pri, &trig);
	XScuGic_GetPriTrigTypeByDistAddr(INTCInst.Config->DistBaseAddress, INTC_STP_WTC_INTERRUPT_ID, &pri, &trig);
	//xil_printf("GPIO Interrupt-> Priority:%d, Trigger:%x\n\r", pri, trig);

	
	//Set the timer interrupt as edge triggered
	XScuGic_SetPriorityTriggerType(&INTCInst, INTC_TMR_INTERRUPT_ID,0, 0);
	XScuGic_SetPriorityTriggerType(&INTCInst, INTC_STP_WTC_INTERRUPT_ID,0, 0);

	//XScuGic_SetPriorityTriggerType(&SWInst, INTC_GPIO_INTERRUPT_SW,8, XGPIOPS_IRQ_TYPE_EDGE_RISING);

	return XST_SUCCESS;
}


void setAlarmMinutes(){

	alarmMinut0++;
	if(alarmMinut0 > 9){
		alarmMinut10++;
		alarmMinut0 = 0;
	}
	if(alarmMinut10 > 5){
		alarmMinut10 = 0;
	}
	clearScreen();
	printAlarmStatus();
	printState();
	xil_printf("Set Minutes\n\r");
	printAlarm();
}


void setAlarmHour(){

	alarmHour0++;
	if(alarmHour0 > 9){
		alarmHour10++;
		alarmHour0 = 0;
	}
	if(alarmHour10 > 2){
		alarmHour10 = 0;
	}
	clearScreen();
	printAlarmStatus();
	printState();
	xil_printf("Set Hour\n\r");
	printAlarm();
}


void setClockHour(){
	hour0++;
	if(hour0 > 9)
	{
		hour0 = 0;
		hour10++;
	}
	if(hour10 > 2)
	{
		hour10 = 0;
	}
}


void setClockMinute(){

	minut0++;
	if(minut0 > 9)
	{
		minut0 = 0;
		minut10++;
	}
	if(minut10 > 5)
	{
		minut10 = 0;
	}
}


void printTime(){
	if(timemode == TWENTYFOUR_HOUR_TIME_FORMATE){
	xil_printf("%i%i : %i%i : %i%i  %s",hour10,hour0, minut10,minut0, sec10, sec0 , getMonth(month));
	}
	else
	{
		if(hour10 == 1 && hour0 > 2)
		{
			hour12format = (hour0 + 10) - 12;
			xil_printf("%i : %i%i : %i%i PM  %s",hour12format, minut10,minut0, sec10, sec0 , getMonth(month));
		}
		else if(hour10 == 2)
		{
			hour12format = (hour0+10) - 2;
			xil_printf("%i : %i%i : %i%i PM  %s",hour12format, minut10,minut0, sec10, sec0 , getMonth(month));
		}
		else
		{
			xil_printf("%i%i : %i%i : %i%i AM  %s",hour10,hour0, minut10,minut0, sec10, sec0 , getMonth(month));
		}
	}
}


void printAlarm(){
	xil_printf("%i%i : %i%i : %i%i",alarmHour10, alarmHour0, alarmMinut10, alarmMinut0, alarmSec10, alarmSec0);
}


void clearScreen(){


	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");
	xil_printf("\033[3J\n\r");


	//xil_printf("\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\r");
}

void printAlarmStatus(){
	if(alarmStatus == OFF){xil_printf("\n\r");}
	else if(alarmStatus == DAILY_TIME_AND_ALARM_ON){xil_printf("DTS  ALRM\n\r");}
	else if(alarmStatus == ALARM_ON){xil_printf("     ALRM\n\r");}
	else if(alarmStatus == DAILY_TIME_ON){xil_printf("DTS\n\r");}
}


void printStopWatch(){
	xil_printf("%i%i : %i%i : %i%i",swMinut10, swMinut0, swSec10, swSec0, swCenSec10, swCenSec0);
}



void printState(){
	if(state == CLOCK || state == ALARM_IS_ACTIVE)
	{
		xil_printf("Clock mode\n\r");
	}
	else if(state == ALARM_SETTING || state == SET_ALARM_MINUTES || state == SET_ALARM_HOUR)
	{
		xil_printf("Alarm mode\n\r");
	}
	else if(state == STOPWATCH || state == STOPWATCH_RUNNING)
	{
		xil_printf("Stop watch mode\n\r");
	}
	else if(state == SETTINGS || SET_CLOCK_HOUR || SET_CLOCK_MINUTE || SET_MONTH){
		xil_printf("Settings\n\r");
	}

}

void reInitTimer(){
	  int status;
	  status = XTmrCtr_Initialize(&TMRInst, TMR_DEVICE_ID);
	  if(status != XST_SUCCESS) return XST_FAILURE;
	  XTmrCtr_SetHandler(&TMRInst, TMR_Intr_Handler, &TMRInst);
	  XTmrCtr_SetResetValue(&TMRInst, 0, timerLoad);	/////////////////////////////////////////////////////////////////////
	  XTmrCtr_SetOptions(&TMRInst, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);

	  // Initialize interrupt controller
	  status = IntcInitFunction(INTC_DEVICE_ID, &TMRInst, &StpWtcTMRInst , &BTNInst, &SWInst);
	  if(status != XST_SUCCESS) return XST_FAILURE;

	  XTmrCtr_Start(&TMRInst, 0);
	  //Here we get the time when the timer first started
	  XTime_GetTime(&tStart);
}

void incrementStopWatch(){

	swCenSec0++;

	if(swCenSec0 > 9)
	{
		swCenSec0 = 0;
		swCenSec10++;
	}
	if(swCenSec10 > 9)
	{
		swCenSec10 = 0;
		swSec0++;
	}
	if(swSec0 > 9)
	{
		swSec0 = 0;
		swSec10++;
	}
	if(swSec10 > 5)
	{
		swSec10 = 0;
		swMinut0++;
	}
	if(swMinut0 > 9)
	{
		swMinut0 = 0;
		swMinut10++;
	}
	if(swMinut10 > 5)
	{
		swMinut10 = 0;
	}
}

void resetStopWatch(){

	swCenSec0 = 0;
	swCenSec10 = 0;
	swSec0 = 0;
	swSec10 = 0;
	swMinut0 = 0;
	swMinut10 = 0;
}

char *getMonth(int month){

	switch(month)
	{
	case Jan: return "Jan"; break;
	case Feb: return "Feb"; break;
	case Mar: return "Mar"; break;
	case Apr: return "Apr"; break;
	case May: return "May"; break;
	case Jun: return "Jun"; break;
	case Jul: return "Jul"; break;
	case Aug: return "Aug"; break;
	case Sep: return "Sep"; break;
	case Oct: return "Oct"; break;
	case Nov: return "Nov"; break;
	case Dec: return "Dec"; break;
	}
	return NULL;
}

void setMonth(){

	month++;
	if(month > Dec){month = Jan;}
}

void printToDisplay(){


	if(state == CLOCK && alarmStatus == OFF)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("\n\r");
		printTime();
	}

	else if((state == CLOCK && alarmStatus == ALARM_ON) || (state == CLOCK && alarmStatus == DAILY_TIME_AND_ALARM_ON))
	{
		if(alarmHour10 == hour10 && alarmHour0 == hour0 && alarmMinut10 == minut10 && alarmMinut0 == minut0 && sec10 < 2 && sec0 == 0 && sec10 < 1)
		{
			state = ALARM_IS_ACTIVE;
			clearScreen();
			printAlarmStatus();
			printState();
			xil_printf("ALARM!!!!!\n\r");
			XGpio_DiscreteWrite(&BUZInst, 1, 0x01);
			//alarmSound();
			printTime();
		}

		else
		{
			XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
			clearScreen();
			printAlarmStatus();
			printState();
			xil_printf("\n\r");
			printTime();
		}
	}

	else if(state == ALARM_IS_ACTIVE)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("ALARM!!!!!\n\r");
		printTime();
	}

	else if(state == CLOCK && alarmStatus == DAILY_TIME_ON)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("\n\r");
		printTime();
	}

	else if(state == CLOCK && alarmStatus ==  DAILY_TIME_AND_ALARM_ON)
	{
		if(alarmHour10 == hour10 && alarmHour0 == hour0 && alarmMinut10 == minut10 && alarmMinut0 == minut0 && sec10 < 2 && sec0 == 0 && sec10 < 1)
		{
			clearScreen();
			printAlarmStatus();
			printState();
			xil_printf("ALARM!!!!!\n\r");
			XGpio_DiscreteWrite(&BUZInst, 1, 0x01);
			//alarmSound();
			printTime();
		}
	}

	else if(state == ALARM_SETTING)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("\n\r");
		printAlarm();
	}

	else if(state == SET_ALARM_HOUR)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("Set Alarm Hour\n\r");
		printAlarm();
	}

	else if(state == SET_ALARM_MINUTES)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("Set Alarm Minutes\n\r");
		printAlarm();
	}

	else if(state == STOPWATCH)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("\n\r");
		printStopWatch();
	}

	else if(state == STOPWATCH_RUNNING)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("\n\r");
		printStopWatch();
	}

	else if(state == SETTINGS)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("Set seconds\n\r");
		printTime();
	}

	else if(state == SET_CLOCK_HOUR)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("Set hour\n\r");
		printTime();
	}

	else if(state == SET_CLOCK_MINUTE)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("Set minute\n\r");
		printTime();
	}

	else if(state == SET_MONTH)
	{
		clearScreen();
		printAlarmStatus();
		printState();
		xil_printf("Set month\n\r");
		printTime();
	}
}

void dailyTimeSignal(){
	XGpio_DiscreteWrite(&BUZInst, 1, 0x01);
	usleep(100000);
	XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
}

void alarmSound(){
	XGpio_DiscreteWrite(&BUZInst, 1, 0x01);
	usleep(100000);
	XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
	usleep(100000);
	XGpio_DiscreteWrite(&BUZInst, 1, 0x01);
	usleep(100000);
	XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
	usleep(100000);
	XGpio_DiscreteWrite(&BUZInst, 1, 0x01);
	usleep(100000);
	XGpio_DiscreteWrite(&BUZInst, 1, 0x00);
	usleep(700000);
}


