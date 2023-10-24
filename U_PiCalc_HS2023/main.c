/*
 * U_PiCalc_HS2023.c
 *
 * Created: 3.10.2023:18:15:00
 * Author : -
 */ 

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "avr_compiler.h"
#include "pmic_driver.h"
#include "TC_driver.h"
#include "clksys_driver.h"
#include "sleepConfig.h"
#include "port_driver.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "event_groups.h"
#include "stack_macros.h"

#include "mem_check.h"

#include "init.h"
#include "utils.h"
#include "errorHandler.h"
#include "NHD0420Driver.h"

#include "ButtonHandler.h"

TaskHandle_t vleibniz_tsk;
TaskHandle_t vnil_som_tsk;
eTaskState taskStateLeibniz;
eTaskState taskStateNilSom;

void vControllerTask(void* pvParameters);
void vCalculationTaskLeibniz(void* pvParameters);
void vCalculationTaskNilakanthaSomayaji(void* pvParamters);
void vUi_task(void* pvParameters);


#define EVBUTTONS_S1	1<<0 //Switch between Pi calculation algorithm
#define EVBUTTONS_S2	1<<1 //Start Pi calculation
#define EVBUTTONS_S3	1<<2 //Stop Pi calculation
#define EVBUTTONS_S4	1<<3 //Reset selected algorithm
#define EVBUTTONS_CLEAR	0xFF
EventGroupHandle_t evButtonEvents;

#define EVCALC_RUN      1<<0 //Task is free to run now
#define EVCALC_WAIT     1<<1 //Task must wait now
#define EVCALC_RUNNING  1<<2 //Task is running
#define EVCALC_WAITING  1<<3 //Task is waiting
#define EVCALC_RESET    1<<4 //Reset the all variables for the calculation
#define EVCALC_CLEAR    0xFF
EventGroupHandle_t evCalcTaskEvents;



TickType_t startTime, endTime;

int time_ms = 0;
float pi_calculated = 0.0;

//Leibniz variables
float term = 0.0;
int leibniz_approx_count = 0;

//sign variable, used in both algorithm's
int sign = 1;

//Nilakantha variables
int numerator = 2;


int main(void)
{
	vInitClock();
	vInitDisplay();
	
	//Initialize EventGroups	
	evButtonEvents = xEventGroupCreate();
	
	xTaskCreate( vControllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);
	xTaskCreate( vCalculationTaskLeibniz, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE+300, NULL, 1, &vleibniz_tsk);
	xTaskCreate( vCalculationTaskNilakanthaSomayaji, (const char *) "nil_som_tsk", configMINIMAL_STACK_SIZE+300, NULL, 1, &vnil_som_tsk);
	xTaskCreate( vUi_task, (const char *) "ui_tsk", configMINIMAL_STACK_SIZE+50, NULL, 2, NULL);
	
	vTaskSuspend(vnil_som_tsk);
	vTaskSuspend(vleibniz_tsk);
	
	vTaskStartScheduler();
	return 0;
}


void vCalculationTaskLeibniz(void* pvParameters){
	
	
	for(;;){
		
		uint32_t calcStateLBNZ = (xEventGroupGetBits(evCalcTaskEvents)) & 0x000000FF;
			
		term += sign*(1.0/(2*leibniz_approx_count+1));
		sign *= (-1);
		pi_calculated = term*4;
		leibniz_approx_count++;
		
		if(pi_calculated > 3.14159 && pi_calculated < 3.1416){
			endTime = xTaskGetTickCount();
			time_ms = (endTime - startTime) * portTICK_PERIOD_MS;
		}
	}
}

void vCalculationTaskNilakanthaSomayaji(void* pvParameters){

	for(;;){
		
		uint32_t calcStateNIL = (xEventGroupGetBits(evCalcTaskEvents)) & 0x000000FF;
		xEventGroupClearBits(evButtonEvents, EVCALC_CLEAR);
		
		if(calcStateNIL & EVCALC_RUN){
			
			if (pi_calculated < 3.0){
				pi_calculated = 3.0;
			}
			
		    pi_calculated += sign * (4.0 / (numerator * (numerator + 1) * (numerator + 2)));
		    sign *= -1;         
		    numerator += 2;     
			
			if(pi_calculated > 3.14159 && pi_calculated < 3.1416){
				endTime = xTaskGetTickCount();
				time_ms = (endTime - startTime) * portTICK_PERIOD_MS;
			}
		}else{
			xEventGroupSetBits(evCalcTaskEvents, EVCALC_WAITING);
		}
	}
}

void vControllerTask(void* pvParameters) {
	initButtons();
	for(;;) {
		updateButtons();
		if(getButtonPress(BUTTON1) == SHORT_PRESSED) {
			xEventGroupSetBits(evButtonEvents, EVBUTTONS_S1);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {
			xEventGroupSetBits(evButtonEvents, EVBUTTONS_S2);			
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {
			xEventGroupSetBits(evButtonEvents, EVBUTTONS_S3);			
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {
			xEventGroupSetBits(evButtonEvents, EVBUTTONS_S4);			
		}
		vTaskDelay(10/portTICK_RATE_MS);
	}
}

//UIModes for Finite State Machine
#define UIMODE_INIT				0
#define UIMODE_NIL_SOM_CALC		1
#define UIMODE_LEIBNIZ_CALC		2

uint8_t uiMode = UIMODE_INIT;

//vUi_task -> to handle the UI

void vUi_task(void* pvParameters){
	
	char loadString[20] = "               ";
	char pistring[20];
	char timeString[20];
	uint8_t uidelay = 10;
		
	for(;;){
		vDisplayClear();
		
		uint32_t calcStateUI = (xEventGroupGetBits(evCalcTaskEvents)) & 0x000000FF;
		xEventGroupClearBits(evCalcTaskEvents, EVCALC_CLEAR);
		
		if(calcStateUI & EVCALC_WAITING){
			sprintf(&pistring[0], "PI: %.8f", pi_calculated);
			sprintf(&timeString[0], "Time: %.8d ms", time_ms);
		}else{
			
		}
		
		uint32_t buttonState = (xEventGroupGetBits(evButtonEvents)) & 0x000000FF;
		uint32_t calcStateUI = (xEventGroupGetBits(evCalcTaskEvents)) & 0x000000FF;
		xEventGroupClearBits(evButtonEvents, EVBUTTONS_CLEAR);
		
		switch(uiMode){
			case UIMODE_INIT:{
				vDisplayWriteStringAtPos(0,0,"PI-Calc HS2023");
				switch(uidelay) {
					case 10:
					loadString[0] = '.';
					break;
					case 9:
					loadString[1] = '.';
					break;
					case 8:
					loadString[2] = '.';
					break;
					case 7:
					loadString[3] = '.';
					break;
					case 6:
					loadString[4] = '.';
					break;
					case 5:
					loadString[5] = '.';
					break;
					case 4:
					loadString[6] = '.';
					break;
					case 3:
					loadString[7] = '.';
					break;
					case 2:
					loadString[8] = '.';
					break;
					case 1:
					loadString[9] = '.';
					break;
				}
				vDisplayWriteStringAtPos(2,0, "Loading.%s", loadString);
				if(uidelay > 0){
					uidelay--;
				}else{
					uiMode = UIMODE_LEIBNIZ_CALC;
				}
			}
			break;
			
			case UIMODE_LEIBNIZ_CALC:
				vDisplayClear();
				eTaskState taskStateLeibniz = eTaskGetState(vleibniz_tsk);
				vDisplayWriteStringAtPos(0,0, "Leibniz-Reihe:");
				vDisplayWriteStringAtPos(1,0, "%s", pistring);
				vDisplayWriteStringAtPos(2,0, "%s", timeString);
				if(taskStateLeibniz == eSuspended){
					vDisplayWriteStringAtPos(3,4, "Start");
					vDisplayWriteStringAtPos(3,0, "|<|");
					vDisplayWriteStringAtPos(3,17, "|>|");
					vDisplayWriteStringAtPos(3,11, "|Reset");
				}else{
					vDisplayWriteStringAtPos(3,4, "Stop |");
					vDisplayWriteStringAtPos(3,0, "| |");
					vDisplayWriteStringAtPos(3,17, "| |");
					vDisplayWriteStringAtPos(3,11, "|     ");						
				}
				if(buttonState & EVBUTTONS_S1){
					if(taskStateLeibniz == eSuspended){
						pi_calculated = 0.0;
						term = 0.0;
						leibniz_approx_count = 0;
						sign = 1;		
						time_ms = 0;							
						uiMode = UIMODE_NIL_SOM_CALC;
					}
				}
				if(buttonState & EVBUTTONS_S2){
					if(taskStateLeibniz == eSuspended){
						startTime = xTaskGetTickCount();
						vTaskResume(vleibniz_tsk);
					}else{
						vTaskSuspend(vleibniz_tsk);
					}
				}
				if(buttonState & EVBUTTONS_S3){
					if(taskStateLeibniz == eSuspended){			
						pi_calculated = 0.0;
						term = 0.0;
						leibniz_approx_count = 0;
						sign = 1;
						time_ms = 0;
					}					
				}
				if(buttonState & EVBUTTONS_S4){
					if(taskStateLeibniz == eSuspended){
						pi_calculated = 0.0;
						term = 0.0;
						leibniz_approx_count = 0;
						sign = 1;
						time_ms = 0;						
						uiMode = UIMODE_NIL_SOM_CALC;
					}
				}				
			break;
			case UIMODE_NIL_SOM_CALC:
				vDisplayClear();
				eTaskState taskStateNilSom = eTaskGetState(vnil_som_tsk);
				vDisplayWriteStringAtPos(0,0, "Nilakantha-Reihe:");
				vDisplayWriteStringAtPos(1,0, "%s", pistring);
				vDisplayWriteStringAtPos(2,0, "%s", timeString);
				if(taskStateNilSom == eSuspended){
					vDisplayWriteStringAtPos(3,4, "Start|");
					vDisplayWriteStringAtPos(3,0, "|<|");
					vDisplayWriteStringAtPos(3,17, "|>|");
					vDisplayWriteStringAtPos(3,11, "|Reset");
				}else{
					vDisplayWriteStringAtPos(3,4, "Stop |");
					vDisplayWriteStringAtPos(3,0, "| |");
					vDisplayWriteStringAtPos(3,17, "| |");
					vDisplayWriteStringAtPos(3,11, "|     ");
				}
				if(buttonState & EVBUTTONS_S1){
					if(taskStateNilSom == eSuspended){
						sign = 1;
						numerator = 2;
						pi_calculated = 0.0;
						time_ms = 0;						
						uiMode = UIMODE_LEIBNIZ_CALC;
					}
				}
				if(buttonState & EVBUTTONS_S2){
					if(taskStateNilSom == eSuspended){
						startTime = xTaskGetTickCount();
						vTaskResume(vnil_som_tsk);
					}else{
						vTaskSuspend(vnil_som_tsk);
					}
				}
				if(buttonState & EVBUTTONS_S3){
					if(taskStateNilSom == eSuspended){
						sign = 1;
						numerator = 2;
						pi_calculated = 0.0;
						time_ms = 0;			
					}
				}
				if(buttonState & EVBUTTONS_S4){
					if(taskStateNilSom == eSuspended){
						sign = 1;
						numerator = 2;
						pi_calculated = 0.0;
						time_ms = 0;						
						uiMode = UIMODE_LEIBNIZ_CALC;
					}
				}
			break;
		}
		vTaskDelay(500/portTICK_RATE_MS);
	}
}