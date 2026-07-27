#ifndef TEST_HMP_H
#define TEST_HMP_H

#include <stdint.h>

#define HMP_TRACKS 32

#define SWAPINT(x)                                   \
	((int) ((((uint32_t) (x) & 0x000000ffU) << 24) | \
	        (((uint32_t) (x) & 0x0000ff00U) << 8) |  \
	        (((uint32_t) (x) & 0x00ff0000U) >> 8) |  \
	        (((uint32_t) (x) & 0xff000000U) >> 24)))
#define SWAPSHORT(x)                              \
	((short) ((((uint16_t) (x) & 0x00ffU) << 8) | \
	          (((uint16_t) (x) & 0xff00U) >> 8)))

typedef struct hmp_track {
	unsigned char *data;
	unsigned int len;
	int loop_set;
} hmp_track;

typedef struct hmp_file {
	long long filesize;
	int num_trks;
	hmp_track trks[HMP_TRACKS];
	int tempo;
} hmp_file;

void hmp_close(hmp_file *hmp);

#endif
