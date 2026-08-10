/* Shared Android music controls for d1/main/songs.c and d2/main/songs.c. */

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "rbaudio.h"
#include "songs.h"
#include "timer.h"
#include "track_names.h"
#ifdef USE_SDLMIXER
#include "jukebox.h"
#endif

#include "songs_android_shared.h"

extern int Song_playing;
extern bim_song_info *BIMSongs;
extern int Num_bim_songs;

static int Android_music_prefer_mission_soundtrack = 1;

int android_music_redbook_track_for_ordinal(int first, int last, int ordinal)
{
	int audio_count = 0;
	int i;

	if (first < 1)
		first = 1;
	if (last > RBAGetNumberOfTracks())
		last = RBAGetNumberOfTracks();
	for (i = first; i <= last; i++)
		if (RBAIsAudioTrack(i))
			audio_count++;
	if (audio_count == 0)
		return 0;

	ordinal %= audio_count;
	if (ordinal < 0)
		ordinal += audio_count;
	for (i = first; i <= last; i++) {
		if (!RBAIsAudioTrack(i))
			continue;
		if (ordinal-- == 0)
			return i;
	}
	return 0;
}

int android_music_redbook_track_offset(int first, int last, int current, int offset)
{
	int ordinal = 0;
	int i;

	for (i = first; i <= last; i++) {
		if (!RBAIsAudioTrack(i))
			continue;
		if (i == current)
			return android_music_redbook_track_for_ordinal(first, last, ordinal + offset);
		ordinal++;
	}
	return android_music_redbook_track_for_ordinal(first, last, offset < 0 ? -1 : 0);
}

void android_music_set_prefer_mission_soundtrack(int enabled)
{
	Android_music_prefer_mission_soundtrack = enabled ? 1 : 0;
}

int android_music_get_prefer_mission_soundtrack(void)
{
	return Android_music_prefer_mission_soundtrack;
}

static void redbook_resume_programmed_song(void)
{
	int programmed_song = Song_playing;

	if (GameCfg.MusicType != MUSIC_TYPE_REDBOOK || programmed_song < 0)
		return;

	stop_time();
	Song_playing = -1;
	if (programmed_song >= SONG_FIRST_LEVEL_SONG)
		songs_play_level_song(programmed_song - SONG_FIRST_LEVEL_SONG + 1, 0);
	else
		songs_play_song(programmed_song, 1);
	start_time();
}

static int redbook_play_track_with_resume(int track)
{
	if (!RBAEnabled() || !RBAIsAudioTrack(track))
		return 0;
	if (RBAPlayTracks(track, track, redbook_resume_programmed_song)) {
		track_overlay_notify(track, 0, RBAGetDiscID());
		return 1;
	}
	return 0;
}

static int redbook_find_adjacent_audio_track(int direction)
{
	int total_tracks, current_track, i;

	if (!RBAEnabled())
		return 0;
	total_tracks = RBAGetNumberOfTracks();
	if (total_tracks <= 0)
		return 0;
	current_track = RBAGetTrackNum();
	if (current_track < 1 || current_track > total_tracks)
		current_track = 1;

	for (i = 1; i <= total_tracks; i++) {
		int track = current_track + direction * i;
		while (track < 1)
			track += total_tracks;
		while (track > total_tracks)
			track -= total_tracks;
		if (RBAIsAudioTrack(track))
			return track;
	}

	return 0;
}

int songs_next_track(void)
{
	int n_level_songs;

	switch (GameCfg.MusicType) {
		case MUSIC_TYPE_BUILTIN: {
			int track;
			n_level_songs = Num_bim_songs - SONG_FIRST_LEVEL_SONG;
			if (n_level_songs <= 0)
				return 0;
			if (Song_playing < SONG_FIRST_LEVEL_SONG)
				track = SONG_FIRST_LEVEL_SONG;
			else
				track = SONG_FIRST_LEVEL_SONG +
				        ((Song_playing - SONG_FIRST_LEVEL_SONG + 1) % n_level_songs);
			if (songs_play_file(BIMSongs[track].filename, 1, NULL)) {
				Song_playing = track;
				track_overlay_notify_mission_music(BIMSongs[track].filename, track);
				return 1;
			}
			return 0;
		}

		case MUSIC_TYPE_REDBOOK: {
			int track = redbook_find_adjacent_audio_track(1);
			if (track > 0 && redbook_play_track_with_resume(track))
				return track;
			return 0;
		}

#ifdef USE_SDLMIXER
		case MUSIC_TYPE_CUSTOM:
			if (!jukebox_is_loaded() || GameCfg.CMLevelMusicTrack[1] <= 0)
				return 0;
			GameCfg.CMLevelMusicTrack[0]++;
			if (GameCfg.CMLevelMusicTrack[0] >= GameCfg.CMLevelMusicTrack[1])
				GameCfg.CMLevelMusicTrack[0] = 0;
			return jukebox_play();
#endif
		default:
			return 0;
	}
}

int songs_prev_track(void)
{
	int n_level_songs;

	switch (GameCfg.MusicType) {
		case MUSIC_TYPE_BUILTIN: {
			int track;
			n_level_songs = Num_bim_songs - SONG_FIRST_LEVEL_SONG;
			if (n_level_songs <= 0)
				return 0;
			if (Song_playing < SONG_FIRST_LEVEL_SONG)
				track = SONG_FIRST_LEVEL_SONG;
			else
				track = SONG_FIRST_LEVEL_SONG +
				        ((Song_playing - SONG_FIRST_LEVEL_SONG - 1 + n_level_songs) % n_level_songs);
			if (songs_play_file(BIMSongs[track].filename, 1, NULL)) {
				Song_playing = track;
				track_overlay_notify_mission_music(BIMSongs[track].filename, track);
				return 1;
			}
			return 0;
		}

		case MUSIC_TYPE_REDBOOK: {
			int track = redbook_find_adjacent_audio_track(-1);
			if (track > 0 && redbook_play_track_with_resume(track))
				return track;
			return 0;
		}

#ifdef USE_SDLMIXER
		case MUSIC_TYPE_CUSTOM:
			if (!jukebox_is_loaded() || GameCfg.CMLevelMusicTrack[1] <= 0)
				return 0;
			GameCfg.CMLevelMusicTrack[0]--;
			if (GameCfg.CMLevelMusicTrack[0] < 0)
				GameCfg.CMLevelMusicTrack[0] = GameCfg.CMLevelMusicTrack[1] - 1;
			return jukebox_play();
#endif
		default:
			return 0;
	}
}

int songs_play_specific_track(int track)
{
	switch (GameCfg.MusicType) {
		case MUSIC_TYPE_BUILTIN:
			if (track < SONG_FIRST_LEVEL_SONG || track >= Num_bim_songs)
				return 0;
			if (songs_play_file(BIMSongs[track].filename, 1, NULL)) {
				Song_playing = track;
				track_overlay_notify_mission_music(BIMSongs[track].filename, track);
				return 1;
			}
			return 0;

		case MUSIC_TYPE_REDBOOK:
			return redbook_play_track_with_resume(track);

#ifdef USE_SDLMIXER
		case MUSIC_TYPE_CUSTOM:
			if (!jukebox_is_loaded() || track < 0 || track >= GameCfg.CMLevelMusicTrack[1])
				return 0;
			GameCfg.CMLevelMusicTrack[0] = track;
			return jukebox_play();
#endif
		default:
			return 0;
	}
}

int songs_get_track_info(int *out_type, int *out_track, int *out_total,
                         char *out_name, int name_size)
{
	*out_type = GameCfg.MusicType;
	*out_name = '\0';

	switch (GameCfg.MusicType) {
		case MUSIC_TYPE_BUILTIN:
			if (Song_playing < SONG_FIRST_LEVEL_SONG || Song_playing >= Num_bim_songs)
				return -1;
			*out_track = Song_playing;
			*out_total = Num_bim_songs - SONG_FIRST_LEVEL_SONG;
			{
				const char *decoded = mission_music_names_lookup(BIMSongs[Song_playing].filename);
				strncpy(out_name, decoded && decoded[0] ? decoded : BIMSongs[Song_playing].filename, name_size - 1);
			}
			out_name[name_size - 1] = '\0';
			return 0;

		case MUSIC_TYPE_REDBOOK: {
			int src_idx = 0;
			if (RBAGetCurrentTrackInfo(out_track, out_name, name_size, &src_idx) != 0)
				return -1;
			*out_total = RBAGetNumberOfTracks();
			return 0;
		}

#ifdef USE_SDLMIXER
		case MUSIC_TYPE_CUSTOM:
			if (!jukebox_is_loaded() || GameCfg.CMLevelMusicTrack[0] < 0)
				return -1;
			*out_track = GameCfg.CMLevelMusicTrack[0];
			*out_total = GameCfg.CMLevelMusicTrack[1];
			{
				char *cur = jukebox_current();
				const char *decoded = cur ? jukebox_names_lookup(cur) : NULL;
				if (decoded && decoded[0]) {
					strncpy(out_name, decoded, name_size - 1);
				} else if (cur)
					strncpy(out_name, cur, name_size - 1);
				out_name[name_size - 1] = '\0';
			}
			return 0;
#endif
		default:
			return -1;
	}
}

static void songs_json_escape(const char *src, char *dst, int dst_size)
{
	int pos = 0;
	unsigned char ch;

	if (!dst || dst_size <= 0)
		return;

	if (!src)
		src = "";

	while ((ch = (unsigned char) *src++) != '\0' && pos < dst_size - 1) {
		switch (ch) {
			case '\\':
			case '"':
				if (pos >= dst_size - 2)
					goto done;
				dst[pos++] = '\\';
				dst[pos++] = (char) ch;
				break;

			case '\n':
				if (pos >= dst_size - 2)
					goto done;
				dst[pos++] = '\\';
				dst[pos++] = 'n';
				break;

			case '\r':
				if (pos >= dst_size - 2)
					goto done;
				dst[pos++] = '\\';
				dst[pos++] = 'r';
				break;

			case '\t':
				if (pos >= dst_size - 2)
					goto done;
				dst[pos++] = '\\';
				dst[pos++] = 't';
				break;

			default:
				if (ch < 0x20)
					ch = ' ';
				dst[pos++] = (char) ch;
				break;
		}
	}

done:
	dst[pos] = '\0';
}

int songs_get_track_list(char *buf, int buf_size)
{
	int pos = 0;
	int i;

	pos += snprintf(buf + pos, buf_size - pos, "[");

	switch (GameCfg.MusicType) {
		case MUSIC_TYPE_BUILTIN: {
			int n = Num_bim_songs - SONG_FIRST_LEVEL_SONG;
			for (i = 0; i < n && pos < buf_size - 2; i++) {
				char escaped_name[256];
				const char *name = mission_music_names_lookup(BIMSongs[SONG_FIRST_LEVEL_SONG + i].filename);
				if (i > 0)
					pos += snprintf(buf + pos, buf_size - pos, ",");
				songs_json_escape(name && name[0] ? name : BIMSongs[SONG_FIRST_LEVEL_SONG + i].filename,
				                  escaped_name, sizeof(escaped_name));
				pos += snprintf(buf + pos, buf_size - pos,
				                "{\"index\":%d,\"name\":\"%s\"}",
				                SONG_FIRST_LEVEL_SONG + i,
				                escaped_name);
			}
			break;
		}
		case MUSIC_TYPE_REDBOOK: {
			int total = RBAGetNumberOfTracks();
			for (i = 1; i <= total && pos < buf_size - 2; i++) {
				char escaped_name[256];
				const char *name;
				if (!RBAIsAudioTrack(i))
					continue;
				name = RBAGetTrackName(i);
				if (pos > 1)
					pos += snprintf(buf + pos, buf_size - pos, ",");
				songs_json_escape(name ? name : "", escaped_name, sizeof(escaped_name));
				pos += snprintf(buf + pos, buf_size - pos,
				                "{\"index\":%d,\"name\":\"%s\"}",
				                i, escaped_name);
			}
			break;
		}
#ifdef USE_SDLMIXER
		case MUSIC_TYPE_CUSTOM: {
			int n = jukebox_numtracks();
			for (i = 0; i < n && pos < buf_size - 2; i++) {
				char escaped_name[256];
				const char *name = jukebox_get_track_name(i);
				if (i > 0)
					pos += snprintf(buf + pos, buf_size - pos, ",");
				songs_json_escape(name ? name : "", escaped_name, sizeof(escaped_name));
				pos += snprintf(buf + pos, buf_size - pos,
				                "{\"index\":%d,\"name\":\"%s\"}",
				                i, escaped_name);
			}
			break;
		}
#endif
	}

	snprintf(buf + pos, buf_size - pos, "]");
	return 0;
}
