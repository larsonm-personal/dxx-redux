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

/* Call when a mission zip built-in/addon track starts playing.
 * Uses mission_music_names.json when present, otherwise falls back to the
 * legacy MIDI Track N label. */
void track_overlay_notify_mission_music(const char *filename, int song_index);

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

/* Call when a jukebox track starts playing.
 * Strips the path/extension and sends a clean name to the overlay.
 * If chromaprint names were loaded, uses the decoded name instead. */
void track_overlay_notify_jukebox(const char *filename);

/* Load jukebox track names from custom_music_names.json (absolute path).
 * Called from jukebox_load() after reading the M3U playlist. */
void jukebox_names_load(const char *json_path);
void jukebox_names_load_for_playlist(const char *playlist_path);

/* Look up a chromaprint-decoded name for a jukebox file path.
 * Returns NULL if no name is known. */
const char *jukebox_names_lookup(const char *filepath);
const char *jukebox_track_display_name(char *const *songs, int song_count,
	int index);

/* Load and look up mission zip built-in/addon music names from PhysFS. */
void mission_music_names_load(void);
const char *mission_music_names_lookup(const char *filename);

#endif
