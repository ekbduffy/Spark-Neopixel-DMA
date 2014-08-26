#include "ws2812.h"
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_dma.h"
#include "spark_wiring_usbserial.h"
#include "misc.h"



TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
TIM_OCInitTypeDef  TIM_OCInitStructure;
GPIO_InitTypeDef GPIO_InitStructure;
DMA_InitTypeDef DMA_InitStructure;
NVIC_InitTypeDef NVIC_InitStructure;



void ws2812::init_buffers(int size)
{
	int pwm_size;
	pwm_size = size*24+48;
	pwm_buffer_size = pwm_size;
	framebuffer_size = size;

	PWM_Buffer = (uint16_t*) calloc (pwm_buffer_size,sizeof(uint16_t));
	framebuffer = (led*) calloc (framebuffer_size,sizeof(led));
}

void ws2812::init(int size) {
	init_buffers(size);


	uint16_t PrescalerValue;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	/* GPIOA Configuration: TIM3 Channel 1 as alternate function push-pull */
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;        /// D1 on board
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	/* Compute the prescaler value */
	PrescalerValue = (uint16_t) (SystemCoreClock / 24000000) - 1;
	/* Time base configuration */
	TIM_TimeBaseStructure.TIM_Period = 29; // 800kHz  
	TIM_TimeBaseStructure.TIM_Prescaler = PrescalerValue; 
	TIM_TimeBaseStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);
	/* PWM1 Mode configuration: Channel1 */
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 0;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
	TIM_OC1Init(TIM4, &TIM_OCInitStructure);
	/* configure DMA */
	/* DMA clock enable */
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
	/* DMA1 Channel6 Config */
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) & TIM4->CCR1;	// physical address of Timer 4 CCR1
	DMA_InitStructure.DMA_MemoryBaseAddr = (uint32_t) PWM_Buffer;		// this is the buffer memory 
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;						// data shifted from memory to peripheral
	DMA_InitStructure.DMA_BufferSize = pwm_buffer_size;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;					// automatically increase buffer index
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;							// stop DMA feed after buffer size is reached
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;

	DMA_Init(DMA1_Channel1, &DMA_InitStructure);
	//DMA_ITConfig(DMA1_Channel1, DMA_IT_TC , ENABLE);//	DMA_ITConfig(DMA1_Channel1, DMA_IT_TC | DMA_IT_HT, ENABLE);

	/* TIM4 CC1 DMA Request enable */
	TIM_DMACmd(TIM4, TIM_DMA_CC1, ENABLE);
/*	NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 4;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
*/
}

// writes the pwm values of one byte into the array which will be used by the dma
void ws2812::color2pwm(int pos, uint8_t color1, uint8_t color2, uint8_t color3) {
	uint8_t mask;
	int j,pwm_pos = pos*24;
	mask = 0x80;

	if(color1 > 255)
		color1 = 255;
	if(color2 > 255)
		color2 = 255;
	if(color3 > 255)
		color3 = 255;


	for (j = 0; j < 8; j++)				
	{
		if ( (color1<<j) & 0x80 )
		{
			PWM_Buffer[pwm_pos+j] = 15; 	// compare value for logical 1
		}
		else
		{
			PWM_Buffer[pwm_pos+j] = 6;	// compare value for logical 0
		}

		if ( (color2<<j) & 0x80 )	// data sent MSB first, j = 0 is MSB j = 7 is LSB
		{
			PWM_Buffer[pwm_pos+8+j] = 15; 	// compare value for logical 1
		}
		else
		{
			PWM_Buffer[pwm_pos+8+j] = 6;	// compare value for logical 0
		}

		if ( (color3<<j) & 0x80 )	// data sent MSB first, j = 0 is MSB j = 7 is LSB
		{
			PWM_Buffer[pwm_pos+16+j] = 15; 	// compare value for logical 1
		}
		else
		{
			PWM_Buffer[pwm_pos+16+j] = 6;	// compare value for logical 0
		}
	}
}

void ws2812::show(void) {
	Update_Buffer();
	DMA_SetCurrDataCounter(DMA1_Channel1, pwm_buffer_size); 	// load number of bytes to be transferred
	TIM_Cmd(TIM4, ENABLE); 						// enable Timer 3
	DMA_Cmd(DMA1_Channel1, ENABLE); 			// enable DMA channel 6
//	TIM_DMACmd(TIM4, TIM_DMA_CC1, ENABLE);
//	TIM_DMACmd(TIM4, TIM_DMA_Update, ENABLE);

	while(!DMA_GetFlagStatus(DMA1_FLAG_TC1)); 	// wait until transfer complete
	TIM_Cmd(TIM3, DISABLE); 					// disable Timer 3
	DMA_Cmd(DMA1_Channel1, DISABLE); 			// disable DMA channel 6
	DMA_ClearFlag(DMA1_FLAG_TC1); 				// clear DMA1 Channel 6 transfer complete flag

}

void ws2812::direct_show(void) {
	DMA_SetCurrDataCounter(DMA1_Channel1, pwm_buffer_size); 	// load number of bytes to be transferred
	TIM_Cmd(TIM4, ENABLE); 						// enable Timer 3
	DMA_Cmd(DMA1_Channel1, ENABLE); 			// enable DMA channel 6
//	TIM_DMACmd(TIM4, TIM_DMA_CC1, ENABLE);
//	TIM_DMACmd(TIM4, TIM_DMA_Update, ENABLE);

	while(!DMA_GetFlagStatus(DMA1_FLAG_TC1)); 	// wait until transfer complete
	TIM_Cmd(TIM3, DISABLE); 					// disable Timer 3
	DMA_Cmd(DMA1_Channel1, DISABLE); 			// disable DMA channel 6
	DMA_ClearFlag(DMA1_FLAG_TC1); 				// clear DMA1 Channel 6 transfer complete flag

}



void ws2812::Update_Buffer()
{
	struct led *framebufferp;
	int i,k;

	for (int i = 0; i < framebuffer_size; i++) 
	{
		color2pwm(i, framebuffer[i].green,framebuffer[i].red,framebuffer[i].blue); // green
	}

}
