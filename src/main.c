//SPDX-License-Identifier: GPL-3.0-or-later

#include <arpa/inet.h>
#include <argtable2.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <libusb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>

#include "sstarscsi.h"
#include "usbms.h"
#include "log.h"
#include "uboot.h"

#include "main_log.h"

static int usb_libusbinit(struct p3udl_cntx *cntx)
{
	int ret = libusb_init(&cntx->lu_cntx);
	if (ret < 0) {
		printf("failed to init libusb\n");
		return ret;
	}

	return 0;
}

static int usb_probe(struct p3udl_cntx *cntx)
{
	libusb_device_handle *lu_handle;

	lu_handle = libusb_open_device_with_vid_pid(cntx->lu_cntx, SSTARSCSI_VID, SSTARSCSI_PID);
	if (!lu_handle) {
		printf("failed to find device\n");
		return -ENODEV;
	}

	/* check if the kernel driver is attached */
	int ret = libusb_kernel_driver_active(lu_handle, 0);
	if (ret == 1) {
		/* detach kernel device */
		ret = libusb_detach_kernel_driver(lu_handle, 0);
		if (ret) {
			printf("failed to detach kernel driver: %d\n", ret);
			return -ENODEV;
		}
	}

	cntx->lu_handle = lu_handle;
	return 0;
}

// TODO find the endpoints
static int usb_setup(struct p3udl_cntx *cntx)
{
	//struct libusb_device_descriptor dev_desc;
	//libusb_device *dev;

	//dev = libusb_get_device(lu_handle);

	//libusb_get_device_descriptor(dev, &dev_desc);

	cntx->ep_in = 0x81;
	cntx->ep_out = 0x02;

	return 0;
}

static int scsi_probe(struct p3udl_cntx *cntx)
{
	/* get the max lun to check the device is present */
	int ret = usb_massstorage_get_maxlun(cntx);
	if (ret < 0) {
		p3udl_err(cntx, "Couldn't request maxlun %d, need reset?\n", ret);
		return ret;
	}

	struct mass_storage_inquiry_result inquiry_result;
	ret = usb_massstorage_inquiry(cntx, &inquiry_result);
	if (ret)
		return ret;

	printf("VID:PID:REV \"%8s\":\"%8s\":\"%4s\"\n",
			inquiry_result.vid, inquiry_result.pid, inquiry_result.rev);

	return 0;
}

static int parse_cmdline(int argc, char **argv, struct p3udl_cntx *cntx)
{
	struct arg_lit *help;
	struct arg_file *ipl, *uboot;
	struct arg_end *end;

	void *argtable[] = {
			/* help */
			help = arg_lit0("h", "help", "Display this help text"),
			/* the IPL file */
			ipl = arg_file0(NULL, "ipl", "<file path>", "Binary to use for the IPL"),
			/* the u-boot file */
			uboot = arg_file0(NULL, "uboot", "<file path>", "u-boot image file path"),
			end = arg_end(1),
	};

	int ret = arg_parse(argc, argv, argtable);
	if (ret) {
		arg_print_errors(stdout, end, "xxx");
		return -EINVAL;
	}

	if (help->count > 0) {
		arg_print_syntax(stdout, argtable, "\n");
		arg_print_glossary(stdout, argtable, "  %-30s %s\n");
		exit(0);
	}

	if (ipl->count != 1) {
		printf("Tell me where the ipl is\n");
		exit(1);
	}

	if (uboot->count != 1) {
		printf("Tell me where the u-boot is\n");
		exit(1);
	}

	cntx->ipl_path = strdup(ipl->filename[0]);
	cntx->uboot_path = strdup(uboot->filename[0]);

	return 0;
}

static int upload_ipl(struct p3udl_cntx *cntx)
{
	p3udl_info(cntx, "Uploading IPL via boot ROM...\n");
	uint32_t len = 64 * 1024;
	void *buffer = malloc(len);
	int ret;

	memset(buffer, 0, len);

	int iplfd = open(cntx->ipl_path, O_RDONLY);
	if (iplfd < 0) {
		p3udl_err(cntx, "Failed to open IPL binary: %d\n", iplfd);
		return -1;
	}

	int actuallen = read(iplfd, buffer, len);
	p3udl_info(cntx, "Read %d bytes of IPL\n", actuallen);

	ret = sstarscsi_upload_bootrom(cntx, buffer, len);
	if (ret)
		p3udl_err(cntx, "Failed! :(\n");

	free(buffer);

	return 0;
}

static int upload_uboot(struct p3udl_cntx *cntx)
{
	p3udl_info(cntx, "Uploading u-boot via IPL...\n");

	uint32_t len = 2048 * 1024;
	void *buffer = malloc(len);
	int ret;

	memset(buffer, 0, len);

	int ubootfd = open(cntx->uboot_path, O_RDONLY);
	if (ubootfd < 0) {
		p3udl_err(cntx, "Failed to open u-boot image: %d\n", ubootfd);
		return -1;
	}

	int actuallen = read(ubootfd, buffer, len);
	p3udl_info(cntx, "Read %d bytes of u-boot image\n", actuallen);

	struct legacy_img_hdr *hdr = buffer;
	uint32_t magic = ntohl(hdr->ih_magic);
	if (magic != IH_MAGIC) {
		p3udl_err(cntx, "Doesn't look like a u-boot image to me buddy\n");
		return -EINVAL;
	}
	uint32_t loadaddr = ntohl(hdr->ih_load);
	uint32_t loadsz = ntohl(hdr->ih_size);

	p3udl_info(cntx, "u-boot info: load addr 0x%04x, load size 0x%04x\n",
			loadaddr, loadsz);

	/* the usb updater wants a u-boot binary? */
	/* note for the usb update bin the load addr seems to be ingored and it's always 0x23d00000
	 * and the u-boot binary is moved to 0x23e00000? */
	ret = sstarscsi_upload_usbupdater(cntx, 0xFFFFFFFF, buffer, actuallen);
	if (ret)
		p3udl_err(cntx, "Failed! :(\n");

	free(buffer);

	return 0;
}

int main(int argc, char **argv)
{
	struct p3udl_cntx cntx;

	cntx.log_cb = log_printf;

	int ret = parse_cmdline(argc, argv, &cntx);
	if (ret)
		return ret;

	ret = usb_libusbinit(&cntx);
	if (ret)
		return ret;

	ret = usb_probe(&cntx);
	if (ret)
		goto out_deinit;

	ret = usb_setup(&cntx);
	if (ret)
		goto out_close;

	ret = scsi_probe(&cntx);
	if (ret)
		goto out_close;

	ret = upload_ipl(&cntx);
	if (ret)
		goto out_close;

	ret = upload_uboot(&cntx);
	if (ret)
		goto out_close;

out_close:
	libusb_close(cntx.lu_handle);
out_deinit:
	libusb_exit(cntx.lu_cntx);

	return 0;
}
