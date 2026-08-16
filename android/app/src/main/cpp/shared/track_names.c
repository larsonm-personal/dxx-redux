/*
 * Track name overlay -- maps CD track numbers to human-readable names
 * and forwards the result to the Java overlay layer via JNI.
 *
 * Shared between D1 and D2 android builds.  Track names are populated
 * at load time from audio_playlist.json (written by AudioSourceManager
 * with fingerprint-matched or known_discs.json5-sourced names).
 *
 * Jukebox (custom music) names come from custom_music_names.json,
 * a sidecar written by CustomAudioSetManager alongside the M3U playlist.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <physfs.h>
#include "pstypes.h"
#include "songs.h"
#include "track_names.h"

#include "android_jni_overlay.h"
#include "music_name_table.h"
#include "midi_metadata_physfs.h"
#include "overlay_ringbuf.h"

extern bim_song_info *BIMSongs;
extern int Num_bim_songs;

/* -- CUE-parsed / fingerprint-matched titles ------------------------- */
/* Populated by rbaudio_bin.c when loading audio_playlist.json.
 * Fingerprint-matched names from the playlist override CUE TITLE fields. */

#define MAX_CUE_TRACKS 100
#define CUE_TITLE_LEN  64

static char s_cue_titles[MAX_CUE_TRACKS][CUE_TITLE_LEN];
static int s_cue_title_count = 0;

void track_names_set_cue_count(int n)
{
	if (n > MAX_CUE_TRACKS) n = MAX_CUE_TRACKS;
	s_cue_title_count = n;
}

void track_names_set_cue_title(int track, const char *title)
{
	if (track < 1 || track > MAX_CUE_TRACKS) return;
	strncpy(s_cue_titles[track - 1], title, CUE_TITLE_LEN - 1);
	s_cue_titles[track - 1][CUE_TITLE_LEN - 1] = '\0';
}

const char *track_names_get_cue_title(int track)
{
	if (track < 1 || track > s_cue_title_count) return NULL;
	if (s_cue_titles[track - 1][0] == '\0') return NULL;
	return s_cue_titles[track - 1];
}

const char *track_names_lookup(int track, unsigned long disc_id)
{
	(void) disc_id;
	return track_names_get_cue_title(track);
}

/* -- Jukebox (custom music) name table ------------------------------- */
/* Loaded from custom_music_names.json written by CustomAudioSetManager.
 * Maps absolute file paths to chromaprint-decoded track names. */

#define MISSION_MUSIC_NAMES_FILE "mission_music_names.json"
#define MAX_MISSION_MIDI_NAMES 256

static struct {
	char filename[16];
	char display_name[MIDI_METADATA_DISPLAY_BYTES];
} s_mission_midi_names[MAX_MISSION_MIDI_NAMES];
static int s_mission_midi_name_count;

static void mission_midi_names_load(void)
{
	int index;
	s_mission_midi_name_count = 0;
	for (index = 0; BIMSongs && index < Num_bim_songs &&
	                s_mission_midi_name_count < MAX_MISSION_MIDI_NAMES; ++index) {
		midi_metadata metadata;
		char source[MIDI_METADATA_SOURCE_FILENAME_BYTES];
		int inherited;
		midi_metadata_init(&metadata);
		midi_metadata_resolve_physfs(BIMSongs[index].filename, &metadata,
		                             source, sizeof(source), &inherited);
		if (midi_metadata_has_useful_summary(&metadata)) {
			strncpy(s_mission_midi_names[s_mission_midi_name_count].filename,
			        BIMSongs[index].filename,
			        sizeof(s_mission_midi_names[s_mission_midi_name_count].filename) - 1);
			strncpy(s_mission_midi_names[s_mission_midi_name_count].display_name,
			        metadata.display_name,
			        sizeof(s_mission_midi_names[s_mission_midi_name_count].display_name) - 1);
			++s_mission_midi_name_count;
		}
		midi_metadata_free(&metadata);
	}
}

void jukebox_names_load(const char *json_path)
{
	FILE *f;
	long len;
	char *buf;

	music_name_table_clear_jukebox();

	f = fopen(json_path, "rb");
	if (!f) return;

	fseek(f, 0, SEEK_END);
	len = ftell(f);
	if (len <= 2 || len > 512 * 1024) {
		fclose(f);
		return;
	}
	fseek(f, 0, SEEK_SET);

	buf = (char *) malloc(len + 1);
	if (!buf) {
		fclose(f);
		return;
	}
	if (fread(buf, 1, len, f) != (size_t) len) {
		free(buf);
		fclose(f);
		return;
	}
	buf[len] = '\0';
	fclose(f);

	music_name_table_load_jukebox(buf, (size_t) len);
	free(buf);
}

void jukebox_names_load_for_playlist(const char *playlist_path)
{
	char names_path[PATH_MAX];
	const char *separator;

	strncpy(names_path, playlist_path, PATH_MAX - 1);
	names_path[PATH_MAX - 1] = '\0';
	separator = strrchr(names_path, '/');
	if (separator)
		names_path[separator - names_path + 1] = '\0';
	else
		names_path[0] = '\0';
	strncat(names_path, "custom_music_names.json",
	        PATH_MAX - 1 - strlen(names_path));
	jukebox_names_load(names_path);
}

const char *jukebox_names_lookup(const char *filepath)
{
	return music_name_table_lookup_jukebox(filepath);
}

static const char *base_name(const char *path)
{
	const char *base = path;
	const char *p;
	if (!path) return NULL;
	for (p = path; *p; p++)
		if (*p == '/' || *p == '\\')
			base = p + 1;
	return base;
}

const char *jukebox_track_display_name(char *const *songs, int song_count,
                                       int index)
{
	static char namebuf[64];
	const char *decoded;
	const char *base;
	char *extension;

	if (!songs || index < 0 || index >= song_count)
		return NULL;
	decoded = jukebox_names_lookup(songs[index]);
	if (decoded && decoded[0])
		return decoded;
	base = base_name(songs[index]);
	strncpy(namebuf, base, sizeof(namebuf) - 1);
	namebuf[sizeof(namebuf) - 1] = '\0';
	extension = strrchr(namebuf, '.');
	if (extension)
		*extension = '\0';
	return namebuf;
}

void mission_music_names_load(void)
{
	PHYSFS_file *f;
	PHYSFS_sint64 len;
	char *buf;

	music_name_table_clear_mission();
	mission_midi_names_load();

	f = PHYSFS_openRead(MISSION_MUSIC_NAMES_FILE);
	if (!f) return;
	len = PHYSFS_fileLength(f);
	if (len <= 2 || len > 512 * 1024) {
		PHYSFS_close(f);
		return;
	}
	buf = (char *) malloc((size_t) len + 1);
	if (!buf) {
		PHYSFS_close(f);
		return;
	}
	if (PHYSFS_readBytes(f, buf, (PHYSFS_uint64) len) != len) {
		free(buf);
		PHYSFS_close(f);
		return;
	}
	buf[len] = '\0';
	PHYSFS_close(f);

	music_name_table_load_mission(buf, (size_t) len);
	free(buf);
}

const char *mission_music_names_lookup(const char *filename)
{
	const char *sidecar = music_name_table_lookup_mission(filename);
	int index;
	if (sidecar && sidecar[0])
		return sidecar;
	for (index = 0; filename && index < s_mission_midi_name_count; ++index)
		if (!strcasecmp(filename, s_mission_midi_names[index].filename))
			return s_mission_midi_names[index].display_name;
	return NULL;
}

/* -- Overlay formatting ---------------------------------------------- */

#define OVERLAY_TEXT_LEN 80

static char s_overlay_text[OVERLAY_TEXT_LEN];

/* Return 1 if name is exactly "[unknown] - [untitled]" (useless AcoustID placeholder) */
static int is_placeholder_name(const char *name)
{
	return name && strcmp(name, "[unknown] - [untitled]") == 0;
}

void track_overlay_notify(int track_or_song, int is_midi, unsigned long disc_id)
{
	if (is_midi) {
		int display_num = track_or_song - SONG_FIRST_LEVEL_SONG + 1;
		if (display_num < 1) display_num = track_or_song;
		snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "MIDI Track %d", display_num);
	} else {
		const char *name = track_names_lookup(track_or_song, disc_id);
		if (name && !is_placeholder_name(name))
			snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "%s", name);
		else
			snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "Track %d", track_or_song);
	}

	android_send_track_name(s_overlay_text);
	overlay_ringbuf_add("track", s_overlay_text);
}

void track_overlay_notify_mission_music(const char *filename, int song_index)
{
	const char *name = mission_music_names_lookup(filename);
	if (name && name[0] && !is_placeholder_name(name)) {
		snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "%s", name);
	} else {
		int display_num = song_index - SONG_FIRST_LEVEL_SONG + 1;
		if (display_num < 1) display_num = song_index;
		snprintf(s_overlay_text, OVERLAY_TEXT_LEN, "MIDI Track %d", display_num);
	}

	android_send_track_name(s_overlay_text);
	overlay_ringbuf_add("track", s_overlay_text);
}

void level_overlay_notify(int level_num, const char *level_name)
{
	char buf[OVERLAY_TEXT_LEN];
	int has_name = level_name && level_name[0];
	if (level_num < 0) {
		if (has_name)
			snprintf(buf, OVERLAY_TEXT_LEN, "Secret Level %d: %s", -level_num, level_name);
		else
			snprintf(buf, OVERLAY_TEXT_LEN, "Secret Level %d", -level_num);
	} else {
		if (has_name)
			snprintf(buf, OVERLAY_TEXT_LEN, "Level %d: %s", level_num, level_name);
		else
			snprintf(buf, OVERLAY_TEXT_LEN, "Level %d", level_num);
	}
	android_send_level_name(buf);
	overlay_ringbuf_add("level", buf);
}

void track_overlay_notify_jukebox(const char *filename)
{
	char buf[OVERLAY_TEXT_LEN];
	const char *name, *base, *p;

	if (!filename || !filename[0]) return;

	/* Try chromaprint-decoded name first */
	name = jukebox_names_lookup(filename);
	if (name && name[0]) {
		snprintf(buf, OVERLAY_TEXT_LEN, "%s", name);
		android_send_track_name(buf);
		overlay_ringbuf_add("jukebox", buf);
		return;
	}

	/* Fallback: strip path and extension */
	base = filename;
	for (p = filename; *p; p++) {
		if (*p == '/' || *p == '\\')
			base = p + 1;
	}

	strncpy(buf, base, OVERLAY_TEXT_LEN - 1);
	buf[OVERLAY_TEXT_LEN - 1] = '\0';
	p = strrchr(buf, '.');
	if (p)
		buf[p - buf] = '\0';

	android_send_track_name(buf);
	overlay_ringbuf_add("jukebox", buf);
}
