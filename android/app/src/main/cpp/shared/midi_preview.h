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
 *   - Render thread: advances MIDI timeline, renders TSF -> ring buffer
 *   - OpenSL ES callback: reads ring buffer, outputs audio
 * Control calls are serialized, and mutable playback state is protected by
 * one mutex shared with rendering. The callback never waits for that mutex
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
 * Seek to a fractional position (0.0-1.0).
 * MIDI seek requires re-rendering from start, which is fast for
 * typical Descent tracks (2-4 minutes).
 * Returns 1 on success, 0 if not playing.
 */
int midi_preview_seek(float fraction);

/*
 * Query current playback state.
 * Returns MDP_PLAYING, MDP_PAUSED, or MDP_STOPPED.
 */
int midi_preview_get_state(int *out_position_ms, int *out_duration_ms);

#endif /* MIDI_PREVIEW_H */
