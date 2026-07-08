#include "rtos.h"
#include "./memory.h"
#include "./task.h"
#include "stm32f10x.h"
#include <stdlib.h>
volatile uint32_t time_counter=0;

void rtos_init(void)
{
    // 初始化内存管理
    while (!memory_init())
        ;
    // 初始化就绪队列及延时超时队列
    queue_init();
    // 创建第一个任务（空闲任务），并更新current_tcb
	current_tcb=create_task(idle_task, 0, 150, NULL);
    // 初始化systick，单片机启动时，中断默认处于关闭状态
	(*((volatile uint32_t *)0xe000ed20)) |= (((uint32_t)255UL) << 16UL);
    (*((volatile uint32_t *)0xe000ed20)) |= (((uint32_t)255UL) << 24UL);
    /* Stop and clear the SysTick. */
    (*((volatile uint32_t *)0xe000e010)) = 0UL; /*portNVIC_SYSTICK_CTRL_REG*/
    (*((volatile uint32_t *)0xe000e018)) = 0UL; // portNVIC_SYSTICK_CURRENT_VALUE_REG

    /* Configure SysTick to interrupt at the requested rate. */
    (*((volatile uint32_t *)0xe000e014)) = (configSYSTICK_CLOCK_HZ / configTICK_RATE_HZ) - 1UL;
    (*((volatile uint32_t *)0xe000e010)) = ((1UL << 2UL) | (1UL << 1UL) | (1UL << 0UL)); //|处理器时钟|打开定时器中断|使能NVIC|

}

void idle_task(void * arg)
{
	while(1)
	{
		SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
		__WFI();
	}
}

__asm void start_scheduler(void)
{
    PRESERVE8

    /*设置MSP，主栈中已存在的内容会被覆盖*/
    ldr r0, = 0xE000ED08 // VTOR的地址
    ldr r0,[r0] // VTOR的值，即中断向量表的地址
    ldr r0,[r0] // 中断向量表首元素，即MSP初始值
    msr msp,r0
    /*开中断*/
    cpsie i
    cpsie f
    dsb
    isb
    /*调用SVC*/
    svc 0 
	nop 
	nop
}

__asm void SVC_Handler(void)
{
    PRESERVE8
	extern current_tcb;
    ldr r3, = current_tcb 
    ldr r1,[r3]
    ldr r0,[r1] //获取current_tcb第一项，也就是栈顶地址
    ldmia r0 !,{r4 - r11} //手动出栈
    msr psp,r0 
    isb
    mov r0,# 0 
	msr basepri, r0 
	orr r14, #0xFFFFFFFD 
	bx r14
}


void SysTick_Handler(void)
{
    uint32_t pri=enter_critical();
	time_counter++;
	if(!time_counter)//交换延时列表与超时列表
	{
		tm_node* tmp=delay_queue;
		delay_queue=overtime_queue;
		overtime_queue=tmp;
	}
	
	tm_node * tmp=delay_queue->next;
	while(tmp&&tmp->wake_time==time_counter)//需要将所有到时间的任务加入到就绪列表,若只加第一个，后面的任务要等systick溢出后才能被调度
	{
		add_task_to_ready_queue(tmp->tcb);//将任务加入到就绪列表
		tm_node* head=delay_queue->next;
		delay_queue->next=head->next;
		free(head);//从延时列表释放
		tmp=tmp->next;
	}
	//挂起PendSV中断
	*((volatile uint32_t *)0xe000ed04) = 1UL << 28UL;
	exit_critical(pri);
	
}

__asm void PendSV_Handler(void)
{
    extern current_tcb;
    extern task_switch;

    PRESERVE8

    mrs r0, psp // 将进程栈指针PSP的值（栈顶）保存到R0
    isb

    ldr r3,= current_tcb // 将current_tcb的地址读取到R3
    ldr r2,[r3] // 根据R3的值，将current_tcb(tcb的地址)读取到R2

    stmdb r0 !,{r4 - r11} // 手动保存r4 - r11，入进程栈，R0回写
    str r0,[r2] // 将新的栈顶写入tcb

    stmdb sp !,{r3, r14} // r3, r14入主栈
//    mov r0,#configMAX_SYSCALL_INTERRUPT_PRIORITY // 关中断
//    msr basepri,r0
	cpsid i
    dsb
    isb
    bl task_switch // 调用任务切换函数
//    mov r0,# 0 
//	msr basepri, r0 // 开中断
	cpsie i
	
    ldmia sp !,{r3, r14} // 从主栈中读取r3, r14 ，其中r3保存的时current_tcb的地址
	//ldr r3,= current_tcb//也可以再次读取current_tcb的地址来更新r3
	
    ldr r1,[r3] // 将current_tcb写入到R1
    ldr r0,[r1] // 将tcb第一个元素（任务栈顶）的值写入R0
    ldmia r0 !,{r4 - r11} // 将r4 - r11出栈，回写R0
    msr psp,r0 // 将R0的值写入PSP
    isb
    bx r14 
	nop
}


uint32_t enter_critical(void )
{
	uint32_t result,new_pri=BASE_PRI;
	__asm
	{
		mrs result, basepri//保存basepri
		msr basepri, new_pri//写入新的
		dsb
		isb
		
	}
	return result;//返回原来的值
		
}

void exit_critical(uint32_t new_pri)
{
	__asm
	{
		msr basepri ,new_pri
	}
}





