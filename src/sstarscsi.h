//SPDX-License-Identifier: GPL-3.0-or-later

#ifndef __SSTARSCSI_H_
#define __SSTARSCSI_H_
#include "cntx.h"

#define SSTARSCSI_CBD_LEN	10

#define SSTARSCSI_VID	0x1b20
#define SSTARSCSI_PID	0x0300

#define SSTARSCSI_OPCODE						0xE8
#define SSTARSCSI_SUBCODE_DOWNLOAD_KEEP			0x1
#define SSTARSCSI_SUBCODE_GET_RESULT			0x2
#define SSTARSCSI_SUBCODE_GET_STATE				0x3
#define SSTARSCSI_SUBCODE_DOWNLOAD_END			0x4

/* usb updater stage ... */
#define SSTARSCSI_SUBCODE_SUBCODE_UFU_LOADINFO	0x5

#define SSTARSCSI_BOOTROM_MAXTRANSFER		1024

struct sstarscsi_loadinfo {
	uint32_t addr;
	uint32_t size;
	uint8_t md5[16];
};

int sstarscsi_upload_bootrom(struct p3udl_cntx *cntx, void *buf, uint32_t len);
int sstarscsi_upload_usbupdater(struct p3udl_cntx *cntx, uint32_t loadaddr, void *buf, uint32_t len);

#endif /* __SSTARSCSI_H_ */
