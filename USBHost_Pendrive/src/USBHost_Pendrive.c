/*
===============================================================================
 Name        : USBHost_Pendrive.c
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
#include <stdbool.h>

#include "USBMassStorageHost.h"
#include "bootloaderconfig.h"

#include "lpc17xx_iap.h"

// TODO: insert other include files here
static void checkisppin(void);
static void executeuserapp(void);
static bool userapppresent(void);

// TODO: insert other definitions and declarations here

int main(void)
{
	/* Initialize System. */
	SystemInit();

	/* Initialize Clock */
	SystemCoreClockUpdate();

	/* Check to see if there is a user application in the LPC1768's flash memory. */
	if(userapppresent())
	{
		/* There is an application, but need to check if user is pressing the button
		   to indicate they want to upload a new application. */
		checkisppin();
	}

	/* User code not present or isp entry requested. */
	enterusbisp();

	/* Note - should never actually return from enter_usb_isp (). */
	while (1);

	return(0);
}

static void checkisppin(void)
{
    if( (*(volatile uint32_t *)ISP_ENTRY_GPIO_REG) & (0x1 << ISP_ENTRY_PIN) )
	{
		executeuserapp();
	}
	else
	{
	    /* Enter ISP mode. */
	}
}

static void executeuserapp(void)
{
	void (*user_code_entry)(void);

	uint32_t *p;	/* used for loading address of reset handler from user flash. */

	/* Change the Vector Table to the USER_FLASH_START
	in case the user application uses interrupts */
	SCB->VTOR = (USER_FLASH_START & 0x1FFFFF80);

	/* Load contents of second word of user flash - the reset handler address in the applications vector table. */
	p = (uint32_t *)(USER_FLASH_START + 4);

	/* Set user_code_entry to be the address contained in that second word of user flash. */
	user_code_entry = (void *) *p;

	/* Jump to user application. */
    user_code_entry();

}

static bool userapppresent(void)
{
	uint32_t offset;
	BlankCheckSector(USER_START_SECTOR, USER_START_SECTOR, &offset, NULL);

	if(offset == 0)
	{
		return(false);
	}

#ifdef COMPUTE_BINARY_CHECKSUM
/*
 * The reserved Cortex-M3 exception vector location 7 (offset 0x001C
 * in the vector table) should contain the 2’s complement of the
 * checksum of table entries 0 through 6. This causes the checksum
 * of the first 8 table entries to be 0. This code checksums the
 * first 8 locations of the start of user flash. If the result is 0,
 * then the contents is deemed a 'valid' image.
 */
	uint32_t *pmem = (uint32_t *)USER_FLASH_START;
	uint32_t checksum = 0, i;

	for (i = 0; i <= 7; i++)
	{
		checksum += *pmem;
		pmem++;
	}
	if (checksum != 0)
	{
		return (false);
	}
	else
#endif
	{
	    return (true);
	}
}
