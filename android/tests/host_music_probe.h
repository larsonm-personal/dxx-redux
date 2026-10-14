/* Test-only SDL_mixer observation for the headed native configuration runner
 * Included by digi_mixer_music.c only with DXX_HOST_CONFIG_PROBE
 * Postmix PCM includes sound effects; assert music energy only in quiet windows
 */
#ifndef DXX_HOST_MUSIC_PROBE_H
#define DXX_HOST_MUSIC_PROBE_H

#include <stdio.h>
#include <stdint.h>
#include "songs.h"

static uint64_t host_music_pcm_samples;
static uint64_t host_music_pcm_nonzero;
static uint64_t host_music_pcm_absolute;
static uint64_t host_music_pcm_positive_crossings;
static Sint16 host_music_previous_left;
static unsigned host_music_play_requests;
static unsigned host_music_midi_bytes;
static uint64_t host_music_midi_fingerprint;
static int host_music_play_result = -999;
static char host_music_last_track[PATH_MAX];
static int host_music_probe_installed;
static unsigned host_music_peak_cursor;
static struct {
	Uint32 ticks;
	unsigned peak;
} host_music_peaks[256];

static void host_music_postmix(void *unused, Uint8 *stream, int length)
{
	int i;
	unsigned peak = 0;
	(void) unused;
	/* Both native mixers request AUDIO_S16 in host byte order */
	for (i = 0; i + 1 < length; i += 2) {
		Sint16 value;
		memcpy(&value, stream + i, sizeof(value));
		if ((i % 4) == 0) {
			if (value > 0 && host_music_previous_left <= 0) host_music_pcm_positive_crossings++;
			host_music_previous_left = value;
		}
		host_music_pcm_samples++;
		if (value) host_music_pcm_nonzero++;
		{
			unsigned magnitude = value < 0 ? -(int) value : (int) value;
			host_music_pcm_absolute += magnitude;
			if (magnitude > peak) peak = magnitude;
		}
	}
	host_music_peaks[host_music_peak_cursor].ticks = SDL_GetTicks();
	host_music_peaks[host_music_peak_cursor].peak = peak;
	host_music_peak_cursor = (host_music_peak_cursor + 1) % 256;
}

static void host_music_probe_request(const char *filename)
{
	host_music_play_requests++;
	host_music_midi_bytes = 0;
	host_music_midi_fingerprint = 0;
	host_music_play_result = -999;
	snprintf(host_music_last_track, sizeof(host_music_last_track), "%s", filename);
}

static void host_music_probe_midi(const unsigned char *buffer, unsigned length)
{
	unsigned i;
	uint64_t fingerprint = UINT64_C(14695981039346656037);
	if (!buffer || !length) return;
	for (i = 0; i < length; ++i) {
		fingerprint ^= buffer[i];
		fingerprint *= UINT64_C(1099511628211);
	}
	host_music_midi_bytes = length;
	host_music_midi_fingerprint = fingerprint;
}

static void host_music_probe_string(FILE *out, const char *text)
{
	const unsigned char *p = (const unsigned char *) text;
	fputc('"', out);
	for (; *p; ++p) {
		if (*p == '"' || *p == '\\') fputc('\\', out);
		if (*p < 32) fprintf(out, "\\u%04x", *p);
		else fputc(*p, out);
	}
	fputc('"', out);
}

void host_music_probe_snapshot(FILE *out)
{
	uint64_t samples, nonzero, absolute, crossings, midi_fingerprint;
	unsigned peak_1s = 0, i, requests, midi_bytes;
	int play_result;
	char last_track[PATH_MAX];
	Uint32 now = SDL_GetTicks();
	int frequency = 0, channels = 0;
	Uint16 format = 0;
	int audio_open = Mix_QuerySpec(&frequency, &format, &channels);
	if (!host_music_probe_installed && audio_open) {
		Mix_SetPostMix(host_music_postmix, NULL);
		host_music_probe_installed = 1;
	}
	if (audio_open) SDL_LockAudio();
	samples = host_music_pcm_samples;
	nonzero = host_music_pcm_nonzero;
	absolute = host_music_pcm_absolute;
	crossings = host_music_pcm_positive_crossings;
	requests = host_music_play_requests;
	midi_bytes = host_music_midi_bytes;
	midi_fingerprint = host_music_midi_fingerprint;
	play_result = host_music_play_result;
	memcpy(last_track, host_music_last_track, sizeof(last_track));
	for (i = 0; i < 256; ++i)
		if (now - host_music_peaks[i].ticks <= 1000 && host_music_peaks[i].peak > peak_1s)
			peak_1s = host_music_peaks[i].peak;
	if (audio_open) SDL_UnlockAudio();
	fprintf(out, "{\"playing\":%d,\"paused\":%d,\"volume\":%d,\"song\":%d,"
	             "\"requests\":%u,\"play_result\":%d,\"last_track\":",
	        Mix_PlayingMusic(), Mix_PausedMusic(), Mix_VolumeMusic(-1), songs_is_playing(),
	        requests, play_result);
	host_music_probe_string(out, last_track);
	fprintf(out, ",\"midi_bytes\":%u,\"midi_fnv1a64\":\"%016llx\"", midi_bytes, (unsigned long long) midi_fingerprint);
	fprintf(out, ",\"audio_open\":%d,\"frequency\":%d,\"channels\":%d,\"format\":%u,"
	             "\"pcm_samples\":%llu,\"pcm_nonzero\":%llu,\"pcm_absolute\":%llu,\"pcm_peak_1s\":%u,\"pcm_positive_crossings\":%llu}",
	        audio_open, frequency, channels, (unsigned) format,
	        (unsigned long long) samples, (unsigned long long) nonzero, (unsigned long long) absolute, peak_1s, (unsigned long long) crossings);
}
#endif
