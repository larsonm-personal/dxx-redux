/*
 * midi_preview.h -- Standalone MIDI/HMP preview player for the launcher.
 *
 * Uses TinySoundFont (TSF) + TinyMidiLoader (TML) for synthesis and
 * OpenSL ES for audio output.  No SDL, PHYSFS, or game engine dependency.
 *
 * HMP files are converted to standard MIDI in memory using the same
 * algorithm as d2/misc/hmp.c:hmp2mid(), but reading from a memory
 * buffer instead of PHYSFS.
 *
 * Thread model (same as cd_preview.c):
 *   - Main thread: JNI calls (init/start/stop/pause/seek/get_state)
 *   - Render thread: applies queued seeks, advances MIDI, renders TSF -> ring
 *   - OpenSL ES callback: reads ring buffer, outputs audio
 * Control calls are serialized, mutable synth state is protected by one mutex
 * shared with rendering, while pause flags and state queries are atomic. The
 * callback never waits for the synth mutex
 */

#ifndef MIDI_PREVIEW_H
#define MIDI_PREVIEW_H

#include <android/asset_manager.h>

#define MDP_STOPPED 0
#define MDP_PLAYING 1
#define MDP_PAUSED  -1

/*
 * Initialize the MIDI preview system.  Loads gm.sf2 from APK assets.
 * Must be called once before any other midi_preview function.
 * Returns 1 on success, 0 on failure.
 */
int midi_preview_init(AAssetManager *mgr);

/*
 * Start playback of MIDI data.
 *   data/len: raw file bytes (HMP or standard MIDI)
 *   is_hmp:   1 if the data is HMP format, 0 for standard MIDI
 *   sample_rate: output sample rate (e.g. 48000)
 * Returns 1 on success, 0 on failure.
 */
int midi_preview_start(const unsigned char *data, int len,
                       int is_hmp, int sample_rate);

void midi_preview_stop(void);
void midi_preview_pause(void);
void midi_preview_resume(void);

/*
 * Queue an approximate seek on the render thread. Channel state and sounding
 * notes are reconstructed without rendering intervening PCM, so notes that
 * span the seek point restart their envelopes.
 * Returns 1 when the request is accepted, 0 if not playing.
 */
int midi_preview_seek(float fraction);

/*
 * Query the latest nonblocking playback-state snapshot.
 * Returns MDP_PLAYING, MDP_PAUSED, or MDP_STOPPED.
 */
int midi_preview_get_state(int *out_position_ms, int *out_duration_ms);

#endif /* MIDI_PREVIEW_H */
