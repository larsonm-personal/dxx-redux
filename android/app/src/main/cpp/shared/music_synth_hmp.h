#ifndef DXX_MUSIC_SYNTH_HMP_H
#define DXX_MUSIC_SYNTH_HMP_H
#include "music_synth.h"
#include "hmp_android_shared.h"
#include "u_mem.h"

/* A fallback must reselect the GM arrangement, not render FM tracks through SF2 */
static inline int music_synth_convert_hmp(music_synth *synth, const unsigned char *data, int size,
                                          int repeat, unsigned char **midi, int *midi_size,
                                          struct hmp_playback_info *info)
{
	int fm = music_synth_is_fm(synth);
	int fm_size = 0;
	const unsigned char *fm_data = music_synth_fm_sequence(synth, &fm_size);
	/* DOS substitutes HMQ before selecting FM tracks; its programs target the BNK */
	int converted = hmp2mid_playback_device_mem(fm_data ? fm_data : data, fm_data ? fm_size : size,
	                                            repeat, fm, midi, midi_size, info);
	if (fm && (!converted || !music_synth_accepts_midi(synth, *midi, *midi_size))) {
		if (*midi) d_free(*midi);
		*midi = NULL;
		*midi_size = 0;
		music_synth_prepare(synth, NULL, NULL, NULL);
		converted = hmp2mid_playback_mem(data, size, repeat, midi, midi_size, info);
	}
	return converted;
}
#endif
