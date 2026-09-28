#ifndef COOP_SAVE_FORMAT_H
#define COOP_SAVE_FORMAT_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "d1_in_d2_save_format.h"

/* DGSS header is common to D1/D2 co-op saves, including byte-swapped saves */
static inline int coop_save_read_version(FILE *file, int *version)
{
	uint8_t header[8];
	uint32_t value;
	if (!file || !version || fseek(file, 0, SEEK_SET) ||
	    fread(header, sizeof(header), 1, file) != 1 || memcmp(header, "DGSS", 4))
		return 0;
	value = (uint32_t) header[4] | ((uint32_t) header[5] << 8) |
	        ((uint32_t) header[6] << 16) | ((uint32_t) header[7] << 24);
	if (value & 0xffff0000u)
		value = (value >> 24) | ((value >> 8) & 0xff00u) |
		        ((value << 8) & 0xff0000u) | (value << 24);
	if (value > 0xffffu) return 0;
	*version = (int) value;
	return 1;
}

#define COOP_SAVE_META_TAG   0x434F4F50 /* "COOP" */
#define COOP_SAVE_META_VER   13
#define COOP_SAVE_FOOTER_TAG 0x41504643 /* "CFPA" */

typedef struct coop_save_footer {
	uint32_t tag;
	uint16_t version;
	uint16_t reserved;
	uint32_t payload_size;
	uint32_t collection_count;
	uint32_t campaign_size;
	uint32_t checksum;
} coop_save_footer;

static inline uint32_t coop_save_checksum(const void *data, size_t size,
                                          uint32_t checksum)
{
	const uint8_t *bytes = (const uint8_t *) data;
	size_t i;
	for (i = 0; i < size; i++) {
		checksum ^= bytes[i];
		checksum *= 16777619u;
	}
	return checksum;
}

/* Launcher preflight shared by D1 and D2, without loading engine state
 * Game-specific payload semantics are still validated by the restore reader */
static inline int coop_save_format_supported(FILE *file, long trailer_end)
{
	coop_save_footer footer;
	uint8_t buffer[4096];
	uint32_t tag, remaining, checksum = 2166136261u;
	uint16_t version;
	if (!file || trailer_end < (long) sizeof(footer) ||
	    fseek(file, trailer_end - (long) sizeof(footer), SEEK_SET) ||
	    fread(&footer, sizeof(footer), 1, file) != 1 ||
	    footer.tag != COOP_SAVE_FOOTER_TAG ||
	    footer.version != COOP_SAVE_META_VER || footer.payload_size < 6 ||
	    footer.campaign_size > footer.payload_size - 6 ||
	    footer.payload_size > (unsigned long) trailer_end - sizeof(footer) ||
	    fseek(file, trailer_end - (long) sizeof(footer) - footer.payload_size, SEEK_SET))
		return 0;
	remaining = footer.payload_size;
	while (remaining) {
		size_t count = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
		if (fread(buffer, 1, count, file) != count)
			return 0;
		if (remaining == footer.payload_size) {
			memcpy(&tag, buffer, sizeof(tag));
			memcpy(&version, buffer + sizeof(tag), sizeof(version));
			if (tag != COOP_SAVE_META_TAG || version != COOP_SAVE_META_VER)
				return 0;
		}
		checksum = coop_save_checksum(buffer, count, checksum);
		remaining -= (uint32_t) count;
	}
	return checksum == footer.checksum;
}

#endif
