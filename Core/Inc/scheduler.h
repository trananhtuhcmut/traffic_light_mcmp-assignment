/*
 * scheduler.h
 *
 *  Created on: Nov 1, 2026
 *      Author: trananhtu
 */
#ifndef INC_SCHEDULER_H_
#define INC_SCHEDULER_H_

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    void (*pTask)(void);      // Pointer function
    uint32_t Delay;           // Delta delay
    uint32_t Period;          // 0 mean once only
    bool RunMe;            // Flag
    uint32_t TaskID;          // Task ID
} sTask;

#define SCH_MAX_TASKS 10      // Max task
#define NO_TASK_ID    0

//Error definiton
//#define SCH_REPORT_ERRORS

#define RETURN_NORMAL                                   0
#define RETURN_ERROR                                    1

#define ERROR_SCH_TOO_MANY_TASKS                        1
#define ERROR_SCH_CANNOT_DELETE_TASK                    2
#define ERROR_SCH_WAITING_FOR_SLAVE_TO_ACK              3
#define ERROR_SCH_WAITING_FOR_START_COMMAND_FROM_MASTER 4
#define ERROR_SCH_ONE_OR_MORE_SLAVES_DID_NOT_START      5
#define ERROR_SCH_LOST_SLAVE                            6
#define ERROR_SCH_CAN_BUS_ERROR                         7
#define ERROR_I2C_WRITE_BYTE_AT24C64                    8

#ifdef SCH_REPORT_ERRORS
#define Error_port GPIOB->ODR
#endif

extern uint8_t Error_code_G;






void SCH_Go_To_Sleep(void);
void SCH_Init(void);
void SCH_Update(void);
void SCH_Dispatch_Tasks(void);
uint32_t SCH_Add_Task(void (*pFunction)(), uint32_t DELAY, uint32_t PERIOD);
uint8_t SCH_Delete_Task(uint32_t taskID);

#endif /* INC_SCHEDULER_H_ */



