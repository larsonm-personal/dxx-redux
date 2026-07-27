/* Shared Android HMP memory conversion for D1 and D2 */

#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "hmp.h"
#include "u_mem.h"

#include "hmp_android_shared.h"

#ifdef WORDS_BIGENDIAN
#define HMP_MIDI_INT(x)   (x)
#define HMP_MIDI_SHORT(x) (x)
#else
#define HMP_MIDI_INT(x)   SWAPINT(x)
#define HMP_MIDI_SHORT(x) SWAPSHORT(x)
#endif

static uint32_t hmp_read_le32(const unsigned char *p)
{
	return (uint32_t) p[0] | ((uint32_t) p[1] << 8) |
	       ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static hmp_file *hmp_android_open_mem(const unsigned char *buf, int buf_len)
{
	uint32_t num_tracks, raw_track_len, tempo;
	size_t offset, payload_len, remaining;
	hmp_file *hmp;
	int i;

	if (!buf || buf_len < 0x308 + 12)
		return NULL;
	if (memcmp(buf, "HMIMIDIP", 8) != 0)
		return NULL;

	num_tracks = hmp_read_le32(buf + 0x30);
	if (num_tracks < 1 || num_tracks > HMP_TRACKS)
		return NULL;
	tempo = hmp_read_le32(buf + 0x38);
	if (tempo > INT_MAX)
		return NULL;

	hmp = d_calloc(1, sizeof(*hmp));
	if (!hmp)
		return NULL;
	hmp->num_trks = (int) num_tracks;
	hmp->tempo = (int) tempo;

	offset = 0x308;
	for (i = 0; i < hmp->num_trks; i++) {
		remaining = (size_t) buf_len - offset;
		if (remaining < 12)
			goto fail;
		raw_track_len = hmp_read_le32(buf + offset + 4);
		if (raw_track_len < 12)
			goto fail;
		payload_len = (size_t) raw_track_len - 12;
		if (payload_len > remaining - 12 || payload_len > UINT_MAX)
			goto fail;

		hmp->trks[i].len = (unsigned int) payload_len;
		if (payload_len != 0) {
			hmp->trks[i].data = d_malloc(payload_len);
			if (!hmp->trks[i].data)
				goto fail;
			memcpy(hmp->trks[i].data, buf + offset + 12, payload_len);
		}
		hmp->trks[i].loop_set = 0;
		offset += 12 + payload_len;
	}
	hmp->filesize = buf_len;
	return hmp;

fail:
	hmp_close(hmp);
	return NULL;
}

static int hmp_midi_append(unsigned char **midbuf, unsigned int *midlen,
                           const void *data, size_t data_len)
{
	unsigned char *newbuf;

	if (data_len > INT_MAX || *midlen > (unsigned int) (INT_MAX - data_len))
		return 0;
	newbuf = d_realloc(*midbuf, *midlen + data_len);
	if (!newbuf)
		return 0;
	*midbuf = newbuf;
	if (data_len != 0)
		memcpy(newbuf + *midlen, data, data_len);
	*midlen += (unsigned int) data_len;
	return 1;
}

static int hmp_android_convert_track(const unsigned char *data, size_t size,
                                     unsigned char **midbuf, unsigned int *midlen, unsigned int *track_len)
{
	const unsigned char *cursor = data;
	const unsigned char *end;
	unsigned int start_len = *midlen;
	unsigned char last_command = 0;
	int found_end = 0;

	if (!data || size == 0)
		return 0;
	end = data + size;

	while (cursor < end) {
		const unsigned char *delta = cursor;
		size_t delta_len = 0, i, payload_len;
		unsigned char status, converted_delta[4];
		uint32_t meta_len = 0;
		unsigned int meta_len_bytes = 0;

		do {
			if (cursor == end || delta_len == sizeof(converted_delta))
				return 0;
			delta_len++;
		} while ((*cursor++ & 0x80) == 0);

		for (i = 0; i < delta_len; i++) {
			converted_delta[i] = delta[delta_len - i - 1] & 0x7f;
			if (i + 1 < delta_len)
				converted_delta[i] |= 0x80;
		}
		if (!hmp_midi_append(midbuf, midlen, converted_delta, delta_len))
			return 0;
		if (cursor == end)
			return 0;

		status = *cursor++;
		if (status == 0xff) {
			const unsigned char *event_start = cursor - 1;
			unsigned char meta_type;

			if (cursor == end)
				return 0;
			meta_type = *cursor++;
			do {
				unsigned char length_byte;

				if (cursor == end || meta_len_bytes == 4)
					return 0;
				length_byte = *cursor++;
				if (meta_len > (UINT32_MAX >> 7))
					return 0;
				meta_len = (meta_len << 7) | (length_byte & 0x7f);
				meta_len_bytes++;
				if ((length_byte & 0x80) == 0)
					break;
			} while (1);
			if (meta_len > (uint32_t) (end - cursor))
				return 0;
			if (!hmp_midi_append(midbuf, midlen, event_start,
			                     (size_t) (cursor - event_start) + meta_len))
				return 0;
			cursor += meta_len;
			if (meta_type == 0x2f) {
				if (meta_len != 0 || cursor != end)
					return 0;
				found_end = 1;
				break;
			}
			continue;
		}

		if ((status & 0x80) == 0)
			return 0;
		switch (status & 0xf0) {
			case 0x80:
			case 0x90:
			case 0xa0:
			case 0xb0:
			case 0xe0:
				payload_len = 2;
				break;
			case 0xc0:
			case 0xd0:
				payload_len = 1;
				break;
			default:
				return 0;
		}
		if (payload_len > (size_t) (end - cursor))
			return 0;
		if (status != last_command &&
		    !hmp_midi_append(midbuf, midlen, &status, 1))
			return 0;
		if (!hmp_midi_append(midbuf, midlen, cursor, payload_len))
			return 0;
		cursor += payload_len;
		last_command = status;
	}

	if (!found_end)
		return 0;
	*track_len = *midlen - start_len;
	return 1;
}

int hmp_android_convert_mem(
    const unsigned char *hmp_data, int hmp_len,
    unsigned char **out_midi, int *out_len,
    const unsigned char *tempo_track, unsigned int tempo_track_len)
{
	int i;
	short ms;
	hmp_file *hmp = NULL;
	unsigned int midlen = 0;
	unsigned char *midbuf = NULL;

	if (!out_midi || !out_len)
		return 0;
	*out_midi = NULL;
	*out_len = 0;
	if (!tempo_track || tempo_track_len == 0)
		return 0;

	hmp = hmp_android_open_mem(hmp_data, hmp_len);
	if (!hmp || hmp->tempo > SHRT_MAX * 5 / 8)
		goto fail;

	/* Write MIDI header */
	if (!hmp_midi_append(&midbuf, &midlen, "MThd", 4))
		goto fail;
	{
		int mi = HMP_MIDI_INT(6);
		if (!hmp_midi_append(&midbuf, &midlen, &mi, sizeof(mi)))
			goto fail;
	}
	ms = HMP_MIDI_SHORT(1);
	if (!hmp_midi_append(&midbuf, &midlen, &ms, sizeof(ms)))
		goto fail;
	ms = HMP_MIDI_SHORT(hmp->num_trks);
	if (!hmp_midi_append(&midbuf, &midlen, &ms, sizeof(ms)))
		goto fail;
	ms = HMP_MIDI_SHORT((short) (hmp->tempo * 8 / 5));
	if (!hmp_midi_append(&midbuf, &midlen, &ms, sizeof(ms)) ||
	    !hmp_midi_append(&midbuf, &midlen, tempo_track, tempo_track_len))
		goto fail;

	/* Convert HMP tracks */
	for (i = 1; i < hmp->num_trks; i++) {
		unsigned int track_len, track_len_pos;
		int midi_track_len;

		if (!hmp_midi_append(&midbuf, &midlen, "MTrk", 4))
			goto fail;
		track_len_pos = midlen;
		midi_track_len = 0;
		if (!hmp_midi_append(&midbuf, &midlen, &midi_track_len,
		                     sizeof(midi_track_len)) ||
		    !hmp_android_convert_track(hmp->trks[i].data, hmp->trks[i].len,
		                               &midbuf, &midlen, &track_len))
			goto fail;
		midi_track_len = HMP_MIDI_INT((int) track_len);
		memcpy(midbuf + track_len_pos, &midi_track_len, sizeof(midi_track_len));
	}

	hmp_close(hmp);
	*out_midi = midbuf;
	*out_len = (int) midlen;
	return 1;

fail:
	if (hmp)
		hmp_close(hmp);
	if (midbuf)
		d_free(midbuf);
	return 0;
}
