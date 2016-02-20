/*
 * @brief USB Mass Storage Chan FATFS simple abstraction layer
 *
 * @note
 * Copyright(C) NXP Semiconductors, 2012
 * All rights reserved.
 *
 * @par
 * Software that is described herein is for illustrative purposes only
 * which provides customers with programming information regarding the
 * LPC products.  This software is supplied "AS IS" without any warranties of
 * any kind, and NXP Semiconductors and its licensor disclaim any and
 * all warranties, express or implied, including all implied warranties of
 * merchantability, fitness for a particular purpose and non-infringement of
 * intellectual property rights.  NXP Semiconductors assumes no responsibility
 * or liability for the use of the software, conveys no license or rights under any
 * patent, copyright, mask work right, or any other intellectual property rights in
 * or to any products. NXP Semiconductors reserves the right to make changes
 * in the software without notification. NXP Semiconductors also makes no
 * representation or warranty that such application will be suitable for the
 * specified use without further testing or modification.
 *
 * @par
 * Permission to use, copy, modify, and distribute this software and its
 * documentation is hereby granted, under NXP Semiconductors' and its
 * licensor's relevant copyrights in the software, without fee, provided that it
 * is used in conjunction with NXP Semiconductors microcontrollers.  This
 * copyright, permission, and disclaimer notice must appear in all copies of
 * this code.
 */

#include "LPC17xx.h"
#include "USBMassStorageHost.h"
#include "usbdisk.h"
#include "bootloaderconfig.h"

/*****************************************************************************
 * Private types/enumerations/variables
 ****************************************************************************/
static volatile DSTATUS status = STA_NOINIT;	/* Disk status */
static DISK_HANDLE_T *hDisk;

/*****************************************************************************
 * Public types/enumerations/variables
 ****************************************************************************/

/*****************************************************************************
 * Private functions
 ****************************************************************************/

/*****************************************************************************
 * Public functions
 ****************************************************************************/
DRESULT USB_disk_ioctl (BYTE cmd, void *buff)
{
	DRESULT res;

	if (status & STA_NOINIT)
	{
		return(RES_NOTRDY);
	}

	res = RES_ERROR;

	switch (cmd)
	{
		case CTRL_SYNC:	/* Make sure that no pending write process */
			if (FSUSB_DiskReadyWait(hDisk, 50))
			{
				res = RES_OK;
			}
		break;
		case GET_SECTOR_COUNT:	/* Get number of sectors on the disk (DWORD) */
			*(DWORD *) buff = FSUSB_DiskGetSectorCnt(hDisk);
			res = RES_OK;
		break;
		case GET_SECTOR_SIZE:	/* Get R/W sector size (WORD) */
			*(WORD *) buff = FSUSB_DiskGetSectorSz(hDisk);
			res = RES_OK;
		break;
		case GET_BLOCK_SIZE:/* Get erase block size in unit of sector (DWORD) */
			*(DWORD *) buff = FSUSB_DiskGetBlockSz(hDisk);
			res = RES_OK;
		break;
		default:
			res = RES_PARERR;
		break;
	}

	return(res);
}


/***************************************************************************/
DSTATUS USB_disk_status(void)
{
	return(status);
}

/***************************************************************************/
DSTATUS USB_disk_initialize(void)
{
	if (status != STA_NOINIT)
	{
		return(status);					/* card is already enumerated */
	}

	#if !_FS_READONLY
		FSUSB_InitRealTimeClock();
	#endif

	/* Initialize the Card Data Strucutre */
	hDisk = FSUSB_DiskInit();

	/* Reset */
	status = STA_NOINIT;

	FSUSB_DiskInsertWait(hDisk); /* Wait for card to be inserted */

	/* Enumerate the card once detected. Note this function may block for a little while. */
	if (!FSUSB_DiskAcquire(hDisk))
	{
		PRINTDBG("Disk Enumeration failed...\r\n");
		return(status);
	}

	status &= ~STA_NOINIT;

	return(status);
}


/***************************************************************************/
DRESULT USB_disk_read(BYTE *buff, DWORD sector, UINT count)
{
	if (status & STA_NOINIT)
	{
		return(RES_NOTRDY);
	}

	if (FSUSB_DiskReadSectors(hDisk, buff, sector, count))
	{
		return(RES_OK);
	}

	return(RES_ERROR);
}

/***************************************************************************/
DRESULT USB_disk_write(BYTE *buff, DWORD sector, UINT count)
{
	if (status & STA_NOINIT)
	{
		return(RES_NOTRDY);
	}

	if (FSUSB_DiskWriteSectors(hDisk, (void *) buff, sector, count))
	{
		return(RES_OK);
	}
	return(RES_ERROR);
}
