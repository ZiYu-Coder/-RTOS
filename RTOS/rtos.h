#ifndef RTOS_H_
#define RTOS_H_
#include "stm32f10x.h"
#include "config.h"
extern volatile uint32_t time_counter;
void rtos_init(void);
void idle_task(void * arg);
__asm void start_scheduler(void);
uint32_t enter_critical(void );
void exit_critical(uint32_t new_pri);
#endif
