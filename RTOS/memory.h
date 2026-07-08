// 此文件定义了内存管理相关的函数和变量
#ifndef MEMORY_H_
#define MEMORY_H_
#include "stm32f10x.h"
#include <stdbool.h>
#include "config.h"
typedef struct node
{
    uint32_t start_addr;
    uint32_t size;
    struct node *prev;
    struct node *next;
} memory_node;

extern uint8_t memory[memory_size];
extern memory_node memory_list;

bool memory_init(void);
memory_node get_memory(uint32_t size);
void release_memory(memory_node *target);
#endif
