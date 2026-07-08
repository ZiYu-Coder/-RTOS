#include "stm32f10x.h" // Device header
#include "usart.h"
#include <stdio.h>
#include "memory.h"
#include "rtos.h"
#include "task.h"
#define BUFFER_SIZE 20
uint8_t buffer[BUFFER_SIZE];
int receive_flag = 0;

TCB* test1_tcb;
TCB* test2_tcb;
TCB* test3_tcb;
void test1(void *arg)
{
	int counter=0;
    while (1)
    {
        printf("test1:%d\r\n",counter++);
        task_delay(500);
    }
}


void test3(void *arg)
{
	int counter=0;
    while (1)
    {
        printf("test3:%d\r\n",counter++);
        task_delay(500);
    }
}


void test2(void *arg)
{
	int counter=0;
    while (1)
    {
        printf("test2:%d\r\n",counter++);
        task_delay(1000);
		if(counter==10)
		{
			delet_task(test1_tcb);
		}
		else if(counter==20)
		{
			create_task(test3,10,300,NULL);
		}
    }
}

int main(void)
{
    // debug_init
    USER_USART1_INIT();

    rtos_init();
    test1_tcb=create_task(test1, 10, 300, NULL);
	test2_tcb=create_task(test2,10,500,NULL);
    start_scheduler();
    return 0;
}


