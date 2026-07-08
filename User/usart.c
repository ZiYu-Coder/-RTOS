
#include "stm32f10x.h"                  // Device header
#include "usart.h"
#include <stdio.h>
void USER_USART1_INIT(void)
{
	
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);//ʱ��ʹ��
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);//GPIOʹ��
	
	GPIO_InitTypeDef GPIO_InitStruct_TX;
	GPIO_InitStruct_TX.GPIO_Mode=GPIO_Mode_AF_PP;
	GPIO_InitStruct_TX.GPIO_Pin=GPIO_Pin_9;//tx
	GPIO_InitStruct_TX.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct_TX);
	
	GPIO_InitTypeDef GPIO_InitStruct_RX;
	GPIO_InitStruct_RX.GPIO_Mode=GPIO_Mode_IPU;//����״̬Ϊ�ߵ�ƽ�������������������
	GPIO_InitStruct_RX.GPIO_Pin=GPIO_Pin_10;//rx
	GPIO_InitStruct_RX.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct_RX);
	
	USART_DeInit (USART1);
	
	USART_InitTypeDef USART1_InitStruct;
	USART1_InitStruct.USART_BaudRate=115200;
	USART1_InitStruct.USART_HardwareFlowControl=USART_HardwareFlowControl_None;
	USART1_InitStruct.USART_Mode=USART_Mode_Tx|USART_Mode_Rx;
	USART1_InitStruct.USART_Parity=USART_Parity_No;
	USART1_InitStruct.USART_StopBits=USART_StopBits_1;
	USART1_InitStruct.USART_WordLength=USART_WordLength_8b;
	USART_Init(USART1, &USART1_InitStruct);
	
	
	 // 配置串口接收触发DMA
    USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
    // 配置串口开启空闲中断

	NVIC_InitTypeDef nvic_init;
	nvic_init.NVIC_IRQChannel=USART1_IRQn;
	nvic_init.NVIC_IRQChannelCmd=ENABLE;
	nvic_init.NVIC_IRQChannelPreemptionPriority=0;
	nvic_init.NVIC_IRQChannelSubPriority=0;
	NVIC_Init(&nvic_init);
	USART_ITConfig(USART1, USART_IT_IDLE, ENABLE);
	USART_Cmd(USART1, ENABLE);//USART1ֻ��ѡ��PA9��10������ָ������Ϊ���ֹ��ܣ�ʹ��USART1����
	
}





int fputc(int ch, FILE *f)
{
	USART_SendData(USART1, ch);
	while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
	return ch;
}

int fgetc(FILE *stream)
{
	while(!(USART1->SR & (1 << 5))){};//�ȴ����ݽ������
	return USART1->DR;
}


