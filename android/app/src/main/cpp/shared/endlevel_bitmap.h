#ifndef DXX_ENDLEVEL_BITMAP_H
#define DXX_ENDLEVEL_BITMAP_H

#include <stdint.h>
#include <string.h>
#include "physfsx.h"

static inline unsigned endlevel_be16(const unsigned char *p)
{
	return (unsigned) p[0] * 256 + p[1];
}

static inline uint32_t endlevel_be32(const unsigned char *p)
{
	return (uint32_t) p[0] << 24 | (uint32_t) p[1] << 16 | (uint32_t) p[2] << 8 | p[3];
}

/* Validate optional static IFF assets before the legacy decoder allocates or expands them */
static inline int endlevel_check_bitmap(PHYSFS_file *file, int limit, int exit_x, int exit_y)
{
	unsigned char header[20];
	PHYSFS_sint64 end, body_end;
	unsigned width = 0, height = 0, planes = 0, mask = 0, compression = 0;
	int planar, body = 0, palette = 0;
	if (PHYSFS_readBytes(file, header, 12) != 12 || memcmp(header, "FORM", 4)) return 0;
	end = 8 + (PHYSFS_sint64) endlevel_be32(header + 4);
	if (end < 12 || end > PHYSFS_fileLength(file)) return 0;
	planar = !memcmp(header + 8, "ILBM", 4);
	if (!planar && memcmp(header + 8, "PBM ", 4)) return 0;
	while (PHYSFS_tell(file) < end) {
		uint32_t size;
		PHYSFS_sint64 next;
		if (end - PHYSFS_tell(file) < 8 || PHYSFS_readBytes(file, header, 8) != 8) return 0;
		size = endlevel_be32(header + 4);
		next = PHYSFS_tell(file) + size + (size & 1);
		if (next > end) return 0;
		if (!memcmp(header, "BMHD", 4)) {
			if (width || body || size != 20 || PHYSFS_readBytes(file, header, 20) != 20) return 0;
			width = endlevel_be16(header);
			height = endlevel_be16(header + 2);
			planes = planar ? header[8] : 1;
			mask = header[9];
			compression = header[10];
			if (!width || !height || width > (unsigned) limit || height > (unsigned) limit ||
			    !planes || planes > 8 || mask > 2 || compression > 1) return 0;
			/* The old ILBM conversion uses word-aligned planes in a width*height allocation */
			if (planar && (((width + 15) / 16) * 2 * planes > width || (width & 15))) return 0;
			if (exit_x >= 0 && (width < 2 || width != height || (unsigned) exit_x >= width || (unsigned) exit_y >= height)) return 0;
		} else if (!memcmp(header, "CMAP", 4)) {
			if (!size || size > 768 || size % 3) return 0;
			palette = 1;
		} else if (!memcmp(header, "BODY", 4)) {
			unsigned row, rows, stride;
			if (!width || body++) return 0;
			body_end = PHYSFS_tell(file) + size;
			stride = planar ? width / 8 : width;
			stride = (stride + 1) & ~1u;
			rows = height * (planes + (mask == 1));
			if (!compression) {
				if ((PHYSFS_sint64) stride * rows != size) return 0;
			} else
				for (row = 0; row < rows; ++row) {
					unsigned remaining = stride;
					while (remaining) {
						int code;
						unsigned count, bytes;
						if (PHYSFS_tell(file) >= body_end || (code = PHYSFSX_fgetc(file)) == EOF) return 0;
						if (code == 128) continue;
						count = code < 128 ? code + 1 : 257 - code;
						bytes = code < 128 ? count : 1;
						if (count > remaining || body_end - PHYSFS_tell(file) < bytes) return 0;
						if (!PHYSFS_seek(file, PHYSFS_tell(file) + bytes)) return 0;
						remaining -= count;
					}
				}
			if (compression && PHYSFS_tell(file) != body_end) return 0;
		} else if (!memcmp(header, "DLTA", 4) || !memcmp(header, "ANHD", 4)) return 0;
		if (!PHYSFS_seek(file, next)) return 0;
	}
	return body && (palette || exit_x >= 0);
}

static inline int endlevel_bitmap_valid(const char *name, int limit, int exit_x, int exit_y)
{
	PHYSFS_file *file = PHYSFSX_openReadBuffered(name);
	int valid;
	if (!file) return 0;
	valid = endlevel_check_bitmap(file, limit, exit_x, exit_y);
	PHYSFS_close(file);
	return valid;
}

#endif
