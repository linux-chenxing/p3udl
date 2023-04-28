//SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __USBMS_H_
#define __USBMS_H_
#include <stdint.h>
#include <libusb.h>

#include "cntx.h"

struct mass_storage_inquiry_result {
	char vid[9], pid[9], rev[5];
};

int usb_massstorage_get_maxlun(struct p3udl_cntx *cntx);

int usb_massstorage_send_command(struct p3udl_cntx *cntx,
	uint8_t endpoint, uint8_t lun, uint8_t *cdb, uint8_t direction,
	int data_length, uint32_t *ret_tag);
int usb_massstorage_inquiry(struct p3udl_cntx *cntx, struct mass_storage_inquiry_result *result);
int usb_massstorage_status(struct p3udl_cntx *cntx, uint8_t endpoint, uint32_t expected_tag);
void usb_massstorage_sense(struct p3udl_cntx *cntx, uint8_t endpoint_in, uint8_t endpoint_out);

#endif /* __USBMS_H_ */
