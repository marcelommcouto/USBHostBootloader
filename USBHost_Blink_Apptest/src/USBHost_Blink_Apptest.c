/*
===============================================================================
 Name        : USBHost_Blink_Apptest.c
 Author      : $(author)
 Version     :
 Copyright   : $(copyright)
 Description : main definition
===============================================================================
*/

#ifdef __USE_CMSIS
#include "LPC17xx.h"
#endif

#include <cr_section_macros.h>
#include <stdio.h>

int main(void)
{
	uint32_t i, j, k;
	uint8_t loops = 50, repeat = 2;
	uint8_t z = 0;

	/* Initialize System. */
	SystemInit();

	/* Initialize Clock */
	SystemCoreClockUpdate();

	for(i = 4; i < 12; i++)
	{
		LPC_GPIO0->FIODIR |= (1 << i);
	}

	while(1)
	{
		for (k = 0; k < repeat; k++)
		{
			LPC_GPIO0->FIOCLR |= (1 << (4 + z));
			LPC_GPIO0->FIOCLR |= (1 << (11 - z));

			for(j = 0; j < loops; j++)
			{
				for(i=0; i < 30000; i++);
			}

			LPC_GPIO0->FIOSET |= (1 << (4 + z));
			LPC_GPIO0->FIOSET |= (1 << (11 - z));

			for(j = 0; j < loops; j++)
			{
				for(i=0; i < 30000; i++);
			}
		}
		z++;
		if (z == 4) z = 0;
	}
}
