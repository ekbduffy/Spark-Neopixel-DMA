#ifndef __ws2811_H__
#define __ws2811_H__

#include <stm32f10x.h>


	typedef struct led {
		unsigned int red : 8;
		unsigned int green : 8;
		unsigned int blue : 8;
	} led;



class ws2812
{
	public:
	int framebuffer_size;
	int pwm_buffer_size;
	led * framebuffer;
    void init(int size);
	void show(void);
	void direct_show(void);
	uint16_t * PWM_Buffer;
	void Update_Buffer();
	void init_buffers(int size);
	void color2pwm(int pos, uint8_t color1,  uint8_t color2,  uint8_t color3);
};

#endif
