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
#define BTNS_DEVICE_ID		XPAR_AXI_GPIO_0_DEVICE_ID
#define SW_DEVICE_ID		XPAR_AXI_GPIO_1_DEVICE_ID
#define LEDS_DEVICE_ID		XPAR_AXI_GPIO_2_DEVICE_ID
#define INTC_GPIO_INTERRUPT_BTNS XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR
#define INTC_TMR_INTERRUPT_ID XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR
#define INTC_GPIO_INTERRUPT_SW XPAR_FABRIC_AXI_GPIO_1_IP2INTC_IRPT_INTR

#define BTN_INT 			XGPIO_IR_CH1_MASK
#define SW_INT 			XGPIO_IR_CH1_MASK
//#define TMR_LOAD			0xF8000000
//#define TMR_LOAD			100000000

int timerLoad = 100000000;

XGpio LEDInst, BTNInst, SWInst;
XScuGic INTCInst;
XTmrCtr TMRInst;
int led_data;
int btn_value;
int sw_value;
//static int tmr_count;

enum alarmStatus {OFF, ON};
int alarmStatus = OFF;
enum states {TIME, ALARM, SET_ALARM_HOUR, SET_ALARM_MINUTES, STOPWATCH, SETTINGS};	//Watch states
int state = TIME;
enum buttons {A = 0x01, C = 0x04, L = 0x08};


// The clock
	int sec0 = 0;
	int sec10 = 0;
	int minut0 = 0;
	int minut10 = 0;
	int hour0 = 0;
	int hour10 = 0;

//alarm
	int alarmSec0 = 0;
	int alarmSec10 = 0;
	int alarmMinut0 = 0;
	int alarmMinut10 = 0;
	int alarmHour0 = 0;
	int alarmHour10 = 0;
	int alarmMode = 0;

//Stop Watch

	int swMinut10 = 0;
	int swMinut0 = 0;
	int swSec10 = 0;
	int swSec0 = 0;
	int swCenSec10 = 0;
	int swCenSec0 = 0;

XTime tStart, tEnd;

//----------------------------------------------------
// PROTOTYPE FUNCTIONS
//----------------------------------------------------

int InterruptSystemSetup(XScuGic *XScuGicInstancePtr);
int IntcInitFunction(u16 DeviceId, XTmrCtr *TmrInstancePtr, XGpio *GpioInstanceBtnsPtr,  XGpio *GpioInstanceSwPtr);
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

//----------------------------------------------------
// PROTOTYPE INTERRUPT HANDLERS
//----------------------------------------------------

void BTN_Intr_Handler(void *baseaddr_p);
void SW_Intr_Handler(void *baseaddr_p);
void TMR_Intr_Handler(void *InstancePtr, u8 TmrCtrNumber);

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
  XTmrCtr TMRInst;
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

  // Set LEDs direction to outputs
  XGpio_SetDataDirection(&LEDInst, 1, 0x00);
  // Set all buttons direction to inputs
  XGpio_SetDataDirection(&BTNInst, 1, 0xFF);
  // Set Switches to input
  XGpio_SetDataDirection(&SWInst, 1, 0xFF);


  //----------------------------------------------------
  // SETUP THE TIMER
  //----------------------------------------------------
  status = XTmrCtr_Initialize(&TMRInst, TMR_DEVICE_ID);
  if(status != XST_SUCCESS) return XST_FAILURE;
  XTmrCtr_SetHandler(&TMRInst, TMR_Intr_Handler, &TMRInst);
  XTmrCtr_SetResetValue(&TMRInst, 0, timerLoad);	/////////////////////////////////////////////////////////////////////
  XTmrCtr_SetOptions(&TMRInst, 0, XTC_INT_MODE_OPTION | XTC_AUTO_RELOAD_OPTION | XTC_DOWN_COUNT_OPTION);

  // Initialize interrupt controller
  status = IntcInitFunction(INTC_DEVICE_ID, &TMRInst, &BTNInst, &SWInst);
  if(status != XST_SUCCESS) return XST_FAILURE;

  XTmrCtr_Start(&TMRInst, 0);
  //Here we get the time when the timer first started
  XTime_GetTime(&tStart);

  while(1){	// Der stod kun while(1);
	  switch(state)
	  {
	    case TIME: while(state == TIME){};	//Just show the current time.
	    case ALARM:	while(1){} //alarmState();
	    break;
	    case SET_ALARM_HOUR: while(1);
	    case SET_ALARM_MINUTES: while(1);
	    case STOPWATCH: while(1);
	    break;
	    case SETTINGS: while(1);
	    break;
	  }

  }
  return 0;
}

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

		case TIME:
		{
			if(btn_value == C){
			state = ALARM;
			clearScreen();
			printAlarmStatus();
			printState();
			xil_printf("\n\r");
			printAlarm();
			while(btn_value == C){btn_value = XGpio_DiscreteRead(&BTNInst, 1);};
			XGpio_InterruptEnable(&BTNInst, BTN_INT);
			break;
			}
		}
		case ALARM:
		{
			if(btn_value == L)
			{
				clearScreen();
				printAlarmStatus();
				printState();
				xil_printf("Set Hour\n\r");
				printAlarm();
				state = SET_ALARM_HOUR;

				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);};
				XGpio_InterruptEnable(&BTNInst, BTN_INT);
			}

			else if(btn_value == C)
			{
				state = STOPWATCH;
				clearScreen();
				printAlarmStatus();
				printState();
				xil_printf("\n\r");
				printStopWatch();
				while(btn_value == C){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}

			else if(btn_value == A)
			{
				if(alarmStatus == OFF){alarmStatus = ON;}
				else
					alarmStatus = OFF;

				clearScreen();
				printAlarmStatus();
				printState();
				xil_printf("\n\r");
				printAlarm();
			}
			break;
		}

		case SET_ALARM_HOUR:
		{
			if(btn_value == A){
				setAlarmHour();
			}
			else if(btn_value == L){
				clearScreen();
				printAlarmStatus();
				printState();
				xil_printf("Set minute\n\r");
				printAlarm();
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
				state = ALARM;
				clearScreen();
				printAlarmStatus();
				printState();
				xil_printf("\n\r");
				printAlarm();
				while(btn_value == L){btn_value = XGpio_DiscreteRead(&BTNInst, 1);};
			}
			break;
		}

		case STOPWATCH:
		{
			clearScreen();
			printAlarmStatus();
			printState();
			xil_printf("\n\r");
			printStopWatch();
			if(btn_value == C)
			{
				state = SETTINGS;
				clearScreen();
				printAlarmStatus();
				printState();
				xil_printf("\n\r");
				while(btn_value == C){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}
			break;
		}
		case SETTINGS:
		{
			clearScreen();
			printAlarmStatus();
			printState();
			xil_printf("\n\r");
			if(btn_value == C)
			{
				state = TIME;
				clearScreen();
				printAlarmStatus();
				printState();
				xil_printf("\n\r");
				printTime();
				while(btn_value == C){btn_value = XGpio_DiscreteRead(&BTNInst, 1);}
			}
			break;
		}
	}








//----------------------------------------------------
// INTERRUPT HANDLER FUNCTIONS
// - called by the timer, button interrupt, performs
// - LED flashing
//----------------------------------------------------




    XGpio_DiscreteWrite(&LEDInst, 1, led_data);
    (void)XGpio_InterruptClear(&BTNInst, BTN_INT);
    // Enable GPIO interrupts
    XGpio_InterruptEnable(&BTNInst, BTN_INT);
}

void TMR_Intr_Handler(void *InstancePtr, u8 TmrCtrNumber)
{
	//double duration;
	//static int tmr_count;
	XTime_GetTime(&tEnd);
	XTmrCtr* pTMRInst = (XTmrCtr *) InstancePtr;

	//xil_printf("Timer %d interrupt \n", TmrCtrNumber);

	if (TmrCtrNumber==0) { //Handle interrupts generated by timer 0
		//duration = ((double)(tEnd-tStart))/COUNTS_PER_SECOND;
		//printf("Tmr_interrupt, tmr_count= %d, duration=%.6f s\n\r", tmr_count, (double)duration);

		tStart=tEnd;

		if (XTmrCtr_IsExpired(pTMRInst,0)){
			// Once timer has expired 3 times, stop, increment counter
			// reset timer and start running again

				XTmrCtr_Stop(pTMRInst,0);
				//tmr_count = 0;
				led_data++;
				XGpio_DiscreteWrite(&LEDInst, 1, led_data);
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

					if(state == TIME){
					clearScreen();
					printAlarmStatus();
					printState();
					if(alarmStatus == ON && alarmHour10 == hour10 && alarmHour0 == hour0 && alarmMinut10 == minut10 && alarmMinut0 == minut0)
					{
						xil_printf("ALARM!!!!!\n\r");
					}
					else
						xil_printf("\n\r");
					printTime();
					}
					sec0++;


			}


			//else tmr_count++;
		}

	//else {  //Handle interrupts generated by timer 1

	//}

	//XTmrCtr_ClearInterruptFlag(pTMRInst, TmrCtrNumber);
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



int IntcInitFunction(u16 DeviceId, XTmrCtr *TmrInstancePtr, XGpio *GpioInstanceBtnsPtr, XGpio *GpioInstanceSwPtr)
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

	// Enable GPIO interrupts interrupt
	XGpio_InterruptEnable(GpioInstanceBtnsPtr, 1);
	XGpio_InterruptGlobalEnable(GpioInstanceBtnsPtr);

	XGpio_InterruptEnable(GpioInstanceSwPtr, 1);
	XGpio_InterruptGlobalEnable(GpioInstanceSwPtr);

	// Enable GPIO, switches and timer interrupts in the controller
	XScuGic_Enable(&INTCInst, INTC_GPIO_INTERRUPT_BTNS);
	XScuGic_Enable(&INTCInst, INTC_GPIO_INTERRUPT_SW);
	XScuGic_Enable(&INTCInst, INTC_TMR_INTERRUPT_ID);

	//xil_printf("Getting the Timer interrupt info\n\r");
	XScuGic_GetPriTrigTypeByDistAddr(INTCInst.Config->DistBaseAddress, INTC_TMR_INTERRUPT_ID, &pri, &trig);
	//xil_printf("GPIO Interrupt-> Priority:%d, Trigger:%x\n\r", pri, trig);

	
	//Set the timer interrupt as edge triggered
	XScuGic_SetPriorityTriggerType(&INTCInst, INTC_TMR_INTERRUPT_ID,0, 0);

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


void printTime(){
	xil_printf("%i%i : %i%i : %i%i ",hour10,hour0, minut10,minut0, sec10, sec0);
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
	if(alarmStatus == OFF){xil_printf("Off\n\r");}
	else xil_printf("On\n\r");
}


void printStopWatch(){
	xil_printf("%i%i : %i%i : %i%i",swMinut10, swMinut0, swSec10, swSec0, swCenSec10, swCenSec0);
}



void printState(){
	if(state == TIME)
	{
		xil_printf("Clock mode\n\r");
	}
	else if(state == ALARM || state == SET_ALARM_MINUTES || state == SET_ALARM_HOUR)
	{
		xil_printf("Alarm mode\n\r");
	}
	else if(state == STOPWATCH)
	{
		xil_printf("Stop watch mode\n\r");
	}
	else if(state == SETTINGS){
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
	  status = IntcInitFunction(INTC_DEVICE_ID, &TMRInst, &BTNInst, &SWInst);
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
