#include "application.h"
#include "neopixel-dma/ws2812.h"

ws2812 ws2812;


void setup() 
{
	int lednum = 3;
	
	ws2812.init(lednum);
	for(int i=0;i<100;i++)
	{
		for(int k=0;k<lednum;k++)
		{
			ws2812.framebuffer[k].red=i;
			ws2812.framebuffer[k].blue=i;
			ws2812.framebuffer[k].green=i;
		}

		ws2812.show();
		delay(300);
	}




}
void loop() 
{

}
