//SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __CNTX_H
#define __CNTX_H

#include <libusb.h>
#include <stdint.h>
#include <dgputil.h>

struct p3udl_cntx {
	libusb_context *lu_cntx;
	libusb_device_handle *lu_handle;
	uint8_t ep_in, ep_out, lun;
	log_cb log_cb;


	char *ipl_path;
	char *uboot_path;
};
#endif
