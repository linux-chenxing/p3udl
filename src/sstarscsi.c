//SPDX-License-Identifier: GPL-3.0-or-later

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <dgputil.h>
#include <unistd.h>
#include <openssl/md5.h>

#include "usbms.h"
#include "sstarscsi.h"

#include "sstarscsi_log.h"

static void sstarscsi_setup_cdb(uint8_t *cdb, uint8_t subcmd, uint32_t len)
{
	cdb[0] = SSTARSCSI_OPCODE;
	cdb[1] = subcmd;
	cdb[6]  = (uint8_t)((len >> 24) & 0xff);
	cdb[7]  = (uint8_t)((len >> 16) & 0xff);
	cdb[8]  = (uint8_t)((len >>  8) & 0xff);
	cdb[9]  = (uint8_t)(len & 0xff);
}

static int sstarscsi_do_op(struct p3udl_cntx *cntx, uint8_t subcmd, void *buf, uint32_t len, bool writebuffer)
{
	uint8_t cdb[16] = { 0 };
	uint32_t expected_tag;

	sstarscsi_setup_cdb(cdb, subcmd, len);

	/* Send the command */
	sstarscsi_dbg(cntx, "Sending cmd...\n");
	int ret = usb_massstorage_send_command(cntx, cntx->ep_out, cntx->lun, cdb,
			LIBUSB_ENDPOINT_OUT, len, &expected_tag);
	if (ret < 0)
		return ret;

	if (writebuffer) {
		/* Send the buffer */
		sstarscsi_dbg(cntx, "Sending buffer...\n");
		int actual_txed;
		ret = libusb_bulk_transfer(cntx->lu_handle, cntx->ep_out, buf, len, &actual_txed, 1000);
		if (ret < 0) {
			sstarscsi_err(cntx, "Failed to send buffer: %s (%d)\n", libusb_strerror((enum libusb_error) ret), ret);
			return ret;
		}
		sstarscsi_dbg(cntx, "Wanted to send %d bytes, actually sent %d\n", len, actual_txed);
	}
	else
	{
		/* Read the buffer */
		sstarscsi_dbg(cntx, "Reading buffer...\n");
		int actual_txed;
		ret = libusb_bulk_transfer(cntx->lu_handle, cntx->ep_in, buf, len, &actual_txed, 1000);
		if (ret < 0) {
			sstarscsi_err(cntx, "Failed to read buffer: %s (%d)\n", libusb_strerror((enum libusb_error) ret), ret);
			return ret;
		}
		sstarscsi_dbg(cntx, "Wanted to read %d bytes, actually read %d\n", len, actual_txed);
	}

	/* Check status */
	sstarscsi_dbg(cntx, "Check status...\n");
	if (usb_massstorage_status(cntx, cntx->ep_in, expected_tag) == -2) {
		usb_massstorage_sense(cntx, cntx->ep_in, cntx->ep_out);
	}

	return 0;
}

static int sstarscsi_upload_packet(struct p3udl_cntx *cntx, void *buf, uint32_t len, bool last)
{
	uint8_t subcmd = last ? SSTARSCSI_SUBCODE_DOWNLOAD_END : SSTARSCSI_SUBCODE_DOWNLOAD_KEEP;

	return sstarscsi_do_op(cntx, subcmd, buf, len, true);
}

static int sstarscsi_upload_loop(struct p3udl_cntx *cntx, void *buf, uint32_t len)
{
	for (int i = 0; i < len; i += SSTARSCSI_BOOTROM_MAXTRANSFER) {
		bool last = (i + SSTARSCSI_BOOTROM_MAXTRANSFER) >= len;
		uint32_t txsz = min(len - i, SSTARSCSI_BOOTROM_MAXTRANSFER);

		int ret;

		for (int attempt = 0; attempt < 10; attempt++) {
			sstarscsi_dbg(cntx, "Uploading segment 0x%04x->0x%04x (%d bytes), last: %d, attempt: %d\n",
					i, i + txsz, txsz, last, attempt);

			ret = sstarscsi_upload_packet(cntx, buf + i, txsz, last);

			if (!ret)
				break;

			if (ret != LIBUSB_ERROR_TIMEOUT) {
				return -EIO;
			}

			usleep(1000);
		}

		/* Catch falling out of the above loop via timeouts */
		if (ret)
			return ret;
	}

	return 0;
}

int sstarscsi_upload_bootrom(struct p3udl_cntx *cntx, void *buf, uint32_t len)
{
	sstarscsi_info(cntx, "Doing upload using the boot ROM\n");

	/* Boot ROM just wants packets splatted at it */
	return sstarscsi_upload_loop(cntx, buf, len);
}

static void sstarscsi_do_md5(struct p3udl_cntx *cntx, uint8_t *digest, uint8_t *buffer, uint32_t len)
{
	MD5(buffer, len, digest);
}

int sstarscsi_upload_usbupdater(struct p3udl_cntx *cntx, uint32_t loadaddr, void *buf, uint32_t len)
{
	sstarscsi_info(cntx, "Doing upload using the usb updater..\n");

	/* The usb updater IPL expects to get this load address thing first.. */
	struct sstarscsi_loadinfo info = {
		.addr = loadaddr,
		.size = len,
	};

	sstarscsi_do_md5(cntx, info.md5, buf, len);

	int ret = sstarscsi_do_op(cntx, SSTARSCSI_SUBCODE_SUBCODE_UFU_LOADINFO, &info, sizeof(info), true);
	if (ret) {
		sstarscsi_info(cntx, "Failed to set loadinfo: %d\n", ret);
		return ret;
	}

	ret = sstarscsi_upload_loop(cntx, buf, len);
	if (ret) {
		sstarscsi_err(cntx, "Failed to upload buffer to usb updater: %d\n", ret);
		return ret;
	}

	uint8_t result[4];

	ret = sstarscsi_do_op(cntx, SSTARSCSI_SUBCODE_GET_RESULT, result, sizeof(result), false);
	if (ret) {
		sstarscsi_info(cntx, "Failed to get upload result: %d\n", ret);
		return ret;
	}
	sstarscsi_info(cntx, "result 0x%02x:0x%02x:0x%02x:0x%02x\n",
			result[0],result[1],result[2],result[3]);

	return 0;
}
