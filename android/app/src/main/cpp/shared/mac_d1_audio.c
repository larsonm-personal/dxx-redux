#include <stdio.h>
#include <string.h>

#include "pstypes.h"
#include "gr.h"
#include "piggy.h"
#include "physfsx.h"
#include "u_mem.h"
#include "console.h"
#include "android_sound_trace.h"
#include "mac_d1_sound_resource.h"

extern int Num_sound_files;

/* Called only for the retail Macintosh PIG, after checking legacy RAW overrides */
int android_load_mac_d1_sounds(void)
{
	PHYSFS_file *file = PHYSFSX_openReadBuffered(MAC_D1_RESOURCE_FILE);
	if (!file) {
		con_printf(CON_URGENT, "Mac D1 sound resources missing: reimport the MacPlay disc");
		return 0;
	}
	const PHYSFS_sint64 length = PHYSFS_fileLength(file);
	if (length < 16 || length > MAC_D1_RESOURCE_MAX_BYTES) {
		PHYSFS_close(file);
		con_printf(CON_URGENT, "Invalid Mac D1 sound resource size");
		return 0;
	}
	unsigned char *bytes = d_malloc((size_t) length);
	struct mac_d1_sample samples[MAC_D1_SOUND_COUNT];
	const int valid = PHYSFS_read(file, bytes, 1, (PHYSFS_uint32) length) == length &&
	                  mac_d1_parse_sound_resource(bytes, (size_t) length, samples);
	PHYSFS_close(file);
	if (!valid || Num_sound_files != 0) {
		d_free(bytes);
		con_printf(CON_URGENT, "Invalid or unsupported Mac D1 sound resources");
		return 0;
	}
	android_sound_trace_bank_open(MAC_D1_RESOURCE_FILE);
	for (int i = 0; i < MAC_D1_SOUND_COUNT; ++i) {
		digi_sound sound;
		char name[16];
		memset(&sound, 0, sizeof(sound));
		sound.length = samples[i].length;
		sound.data = d_malloc(sound.length);
		memcpy(sound.data, samples[i].data, sound.length);
		sound.bits = 8;
		sound.freq = 11025;
		snprintf(name, sizeof(name), "SND%04d", i);
		piggy_register_sound(&sound, name, 0);
		android_sound_trace_loaded(i, name, samples[i].data - bytes, 1);
	}
	d_free(bytes);
	con_printf(CON_NORMAL, "Loaded %d MacPlay D1 sound effects", MAC_D1_SOUND_COUNT);
	return 1;
}
