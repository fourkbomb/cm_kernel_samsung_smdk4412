/*
 * CMC623 Setting table for SLP7 Machine.
 *
 * Author: InKi Dae  <inki.dae@samsung.com>
 *		Eunchul Kim <chulspro.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "mdnie.h"

static const unsigned short redwood_dynamic_ui[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x00ac,
/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0092, 0x0020,	/*DE pe*/
	0x0093, 0x0020,	/*DE pf*/
	0x0094, 0x0020,	/*DE pb*/
	0x0095, 0x0020,	/*DE ne*/
	0x0096, 0x0020,	/*DE nf*/
	0x0097, 0x0020,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1a04,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x0000, 0x0001,	/*BANK 1*/
	0x001f, 0x0080,	/*CC chsel strength*/
	0x0020, 0x0000,	/*CC lut r   0*/
	0x0021, 0x0a94,	/*CC lut r  16 144*/
	0x0022, 0x18a6,	/*CC lut r  32 160*/
	0x0023, 0x28b8,	/*CC lut r  48 176*/
	0x0024, 0x3ac9,	/*CC lut r  64 192*/
	0x0025, 0x4cd9,	/*CC lut r  80 208*/
	0x0026, 0x5ee7,	/*CC lut r  96 224*/
	0x0027, 0x70f4,	/*CC lut r 112 240*/
	0x0028, 0x82ff,	/*CC lut r 128 255*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_dynamic_gallery[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x00ac,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0090, 0x0080,	/*DE egth*/
	0x0092, 0x0030,	/*DE pe*/
	0x0093, 0x0080,	/*DE pf*/
	0x0094, 0x0080,	/*DE pb*/
	0x0095, 0x0080,	/*DE ne*/
	0x0096, 0x0080,	/*DE nf*/
	0x0097, 0x0080,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1a04,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x0000, 0x0001,	/*BANK 1*/
	0x001f, 0x0080,	/*CC chsel strength*/
	0x0020, 0x0000,	/*CC lut r   0*/
	0x0021, 0x0a94,	/*CC lut r  16 144*/
	0x0022, 0x18a6,	/*CC lut r  32 160*/
	0x0023, 0x28b8,	/*CC lut r  48 176*/
	0x0024, 0x3ac9,	/*CC lut r  64 192*/
	0x0025, 0x4cd9,	/*CC lut r  80 208*/
	0x0026, 0x5ee7,	/*CC lut r  96 224*/
	0x0027, 0x70f4,	/*CC lut r 112 240*/
	0x0028, 0x82ff,	/*CC lut r 128 255*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_dynamic_video[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x00ac,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0092, 0x0080,	/*DE pe*/
	0x0093, 0x0080,	/*DE pf*/
	0x0094, 0x0080,	/*DE pb*/
	0x0095, 0x0080,	/*DE ne*/
	0x0096, 0x0080,	/*DE nf*/
	0x0097, 0x0080,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1a04,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x0000, 0x0001,	/*BANK 1*/
	0x001f, 0x0080,	/*CC chsel strength*/
	0x0020, 0x0000,	/*CC lut r   0*/
	0x0021, 0x0a94,	/*CC lut r  16 144*/
	0x0022, 0x18a6,	/*CC lut r  32 160*/
	0x0023, 0x28b8,	/*CC lut r  48 176*/
	0x0024, 0x3ac9,	/*CC lut r  64 192*/
	0x0025, 0x4cd9,	/*CC lut r  80 208*/
	0x0026, 0x5ee7,	/*CC lut r  96 224*/
	0x0027, 0x70f4,	/*CC lut r 112 240*/
	0x0028, 0x82ff,	/*CC lut r 128 255*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_dynamic_vtcall[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x00ae,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0005,	/*FA cs1 | de8 dnr4 hdr2 fa1*/
	0x0039, 0x0080,	/*FA dnrWeight*/
	0x0080, 0x0fff,	/*DNR dirTh*/
	0x0081, 0x19ff,	/*DNR dirnumTh decon7Th*/
	0x0082, 0xff16,	/*DNR decon5Th maskTh*/
	0x0083, 0x0000,	/*DNR blTh*/
	0x0092, 0x00e0,	/*DE pe*/
	0x0093, 0x00e0,	/*DE pf*/
	0x0094, 0x00e0,	/*DE pb*/
	0x0095, 0x00e0,	/*DE ne*/
	0x0096, 0x00e0,	/*DE nf*/
	0x0097, 0x00e0,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0010,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1a04,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x0000, 0x0001,	/*BANK 1*/
	0x001f, 0x0080,	/*CC chsel strength*/
	0x0020, 0x0000,	/*CC lut r   0*/
	0x0021, 0x0a94,	/*CC lut r  16 144*/
	0x0022, 0x18a6,	/*CC lut r  32 160*/
	0x0023, 0x28b8,	/*CC lut r  48 176*/
	0x0024, 0x3ac9,	/*CC lut r  64 192*/
	0x0025, 0x4cd9,	/*CC lut r  80 208*/
	0x0026, 0x5ee7,	/*CC lut r  96 224*/
	0x0027, 0x70f4,	/*CC lut r 112 240*/
	0x0028, 0x82ff,	/*CC lut r 128 255*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_movie_ui[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0020,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00f0,	/*SCR KgWg*/
	0x00ec, 0x00e6,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_movie_gallery[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0020,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00f0,	/*SCR KgWg*/
	0x00ec, 0x00e6,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_movie_video[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0020,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0092, 0x0000,	/*DE pe*/
	0x0093, 0x0000,	/*DE pf*/
	0x0094, 0x0000,	/*DE pb*/
	0x0095, 0x0000,	/*DE ne*/
	0x0096, 0x0000,	/*DE nf*/
	0x0097, 0x0000,	/*DE nb*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1004,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00f0,	/*SCR KgWg*/
	0x00ec, 0x00e6,	/*SCR KbWb*/
	0x0000, 0x0001,	/*BANK 1*/
	0x001f, 0x0000,	/*CC chsel strength*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_movie_vtcall[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x002e,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0005,	/*FA cs1 | de8 dnr4 hdr2 fa1*/
	0x0039, 0x0080,	/*FA dnrWeight*/
	0x0080, 0x0fff,	/*DNR dirTh*/
	0x0081, 0x19ff,	/*DNR dirnumTh decon7Th*/
	0x0082, 0xff16,	/*DNR decon5Th maskTh*/
	0x0083, 0x0000,	/*DNR blTh*/
	0x0092, 0x0040,	/*DE pe*/
	0x0093, 0x0040,	/*DE pf*/
	0x0094, 0x0040,	/*DE pb*/
	0x0095, 0x0040,	/*DE ne*/
	0x0096, 0x0040,	/*DE nf*/
	0x0097, 0x0040,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0010,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1204,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00f0,	/*SCR KgWg*/
	0x00ec, 0x00e6,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_natural_ui[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x002c,	/*Dither8 UC4 ABC2 CP1 |
					CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0092, 0x0020,	/*DE pe*/
	0x0093, 0x0020,	/*DE pf*/
	0x0094, 0x0020,	/*DE pb*/
	0x0095, 0x0020,	/*DE ne*/
	0x0096, 0x0020,	/*DE nf*/
	0x0097, 0x0020,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xd6ac,	/*SCR RrCr*/
	0x00e2, 0x32ff,	/*SCR RgCg*/
	0x00e3, 0x2ef0,	/*SCR RbCb*/
	0x00e4, 0xa5fa,	/*SCR GrMr*/
	0x00e5, 0xff4d,	/*SCR GgMg*/
	0x00e6, 0x59ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00fb,	/*SCR BgYg*/
	0x00e9, 0xff61,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00fa,	/*SCR KgWg*/
	0x00ec, 0x00f8,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_natural_gallery[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x002c,	/*Dither8 UC4 ABC2 CP1 |
					CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0092, 0x0060,	/*DE pe*/
	0x0093, 0x0060,	/*DE pf*/
	0x0094, 0x0060,	/*DE pb*/
	0x0095, 0x0060,	/*DE ne*/
	0x0096, 0x0060,	/*DE nf*/
	0x0097, 0x0060,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xd6ac,	/*SCR RrCr*/
	0x00e2, 0x32ff,	/*SCR RgCg*/
	0x00e3, 0x2ef0,	/*SCR RbCb*/
	0x00e4, 0xa5fa,	/*SCR GrMr*/
	0x00e5, 0xff4d,	/*SCR GgMg*/
	0x00e6, 0x59ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00fb,	/*SCR BgYg*/
	0x00e9, 0xff61,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00fa,	/*SCR KgWg*/
	0x00ec, 0x00f8,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_natural_video[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x002c,	/*Dither8 UC4 ABC2 CP1 |
					CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0092, 0x0060,	/*DE pe*/
	0x0093, 0x0060,	/*DE pf*/
	0x0094, 0x0060,	/*DE pb*/
	0x0095, 0x0060,	/*DE ne*/
	0x0096, 0x0060,	/*DE nf*/
	0x0097, 0x0060,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xd6ac,	/*SCR RrCr*/
	0x00e2, 0x32ff,	/*SCR RgCg*/
	0x00e3, 0x2ef0,	/*SCR RbCb*/
	0x00e4, 0xa5fa,	/*SCR GrMr*/
	0x00e5, 0xff4d,	/*SCR GgMg*/
	0x00e6, 0x59ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00fb,	/*SCR BgYg*/
	0x00e9, 0xff61,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00fa,	/*SCR KgWg*/
	0x00ec, 0x00f8,	/*SCR KbWb*/
	0x0000, 0x0001,	/*BANK 1*/
	0x001f, 0x0000,	/*CC chsel strength*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_natural_vtcall[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x002e,	/*Dither8 UC4 ABC2 CP1 |
					CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0005,	/*FA cs1 | de8 dnr4 hdr2 fa1*/
	0x0039, 0x0080,	/*FA dnrWeight*/
	0x0080, 0x0fff,	/*DNR dirTh*/
	0x0081, 0x19ff,	/*DNR dirnumTh decon7Th*/
	0x0082, 0xff16,	/*DNR decon5Th maskTh*/
	0x0083, 0x0000,	/*DNR blTh*/
	0x0092, 0x00c0,	/*DE pe*/
	0x0093, 0x00c0,	/*DE pf*/
	0x0094, 0x00c0,	/*DE pb*/
	0x0095, 0x00c0,	/*DE ne*/
	0x0096, 0x00c0,	/*DE nf*/
	0x0097, 0x00c0,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0010,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xd6ac,	/*SCR RrCr*/
	0x00e2, 0x32ff,	/*SCR RgCg*/
	0x00e3, 0x2ef0,	/*SCR RbCb*/
	0x00e4, 0xa5fa,	/*SCR GrMr*/
	0x00e5, 0xff4d,	/*SCR GgMg*/
	0x00e6, 0x59ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00fb,	/*SCR BgYg*/
	0x00e9, 0xff61,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00fa,	/*SCR KgWg*/
	0x00ec, 0x00f8,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_standard_ui[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0028,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_standard_gallery[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x002c,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0090, 0x0080,	/*DE egth*/
	0x0092, 0x0000,	/*DE pe*/
	0x0093, 0x0060,	/*DE pf*/
	0x0094, 0x0060,	/*DE pb*/
	0x0095, 0x0060,	/*DE ne*/
	0x0096, 0x0060,	/*DE nf*/
	0x0097, 0x0060,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0,  0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_standard_video[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x002c,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0092, 0x0060,	/*DE pe*/
	0x0093, 0x0060,	/*DE pf*/
	0x0094, 0x0060,	/*DE pb*/
	0x0095, 0x0060,	/*DE ne*/
	0x0096, 0x0060,	/*DE nf*/
	0x0097, 0x0060,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x0000, 0x0001,	/*BANK 1*/
	0x001f, 0x0000,	/*CC chsel strength*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_standard_vtcall[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x002e,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0005,	/*FA cs1 | de8 dnr4 hdr2 fa1*/
	0x0039, 0x0080,	/*FA dnrWeight*/
	0x0080, 0x0fff,	/*DNR dirTh*/
	0x0081, 0x19ff,	/*DNR dirnumTh decon7Th*/
	0x0082, 0xff16,	/*DNR decon5Th maskTh*/
	0x0083, 0x0000,	/*DNR blTh*/
	0x0092, 0x00c0,	/*DE pe*/
	0x0093, 0x00c0,	/*DE pf*/
	0x0094, 0x00c0,	/*DE pb*/
	0x0095, 0x00c0,	/*DE ne*/
	0x0096, 0x00c0,	/*DE nf*/
	0x0097, 0x00c0,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0010,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_camera[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0028,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0090, 0x0080,	/*DE egth*/
	0x0092, 0x0000,	/*DE pe*/
	0x0093, 0x0060,	/*DE pf*/
	0x0094, 0x0060,	/*DE pb*/
	0x0095, 0x0060,	/*DE ne*/
	0x0096, 0x0060,	/*DE nf*/
	0x0097, 0x0060,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg ry*/
	0x00b1, 0x1010,	/*CS hg gc*/
	0x00b2, 0x1010,	/*CS hg bm*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_camera_outdoor[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0428,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x0090, 0x0080,	/*DE egth*/
	0x0092, 0x0000,	/*DE pe*/
	0x0093, 0x0060,	/*DE pf*/
	0x0094, 0x0060,	/*DE pb*/
	0x0095, 0x0060,	/*DE ne*/
	0x0096, 0x0060,	/*DE nf*/
	0x0097, 0x0060,	/*DE nb*/
	0x0098, 0x1000,	/*DE max ratio*/
	0x0099, 0x0100,	/*DE min ratio*/
	0x00b0, 0x1010,	/*CS hg RY*/
	0x00b1, 0x1010,	/*CS hg GC*/
	0x00b2, 0x1010,	/*CS hg BM*/
	0x00b3, 0x1804,	/*CS weight grayTH*/
	0x00e1, 0xff00,	/*SCR RrCr*/
	0x00e2, 0x00ff,	/*SCR RgCg*/
	0x00e3, 0x00ff,	/*SCR RbCb*/
	0x00e4, 0x00ff,	/*SCR GrMr*/
	0x00e5, 0xff00,	/*SCR GgMg*/
	0x00e6, 0x00ff,	/*SCR GbMb*/
	0x00e7, 0x00ff,	/*SCR BrYr*/
	0x00e8, 0x00e4,	/*SCR BgYg*/
	0x00e9, 0xff00,	/*SCR BbYb*/
	0x00ea, 0x00ff,	/*SCR KrWr*/
	0x00eb, 0x00ff,	/*SCR KgWg*/
	0x00ec, 0x00ff,	/*SCR KbWb*/
	0x0000, 0x0001,	/*BANK 1*/
	0x00d0, 0x01c0,	/*UC y*/
	0x00d1, 0x01ff,	/*UC cs*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_browser_tone1[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0020,	/*Dither8 UC4 ABC2 CP1 |
					CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x00e1, 0xaf00,	/*SCR RrCr*/
	0x00e2, 0x00b7,	/*SCR RgCg*/
	0x00e3, 0x00bc,	/*SCR RbCb*/
	0x00e4, 0x00af,	/*SCR GrMr*/
	0x00e5, 0xb700,	/*SCR GgMg*/
	0x00e6, 0x00bc,	/*SCR GbMb*/
	0x00e7, 0x00af,	/*SCR BrYr*/
	0x00e8, 0x00b7,	/*SCR BgYg*/
	0x00e9, 0xbc00,	/*SCR BbYb*/
	0x00ea, 0x00af,	/*SCR KrWr*/
	0x00eb, 0x00b7,	/*SCR KgWg*/
	0x00ec, 0x00bc,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_browser_tone2[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0020,	/*Dither8 UC4 ABC2 CP1 |
					CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x00e1, 0xa000,	/*SCR RrCr*/
	0x00e2, 0x00a8,	/*SCR RgCg*/
	0x00e3, 0x00b2,	/*SCR RbCb*/
	0x00e4, 0x00a0,	/*SCR GrMr*/
	0x00e5, 0xa800,	/*SCR GgMg*/
	0x00e6, 0x00b2,	/*SCR GbMb*/
	0x00e7, 0x00a0,	/*SCR BrYr*/
	0x00e8, 0x00a8,	/*SCR BgYg*/
	0x00e9, 0xb200,	/*SCR BbYb*/
	0x00ea, 0x00a0,	/*SCR KrWr*/
	0x00eb, 0x00a8,	/*SCR KgWg*/
	0x00ec, 0x00b2,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_browser_tone3[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0020,	/*Dither8 UC4 ABC2 CP1 |
					CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 de8 hdr2 fa1*/
	0x00e1, 0x9100,	/*SCR RrCr*/
	0x00e2, 0x0099,	/*SCR RgCg*/
	0x00e3, 0x00a3,	/*SCR RbCb*/
	0x00e4, 0x0091,	/*SCR GrMr*/
	0x00e5, 0x9900,	/*SCR GgMg*/
	0x00e6, 0x00a3,	/*SCR GbMb*/
	0x00e7, 0x0091,	/*SCR BrYr*/
	0x00e8, 0x0099,	/*SCR BgYg*/
	0x00e9, 0xa300,	/*SCR BbYb*/
	0x00ea, 0x0091,	/*SCR KrWr*/
	0x00eb, 0x0099,	/*SCR KgWg*/
	0x00ec, 0x00a3,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_bypass[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0000,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0030, 0x0000,	/*FA cs1 | de8 hdr2 fa1*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_negative[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x0020,	/*SCR*/
	0x00e1, 0x00ff,	/*SCR RrCr*/
	0x00e2, 0xff00,	/*SCR RgCg*/
	0x00e3, 0xff00,	/*SCR RbCb*/
	0x00e4, 0xff00,	/*SCR GrMr*/
	0x00e5, 0x00ff,	/*SCR GgMg*/
	0x00e6, 0xff00,	/*SCR GbMb*/
	0x00e7, 0xff00,	/*SCR BrYr*/
	0x00e8, 0xff00,	/*SCR BgYg*/
	0x00e9, 0x00ff,	/*SCR BbYb*/
	0x00ea, 0xff00,	/*SCR KrWr*/
	0x00eb, 0xff00,	/*SCR KgWg*/
	0x00ec, 0xff00,	/*SCR KbWb*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_outdoor[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x04ac,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0000, 0x0001,	/*BANK 1*/
	0x00d0, 0x01c0,	/*UC y*/
	0x00d1, 0x01ff,	/*UC cs*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_cold[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x00ec,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0000, 0x0001,	/*BANK 1*/
	0x0001, 0x0064,	/*MCM 10000K*/
	0x0009, 0xa08e,	/*MCM 5cb 1cr W*/
	0x000b, 0x7979,	/*MCM 4cr 5cr W*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_cold_outdoor[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x04ec,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0000, 0x0001,	/*BANK 1*/
	0x0001, 0x0064,	/*MCM 10000K*/
	0x0009, 0xa08e,	/*MCM 5cb 1cr W*/
	0x000b, 0x7979,	/*MCM 4cr 5cr W*/
	0x00d0, 0x01c0,	/*UC y*/
	0x00d1, 0x01ff,	/*UC cs*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_warm[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x00ec,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0000, 0x0001,	/*BANK 1*/
	0x0001, 0x0028,	/*MCM 4000K*/
	0x0007, 0x7575,	/*MCM 1cb 2cb W*/
	0x0009, 0xa08e,	/*MCM 5cb 1cr W*/
	0x00ff, 0x0000,	/*Mask Release*/
};

static const unsigned short redwood_warm_outdoor[] = {
	0x0000, 0x0000,	/*BANK 0*/
	0x0008, 0x04ec,
	/*Dither8 UC4 ABC2 CP1 | CC8 MCM4 SCR2 SCC1 | CS8 DE4 DNR2 HDR1*/
	0x0000, 0x0001,	/*BANK 1*/
	0x0001, 0x0028,	/*MCM 4000K*/
	0x0007, 0x7575,	/*MCM 1cb 2cb W*/
	0x0009, 0xa08e,	/*MCM 5cb 1cr W*/
	0x00d0, 0x01c0,	/*UC y*/
	0x00d1, 0x01ff,	/*UC cs*/
	0x00ff, 0x0000,	/*Mask Release*/
};

const struct mdnie_tables redwood_mdnie_tables[] = {
	{ "dynamic_ui", redwood_dynamic_ui,
		ARRAY_SIZE(redwood_dynamic_ui) },
	{ "dynamic_gallery", redwood_dynamic_gallery,
		ARRAY_SIZE(redwood_dynamic_gallery) },
	{ "dynamic_video", redwood_dynamic_video,
		ARRAY_SIZE(redwood_dynamic_video) },
	{ "dynamic_vtcall", redwood_dynamic_vtcall,
		ARRAY_SIZE(redwood_dynamic_vtcall) },
	{ "standard_ui", redwood_standard_ui,
		ARRAY_SIZE(redwood_standard_ui) },
	{ "standard_gallery", redwood_standard_gallery,
		ARRAY_SIZE(redwood_standard_gallery) },
	{ "standard_video", redwood_standard_video,
		ARRAY_SIZE(redwood_standard_video) },
	{ "standard_vtcall", redwood_standard_vtcall,
		ARRAY_SIZE(redwood_standard_vtcall) },
	{ "natural_ui", redwood_natural_ui,
		ARRAY_SIZE(redwood_natural_ui) },
	{ "natural_gallery",	 redwood_natural_gallery,
		ARRAY_SIZE(redwood_natural_gallery) },
	{ "natural_video", redwood_natural_video,
		ARRAY_SIZE(redwood_natural_video) },
	{ "natural_vtcall", redwood_natural_vtcall,
		ARRAY_SIZE(redwood_natural_vtcall) },
	{ "movie_ui", redwood_movie_ui,
		ARRAY_SIZE(redwood_movie_ui) },
	{ "movie_gallery", redwood_movie_gallery,
		ARRAY_SIZE(redwood_movie_gallery) },
	{ "movie_video", redwood_movie_video,
		ARRAY_SIZE(redwood_movie_video) },
	{ "movie_vtcall", redwood_movie_vtcall,
		ARRAY_SIZE(redwood_movie_vtcall) },
	{ "camera", redwood_camera,
		ARRAY_SIZE(redwood_camera) },
	{ "camera_outdoor", redwood_camera_outdoor,
		ARRAY_SIZE(redwood_camera_outdoor) },
	{ "browser_tone1",	 redwood_browser_tone1,
		ARRAY_SIZE(redwood_browser_tone1) },
	{ "browser_tone2",	 redwood_browser_tone2,
		ARRAY_SIZE(redwood_browser_tone2) },
	{ "browser_tone3",	 redwood_browser_tone3,
		ARRAY_SIZE(redwood_browser_tone3) },
	{ "negative", redwood_negative, ARRAY_SIZE(redwood_negative) },
	{ "bypass", redwood_bypass, ARRAY_SIZE(redwood_bypass) },
	{ "outdoor", redwood_outdoor, ARRAY_SIZE(redwood_outdoor) },
	{ "warm", redwood_warm, ARRAY_SIZE(redwood_warm) },
	{ "warm_outdoor", redwood_warm_outdoor,
		ARRAY_SIZE(redwood_warm_outdoor) },
	{ "cold", redwood_cold, ARRAY_SIZE(redwood_cold) },
	{ "cold_outdoor", redwood_cold_outdoor,
		ARRAY_SIZE(redwood_cold_outdoor) },
};

