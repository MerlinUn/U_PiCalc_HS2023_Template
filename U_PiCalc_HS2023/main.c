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

int pi_integer = 0;
int pi_decimal = 0;
double pi_calculated = 0.0;
double pi = 3.14159;

void separateIntegerAndDecimal(double number, int *integerPart, int *decimalAsInt) {
	*integerPart = (int)number;
	double decimal = number - *integerPart;
	*decimalAsInt = decimal * 100000;
}

int main(void)
{
	vInitClock();
	vInitDisplay();
	
	//Initialize EventGroups	
	evButtonEvents = xEventGroupCreate();
	
	xTaskCreate( vControllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);
	xTaskCreate( vCalculationTaskLeibniz, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, &vleibniz_tsk);
	xTaskCreate( vCalculationTaskNilakanthaSomayaji, (const char *) "nil_som_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, &vnil_som_tsk);
	xTaskCreate( vUi_task, (const char *) "ui_tsk", configMINIMAL_STACK_SIZE+50, NULL, 2, NULL);
	
	vTaskSuspend(vnil_som_tsk);
	vTaskSuspend(vleibniz_tsk);
	
	vTaskStartScheduler();
	return 0;
}


void vCalculationTaskLeibniz(void* pvParameters){
	double term = 1.0;
	int i = 0;
	
	for(;;){
		if(i % 2 == 0){
			pi_calculated += term;
		} else {
			pi_calculated -= term;
		}
		
		term = 1.0 / (2 * i + 3);
		i++;
		
		if(term < 0.000005){
			pi_calculated *= 4;
			separateIntegerAndDecimal(pi_calculated, &pi_integer, &pi_decimal);
			break;
		}
	}
}

void vCalculationTaskNilakanthaSomayaji(void* pvParameters){
	int num_terms = 1;
	double epsilon = 0.00001;
	pi_calculated = 3.0;
	
	for(;;){
		int divisor = 2 * num_terms * (2 * num_terms + 1) * (2 * num_terms + 2);
		double term = 4.0 / divisor;
		
		if(num_terms % 2 == 1){
			pi_calculated += term;
		} else {
			pi_calculated -= term;
		}
		
		if (term < epsilon){
			separateIntegerAndDecimal(pi_calculated, &pi_integer, &pi_decimal);
			break;
		}
		
		num_terms++;
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
	
	char timestring[20] = "               ";
	uint8_t uidelay = 10;
		
	for(;;){
		vDisplayClear();
		uint32_t buttonState = (xEventGroupGetBits(evButtonEvents)) & 0x000000FF;
		xEventGroupClearBits(evButtonEvents, EVBUTTONS_CLEAR);
		switch(uiMode){
			case UIMODE_INIT:{
				vDisplayWriteStringAtPos(0,0,"PI-Calc HS2023");
				switch(uidelay) {
					case 10:
					timestring[0] = '.';
					break;
					case 9:
					timestring[1] = '.';
					break;
					case 8:
					timestring[2] = '.';
					break;
					case 7:
					timestring[3] = '.';
					break;
					case 6:
					timestring[4] = '.';
					break;
					case 5:
					timestring[5] = '.';
					break;
					case 4:
					timestring[6] = '.';
					break;
					case 3:
					timestring[7] = '.';
					break;
					case 2:
					timestring[8] = '.';
					break;
					case 1:
					timestring[9] = '.';
					break;
				}
				vDisplayWriteStringAtPos(2,0, "Loading.%s", timestring);
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
				vDisplayWriteStringAtPos(1,0, "PI: 3.14159");
				vDisplayWriteStringAtPos(2,0, "PI: %d.%d", pi_integer, pi_decimal);
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
						uiMode = UIMODE_NIL_SOM_CALC;
					}
				}
				if(buttonState & EVBUTTONS_S2){
					if(taskStateLeibniz == eSuspended){
						vTaskResume(vleibniz_tsk);
					}else{
						vTaskSuspend(vleibniz_tsk);
					}
				}
				if(buttonState & EVBUTTONS_S3){
					if(taskStateLeibniz == eSuspended){			
						pi_calculated = 0.0;
					}					
				}
				if(buttonState & EVBUTTONS_S4){
					if(taskStateLeibniz == eSuspended){
						uiMode = UIMODE_NIL_SOM_CALC;
					}
				}				
			break;
			case UIMODE_NIL_SOM_CALC:
				vDisplayClear();
				eTaskState taskStateNilSom = eTaskGetState(vnil_som_tsk);
				vDisplayWriteStringAtPos(0,0, "Nilakantha-Somayaji-Reihe:");
				vDisplayWriteStringAtPos(1,0, "PI: 3.14159");
				vDisplayWriteStringAtPos(2,0, "PI: %d.%d", pi_integer, pi_decimal);
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
						uiMode = UIMODE_LEIBNIZ_CALC;
					}
				}
				if(buttonState & EVBUTTONS_S2){
					if(taskStateNilSom == eSuspended){
						vTaskResume(vnil_som_tsk);
					}else{
						vTaskSuspend(vnil_som_tsk);
					}
				}
				if(buttonState & EVBUTTONS_S3){
					if(taskStateNilSom == eSuspended){
						pi_calculated = 0.0;
						pi_integer = 0;
						pi_decimal = 0;						
					}
				}
				if(buttonState & EVBUTTONS_S4){
					if(taskStateNilSom == eSuspended){
						uiMode = UIMODE_LEIBNIZ_CALC;
					}
				}
			break;
		}
		vTaskDelay(500/portTICK_RATE_MS);
	}
}