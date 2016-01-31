/*
 * USBMassStorageHost.c
 *
 *  Created on: Feb 10, 2015
 *      Author: marcelo
 */

#include "USBMassStorageHost.h"

#include "USB.h"
#include "MassStorageClassHost.h"

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

extern const unsigned sector_start_map[];

static SCSI_Capacity_t DiskCapacity;
static uint8_t buffer[_MIN_SS];
static FATFS fatFS;	/* File system object */
static FIL fileObj;	/* File object */

/* Task to Write Data in USBPendrive. */
void enter_usb_isp(void)
{
	USB_CurrentMode = USB_MODE_Host;
	USB_Disable();
	USB_Init();

	printf("\nInit USB Write Data Process.");

	USB_ReadWriteFile();

	while (1);
}

/* Function to spin forever when there is an error */
static void die(FRESULT rc)
{
	printf("\n*******DIE %d******* ");
	while (1);/* Spin for ever */
}

/* Function to do the read/write to USB Disk */
void USB_ReadWriteFile(void)
{
	FRESULT rc;		/* Result code */
	int i;
	UINT br;/*, bw*/;
	uint8_t *ptr;

	if(f_mount(&fatFS, "0:", 1) != FR_OK) goto disk_error;	/* First... Mounting USB Unit. */

	rc = f_open(&fileObj, "0:firmware.bin", FA_READ);
	if (rc)
	{
		printf("\nUnable to open firmware.bin from USB Disk ");
	}
	else
	{
//		unsigned *address = USER_FLASH_START;
//
//		printf("\nOpened file firmware.bin from USB Disk. Update User Memory Area.  ");
//
//		for (;;)
//		{
//			/* Read a chunk of file */
//			rc = f_read(&fileObj, buffer, sizeof(buffer), &br);
//
//			if (rc || !br)
//			{
//				break;					/* Error or end of file */
//			}
//
//			ptr = (uint8_t *) buffer;
//
//			if(address == USER_FLASH_START)
//			{
//				unsigned checksum, i;
//				uint8_t *tmpptr;
//
//				tmpptr = (uint8_t *) buffer;
//
//				checksum = 0;
//				for (i = 0; i <= 7; i++)
//				{
//					checksum += *tmpptr;
//					tmpptr++;
//				}
//				if (checksum != 0)
//				{
//					rc = 1;
//					break;					/* Error! */
//				}
//				erase_user_flash();
//			}
//			//
//			write_flash(address, ptr, sizeof(buffer));
//			//
//			address += _MIN_SS;
//		}

		if (rc)
		{
			die(rc);
		}

		printf("\nClose the file.");

		rc = f_close(&fileObj);
		if (rc)
		{
			die(rc);
		}
	}

	printf("\nTest completed.");
	USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);

	while(1);

disk_error:
	printf("\nError Found.");
	while(1);
}

/** Event handler for the USB_DeviceAttached event. This indicates that a device has been attached to the host, and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Host_DeviceAttached(const uint8_t corenum)
{
	printf(("\nDevice Attached on port %d."), corenum);
}

/** Event handler for the USB_DeviceUnattached event. This indicates that a device has been removed from the host, and
 *  stops the library USB task management process.
 */
void EVENT_USB_Host_DeviceUnattached(const uint8_t corenum)
{
	printf(("\nDevice Unattached on port %d."), corenum);
}

/** Event handler for the USB_DeviceEnumerationComplete event. This indicates that a device has been successfully
 *  enumerated by the host and is now ready to be used by the application.
 */
void EVENT_USB_Host_DeviceEnumerationComplete(const uint8_t corenum)
{
	uint16_t ConfigDescriptorSize;
	uint8_t  ConfigDescriptorData[512];
	uint8_t text[128];

	if (USB_Host_GetDeviceConfigDescriptor(corenum, 1, &ConfigDescriptorSize, ConfigDescriptorData,
										   sizeof(ConfigDescriptorData)) != HOST_GETCONFIG_Successful) {
		printf("\nError Retrieving Configuration Descriptor.");
		return;
	}

	FlashDisk_MS_Interface.Config.PortNumber = corenum;
	if (MS_Host_ConfigurePipes(&FlashDisk_MS_Interface, ConfigDescriptorSize, ConfigDescriptorData) != MS_ENUMERROR_NoError) {
		printf("\nAttached Device Not a Valid Mass Storage Device.");
		return;
	}

	if (USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 1) != HOST_SENDCONTROL_Successful) {
		printf("\nError Setting Device Configuration.");
		return;
	}

	uint8_t MaxLUNIndex;
	if (MS_Host_GetMaxLUN(&FlashDisk_MS_Interface, &MaxLUNIndex)) {
		printf("\nError retrieving max LUN index.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return;
	}

	sprintf(text, "\nTotal LUNs: %d - Using first LUN in device.", (MaxLUNIndex + 1));
	printf(text);

	if (MS_Host_ResetMSInterface(&FlashDisk_MS_Interface)) {
		printf("\nError resetting Mass Storage interface.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return;
	}

	SCSI_Request_Sense_Response_t SenseData;
	if (MS_Host_RequestSense(&FlashDisk_MS_Interface, 0, &SenseData) != 0) {
		printf("\nError retrieving device sense.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return;
	}

	if (MS_Host_PreventAllowMediumRemoval(&FlashDisk_MS_Interface, 0, true)) {
		printf("\nError setting Prevent Device Removal bit.");
	    USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
	    return;
	}

	SCSI_Inquiry_Response_t InquiryData;
	if (MS_Host_GetInquiryData(&FlashDisk_MS_Interface, 0, &InquiryData)) {
		printf("\nError retrieving device Inquiry data.");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return;
	}

//	sprintf(text, "Vendor \"%.8s\"\r\nProduct \"%.16s\"\r\nRevision \"%.4s\"\r\n", InquiryData.VendorID, InquiryData.ProductID, InquiryData.RevisionID);
//	printf(text);

	printf("\nMass Storage Device Enumerated.\r\n");
}

/** Event handler for the USB_HostError event. This indicates that a hardware error occurred while in host mode. */
void EVENT_USB_Host_HostError(const uint8_t corenum, const uint8_t ErrorCode)
{
	USB_Disable();

	printf(("Host Mode Error\r\n"
			  " -- Error port %d\r\n"
			  " -- Error Code %d\r\n" ), corenum, ErrorCode);

	for (;; ) {}
}

/** Event handler for the USB_DeviceEnumerationFailed event. This indicates that a problem occurred while
 *  enumerating an attached USB device.
 */
void EVENT_USB_Host_DeviceEnumerationFailed(const uint8_t corenum,
											const uint8_t ErrorCode,
											const uint8_t SubErrorCode)
{
	printf(("Dev Enum Error\r\n"
			  " -- Error port %d\r\n"
			  " -- Error Code %d\r\n"
			  " -- Sub Error Code %d\r\n"
			  " -- In State %d\r\n" ),
			 corenum, ErrorCode, SubErrorCode, USB_HostState[corenum]);
}

/**
 * Dummy callback function for DeviceStandardReq.c,
 * this way i don't need remove files.
 */
uint16_t CALLBACK_USB_GetDescriptor(const uint16_t wValue,
									const uint8_t wIndex,
									const void * *const DescriptorAddress)
{
	return NO_DESCRIPTOR;
}

/* Get the disk data structure */
DISK_HANDLE_T *FSUSB_DiskInit(void)
{
	return &FlashDisk_MS_Interface;
}

/* Wait for disk to be inserted */
int FSUSB_DiskInsertWait(DISK_HANDLE_T *hDisk)
{
	while (USB_HostState[hDisk->Config.PortNumber] != HOST_STATE_Configured) {
		MS_Host_USBTask(hDisk);
		USB_USBTask();
	}
	return 1;
}

/* Disk acquire function that waits for disk to be ready */
int FSUSB_DiskAcquire(DISK_HANDLE_T *hDisk)
{
	printf("Waiting for ready...");
	for (;; ) {
		uint8_t ErrorCode = MS_Host_TestUnitReady(hDisk, 0);

		if (!(ErrorCode)) {
			break;
		}

		/* Check if an error other than a logical command error (device busy) received */
		if (ErrorCode != MS_ERROR_LOGICAL_CMD_FAILED) {
			printf("Failed\r\n");
			USB_Host_SetDeviceConfiguration(hDisk->Config.PortNumber, 0);
			return 0;
		}
	}
	printf("Done.\r\n");

	if (MS_Host_ReadDeviceCapacity(hDisk, 0, &DiskCapacity)) {
		printf("Error retrieving device capacity.\r\n");
		USB_Host_SetDeviceConfiguration(hDisk->Config.PortNumber, 0);
		return 0;
	}

	printf(("%lu blocks of %lu bytes.\r\n"), DiskCapacity.Blocks, DiskCapacity.BlockSize);
	return 1;
}

/* Get sector count */
uint32_t FSUSB_DiskGetSectorCnt(DISK_HANDLE_T *hDisk)
{
	return DiskCapacity.Blocks;
}

/* Get Block size */
uint32_t FSUSB_DiskGetSectorSz(DISK_HANDLE_T *hDisk)
{
	return DiskCapacity.BlockSize;
}

/* Read sectors */
int FSUSB_DiskReadSectors(DISK_HANDLE_T *hDisk, void *buff, uint32_t secStart, uint32_t numSec)
{
	if (MS_Host_ReadDeviceBlocks(hDisk, 0, secStart, numSec, DiskCapacity.BlockSize, buff)) {
		printf("Error reading device block.\r\n");
		USB_Host_SetDeviceConfiguration(FlashDisk_MS_Interface.Config.PortNumber, 0);
		return 0;
	}
	return 1;
}

/* Write Sectors */
int FSUSB_DiskWriteSectors(DISK_HANDLE_T *hDisk, void *buff, uint32_t secStart, uint32_t numSec)
{
	if (MS_Host_WriteDeviceBlocks(hDisk, 0, secStart, numSec, DiskCapacity.BlockSize, buff)) {
		printf("Error writing device block.\r\n");
		return 0;
	}
	return 1;
}

/* Disk ready function */
int FSUSB_DiskReadyWait(DISK_HANDLE_T *hDisk, int tout)
{
	volatile int i = tout * 100;
	while (i--) {	/* Just delay */
	}
	return 1;
}

int FSUSB_InitRealTimeClock(void)
{
	return 1;
}
