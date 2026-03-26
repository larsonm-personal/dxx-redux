/*
 * cd_preview.h -- Standalone CD audio preview player for the launcher.
 *
 * Uses direct file I/O + OpenSL ES (no SDL or PHYSFS dependency).
 * Shares the same sector format constants and PCM decode/resample
 * approach as rbaudio_bin.c so bugs surface in both code paths.
 *
 * Duplicated constants (keep in sync with rbaudio_bin.c):
 *   SECTOR_SIZE, CD_SAMPLE_RATE, FRAMES_PER_SECTOR
 */

#ifndef CD_PREVIEW_H
#define CD_PREVIEW_H

#define CDP_STOPPED 0
#define CDP_PLAYING 1
#define CDP_PAUSED  -1

/*
 * Start preview of the nth audio track (1-based among audio tracks).
 * Parses the CUE file to find track boundaries, opens the BIN file,
 * and starts OpenSL ES playback at the given output sample rate.
 * Returns 1 on success, 0 on failure.
 */
int cd_preview_start(const char *bin_path, const char *cue_path,
                     int audio_track_1based, int sample_rate);

/*
 * Start preview using a file descriptor for the BIN file instead of
 * a path.  The fd is dup'd internally so the caller may close it.
 * Returns 1 on success, 0 on failure.
 */
int cd_preview_start_fd(int fd, const char *cue_path,
                        int audio_track_1based, int sample_rate);

/* Stop preview and release all resources */
void cd_preview_stop(void);

void cd_preview_pause(void);
void cd_preview_resume(void);

/*
 * Seek to a fractional position within the current track (0.0-1.0).
 * Returns 1 on success, 0 if not playing.
 */
int cd_preview_seek(float fraction);

/*
 * Query current playback state.
 * Returns CDP_PLAYING, CDP_PAUSED, or CDP_STOPPED.
 * If out_position_ms / out_duration_ms are non-NULL, they receive the
 * current position and total duration in milliseconds.
 */
int cd_preview_get_state(int *out_position_ms, int *out_duration_ms);

#endif /* CD_PREVIEW_H */
