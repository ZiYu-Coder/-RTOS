#include "memory.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

uint8_t memory[memory_size];
memory_node memory_list; // 链表头，使用双向链表维护内存

bool memory_init(void)
{
    // 首节点设置为哨兵节点，方便操作
    memory_list.size = 0;
    memory_list.start_addr = 0;
    // 创建首个节点
    memory_node *new_node = (memory_node *)malloc(sizeof(memory_node));
    if (new_node == NULL)
        return false;
    new_node->next = &memory_list;
    new_node->prev = &memory_list;
    memory_list.next = new_node;
    memory_list.prev = new_node;
    new_node->start_addr = (uint32_t)memory;
    new_node->size = memory_size;
    return true;
}

/**
 * 分配内存,选择最合适的内存块进行分配,
 * return addr,0:false
 */
memory_node get_memory(uint32_t size)
{
    memory_node result = {0, 0, NULL, NULL};
    // 寻找最合适的内存块
    memory_node *target = NULL;
    int target_size = memory_size + 1;
    for (memory_node *tmp = memory_list.next; tmp != &memory_list; tmp = tmp->next)
    {
        if (tmp->size >= size && tmp->size < target_size)
        {
            target_size = tmp->size;
            target = tmp;
        }
    }
    if (target_size == memory_size + 1) // 未找到合适的块
        return result;

    result.start_addr = target->start_addr;
    uint32_t next_addr = target->start_addr + size; // 下一块的地址

    if (target->size == size) // 内存被完全分配
    {
        result.size = target->size;
        target->next->prev = target->prev;
        target->prev->next = target->next;
        free(target);
    }
    else // 内存未被完全分配
    {
        result.size = size;
        target->size -= size;
        target->start_addr = next_addr;
    }

    return result;
}

void release_memory(memory_node *target)
{
    memory_node *current = target;
    for (memory_node *tmp = memory_list.next; tmp != &memory_list; tmp = tmp->next)
    {
        if (tmp->start_addr + tmp->size == target->start_addr) // 向前合并
        {
            tmp->size += target->size;
            current = tmp;
            break;
        }
    }

    for (memory_node *tmp = memory_list.next; tmp != &memory_list; tmp = tmp->next)
    {
        if (current->start_addr + current->size == tmp->start_addr) // 向后合并
        {
            tmp->start_addr = current->start_addr;
            tmp->size += current->size;
            if (current != target) // 已经向前合并过,需要将前面的节点删除
            {
                current->prev->next = tmp;
                tmp->prev = current->prev;
                free(current);
            }
            current = tmp;
            break;
        }
    }
    if (current == target) // 不能向前或向后合并
    {
        // 插入
        memory_node *new_node = malloc(sizeof(memory_node));
        new_node->size = target->size;
        new_node->start_addr = target->start_addr;

        memory_node *tmp = &memory_list;
        while (1)
        {
            if (tmp->next == &memory_list || tmp->next->start_addr > new_node->start_addr)
            {
                new_node->next = tmp->next;
                new_node->prev = tmp;
                tmp->next->prev = new_node;
                tmp->next = new_node;
                break;
            }
            tmp = tmp->next;
        }
    }
}
