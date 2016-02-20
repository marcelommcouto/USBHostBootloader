/*
 * USBMassStorageHost.c
 *
 *  Created on: Feb 10, 2015
 *      Author: marcelo
 */

#include "LPC17xx.h"

#include "USBMassStorageHost.h"

#include "USB.h"
#include "MassStorageClassHost.h"
#include "bootloaderconfig.h"

#include "lpc17xx_iap.h"

#include "ff.h"
#include "ffconf.h"
#include "usbdisk.h"

/** LPCUSBlib Mass Storage Class driver interface configuration and state information. This structure is
 *  passed to all Mass Storage Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
static USB_ClassInfo_MS_Host_t FlashDisk_MS_Interface = {
	.Config = {
		.DataINPipeNumber       = 1,
		.DataINPipeDoubleBank   = false,
		.DataOUTPipeNumber      = 2,
		.DataOUTPipeDoubleBank  = false,
		.PortNumber = 0,
	},
};

const uint32_t sector_start_map[MAX_FLASH_SECTOR] = {SECTOR_0_START,             \
SECTOR_1_START,SECTOR_2_START,SECTOR_3_START,SECTOR_4_START,SECTOR_5_START,      \
SECTOR_6_START,SECTOR_7_START,SECTOR_8_START,SECTOR_9_START,SECTOR_10_START,     \
SECTOR_11_START,SECTOR_12_START,SECTOR_13_START,SECTOR_14_START,SECTOR_15_START, \
SECTOR_16_START,SECTOR_17_START,SECTOR_18_START,SECTOR_19_START,SECTOR_20_START, \
SECTOR_21_START,SECTOR_22_START,SECTOR_23_START,SECTOR_24_START,SECTOR_25_START, \
SECTOR_26_START,SECTOR_27_START,SECTOR_28_START,SECTOR_29_START					 };

const uint32_t sector_end_map[MAX_FLASH_SECTOR] = {SECTOR_0_END,SECTOR_1_END,    \
SECTOR_2_END,SECTOR_3_END,SECTOR_4_END,SECTOR_5_END,SECTOR_6_END,SECTOR_7_END,   \
SECTOR_8_END,SECTOR_9_END,SECTOR_10_END,SECTOR_11_END,SECTOR_12_END,             \
SECTOR_13_END,SECTOR_14_END,SECTOR_15_END,SECTOR_16_END,SECTOR_17_END,           \
SECTOR_18_END,SECTOR_19_END,SECTOR_20_END,SECTOR_21_END,SECTOR_22_END,           \
SECTOR_23_END,SECTOR_24_END,SECTOR_25_END,SECTOR_26_END,                         \
SECTOR_27_END,SECTOR_28_END,SECTOR_29_END										 };

static SCSI_Capacity_t DiskCapacity;
static uint8_t buffer[_MIN_SS];
static FATFS fatFS;	/* File system object */
static FIL fileObj;	/* File object */

/* Task to Write Data in USBPendrive. */
void enterusbisp(void)
{
	/* Configure LED stats direction. */
	LEDPORT->FIODIR |= (1 << LEDPORTPIN);

	/* Configure the NVIC Preemption Priority Bits */
	NVIC_SetPriorityGrouping(1);

	USB_CurrentMode = USB_MODE_Host;
	USB_Disable();
	USB_Init();

	PRINTDBG("\nInit USB Bootloader Read.");

	if(USB_ReadWriteFile() == true)
	{
		LEDPORT->FIOPIN |= (1 << LEDPORTPIN);			/* LED's OFF. */
	}

	while (1);
}

/* Function to spin forever when there is an error */
static void die(FRESULT rc)
{
	PRINTDBG("\n!!!! CRITICAL ERROR !!!!");
	while (1);/* Spin for ever */
}

/* Function to do the read/write to USB Disk */
bool USB_ReadWriteFile(void)
{
	FRESULT rc;		/* Result code */
	UINT br;
	uint32_t *address = USER_FLASH_START;
	uint16_t i = 0;

	if(f_mount(&fatFS, "0:", 1) != FR_OK)
	{
		PRINTDBG("\nUnable to mount pendrive.");	/* First... Mounting USB Unit. */
		return(false);
	}

	if (f_open(&fileObj, "0:firmware.bin", FA_READ) != FR_OK)
	{
		PRINTDBG("\nUnable to open firmware.bin from USB.");
		return(false);
	}

	/* LED's ON. */
	LEDPORT->FIOPIN &= ~(1 << LEDPORTPIN);

	PRINTDBG("\nReading firmware.bin from USB.");

	for (;;)
	{
		/* Clear Buffer. */
		for(i = 0; i < _MIN_SS; i++) buffer[i] = 0;

		/* Read a chunk of file */
		rc = f_read(&fileObj, &buffer, sizeof(buffer), &br);

		if (rc || !br)
		{
			break;					/* Error or end of file */
		}

		if(address == (uint32_t *)USER_FLASH_START)
		{
			uint32_t checksum = 0;
			uint32_t *tmpptr;

			tmpptr = (uint32_t *)&buffer[0];
			for (i = 0; i < 8; i++)
			{
				checksum += *tmpptr;
				tmpptr++;
			}
			if (checksum != 0)
			{
				rc = 1;
				PRINTDBG("\nChecksum Error.");
				return(false);
			}
			PRINTDBG("\nChecksum Valid.");
		}
		/* */
		__disable_irq();

		for(i = USER_START_SECTOR; i <= MAX_USER_SECTOR; i++)
		{
			if(address < sector_end_map[i])
			{
				if(address == sector_start_map[i])
				{
					EraseSector(i,i);
				}
				break;
			}
		}
		//
		if(CopyRAM2Flash((uint8_t *)address, &buffer[0], FLASH_BUF_SIZE) != CMD_SUCCESS)
		{
			return(false); /* No way to recover. Just let Windows report a write failure */
		}
		//
#if DEBUG
		printf("\n%d bytes writed in %#08x.", br, address);
#endif
		address += (FLASH_BUF_SIZE / sizeof(uint32_t));
		__enable_irq();

	}

	if (f_close(&fileObj) != FR_OK)
	{
		PRINTDBG("\nError to close file.");
		return(false);
	}

	PRINTDBG("\nClose the file.");

	USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);

	PRINTDBG("\nFlash userdata updated.");

	return(true);
}

/** Event handler for the USB_DeviceAttached event. This indicates that a device has been attached to the host, and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Host_DeviceAttached(const uint8_t corenum)
{
	PRINTDBGA(("\nDevice Attached on port %d."), corenum);
}

/** Event handler for the USB_DeviceUnattached event. This indicates that a device has been removed from the host, and
 *  stops the library USB task management process.
 */
void EVENT_USB_Host_DeviceUnattached(const uint8_t corenum)
{
	PRINTDBGA(("\nDevice Unattached on port %d."), corenum);
}

/** Event handler for the USB_DeviceEnumerationComplete event. This indicates that a device has been successfully
 *  enumerated by the host and is now ready to be used by the application.
 */
void EVENT_USB_Host_DeviceEnumerationComplete(const uint8_t corenum)
{
	uint16_t ConfigDescriptorSize;
	uint8_t  ConfigDescriptorData[512];

	if (USB_Host_GetDeviceConfigDescriptor(corenum, 1, &ConfigDescriptorSize, ConfigDescriptorData,
										   sizeof(ConfigDescriptorData)) != HOST_GETCONFIG_Successful) {
		PRINTDBG("\nError Retrieving Configuration Descriptor.");
		return;
	}

	FlashDisk_MS_Interface.Config.PortNumber = corenum;
	if (MS_Host_ConfigurePipes(&FlashDisk_MS_Interface, ConfigDescriptorSize, ConfigDescriptorData) != MS_ENUMERROR_NoError) {
		PRINTDBG("\nAttached Device Not a Valid Mass Storage Device.");
		return;
	}

	if (USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 1) != HOST_SENDCONTROL_Successful) {
		PRINTDBG("\nError Setting Device Configuration.");
		return;
	}

	uint8_t MaxLUNIndex;
	if (MS_Host_GetMaxLUN(&FlashDisk_MS_Interface, &MaxLUNIndex))
	{
		PRINTDBG("\nError retrieving max LUN index.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return;
	}

	PRINTDBGA("\nTotal LUNs: %d - Using first LUN in device.", (MaxLUNIndex + 1));

	if (MS_Host_ResetMSInterface(&FlashDisk_MS_Interface)) {
		PRINTDBG("\nError resetting Mass Storage interface.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return;
	}

	SCSI_Request_Sense_Response_t SenseData;
	if (MS_Host_RequestSense(&FlashDisk_MS_Interface, 0, &SenseData) != 0) {
		PRINTDBG("\nError retrieving device sense.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return;
	}

	if (MS_Host_PreventAllowMediumRemoval(&FlashDisk_MS_Interface, 0, true)) {
		PRINTDBG("\nError setting Prevent Device Removal bit.");
	    USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
	    return;
	}

	SCSI_Inquiry_Response_t InquiryData;
	if (MS_Host_GetInquiryData(&FlashDisk_MS_Interface, 0, &InquiryData)) {
		PRINTDBG("\nError retrieving device Inquiry data.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return;
	}

	PRINTDBG("\nMass Storage Device Enumerated.");
}

/** Event handler for the USB_HostError event. This indicates that a hardware error occurred while in host mode. */
void EVENT_USB_Host_HostError(const uint8_t corenum, const uint8_t ErrorCode)
{
	USB_Disable();

	for (;; ) {}
}

/** Event handler for the USB_DeviceEnumerationFailed event. This indicates that a problem occurred while
 *  enumerating an attached USB device.
 */
void EVENT_USB_Host_DeviceEnumerationFailed(const uint8_t corenum,
											const uint8_t ErrorCode,
											const uint8_t SubErrorCode)
{

}

/**
 * Dummy callback function for DeviceStandardReq.c,
 * this way i don't need remove files.
 */
uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue,
									const uint8_t wIndex,
									const void * *const DescriptorAddress)
{
	return (NO_DESCRIPTOR);
}

/* Get the disk data structure */
DISK_HANDLE_T *FSUSB_DiskInit(void)
{
	return (&FlashDisk_MS_Interface);
}

/* Wait for disk to be inserted */
int FSUSB_DiskInsertWait(DISK_HANDLE_T *hDisk)
{
	while (USB_HostState[hDisk->Config.PortNumber] != HOST_STATE_Configured) {
		MS_Host_USBTask(hDisk);
		USB_USBTask();
	}
	return (1);
}

/* Disk acquire function that waits for disk to be ready */
int FSUSB_DiskAcquire(DISK_HANDLE_T *hDisk)
{
	PRINTDBG("\nWaiting for ready.");
	for (;; ) {
		uint8_t ErrorCode = MS_Host_TestUnitReady(hDisk, 0);

		if (!(ErrorCode)) {
			break;
		}

		/* Check if an error other than a logical command error (device busy) received */
		if (ErrorCode != MS_ERROR_LOGICAL_CMD_FAILED) {
			PRINTDBG("\nFailed.");
			USB_Host_SetDeviceConfiguration(hDisk->Config.PortNumber, 0);
			return(0);
		}
	}
	PRINTDBG("\nDone.");

	if (MS_Host_ReadDeviceCapacity(hDisk, 0, &DiskCapacity)) {
		PRINTDBG("\nError retrieving device capacity.");
		USB_Host_SetDeviceConfiguration(hDisk->Config.PortNumber, 0);
		return(0);
	}

	PRINTDBGA("\nPendrive size: %lu bytes.", (DiskCapacity.Blocks * DiskCapacity.BlockSize));

	return(1);
}

/* Get sector count */
uint32_t FSUSB_DiskGetSectorCnt(DISK_HANDLE_T *hDisk)
{
	return(DiskCapacity.Blocks);
}

/* Get Block size */
uint32_t FSUSB_DiskGetSectorSz(DISK_HANDLE_T *hDisk)
{
	return(DiskCapacity.BlockSize);
}

/* Read sectors */
int FSUSB_DiskReadSectors(DISK_HANDLE_T *hDisk, void *buff, uint32_t secStart, uint32_t numSec)
{
	if (MS_Host_ReadDeviceBlocks(hDisk, 0, secStart, numSec, DiskCapacity.BlockSize, buff)) {
		PRINTDBG("\nError reading device block.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return(0);
	}
	return(1);
}

/* Write Sectors */
int FSUSB_DiskWriteSectors(DISK_HANDLE_T *hDisk, void *buff, uint32_t secStart, uint32_t numSec)
{
	if (MS_Host_WriteDeviceBlocks(hDisk, 0, secStart, numSec, DiskCapacity.BlockSize, buff)) {
		PRINTDBG("\nError writing device block.");
		return(0);
	}
	return(1);
}

/* Disk ready function */
int FSUSB_DiskReadyWait(DISK_HANDLE_T *hDisk, int tout)
{
	volatile int i = tout * 100;
	while (i--) {	/* Just delay */
	}
	return(1);
}

int FSUSB_InitRealTimeClock(void)
{
	return(1);
}

