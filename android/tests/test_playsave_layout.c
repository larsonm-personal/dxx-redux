#include "playsave_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_ID 0x44504c52u
#define HLI_SIZE 10u
#define SAVED_GAMES_SIZE 1200u
#define MAX_CONTROLS_FIXTURE 50u
#define MAX_MESSAGE_LEN_FIXTURE 35u

static void put_u16(unsigned char *data, size_t offset, unsigned int value,
	int swapped)
{
	data[offset + (swapped ? 1 : 0)] = (unsigned char)(value & 0xff);
	data[offset + (swapped ? 0 : 1)] = (unsigned char)(value >> 8);
}

static void put_u32(unsigned char *data, size_t offset, unsigned int value)
{
	data[offset] = (unsigned char)(value & 0xff);
	data[offset + 1] = (unsigned char)((value >> 8) & 0xff);
	data[offset + 2] = (unsigned char)((value >> 16) & 0xff);
	data[offset + 3] = (unsigned char)(value >> 24);
}

static FILE *make_file(const unsigned char *data, size_t size)
{
	FILE *f = tmpfile();

	if (!f || fwrite(data, 1, size, f) != size || fseek(f, 0, SEEK_SET)) {
		if (f)
			fclose(f);
		return NULL;
	}
	return f;
}

static int check_d1(int version, int registered, int d1x_extra)
{
	const unsigned int n_highest = version <= 5 ? 3 : 2;
	const size_t base_size = (registered ? 2252 : 2212) -
	                         (version == 7 ? SAVED_GAMES_SIZE : 0);
	const size_t file_size = base_size + d1x_extra + n_highest * HLI_SIZE;
	unsigned char *data = calloc(file_size, 1);
	struct playsave_binary_layout layout;
	FILE *f;
	size_t expected;
	int ok;

	if (!data)
		return 0;
	put_u32(data, 0, SAVE_ID);
	put_u16(data, 4, (unsigned)version, 0);
	put_u16(data, 6, 16, 0);
	put_u32(data, 8, n_highest);
	f = make_file(data, file_size);
	free(data);
	if (!f)
		return 0;
	ok = playsave_d1_get_layout(f, SAVE_ID, 4, 16, 100,
		HLI_SIZE, SAVED_GAMES_SIZE, MAX_CONTROLS_FIXTURE, &layout);
	fclose(f);
	if (!ok)
		return 0;
	expected = 20 + (size_t)d1x_extra;
	if (version > 5)
		expected += n_highest * HLI_SIZE;
	if (version != 7)
		expected += SAVED_GAMES_SIZE;
	expected += 4 * (registered ? 35 : 25);
	return layout.keysettings == (long)expected &&
	       layout.mouse == (long)(expected + 5 * MAX_CONTROLS_FIXTURE) &&
	       layout.control_dos ==
	           (long)(expected + 7 * MAX_CONTROLS_FIXTURE);
}

static int check_d2(int version, int swapped)
{
	const unsigned int n_highest = 2;
	const size_t header = version >= 19 ? 19 : 18;
	const size_t keysettings = header + 2 + n_highest * HLI_SIZE +
	                           4 * MAX_MESSAGE_LEN_FIXTURE;
	const size_t control = keysettings +
	                       (version >= 20 ? 8 : 7) * MAX_CONTROLS_FIXTURE;
	const size_t weapon = control + 1 + (version >= 21 ? 1 : 0) + 1;
	const size_t file_size = weapon + 22;
	unsigned char *data = calloc(file_size, 1);
	struct playsave_binary_layout layout;
	FILE *f;
	int ok;

	if (!data)
		return 0;
	put_u32(data, 0, SAVE_ID);
	put_u16(data, 4, (unsigned)version, swapped);
	put_u16(data, header, n_highest, swapped);
	f = make_file(data, file_size);
	free(data);
	if (!f)
		return 0;
	ok = playsave_d2_get_layout(f, SAVE_ID, 17, 100, HLI_SIZE,
		MAX_CONTROLS_FIXTURE, MAX_MESSAGE_LEN_FIXTURE, &layout);
	fclose(f);
	return ok && layout.version == version && layout.byte_swapped == swapped &&
	       layout.keysettings == (long)keysettings &&
	       layout.mouse == (long)(keysettings + 5 * MAX_CONTROLS_FIXTURE) &&
	       layout.control_dos == (long)control &&
	       layout.control_win == (version >= 21 ? (long)control + 1 : -1) &&
	       layout.weapon_order == (long)weapon;
}

int main(void)
{
	if (!check_d1(4, 0, 0) || !check_d1(5, 1, 0) ||
	    !check_d1(6, 1, 0) || !check_d1(7, 0, 0) ||
	    !check_d1(7, 1, 0) || !check_d1(8, 0, 0) ||
	    !check_d1(8, 1, 0) || !check_d1(8, 0, 8) ||
	    !check_d1(8, 1, 8)) {
		fprintf(stderr, "D1 layout fixture failed\n");
		return 1;
	}
	if (!check_d2(17, 0) || !check_d2(18, 0) || !check_d2(19, 0) ||
	    !check_d2(20, 0) || !check_d2(21, 0) || !check_d2(24, 0) ||
	    !check_d2(24, 1)) {
		fprintf(stderr, "D2 layout fixture failed\n");
		return 1;
	}
	return 0;
}
