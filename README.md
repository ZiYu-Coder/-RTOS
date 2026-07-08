# 手写一个轻量级 RTOS

这是一个运行在 STM32F103C8 上的轻量级 RTOS 学习工程。代码没有直接移植 FreeRTOS，而是用 C 和少量 Cortex-M3 汇编实现了任务创建、任务调度、任务延时、任务删除、简单内存管理和上下文切换。

项目的重点不是做一个功能完整的操作系统，而是把 RTOS 最核心的机制拆开：任务栈怎么初始化，第一次任务怎么启动，任务之间怎么切换，`SysTick`、`SVC`、`PendSV` 在调度里分别负责什么。

## 项目特点

- 基于 STM32F103C8 和 Keil MDK 工程
- 使用 `SysTick` 作为系统节拍，默认 1ms tick
- 使用 `SVC` 启动第一个任务
- 使用 `PendSV` 完成任务上下文切换
- 支持任务创建、延时、删除和优先级调度
- 使用简单内存池为任务栈分配空间
- 通过串口打印测试多个任务的运行效果

## 工程结构

```text
.
├── Start/          STM32 启动文件、异常向量表、Cortex-M3 支撑代码
├── Library/        STM32F10x 标准外设库
├── User/           用户代码，包含 main.c 和串口初始化
├── RTOS/           手写 RTOS 内核代码
├── DebugConfig/    Keil 调试配置
└── Project.uvprojx Keil 工程文件
```

`RTOS` 目录是核心：

```text
RTOS
├── config.h   系统配置：tick 频率、优先级数量、内存池大小
├── memory.c   简单内存池，实现任务栈空间申请和释放
├── memory.h
├── task.c     任务创建、就绪队列、延时队列、任务切换
├── task.h
├── rtos.c     RTOS 初始化、SysTick、SVC、PendSV、临界区
└── rtos.h
```

## 运行流程

系统启动后，大致会走这条链路：

```text
Reset_Handler
    ↓
SystemInit
    ↓
main
    ↓
rtos_init
    ↓
create_task
    ↓
start_scheduler
    ↓
SVC_Handler 启动第一个任务
    ↓
SysTick_Handler 周期触发系统节拍
    ↓
PendSV_Handler 执行上下文切换
```

`main.c` 中创建了两个测试任务：

```c
rtos_init();
test1_tcb = create_task(test1, 10, 300, NULL);
test2_tcb = create_task(test2, 10, 500, NULL);
start_scheduler();
```

`test1` 每 500ms 打印一次，`test2` 每 1000ms 打印一次。`test2` 运行一段时间后会删除 `test1`，之后再创建 `test3`，用于验证任务删除和动态创建。

## 核心实现

### 任务控制块

每个任务使用 `TCB` 描述：

```c
typedef struct task_control_block
{
    void *stack_top;
    void *stack_start;
    uint32_t stack_size;
    uint32_t priority;
    enum task_state state;
} TCB;
```

其中 `stack_top` 保存任务当前栈顶。任务切出时保存现场，任务切回时根据这个地址恢复现场。

### 任务栈初始化

创建任务时，系统会在任务栈中提前构造一份 Cortex-M3 异常返回现场，包括：

```text
r4-r11, r0-r3, r12, LR, PC, xPSR
```

`PC` 被设置为任务函数地址，`r0` 被设置为任务参数。这样第一次恢复任务现场时，CPU 会像从异常返回一样跳入任务函数。

### 上下文切换

上下文切换由 `PendSV_Handler` 完成。流程可以简化为：

```text
保存当前任务 r4-r11
更新当前任务 stack_top
调用 task_switch 选择下一个任务
恢复新任务 r4-r11
更新 PSP
异常返回到新任务
```

Cortex-M3 进入异常时会自动保存 `r0-r3、r12、LR、PC、xPSR`，所以汇编里主要手动处理 `r4-r11`。

### 任务延时

`task_delay()` 不会忙等。它会把当前任务放入延时队列，并把状态改为 `BLOCKED`。之后触发 `PendSV` 切换到其他任务。

`SysTick_Handler` 每 1ms 检查延时队列，到时间的任务会重新进入就绪队列，等待调度。

## 如何打开工程

1. 安装 Keil MDK。
2. 安装 STM32F1 系列芯片支持包。
3. 使用 Keil 打开 `Project.uvprojx`。
4. 编译并下载到 STM32F103C8 开发板。
5. 打开串口工具，查看任务打印输出。

串口初始化代码在 `User/usart.c` 中，测试任务在 `User/main.c` 中。

## 当前限制

这个工程主要用于学习 RTOS 内核原理，还不是完整的生产级 RTOS。目前没有实现：

- 信号量
- 互斥锁
- 消息队列
- 软件定时器
- 任务栈溢出检测
- 完整的错误处理机制

代码里也有一些值得继续改进的地方，例如任务优先级边界检查、延时队列节点释放后的访问安全、内存池和 `malloc/free` 的统一管理等。

## 学习重点

读这个工程时，可以重点看这几个文件：

- `RTOS/task.c`：任务创建、就绪队列、延时队列、任务切换
- `RTOS/rtos.c`：`SysTick`、`SVC`、`PendSV` 和临界区
- `RTOS/memory.c`：简单内存池
- `User/main.c`：任务创建和运行测试
- `Start/startup_stm32f10x_md.s`：异常向量表和启动流程

如果想理解 RTOS 的底层原理，最值得盯住的是这条线：

```text
任务栈初始化 → SVC 启动第一个任务 → SysTick 产生节拍 → PendSV 切换上下文
```

理解了这条线，再去看 FreeRTOS、RT-Thread 这类成熟 RTOS，很多宏、链表和汇编代码就会清楚很多。