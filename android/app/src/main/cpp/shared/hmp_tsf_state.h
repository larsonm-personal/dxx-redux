#ifndef DXX_HMP_TSF_STATE_H
#define DXX_HMP_TSF_STATE_H

#include "tsf.h"

/* HMI song transitions reset transient controllers/voices but retain program,
 * bank and pan. TSF's reset also erases those, so retain them explicitly across
 * HMP songs. This snapshot belongs to a single, persistent soundfont instance.
 */
struct hmp_tsf_state {
	int valid;
	int preset[16];
	int bank[16];
	unsigned int pan[16];
};

static inline void hmp_tsf_capture(tsf *synth, struct hmp_tsf_state *state)
{
	int channel;
	for (channel = 0; channel < 16; channel++) {
		float pan = tsf_channel_get_pan(synth, channel);
		/* Pinned TSF 853a0a17 subtracts 0.5 from panOffset in this getter,
		 * though its setter already stores pan - 0.5. Existing channels
		 * therefore return [-1, 0]; unallocated channels return 0.5.
		 * Revisit this compensation when updating the dependency.
		 */
		if (pan <= 0.0f) pan += 1.0f;
		state->preset[channel] = tsf_channel_get_preset_index(synth, channel);
		state->bank[channel] = tsf_channel_get_preset_bank(synth, channel);
		state->pan[channel] = (unsigned int) (pan * 16383.0f + 0.5f);
	}
	state->valid = 1;
}

static inline void hmp_tsf_begin(tsf *synth, const struct hmp_tsf_state *state)
{
	int channel;
	for (channel = 0; channel < 16; channel++) {
		if (state->valid) {
			tsf_channel_set_bank(synth, channel, state->bank[channel]);
			tsf_channel_set_presetindex(synth, channel, state->preset[channel]);
			tsf_channel_midi_control(synth, channel, 10, (int) (state->pan[channel] >> 7));
			tsf_channel_midi_control(synth, channel, 42, (int) (state->pan[channel] & 127));
		}
		/* Clear TSF's full-scale default LSB as well as the HMP's 7-bit volume */
		tsf_channel_midi_control(synth, channel, 39, 0);
		tsf_channel_midi_control(synth, channel, 7, 0);
	}
}

#endif
