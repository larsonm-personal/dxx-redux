#include "playsave_layout.h"

#include <limits.h>
#include <stdint.h>

static int read_u16le(FILE *f, unsigned int *value)
{
	unsigned char b[2];

	if (fread(b, 1, sizeof(b), f) != sizeof(b))
		return 0;
	*value = b[0] | ((unsigned int) b[1] << 8);
	return 1;
}

static int read_u32le(FILE *f, unsigned int *value)
{
	unsigned char b[4];

	if (fread(b, 1, sizeof(b), f) != sizeof(b))
		return 0;
	*value = b[0] | ((unsigned int) b[1] << 8) |
	         ((unsigned int) b[2] << 16) | ((unsigned int) b[3] << 24);
	return 1;
}

static int get_file_size(FILE *f, long *size)
{
	if (fseek(f, 0, SEEK_END) || (*size = ftell(f)) < 0 ||
	    fseek(f, 0, SEEK_SET))
		return 0;
	return 1;
}

static int checked_add(size_t *value, size_t count, size_t width)
{
	if (width && count > (SIZE_MAX - *value) / width)
		return 0;
	*value += count * width;
	return *value <= LONG_MAX;
}

int playsave_d1_get_layout(FILE *f, unsigned int save_file_id,
                           int compatible_saved_version, int compatible_player_version,
                           int max_missions, size_t hli_size, size_t saved_games_size,
                           size_t max_controls, struct playsave_binary_layout *layout)
{
	unsigned int id, version, player_version, n_highest;
	long file_size;
	long size_without_hli;
	size_t offset = 20;
	int shareware = -1;
	int d1x_extra = 0;

	if (!f || !layout || !get_file_size(f, &file_size) ||
	    !read_u32le(f, &id) || !read_u16le(f, &version) ||
	    !read_u16le(f, &player_version) || !read_u32le(f, &n_highest) ||
	    id != save_file_id || version < (unsigned) compatible_saved_version ||
	    version > 8 || !hli_size ||
	    player_version < (unsigned) compatible_player_version ||
	    n_highest > (unsigned) max_missions ||
	    n_highest > (unsigned) LONG_MAX / hli_size)
		return 0;

	size_without_hli = file_size - (long) (n_highest * hli_size);
	switch (version) {
		case 4: shareware = 1; break;
		case 5:
		case 6: shareware = 0; break;
		case 7:
			if (size_without_hli == 2212 - (long) saved_games_size)
				shareware = 1;
			else if (size_without_hli == 2252 - (long) saved_games_size)
				shareware = 0;
			break;
		case 8:
			if (size_without_hli == 2212 || size_without_hli == 2220) {
				shareware = 1;
				d1x_extra = size_without_hli == 2220 ? 8 : 0;
			} else if (size_without_hli == 2252 || size_without_hli == 2260) {
				shareware = 0;
				d1x_extra = size_without_hli == 2260 ? 8 : 0;
			}
			break;
	}
	if (shareware < 0 || !checked_add(&offset, 1, (size_t) d1x_extra) ||
	    (version > 5 && !checked_add(&offset, n_highest, hli_size)) ||
	    (version != 7 && !checked_add(&offset, 1, saved_games_size)) ||
	    !checked_add(&offset, 4, shareware ? 25 : 35))
		return 0;

	layout->keysettings = (long) offset;
	if (!checked_add(&offset, 5, max_controls))
		return 0;
	layout->mouse = (long) offset;
	if (!checked_add(&offset, 2, max_controls))
		return 0;
	layout->control_dos = (long) offset;
	layout->control_win = -1;
	layout->sensitivity = (long) offset + 1;
	layout->weapon_order = -1;
	layout->version = (int) version;
	layout->byte_swapped = 0;
	return layout->sensitivity < file_size;
}

int playsave_d2_get_layout(FILE *f, unsigned int save_file_id,
                           int compatible_version, int max_missions, size_t hli_size,
                           size_t max_controls, size_t max_message_len,
                           struct playsave_binary_layout *layout)
{
	unsigned int id, raw_version, raw_n_highest;
	long file_size;
	size_t offset;
	int swapped;
	int version;

	if (!f || !layout || !get_file_size(f, &file_size) ||
	    !read_u32le(f, &id) || !read_u16le(f, &raw_version) ||
	    id != save_file_id)
		return 0;
	swapped = raw_version > 255;
	version = swapped ? (int) (((raw_version & 0xff) << 8) |
	                           (raw_version >> 8))
	                  : (int) raw_version;
	if (version < compatible_version)
		return 0;
	offset = version >= 19 ? 19 : 18;
	if (fseek(f, (long) offset, SEEK_SET) || !read_u16le(f, &raw_n_highest))
		return 0;
	if (swapped)
		raw_n_highest = ((raw_n_highest & 0xff) << 8) |
		                (raw_n_highest >> 8);
	if (raw_n_highest > (unsigned) max_missions ||
	    !checked_add(&offset, 1, 2) ||
	    !checked_add(&offset, raw_n_highest, hli_size) ||
	    !checked_add(&offset, 4, max_message_len))
		return 0;

	layout->keysettings = (long) offset;
	if (!checked_add(&offset, 5, max_controls))
		return 0;
	layout->mouse = (long) offset;
	if (!checked_add(&offset, 2, max_controls))
		return 0;
	if (version >= 20 && !checked_add(&offset, 1, max_controls))
		return 0;
	layout->control_dos = (long) offset;
	if (!checked_add(&offset, 1, 1))
		return 0;
	layout->control_win = version >= 21 ? (long) offset : -1;
	if (version >= 21 && !checked_add(&offset, 1, 1))
		return 0;
	layout->sensitivity = (long) offset;
	if (!checked_add(&offset, 1, 1))
		return 0;
	layout->weapon_order = (long) offset;
	layout->version = version;
	layout->byte_swapped = swapped;
	return checked_add(&offset, 22, 1) && (long) offset <= file_size;
}
