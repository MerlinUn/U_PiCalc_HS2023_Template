/*
 * U_PiCalc_HS2023.c
 *
 * Created: 3.10.2023:18:15:00
 * Author : -
 */ 

#include <math.h>
#include <stdio.h>
#include <string.h>
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


void vControllerTask(void* pvParameters);
void vCalculationTaskLeibniz(void* pvParameters);
void vCalculationTaskNilakanthaSomayaji(void* pvParamters);
void vUi_task(void* pvParameters);


#define EVBUTTONS_S1	1<<0
#define EVBUTTONS_S2	1<<1
#define EVBUTTONS_S3	1<<2
#define EVBUTTONS_S4	1<<3
#define EVBUTTONS_L1	1<<4
#define EVBUTTONS_L2	1<<5
#define EVBUTTONS_L3	1<<6
#define EVBUTTONS_L4	1<<7
#define EVBUTTONS_CLEAR	0xFF
EventGroupHandle_t evButtonEvents;

double pi_calculated = 0.0;

int main(void)
{
	vInitClock();
	vInitDisplay();
	
	//Initialize EventGroups	
	evButtonEvents = xEventGroupCreate();
	
	xTaskCreate( vControllerTask, (const char *) "control_tsk", configMINIMAL_STACK_SIZE+150, NULL, 3, NULL);
	xTaskCreate( vCalculationTaskLeibniz, (const char *) "leibniz_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL);
	xTaskCreate( vCalculationTaskNilakanthaSomayaji, (const char *) "nil_som_tsk", configMINIMAL_STACK_SIZE+150, NULL, 1, NULL);
	xTaskCreate( vUi_task, (const char *) "ui_tsk", configMINIMAL_STACK_SIZE+50, NULL, 2, NULL);
	
	vDisplayClear();
	vDisplayWriteStringAtPos(0,0,"PI-Calc HS2023");
	
	vTaskStartScheduler();
	return 0;
}


void vCalculationTaskLeibniz(void* pvParameters){
	double term = 1.0;
	int i = 0;
	
	for(;;){
		if(1 % 2 == 0){
			pi_calculated += term;
		} else {
			pi_calculated -= term;
		}
		
		term = 1.0 / (2 * i + 3);
		i++;
		
		if(term < 0.000005){
			pi_calculated *= 4;
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
			char pistring[12];
			sprintf(&pistring[0], "PI: %.8f", M_PI);
			vDisplayWriteStringAtPos(1,0, "%s", pistring);
		}
		if(getButtonPress(BUTTON2) == SHORT_PRESSED) {
			
		}
		if(getButtonPress(BUTTON3) == SHORT_PRESSED) {
			
		}
		if(getButtonPress(BUTTON4) == SHORT_PRESSED) {
			
		}
		if(getButtonPress(BUTTON1) == LONG_PRESSED) {
			
		}
		if(getButtonPress(BUTTON2) == LONG_PRESSED) {
			
		}
		if(getButtonPress(BUTTON3) == LONG_PRESSED) {
			
		}
		if(getButtonPress(BUTTON4) == LONG_PRESSED) {
			
		}
		vTaskDelay(10/portTICK_RATE_MS);
	}
}

//UIModes for Finite State Machine
#define UIMODE_INIT				0
#define UIMODE_MAIN				1
#define UIMODE_NIL_SOM_CALC		2
#define UIMODE_LEIBNIZ_CALC		3

uint8_t uiMode = UIMODE_INIT;

//vUi_task -> to handle the UI

void vUi_task(void* pvParameters){	
	for(;;){
		vTaskDelay(500/portTICK_RATE_MS);
	}
}