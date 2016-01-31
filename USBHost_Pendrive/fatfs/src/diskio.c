/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2014        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */
#include "usbdisk.h"	/* Example: Header file of existing USB MSD control module */

/* Definitions of physical drive number for each drive */
#define USB		0	/* Example: Map USB MSD to physical drive 2 */


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;

	switch (pdrv)
	{
		case USB :
			stat = USB_disk_status();
			return(stat);
		break;
	}
	return(STA_NOINIT);
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	DSTATUS stat;

	switch (pdrv)
	{
		case USB :
			stat = USB_disk_initialize();
			return(stat);
		break;
	}
	return(STA_NOINIT);
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Sector address in LBA */
	UINT count		/* Number of sectors to read */
)
{
	DRESULT res;

	switch (pdrv)
	{
		case USB :
			res = USB_disk_read(buff, sector, count);
			return(res);
		break;
	}
	return(RES_PARERR);
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if _USE_WRITE
DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Sector address in LBA */
	UINT count			/* Number of sectors to write */
)
{
	DRESULT res;

	switch (pdrv)
	{
		case USB :
			res = USB_disk_write(buff, sector, count);
			return(res);
		break;
	}

	return(RES_PARERR);
}
#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

#if _USE_IOCTL
DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	DRESULT res;

	switch (pdrv)
	{
		case USB :
			res = USB_disk_ioctl(cmd, buff);
			return(res);
		break;
	}

	return(RES_PARERR);
}
#endif
