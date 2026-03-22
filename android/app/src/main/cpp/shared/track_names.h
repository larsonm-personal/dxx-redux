/*
 * Track name overlay -- resolves the current music track to a
 * human-readable name and forwards it to the Java UI layer via JNI.
 */

#ifndef _TRACK_NAMES_H
#define _TRACK_NAMES_H

/* Call when a new music track starts playing.
 * For redbook audio: pass the 1-based CD track number, is_midi=0, and the
 *   disc ID from RBAGetDiscID() so hardcoded names are only used for known discs.
 * For MIDI/HMP: pass the song index (SONG_FIRST_LEVEL_SONG-based), is_midi=1,
 *   disc_id is ignored. */
void track_overlay_notify(int track_or_song, int is_midi, unsigned long disc_id);

/* Return the CUE-parsed title for a 1-based CD track, or NULL. */
const char *track_names_get_cue_title(int track);

/* Set a CUE-parsed title for a 1-based CD track (called from CUE parser). */
void track_names_set_cue_title(int track, const char *title);

/* Set the total number of CUE tracks parsed. */
void track_names_set_cue_count(int n);

/* Look up a track name from CUE titles or the hardcoded table.
 * Returns NULL if no name is known.  Used by RBAGetTrackName() fallback. */
const char *track_names_lookup(int track, unsigned long disc_id);

/* Call when a new level starts to show "Level N: Name" overlay. */
void level_overlay_notify(int level_num, const char *level_name);

#endif
