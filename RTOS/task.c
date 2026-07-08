#include "task.h"
#include "memory.h"
#include <stdlib.h>
#include <assert.h>
#include "rtos.h"
TCB *volatile current_tcb = NULL;

tcb_node ready_queue[MAX_PRIORITY]; // 支持MAX_PRIORITY个优先级
tm_node delay_queue_dummy;          // 延时队列
tm_node overtime_queue_dummy;       // 超时队列

tm_node *delay_queue = &delay_queue_dummy;       // 指向当前延时队列的第一个节点
tm_node *overtime_queue = &overtime_queue_dummy; // 指向当前超时队列的第一个节点
/**
 *
 * @param task_name 任务函数指针
 * @param priority 优先级
 * @param stack_size 栈大小
 * @param arg 任务参数
 * @return 创建成功返回TCB指针，失败返回NULL
 *
 */
TCB *create_task(task_func_ptr task_name, uint8_t priority, uint16_t stack_size, void *arg)
{
    // 参数校验
    if (stack_size < sizeof(TCB) + sizeof(stack_frame) + 8)
    {
        return NULL;
    }
    uint32_t pri = enter_critical();
    memory_node node = get_memory(stack_size);
    if (node.size == 0) // 内存校验
    {
        exit_critical(pri);
        return NULL;
    }
    TCB *tcb = (TCB *)node.start_addr;                                                    // TCB地址
    stack_frame *stack = (stack_frame *)((node.start_addr + node.size - 16 * 4) & ~0x07); // 栈帧地址,16个uint32_t,每个uint32_t占4字节
    // 初始化栈帧
    stack->r0 = (uint32_t)arg; // 传递参数给任务函数
    stack->PC = (uint32_t)task_name & ((uint32_t)0xfffffffeUL);
    stack->LR = (uint32_t)0xFFFFFFFE; // EXC_RETURN，返回线程模式并使用PSP栈
    stack->xPSR = 0x01000000;         // 处于Thumb模式
    // 初始化TCB
    tcb->stack_top = (void *)&stack->r4;
    tcb->stack_start = (void *)node.start_addr;
    tcb->stack_size = stack_size;
    tcb->priority = priority;
    // 加入就绪队列
    add_task_to_ready_queue(tcb);
	
//	if(current_tcb==NULL||current_tcb->priority<tcb->priority)
//	{
//		current_tcb=tcb;
//	}
//	/*如果当前有正在运行的任务，则要先保存当前任务在执行一次任务切换，
//	如果当前没有正在运行的任务，即这是第一个被创建的任务（空闲任务），不需要进行保存*/
//	if(current_tcb!=NULL)
//	{
//		//add_task_to_ready_queue(current_tcb);
//		// 挂起pendsv,执行一次任务切换
//		*((volatile uint32_t *)0xe000ed04) = 1UL << 28UL;
//	}
	
    exit_critical(pri);
    return tcb;
}

void add_task_to_ready_queue(TCB *tcb)
{
    uint32_t pri = enter_critical();
    // 将节点插入到链表尾
    tcb_node *tmp = &ready_queue[tcb->priority];
    while (tmp->next != NULL)
    {
        tmp = tmp->next;
    }
    // tmp已经指向了最后一个元素
    tcb_node *new_node = malloc(sizeof(tcb_node));
    new_node->tcb = tcb;
    new_node->next = NULL;
    tmp->next = new_node;
    tcb->state = READY; // 修改状态
    exit_critical(pri);
}
void task_switch(void)
{
    uint32_t pri = enter_critical();
    // 将当前任务加入到就绪列表
    if (current_tcb->state == RUNNING) // 在运行态的任务才加入就绪列表
        add_task_to_ready_queue(current_tcb);
    int index = MAX_PRIORITY;
    while (index >= 0)
    {
        if (ready_queue[index].next) // 寻找最高优先级的就绪任务
            break;
        index--;
    }
    if (index == -1 || ready_queue[index].next == NULL) // 就绪队列为空，系统进入空闲状态
        while (1)
            ;
    TCB *result = ready_queue[index].next->tcb;
    // 从就绪队列删除
    tcb_node *tmp = ready_queue[index].next;
    ready_queue[index].next = tmp->next;
    free(tmp);
    result->state = RUNNING; // 修改状态
    current_tcb = result;
    exit_critical(pri);
}
/**
 * @brief 初始化就绪队列、延时队列、超时队列
 */
void queue_init(void)
{
    delay_queue_dummy.tcb = NULL;
    delay_queue_dummy.next = NULL;
    overtime_queue_dummy.tcb = NULL;
    overtime_queue_dummy.next = NULL;
    for (int i = 0; i < MAX_PRIORITY; i++)
    {
        ready_queue[i].tcb = NULL;
        ready_queue[i].next = NULL;
    }
}

/**
 * @brief 将任务加入到延时队列或超时队列
 * @param delay_time 延时时间
 */
void task_delay(uint32_t delay_time)
{
    uint32_t pri = enter_critical();
    if (current_tcb->state != RUNNING)
    {
        exit_critical(pri);
        return; // 任务不在运行态，不加入队列
    }
    uint32_t target_time = time_counter + delay_time;
    tm_node *queue_head = NULL;
    if (target_time > time_counter) // 当前时间+延时时间>当前时间，说明未溢出,加入到延时队列
        queue_head = delay_queue;
    else // 溢出，加入到超时队列
        queue_head = overtime_queue;
    tm_node *tmp = queue_head;
    while (tmp->next != NULL && tmp->next->wake_time < target_time)
        tmp = tmp->next;

    // tmp已经指向了最后一个元素或最后一个元素的唤醒时间大于target_time
    tm_node *new_node = malloc(sizeof(tm_node));
    new_node->tcb = current_tcb;
    new_node->wake_time = target_time;
    new_node->next = tmp->next;
    tmp->next = new_node;
    current_tcb->state = BLOCKED; // 修改状态
    // 挂起pendsv,执行一次任务切换
    *((volatile uint32_t *)0xe000ed04) = 1UL << 28UL;
    exit_critical(pri);
}

bool delet_task(TCB *tcb)
{
    uint32_t pri = enter_critical();
    memory_node delete_memory = {(uint32_t)tcb->stack_start, (uint32_t)tcb->stack_size, NULL, NULL};
    if (tcb->state == BLOCKED)
    {
        // 从延时队列删除
        tm_node *tmp = NULL;
        tm_node *tmp_delay = delay_queue;
        while (tmp_delay->next != NULL && tmp_delay->next->tcb != tcb)
            tmp_delay = tmp_delay->next;
        if (tmp_delay->next != NULL)
        {
            tmp = tmp_delay->next;
            tmp_delay->next = tmp->next;
            free(tmp);
            release_memory(&delete_memory);
            exit_critical(pri);
            return true;
        }
        // 从超时队列删除
        tm_node *tmp_overtime = overtime_queue;
        while (tmp_overtime->next != NULL && tmp_overtime->next->tcb != tcb)
            tmp_overtime = tmp_overtime->next;
        if (tmp_overtime->next != NULL)
        {
            tmp = tmp_overtime->next;
            tmp_overtime->next = tmp->next;
            free(tmp);
            release_memory(&delete_memory);
            exit_critical(pri);
            return true;
        }
    }
    else if (tcb->state == READY)
    {
        tcb_node *tmp = &ready_queue[tcb->priority];
        while (tmp->next != NULL && tmp->next->tcb != tcb)
            tmp = tmp->next;
        if (tmp->next != NULL)
        {
            tcb_node *tmp_node = tmp->next;
            tmp->next = tmp_node->next;
            free(tmp_node);
            release_memory(&delete_memory);
            exit_critical(pri);
            return true;
        }
    }
    exit_critical(pri);
    return false;
}
