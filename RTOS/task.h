#ifndef _TASK_H_
#define _TASK_H_
#include "stm32f10x.h"
#include <stdbool.h>
#include "config.h"
#include "rtos.h"
enum task_state
{
    RUNNING, // 运行态
    READY,   // 就绪态
    BLOCKED  // 阻塞态
};
typedef struct task_control_block
{
    void *stack_top;       // 任务栈顶
    void *stack_start;     // 任务栈起始位置
    uint32_t stack_size;   // 任务栈大小，与申请的内存大小一致
    uint32_t priority;     // 任务优先级
    enum task_state state; // 任务状态

} TCB;

typedef struct stack_frame // 任务栈帧
{
    // 手动保存
    uint32_t r4; // 低地址
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    // 硬件自动保存
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t LR;
    uint32_t PC;
    uint32_t xPSR; // 高地址
} stack_frame;
typedef void (*task_func_ptr)(void *); // 任务函数指针

typedef struct tcb_node
{
    TCB *tcb;
    struct tcb_node *next;
} tcb_node;
extern TCB * volatile current_tcb;
extern tcb_node ready_queue[MAX_PRIORITY]; // 就绪队列,不能用TCB，要用链表,调度时，从链表头选择任务进行调度，将任务加到链表尾

// 延时相关实现
// 时间管理节点
typedef struct tm_node
{
    TCB *tcb;
    uint32_t wake_time; // 唤醒时间
    struct tm_node *next;
} tm_node;

extern tm_node delay_queue_dummy;    // 延时队列，按照时间顺序排列
extern tm_node overtime_queue_dummy; // 超时队列，按照时间顺序排列
extern tm_node *delay_queue;         // 指向当前延时队列的第一个节点
extern tm_node *overtime_queue;      // 指向当前超时队列的第一个节点

TCB *create_task(task_func_ptr task_name, uint8_t priority, uint16_t stack_size, void *arg);
void add_task_to_ready_queue(TCB *tcb);
void queue_init(void);
void task_switch(void);
void task_delay(uint32_t delay_time);
bool delet_task(TCB *tcb);
#endif
