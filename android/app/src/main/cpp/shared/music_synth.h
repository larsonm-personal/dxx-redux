#ifndef DXX_MUSIC_SYNTH_H
#define DXX_MUSIC_SYNTH_H

#include <stddef.h>
#include "hmp_tsf_state.h"

#ifdef __cplusplus
extern "C" {
#endif
struct AAssetManager;
typedef struct music_synth music_synth;
/* Reader supplies SNG/BNK and optional HMQ assets as malloc-owned bounded buffers */
typedef unsigned char *(*music_bank_reader)(void *context, const char *name, size_t *size);
music_synth *music_synth_load(struct AAssetManager *assets, const char *sf2, int prefer_fm);
void music_synth_close(music_synth *synth);
/* Returns whether this song uses FM; otherwise uses the selected SF2 */
int music_synth_prepare(music_synth *synth, const char *song, music_bank_reader reader, void *context);
int music_synth_is_fm(const music_synth *synth);
/* Optional DOS FM arrangement; borrowed until the next prepare/close */
const unsigned char *music_synth_fm_sequence(const music_synth *synth, int *size);
/* Reject missing or invalid patches before any audio is played */
int music_synth_accepts_midi(music_synth *synth, const unsigned char *midi, int size);
void music_synth_reset(music_synth *synth);
void music_synth_set_output(music_synth *synth, enum TSFOutputMode mode, int rate, float gain);
/* Change effects only while playback is stopped */
void music_synth_set_effects(music_synth *synth, int reverb, int chorus);
void music_synth_set_max_voices(music_synth *synth, int voices);
int music_synth_get_presetcount(music_synth *synth);
int music_synth_active_voice_count(music_synth *synth);
void music_synth_render_short(music_synth *synth, short *out, int frames, int mixing);
void music_synth_channel_note_on(music_synth *synth, int channel, int key, float velocity);
void music_synth_channel_note_off(music_synth *synth, int channel, int key);
void music_synth_channel_set_presetnumber(music_synth *synth, int channel, int program, int drums);
void music_synth_channel_midi_control(music_synth *synth, int channel, int control, int value);
void music_synth_channel_set_pitchwheel(music_synth *synth, int channel, int value);
void music_synth_hmp_begin(music_synth *synth, const struct hmp_tsf_state *state);
void music_synth_hmp_capture(music_synth *synth, struct hmp_tsf_state *state);
void music_synth_hmp_control(music_synth *synth, int channel, int control, int value);
#ifdef __cplusplus
}
#endif
#endif
