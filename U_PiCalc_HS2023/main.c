/*
 * U_PiCalc_HS2023.c
 *
 * Created: 3.10.2023:18:15:00
 * Author : Merlin Untern�hrer
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
#include "avr_f64.h"

#include "ButtonHandler.h"


// Task handles and states
TaskHandle_t vLeibniz_tsk;			// Handle for Leibniz calculation task
TaskHandle_t vNil_tsk;				// Handle for Nilakantha calculation task
eTaskState taskStateLeibniz;		// State of the Leibniz calculation task
eTaskState taskStateNilakantha;     // State of the Nilakantha calculation task

// Function declarations
void vControllerTask(void* pvParameters);
void vCalculationTaskLeibniz(void* pvParameters);
void vCalculationTaskNilakantha(void* pvParameters);
void vUi_task(void* pvParameters);

// Event flags for button and task events
#define EVBUTTONS_S1            1<<0	// Event flag for switching to the right between Pi calculation algorithm
#define EVBUTTONS_S2            1<<1	// Event flag for starting Pi calculation
#define EVBUTTONS_S3            1<<2	// Event flag for resetting the selected algorithm
#define EVBUTTONS_S4            1<<3	// Event flag for switching to the left Pi calculation algorithm
#define EVBUTTONS_CLEAR         0xFF	// Used to clear button-related event flags
EventGroupHandle_t evButtonEvents;		// Event group for button events

#define EVCALC_WAIT              1<<0	// Event flag for task waiting
#define EVCALC_WAITING           1<<1	// Event flag indicating that a task is waiting
EventGroupHandle_t evCalcTaskEvents;	// Event group for task events
uint32_t calcStateBits;					// Bits to track task state

TickType_t startTime, endTime;

int time_ms = 0;						// Time in milliseconds
float32_t pi_approx = 0.0;				// Approximation of Pi as float

// Leibniz+Nilakantha variables
uint32_t iterations = 0;
float32_t term = 0.0;
int sign = 1;


// Main function
int main(void) {
    // Initialize clock and display
    vInitClock();
    vInitDisplay();
    
    // Initialize EventGroups for task synchronization
    evButtonEvents = xEventGroupCreate();
    evCalcTaskEvents = xEventGroupCreate();
    
    // Create tasks
    xTaskCreate(vControllerTask, (const char*) "control_tsk", configMINIMAL_STACK_SIZE + 150, NULL, 3, NULL);
    xTaskCreate(vCalculationTaskLeibniz, (const char*) "leibniz_tsk", configMINIMAL_STACK_SIZE + 300, NULL, 1, &vLeibniz_tsk);
    xTaskCreate(vCalculationTaskNilakantha, (const char*) "nilakantha_tsk", configMINIMAL_STACK_SIZE + 300, NULL, 1, &vNil_tsk);
    xTaskCreate(vUi_task, (const char*) "ui_tsk", configMINIMAL_STACK_SIZE + 150, NULL, 2, NULL);
    
    // Suspend calculation tasks initially
    vTaskSuspend(vNil_tsk);
    vTaskSuspend(vLeibniz_tsk);
    
    // Start FreeRTOS scheduler
    vTaskStartScheduler();
    return 0;
}


// Finite state machine for calculation tasks
#define RUN         0
#define WAIT        1
uint8_t smCalc = WAIT;
 
void vCalculationTaskLeibniz(void* pvParameters) {
    for (;;) {
        // Get task state bits for this task
        calcStateBits = (xEventGroupGetBits(evCalcTaskEvents)) & 0x000000FF;

        if (calcStateBits & EVCALC_WAIT) {
            smCalc = WAIT;
        } else {
            smCalc = RUN;
        }

        switch (smCalc) {
            case RUN: {
                // Calculate the next term of the series
                term = (float)sign / (2 * iterations + 1);

                // Multiply the term by 4 to get an approximation of pi
                term *= 4;

                // Update the approximation for pi
                pi_approx += term;

                // Reverse the sign for the next term
                sign = -sign;

                // Increase the number of iterations
                iterations++;

                if ((pi_approx > 3.14159 && pi_approx < 3.1416) && time_ms == 0) {
                    // Calculate and store the time in milliseconds
                    endTime = xTaskGetTickCount();
                    time_ms = (endTime - startTime) * portTICK_PERIOD_MS;
                }
            }
            break;
            case WAIT:
                // Indicate that this task is waiting
                xEventGroupSetBits(evCalcTaskEvents, EVCALC_WAITING);
            break;
        }
    }
}

void vCalculationTaskNilakantha(void* pvParameters) {
    for (;;) {
		
        // Get task state bits for this task
        calcStateBits = (xEventGroupGetBits(evCalcTaskEvents)) & 0x000000FF;

        if (calcStateBits & EVCALC_WAIT) {
            smCalc = WAIT;
        } else {
            smCalc = RUN;
        }

        switch (smCalc) {
            case RUN:
                if (pi_approx < 3.0) {
                    // Ensure the initial approximation is at least 3
                    pi_approx = 3.0;
                }
                
                // Update the approximation using the Nilakantha series
				pi_approx += sign * (4.0 / (2.0*iterations * (2.0*iterations + 1) * (2.0*iterations + 2)));
				sign *= (-1);
				iterations++;
                if ((pi_approx > 3.14159 && pi_approx < 3.1416) && time_ms == 0) {
                    // Calculate and store the time in milliseconds
                    endTime = xTaskGetTickCount();
                    time_ms = (endTime - startTime) * portTICK_PERIOD_MS;
                }
            break;
            case WAIT:
                // Indicate that this task is waiting
                xEventGroupSetBits(evCalcTaskEvents, EVCALC_WAITING);
            break;
        }
    }
}

// Controller task to handle button events
void vControllerTask(void* pvParameters) {
    // Initialize and configure buttons
    initButtons();
    for (;;) {
        // Check and handle button presses
        updateButtons();
        if (getButtonPress(BUTTON1) == SHORT_PRESSED) {
            xEventGroupSetBits(evButtonEvents, EVBUTTONS_S1);
        }
        if (getButtonPress(BUTTON2) == SHORT_PRESSED) {
            xEventGroupSetBits(evButtonEvents, EVBUTTONS_S2);
        }
        if (getButtonPress(BUTTON3) == SHORT_PRESSED) {
            xEventGroupSetBits(evButtonEvents, EVBUTTONS_S3);
        }
        if (getButtonPress(BUTTON4) == SHORT_PRESSED) {
            xEventGroupSetBits(evButtonEvents, EVBUTTONS_S4);
        }
        // Delay the task for 10 milliseconds
        vTaskDelay(10 / portTICK_RATE_MS);
    }
}

// UI Modes for the finite state machine
#define UIMODE_INIT				 0
#define UIMODE_NILAKANTHA_CALC   1
#define UIMODE_LEIBNIZ_CALC      2

uint8_t uiMode = UIMODE_INIT;

//vUi_task -> to handle the UI

void vUi_task(void* pvParameters) {
	char pistring[20];			// Character array to store the formatted value of pi
	char timeString[20];		// Character array to store the formatted time in milliseconds
	EventBits_t bitsCalcTskEv;  // Bitmask to store event flags related to calculation tasks

	for (;;) {
		// Get the state of the Leibniz, Nilakantha calculation tasks
		eTaskState taskStateNilakantha = eTaskGetState(vNil_tsk);
		eTaskState taskStateLeibniz = eTaskGetState(vLeibniz_tsk);		

		// Set the EVCALC_WAIT bit in the event group to signal calculation tasks to wait
		xEventGroupSetBits(evCalcTaskEvents, EVCALC_WAIT);

		// If both calculation tasks are not suspended, wait for the EVCALC_WAITING event flag
		if (!((taskStateLeibniz == eSuspended) && (taskStateNilakantha == eSuspended))) {
			bitsCalcTskEv = xEventGroupWaitBits(evCalcTaskEvents, EVCALC_WAITING, pdTRUE, pdTRUE, portMAX_DELAY);
		}

		// Check if the EVCALC_WAITING event flag is set or both tasks are suspended
		if ((bitsCalcTskEv & EVCALC_WAITING) || ((taskStateLeibniz == eSuspended) && (taskStateNilakantha == eSuspended))) {
			// Clear the display
			vDisplayClear();
			
			// Format and store the value of pi and time in milliseconds
			sprintf(&pistring[0], "PI: %.8f", pi_approx);
			sprintf(&timeString[0], "Time: %.6d ms", time_ms);
			
			
			// Get the state of the button events
			uint32_t buttonState = (xEventGroupGetBits(evButtonEvents)) & 0x000000FF;
			xEventGroupClearBits(evButtonEvents, EVBUTTONS_CLEAR);

			// Handle the user interface based on the current UI mode
			switch (uiMode) {
				case UIMODE_INIT:
				// Initialize the UI mode to Leibniz calculation
				uiMode = UIMODE_LEIBNIZ_CALC;
				break;

				case UIMODE_LEIBNIZ_CALC:
				// Clear the display and update it with Leibniz calculation information
				vDisplayClear();
				eTaskState taskStateLeibniz = eTaskGetState(vLeibniz_tsk);
				vDisplayWriteStringAtPos(0, 0, "Leibniz-Reihe:");
				vDisplayWriteStringAtPos(1, 0, "%s", pistring);
				vDisplayWriteStringAtPos(2, 0, "%s", timeString);

				// Update UI elements based on the task state
				if (taskStateLeibniz == eSuspended) {
					vDisplayWriteStringAtPos(3, 4, "Start");
					vDisplayWriteStringAtPos(3, 0, "|<|");
					vDisplayWriteStringAtPos(3, 17, "|>|");
					vDisplayWriteStringAtPos(3, 10, "|Reset");
					} else {
					vDisplayWriteStringAtPos(3, 4, "Stop");
					vDisplayWriteStringAtPos(3, 0, "| |");
					vDisplayWriteStringAtPos(3, 17, "| |");
					vDisplayWriteStringAtPos(3, 10, "|Reset");
				}

				// Handle button events
				if (buttonState & EVBUTTONS_S1) {
					if (taskStateLeibniz == eSuspended) {
						// Reset Leibniz calculation variables and switch to Nilakantha calculation
						pi_approx = 0.0;				
						term = 0.0;
						iterations = 1;
						sign = 1;
						time_ms = 0;
						uiMode = UIMODE_NILAKANTHA_CALC;
					}
				}
				if (buttonState & EVBUTTONS_S2) {
					if (taskStateLeibniz == eSuspended) {
						// Start or stop Leibniz calculation task
						startTime = xTaskGetTickCount();
						vTaskResume(vLeibniz_tsk);
						} else {
						vTaskSuspend(vLeibniz_tsk);
					}
				}
				if (buttonState & EVBUTTONS_S3) {
						// Reset Leibniz calculation variables
						pi_approx = 0.0;
						term = 0.0;
						iterations = 1;
						sign = 1;
						time_ms = 0;
						startTime = xTaskGetTickCount();						
				}
				if (buttonState & EVBUTTONS_S4) {
					if (taskStateLeibniz == eSuspended) {
						// Reset Leibniz calculation variables and switch to Nilakantha calculation
						pi_approx = 0.0;
						term = 0.0;
						iterations = 1;
						sign = 1;
						time_ms = 0;
						uiMode = UIMODE_NILAKANTHA_CALC;
					}
				}
				break;

				case UIMODE_NILAKANTHA_CALC:
				// Clear the display and update it with Nilakantha calculation information
				vDisplayClear();
				vDisplayWriteStringAtPos(0, 0, "Nilakantha-Reihe:");
				vDisplayWriteStringAtPos(1, 0, "%s", pistring);
				vDisplayWriteStringAtPos(2, 0, "%s", timeString);

				// Update UI elements based on the task state
				if (taskStateNilakantha == eSuspended) {
					vDisplayWriteStringAtPos(3, 4, "Start");
					vDisplayWriteStringAtPos(3, 0, "|<|");
					vDisplayWriteStringAtPos(3, 17, "|>|");
					vDisplayWriteStringAtPos(3, 10, "|Reset");
					} else {
					vDisplayWriteStringAtPos(3, 4, "Stop");
					vDisplayWriteStringAtPos(3, 0, "| |");
					vDisplayWriteStringAtPos(3, 17, "| |");
					vDisplayWriteStringAtPos(3, 10, "|Reset");
				}

				// Handle button events
				if (buttonState & EVBUTTONS_S1) {
					if (taskStateNilakantha == eSuspended) {
						// Reset Nilakantha calculation variables and switch to Leibniz calculation
						sign = 1;
						iterations = 1;
						pi_approx = 0.0;
						time_ms = 0;
						uiMode = UIMODE_LEIBNIZ_CALC;
					}
				}
				if (buttonState & EVBUTTONS_S2) {
					if (taskStateNilakantha == eSuspended) {
						// Start or stop Nilakantha calculation task
						startTime = xTaskGetTickCount();
						vTaskResume(vNil_tsk);
						} else {
						vTaskSuspend(vNil_tsk);
					}
				}
				if (buttonState & EVBUTTONS_S3) {
						// Reset Nilakantha calculation variables
						sign = 1;
						iterations = 1;
						pi_approx = 0.0;
						time_ms = 0;
						startTime = xTaskGetTickCount();						
				}
				if (buttonState & EVBUTTONS_S4) {
					if (taskStateNilakantha == eSuspended) {
						// Reset Nilakantha calculation variables and switch to Wallis Product calculation
						sign = 1;
						iterations = 1;
						pi_approx = 0.0;
						time_ms = 0;
						uiMode = UIMODE_LEIBNIZ_CALC;
					}
				}
				break;
			}
			// Clear the EVCALC_WAIT bit in the event group
			xEventGroupClearBits(evCalcTaskEvents, EVCALC_WAIT);
		}
		// Delay for 500 milliseconds
		vTaskDelay(500 / portTICK_RATE_MS);
	}
}