/* linux/drivers/video/backlight/ea8061_gamma.h
 *
 * Brightness level definition.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * Joongmock Shin <jmock.shin@samsung.com>
 * Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _EA8061_GAMMA_H
#define _EA8061_GAMMA_H

#define MAX_GAMMA_LEVEL		25
#define GAMMA_TABLE_COUNT	26

static const unsigned char ea8061_gamma22_300_revD[] = {
	0xCA, 0x01, 0x00, 0x01, 0x00, 0x01, 0x00, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x31, 0x02
};

static const unsigned char *ea8061_gamma22_table_revD[MAX_GAMMA_LEVEL] = {
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD,
	ea8061_gamma22_300_revD
};

static const unsigned char ea8061_gamma22_300_revA[] = {
	0xCA, 0x00, 0xE8, 0x00, 0xF7, 0x01, 0x03, 0xDB, 0xDB,
	0xDC, 0xD9, 0xD8, 0xDA, 0xCB, 0xC8, 0xCB, 0xD4, 0xD3,
	0xD7, 0xE6, 0xE6, 0xEA, 0xE2, 0xE4, 0xE5, 0xCE, 0xC3,
	0xCF, 0xB9, 0x9D, 0xDE, 0x11, 0x00
};

static const unsigned char *ea8061_gamma22_table_revA[MAX_GAMMA_LEVEL] = {
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA,
	ea8061_gamma22_300_revA
};

#endif /* _EA8061_GAMMA_H */
