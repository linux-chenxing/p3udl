//SPDX-License-Identifier: GPL-3.0-or-later
/*
 * GPLv3, maybe..
 * Based on: https://github.com/libusb/libusb/blob/master/examples/xusb.c
 */

/* Spec: https://www.usb.org/sites/default/files/usbmassbulk_10.pdf */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <libusb.h>

#include "sstarscsi.h"
#include "usbms.h"

#include "usbms_log.h"

#define RETRY_MAX                     5
#define REQUEST_SENSE_LENGTH          0x12
#define INQUIRY_LENGTH                0x24
#define READ_CAPACITY_LENGTH          0x08

#define BOMS_RESET		0xFF
#define BOMS_GET_MAX_LUN	0xFE

int usb_massstorage_get_maxlun(struct p3udl_cntx *cntx)
{
	int ret = libusb_control_transfer(cntx->lu_handle,
			LIBUSB_ENDPOINT_IN|LIBUSB_REQUEST_TYPE_CLASS|LIBUSB_RECIPIENT_INTERFACE,
			BOMS_GET_MAX_LUN, 0, 0, &cntx->lun, 1, 1000);

	return ret;
}

// Section 5.1: Command Block Wrapper (CBW)
struct command_block_wrapper {
	uint8_t dCBWSignature[4];
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];
};

// Section 5.2: Command Status Wrapper (CSW)
struct command_status_wrapper {
	uint8_t dCSWSignature[4];
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};

static const uint8_t cdb_length[256] = {
//	 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  0
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  1
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  2
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  3
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  4
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  5
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  6
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  7
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  8
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  9
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  A
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  B
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  C
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  D
	00,00,00,00,00,00,00,00,SSTARSCSI_CBD_LEN,00,00,00,00,00,00,00,  //  E
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  F
};

int usb_massstorage_send_command(struct p3udl_cntx *cntx, uint8_t endpoint, uint8_t lun,
	uint8_t *cdb, uint8_t direction, int data_length, uint32_t *ret_tag)
{
	static uint32_t tag = 1;
	uint8_t cdb_len;
	int r, size;
	struct command_block_wrapper cbw;

	if (cdb == NULL) {
		return -1;
	}

	if (endpoint & LIBUSB_ENDPOINT_IN) {
		usbms_err(cntx, "send_mass_storage_command: cannot send command on IN endpoint\n");
		return -1;
	}

	cdb_len = cdb_length[cdb[0]];
	if ((cdb_len == 0) || (cdb_len > sizeof(cbw.CBWCB))) {
		usbms_err(cntx, "send_mass_storage_command: don't know how to handle this command (%02X, length %d)\n",
			cdb[0], cdb_len);
		return -1;
	}

	memset(&cbw, 0, sizeof(cbw));
	cbw.dCBWSignature[0] = 'U';
	cbw.dCBWSignature[1] = 'S';
	cbw.dCBWSignature[2] = 'B';
	cbw.dCBWSignature[3] = 'C';
	*ret_tag = tag;
	cbw.dCBWTag = tag++;
	cbw.dCBWDataTransferLength = data_length;
	cbw.bmCBWFlags = direction;
	cbw.bCBWLUN = lun;
	// Subclass is 1 or 6 => cdb_len
	cbw.bCBWCBLength = cdb_len;
	memcpy(cbw.CBWCB, cdb, cdb_len);

	int i = 0;
	do {
		// The transfer length must always be exactly 31 bytes.
		r = libusb_bulk_transfer(cntx->lu_handle, endpoint, (unsigned char*)&cbw, 31, &size, 1000);
		if (r == LIBUSB_ERROR_PIPE) {
			libusb_clear_halt(cntx->lu_handle, endpoint);
		}
		i++;
	} while ((r == LIBUSB_ERROR_PIPE) && (i<RETRY_MAX));

	if (r != LIBUSB_SUCCESS) {
		usbms_err(cntx, "send_mass_storage_command: %s\n", libusb_strerror((enum libusb_error)r));
		return r;
	}

	usbms_dbg(cntx, "sent %d CDB bytes\n", cdb_len);
	return 0;
}

int usb_massstorage_status(struct p3udl_cntx *cntx, uint8_t endpoint, uint32_t expected_tag)
{
	int i, r, size;
	struct command_status_wrapper csw;

	// The device is allowed to STALL this transfer. If it does, you have to
	// clear the stall and try again.
	i = 0;
	do {
		r = libusb_bulk_transfer(cntx->lu_handle, endpoint, (unsigned char*)&csw, 13, &size, 1000);
		if (r == LIBUSB_ERROR_PIPE) {
			libusb_clear_halt(cntx->lu_handle, endpoint);
		}
		i++;
	} while ((r == LIBUSB_ERROR_PIPE) && (i<RETRY_MAX));
	if (r != LIBUSB_SUCCESS) {
		usbms_info(cntx, "get_mass_storage_status: %s\n", libusb_strerror((enum libusb_error)r));
		return -1;
	}
	if (size != 13) {
		usbms_info(cntx, "get_mass_storage_status: received %d bytes (expected 13)\n", size);
		return -1;
	}
	if (csw.dCSWTag != expected_tag) {
		usbms_info(cntx, "get_mass_storage_status: mismatched tags (expected %08X, received %08X)\n",
			expected_tag, csw.dCSWTag);
		return -1;
	}
	// For this test, we ignore the dCSWSignature check for validity...
	usbms_dbg(cntx, "Mass Storage Status: %02X (%s)\n", csw.bCSWStatus, csw.bCSWStatus?"FAILED":"Success");
	if (csw.dCSWTag != expected_tag)
		return -1;
	if (csw.bCSWStatus) {
		// REQUEST SENSE is appropriate only if bCSWStatus is 1, meaning that the
		// command failed somehow.  Larger values (2 in particular) mean that
		// the command couldn't be understood.
		if (csw.bCSWStatus == 1)
			return -2;	// request Get Sense
		else
			return -1;
	}

	// In theory we also should check dCSWDataResidue.  But lots of devices
	// set it wrongly.
	return 0;
}

void usb_massstorage_sense(struct p3udl_cntx *cntx, uint8_t endpoint_in, uint8_t endpoint_out)
{
	uint8_t cdb[16];	// SCSI Command Descriptor Block
	uint8_t sense[18];
	uint32_t expected_tag;
	int size;
	int rc;

	// Request Sense
	printf("Request Sense:\n");
	memset(sense, 0, sizeof(sense));
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x03;	// Request Sense
	cdb[4] = REQUEST_SENSE_LENGTH;

	usb_massstorage_send_command(cntx, endpoint_out, 0, cdb, LIBUSB_ENDPOINT_IN, REQUEST_SENSE_LENGTH, &expected_tag);
	rc = libusb_bulk_transfer(cntx->lu_handle, endpoint_in, (unsigned char*)&sense, REQUEST_SENSE_LENGTH, &size, 1000);
	if (rc < 0)
	{
		printf("libusb_bulk_transfer failed: %s\n", libusb_error_name(rc));
		return;
	}
	printf("   received %d bytes\n", size);

	if ((sense[0] != 0x70) && (sense[0] != 0x71)) {
		printf("   ERROR No sense data\n");
	} else {
		printf("   ERROR Sense: %02X %02X %02X\n", sense[2]&0x0F, sense[12], sense[13]);
	}
	// Strictly speaking, the get_mass_storage_status() call should come
	// before these perr() lines.  If the status is nonzero then we must
	// assume there's no data in the buffer.  For xusb it doesn't matter.
	usb_massstorage_status(cntx, endpoint_in, expected_tag);
}

int usb_massstorage_inquiry(struct p3udl_cntx *cntx, struct mass_storage_inquiry_result *result)
{
	uint8_t cdb[16] = { 0 };
	uint8_t buffer[64];
	uint32_t expected_tag;
	int ret = 0, size;

	// Send Inquiry
	usbms_info(cntx, "Sending Inquiry:\n");
	memset(buffer, 0, sizeof(buffer));
	memset(cdb, 0, sizeof(cdb));
	cdb[0] = 0x12;	// Inquiry
	cdb[4] = INQUIRY_LENGTH;

	ret = usb_massstorage_send_command(cntx, cntx->ep_out, cntx->lun, cdb, LIBUSB_ENDPOINT_IN, INQUIRY_LENGTH, &expected_tag);
	if (ret < 0)
		return ret;

	ret = libusb_bulk_transfer(cntx->lu_handle, cntx->ep_in, (unsigned char*)&buffer, INQUIRY_LENGTH, &size, 1000);
	if (ret < 0)
		return ret;

	usbms_info(cntx, "received %d bytes\n", size);

	/* The following strings are not zero terminated */
	memset(result, 0, sizeof(*result));
	for (int i=0; i<8; i++) {
		result->vid[i] = buffer[8+i];
		result->pid[i] = buffer[16+i];
		result->rev[i/2] = buffer[32+i/2];	// instead of another loop
	}

	if (usb_massstorage_status(cntx, cntx->ep_in, expected_tag) == -2) {
		usb_massstorage_sense(cntx, cntx->ep_in, cntx->ep_out);
	}

	return ret;
}
