/* Exact-source Android callsign reader with only PhysFS adapted to stdio */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int64_t fix64;
typedef int32_t fix;
typedef uint8_t ubyte;
typedef int8_t sbyte;
typedef uint16_t ushort;
typedef unsigned int uint;
typedef uint32_t PHYSFS_uint32;
typedef uint64_t PHYSFS_uint64;
typedef int64_t PHYSFS_sint64;
typedef FILE PHYSFS_file;
#define __pack__ __attribute__((packed))

static int open_files;
static uint32_t swap32(uint32_t value)
{
	return (value >> 24) | ((value >> 8) & 0xff00) |
	       ((value << 8) & 0xff0000) | (value << 24);
}
#define SWAPINT(value) swap32((uint32_t)(value))

static PHYSFS_file *PHYSFSX_openReadBuffered(const char *path)
{
	PHYSFS_file *fp = fopen(path, "rb");
	if (fp) ++open_files;
	return fp;
}
static int PHYSFS_close(PHYSFS_file *fp)
{
	--open_files;
	return fclose(fp) == 0;
}
#define PHYSFS_read(fp, output, size, count) fread(output, size, count, fp)
#define PHYSFS_write(fp, input, size, count) fwrite(input, size, count, fp)
#define PHYSFS_tell(fp) ftell(fp)
static int PHYSFS_seek(PHYSFS_file *fp, PHYSFS_uint64 offset)
{
	return fseek(fp, (long)offset, SEEK_SET) == 0;
}
static int32_t PHYSFSX_readSXE32(PHYSFS_file *fp, int swap)
{
	uint32_t value = 0;
	fread(&value, sizeof(value), 1, fp);
	return (int32_t)(swap ? SWAPINT(value) : value);
}

/* Use the production version policy, never a permissive fixture substitute. */
#include "android_save_format.h"

/* Generated from state.c, player.h and weapon.h, without reader rewrites */
#include "save_callsign_reader_under_test.h"

int main(int argc, char **argv)
{
	char callsign[CALLSIGN_LEN + 1] = "sentinel";
	int accepted;
	if (argc == 2 && !strcmp(argv[1], "--player-size")) {
		printf("%zu\n", sizeof(player_rw));
		return 0;
	}
	if (argc != 3) return 2;
	accepted = state_get_save_file_callsign(argv[1], callsign, sizeof(callsign));
	if (open_files) return 3;
	if (!strcmp(argv[2], "--reject")) return accepted || callsign[0] ? 4 : 0;
	if (!accepted || strcmp(callsign, argv[2])) {
		fprintf(stderr, "callsign mismatch: accepted=%d first_byte=%u expected=%s\n",
		        accepted, (unsigned char)callsign[0], argv[2]);
		return 5;
	}
	return 0;
}
