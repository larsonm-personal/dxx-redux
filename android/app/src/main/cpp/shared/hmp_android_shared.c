/* Shared Android HMP memory conversion for D1 and D2 */

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
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

static const unsigned char hmp_midi_tempo_track[] = {
	'M', 'T', 'r', 'k', 0, 0, 0, 11, 0, 0xff,
	0x51, 3, 0x18, 0x80, 0, 0, 0xff, 0x2f, 0
};

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

static int hmp_android_convert_mem(
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

int hmp2mid_mem(const unsigned char *hmp_data, int hmp_len,
                unsigned char **out_midi, int *out_len)
{
	return hmp_android_convert_mem(hmp_data, hmp_len, out_midi, out_len,
	                               hmp_midi_tempo_track,
	                               sizeof(hmp_midi_tempo_track));
}

/* DOS playback conversion. Keep the legacy interchange converter above stable.
 * Branch records are 24 bytes: track-relative event offset, ID, program,
 * reserved byte, controller byte count, two controller pointers, reserved data.
 * See the DOS MIDI parity report and capture regression for the measured rules.
 */
struct hmp_play_event {
	uint32_t tick, offset, order;
	unsigned char status, a, b, track;
};

static int hmp_play_order(const void *a, const void *b)
{
	const struct hmp_play_event *x = a, *y = b;
	if (x->tick != y->tick)
		return x->tick < y->tick ? -1 : 1;
	if (x->track != y->track)
		return x->track < y->track ? -1 : 1;
	return x->order < y->order ? -1 : x->order != y->order;
}

static int hmp_play_number(const unsigned char **p, const unsigned char *end,
                           uint32_t *value, int hmi)
{
	unsigned int i;
	*value = 0;
	for (i = 0; i < 4 && *p < end; i++) {
		unsigned char b = *(*p)++;
		if (hmi)
			*value |= (uint32_t) (b & 127) << (i * 7);
		else
			*value = (*value << 7) | (b & 127);
		if (!!(b & 128) == hmi)
			return 1;
	}
	return 0;
}

static int hmp_play_delta(unsigned char **out, unsigned int *len, uint32_t value)
{
	unsigned char bytes[4];
	int n = 4;
	if (value > 0x0fffffff)
		return 0;
	bytes[--n] = value & 127;
	while ((value >>= 7) != 0)
		bytes[--n] = (value & 127) | 128;
	return hmp_midi_append(out, len, bytes + n, (size_t) (4 - n));
}

static int hmp_play_emit(unsigned char **out, unsigned int *len, uint32_t *last,
                         uint32_t tick, unsigned char status,
                         unsigned char a, unsigned char b, int fm)
{
	unsigned char msg[3] = { status, a, b };
	size_t size = (status & 0xe0) == 0xc0 ? 2 : 3;
	/* DOS music volume 8 maps every observed CC7 v to max(v - 2, 0).
	 * This is a measured full-volume transfer, not a general HMI gain formula.
	 * The Android user volume remains a separate PCM gain.
	 */
	if (!fm && (status & 0xf0) == 0xb0 && a == 7)
		msg[2] = b > 1 ? (unsigned char) (b - 2) : 0;
	if (tick < *last || !hmp_play_delta(out, len, tick - *last) ||
	    !hmp_midi_append(out, len, msg, size))
		return 0;
	*last = tick;
	return 1;
}

/* The timing track has no device record. Tracks 1..31 have 20-byte records
 * at 0x90: a reserved word and four driver designators. An empty list is
 * universal; otherwise the General MIDI synth must select 0xa000 tracks.
 * Level 7 contains separate FM, GM/GUS and digital-sample arrangements.
 */
static int hmp_play_track_uses_device(const unsigned char *data, int track, int fm)
{
	unsigned int i, any = 0;
	const unsigned char *record;
	if (!track)
		return 1;
	record = data + 0x94 + (track - 1) * 20;
	for (i = 0; i < 4; i++) {
		uint32_t device = hmp_read_le32(record + i * 4);
		if (device == (fm ? 0xa002u : 0xa000u))
			return 1;
		any |= device;
	}
	return !any;
}

int hmp2mid_playback_device_mem(const unsigned char *data, int len, int repeat, int fm,
                                unsigned char **midi, int *midi_len,
                                struct hmp_playback_info *info)
{
	static const unsigned char header[] = {
		'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 0, 120,
		'M', 'T', 'r', 'k', 0, 0, 0, 0,
		0, 0xff, 0x51, 3, 0x0f, 0x42, 0x40
	};
	static const unsigned char eot[] = { 0xff, 0x2f, 0 };
	hmp_file *hmp = NULL;
	struct hmp_play_event *events = NULL, *scheduled = NULL;
	const unsigned char *branch[HMP_TRACKS] = { 0 };
	uint32_t branch_tick[HMP_TRACKS] = { 0 };
	uint32_t track_end[HMP_TRACKS] = { 0 };
	uint32_t track_channel[HMP_TRACKS] = { 0 };
	unsigned char initial_shift[HMP_TRACKS] = { 0 };
	unsigned char selected[HMP_TRACKS] = { 0 };
	unsigned char *out = NULL;
	unsigned int out_len = 0;
	size_t count = 0, capacity, i, n, branch_pos, track_offset = 0x308;
	uint32_t last = 0, end_tick = 0, loop_tick = 0, loop_end = 0;
	uint32_t end_order = 0, loop_count = 0, start_count = 0;
	int track, pass, unsupported = 0, have_loop = 0;

	if (!midi || !midi_len || !info)
		return 0;
	*midi = NULL;
	*midi_len = 0;
	memset(info, 0, sizeof(*info));
	if (!data || len < 0x308 || len > 16 * 1024 * 1024)
		return 0;
	hmp = hmp_android_open_mem(data, len);
	if (!hmp || hmp->tempo < 1 || hmp->tempo > 32767)
		goto fail;
	capacity = (size_t) len / 3;
	if (capacity > 1024 * 1024)
		capacity = 1024 * 1024;
	events = d_malloc(capacity * sizeof(*events));
	scheduled = d_malloc(capacity * sizeof(*scheduled));
	if (!events || !scheduled)
		goto fail;
	for (track = 0; track < hmp->num_trks; track++) {
		const unsigned char *begin = hmp->trks[track].data, *p = begin;
		const unsigned char *end;
		uint32_t tick = 0;
		int first = 1, ended = 0;
		/* Track headers were bounded by hmp_android_open_mem above */
		track_channel[track] = hmp_read_le32(data + track_offset + 8);
		track_offset += 12 + hmp->trks[track].len;
		if (fm && track_channel[track] > 15) goto fail;
		selected[track] = (unsigned char) hmp_play_track_uses_device(data, track, fm);
		if (!p || !hmp->trks[track].len)
			goto fail;
		end = p + hmp->trks[track].len;
		while (p < end) {
			uint32_t delta, offset = (uint32_t) (p - begin);
			unsigned char status, a, b = 0;
			if (!hmp_play_number(&p, end, &delta, 1) || p == end ||
			    delta > 0x0fffffff - tick)
				goto fail;
			tick += delta;
			if (first) {
				/* HMI decrements a nonzero initial countdown on its first tick */
				initial_shift[track] = delta != 0;
				first = 0;
			}
			status = *p++;
			if (status == 0xff) {
				uint32_t size;
				if (p == end)
					goto fail;
				a = *p++;
				if (!hmp_play_number(&p, end, &size, 0) || size > (size_t) (end - p))
					goto fail;
				p += size;
				if (a == 0x2f) {
					if (size || p != end)
						goto fail;
					ended = 1;
					break;
				}
				/* HMP tempo is the header's tick rate, not an SMF tempo map */
				continue;
			}
			if (status < 0x80 || status >= 0xf0 || p == end)
				goto fail;
			a = *p++;
			if ((status & 0xe0) != 0xc0) {
				if (p == end)
					goto fail;
				b = *p++;
			}
			if (a > 127 || (b > 127 && !((status & 0xf0) == 0xb0 && a >= 108 && a <= 111)))
				goto fail;
			if (count == capacity)
				goto fail;
			events[count].tick = tick;
			events[count].offset = offset;
			events[count].order = (uint32_t) count;
			events[count].status = status;
			events[count].a = a;
			events[count].b = b;
			events[count].track = (unsigned char) track;
			count++;
		}
		if (!ended)
			goto fail;
		track_end[track] = tick;
	}
	/* Some FM-only HMQ files tag a loop-control track as GM but have no GM
	 * notes. Preserve the previous approximate playback for those files,
	 * explicitly reporting that no GM arrangement exists rather than muting.
	 */
	for (i = 0; i < count; i++)
		if (selected[events[i].track] && (events[i].status & 0xf0) == 0x90 && events[i].b)
			break;
	if (i == count) {
		if (fm) goto fail;
		info->no_gm_arrangement = 1;
		memset(selected, 1, sizeof(selected));
	}
	for (track = 0; track < hmp->num_trks; track++) {
		uint32_t effective_end = track_end[track] - (fm ? initial_shift[track] : 0);
		if (!selected[track]) info->filtered_tracks++;
		else if (effective_end > end_tick) end_tick = effective_end;
	}
	for (i = 0, n = 0; i < count; i++)
		if (selected[events[i].track]) events[n++] = events[i];
	count = n;
	for (i = 0; i < count; i++) {
		const struct hmp_play_event *e = &events[i];
		if ((e->status & 0xf0) != 0xb0) continue;
		if (e->a == 109 && e->b == 128) {
			loop_tick = e->tick;
			start_count++;
		}
		if (e->a == 110 && e->b != 255) unsupported = 1;
		if (e->a == 111) {
			if (e->b != 128) unsupported = 1;
			loop_end = e->tick - initial_shift[e->track];
			end_order = (uint32_t) i;
			loop_count++;
		}
	}
	if (!count || !end_tick)
		goto fail;
	have_loop = start_count == 1 && loop_count == 1 && loop_end > loop_tick && !unsupported;
	if (loop_count && !have_loop)
		unsupported = 1;
	branch_pos = hmp_read_le32(data + 0x20);
	if (have_loop && !branch_pos) {
		have_loop = 0;
		unsupported = 1;
	}
	if (have_loop) {
		size_t record;
		if (branch_pos < 0x308 || branch_pos > (size_t) len ||
		    (size_t) hmp->num_trks > (size_t) len - branch_pos)
			goto fail;
		record = branch_pos + hmp->num_trks;
		for (track = 0; track < hmp->num_trks; track++) {
			unsigned int j;
			for (j = 0; j < data[branch_pos + track]; j++, record += 24) {
				const unsigned char *r;
				uint32_t controls;
				if (record > (size_t) len || (size_t) len - record < 24)
					goto fail;
				r = data + record;
				controls = hmp_read_le32(r + 8);
				if (!selected[track] || r[4] != 128)
					continue;
				if (branch[track] || r[5] > 127 || (r[7] & 1) ||
				    controls > (unsigned int) len || r[7] > (unsigned int) len - controls)
					goto fail;
				branch[track] = r;
				for (i = 0; i < count; i++)
					if (events[i].track == track && events[i].offset == hmp_read_le32(r))
						break;
				if (i == count)
					goto fail;
				/* The branch offset points after its marker, before the next delta */
				branch_tick[track] = i ? events[i - 1].tick : 0;
				if (branch_tick[track] != loop_tick)
					unsupported = 1;
			}
		}
		if (unsupported)
			have_loop = 0;
		/* Ended tracks are no longer active when HMI takes the global branch */
		for (track = 0; track < hmp->num_trks; track++)
			if (track_end[track] < loop_end)
				branch[track] = NULL;
		for (i = 0; i < count; i++) {
			track = events[i].track;
			if (track_end[track] >= loop_end && !branch[track]) {
				have_loop = 0;
				unsupported = 1;
			}
		}
	}
	if (have_loop)
		end_tick = loop_end;
	if (end_tick > 0x07ffffff)
		goto fail;
	if (!hmp_midi_append(&out, &out_len, header, sizeof(header)))
		goto fail;
	out[12] = (unsigned char) (hmp->tempo >> 8);
	out[13] = (unsigned char) hmp->tempo;
	/* GM starts silent until CC7. FM starts with driver defaults, and a synthetic
	 * wheel message would change its voice-stealing policy before the first note
	 */
	for (track = 0; !fm && track < 16; track++)
		/* Captured HMI reset sends bytes 64,64 (8256), not 0,64 (8192) */
		if (!hmp_play_emit(&out, &out_len, &last, 0, (unsigned char) (0xe0 + track), 64, 64, fm) ||
		    !hmp_play_emit(&out, &out_len, &last, 0, (unsigned char) (0xb0 + track), 7, 0, fm))
			goto fail;
	for (pass = 0; pass <= !!repeat; pass++) {
		uint32_t base = pass ? end_tick + !have_loop : 0;
		if (pass && have_loop) {
			for (track = 0; track < hmp->num_trks; track++) {
				const unsigned char *r = branch[track], *controls;
				unsigned int j;
				unsigned char channel;
				if (!r)
					continue;
				for (i = 0; i < count; i++)
					if (events[i].track == track && events[i].offset == hmp_read_le32(r))
						break;
				channel = events[i].status & 15;
				if (!hmp_play_emit(&out, &out_len, &last, base, (unsigned char) (0xc0 + channel), r[5], 0, fm))
					goto fail;
				controls = data + hmp_read_le32(r + 8);
				for (j = 0; j < r[7]; j += 2) {
					if (controls[j] > 127 || controls[j + 1] > 127 ||
					    !hmp_play_emit(&out, &out_len, &last, base, (unsigned char) (0xb0 + channel), controls[j], controls[j + 1], fm))
						goto fail;
				}
			}
		}
		n = 0;
		for (i = 0; i < count; i++) {
			struct hmp_play_event e = events[i];
			uint32_t shift = initial_shift[e.track];
			if (pass && have_loop) {
				if (!branch[e.track] || e.offset < hmp_read_le32(branch[e.track]))
					continue;
				shift = loop_tick + 1;
			}
			e.tick = e.tick > shift ? e.tick - shift : 0;
			if (have_loop && (events[i].tick - initial_shift[e.track] > loop_end ||
			                  (events[i].tick - initial_shift[e.track] == loop_end && i >= end_order)))
				continue;
			if ((e.status & 0xf0) == 0xb0 && e.a >= 108 && e.a <= 111)
				continue;
			e.tick += base;
			scheduled[n++] = e;
		}
		qsort(scheduled, n, sizeof(*scheduled), hmp_play_order);
		for (i = 0; i < n; i++) {
			const struct hmp_play_event *e = &scheduled[i];
			if (!hmp_play_emit(&out, &out_len, &last, e->tick, e->status, e->a, e->b, fm))
				goto fail;
		}
		if (!have_loop) {
			unsigned int reset_channels = 0;
			/* EOF restarts the song, unlike a branch. HMI stops/reset channels
			 * at EOF and starts the next pass one driver tick later.
			 */
			for (i = 0; i < count; i++) {
				/* DOS FM resets declared track channels, even if events use others */
				unsigned char channel = fm ? (unsigned char) track_channel[events[i].track] : events[i].status & 15;
				unsigned char control = (unsigned char) (0xb0 + channel);
				if (reset_channels & (1u << channel)) continue;
				reset_channels |= 1u << channel;
				if (!hmp_play_emit(&out, &out_len, &last, base + end_tick, control, 123, 0, fm) ||
				    !hmp_play_emit(&out, &out_len, &last, base + end_tick, control, 121, 0, fm) ||
				    !hmp_play_emit(&out, &out_len, &last, base + end_tick, (unsigned char) (0xe0 + channel), fm ? 0 : 64, 64, fm) ||
				    !hmp_play_emit(&out, &out_len, &last, base + end_tick, control, 7, 0, fm))
					goto fail;
			}
		}
	}
	if (!have_loop) end_tick++;
	info->repeat_ms = repeat ? end_tick * (1000.0 / hmp->tempo) : 0;
	if (repeat)
		end_tick += have_loop ? end_tick - loop_tick : end_tick;
	info->end_ms = end_tick * (1000.0 / hmp->tempo);
	if (info->end_ms > INT_MAX)
		goto fail;
	info->branch_loop = have_loop;
	info->unsupported_branches = unsupported;
	if (end_tick < last || !hmp_play_delta(&out, &out_len, end_tick - last) ||
	    !hmp_midi_append(&out, &out_len, eot, sizeof(eot)))
		goto fail;
	{
		uint32_t bytes = out_len - 22;
		out[18] = (unsigned char) (bytes >> 24);
		out[19] = (unsigned char) (bytes >> 16);
		out[20] = (unsigned char) (bytes >> 8);
		out[21] = (unsigned char) bytes;
	}
	d_free(events);
	d_free(scheduled);
	hmp_close(hmp);
	*midi = out;
	*midi_len = (int) out_len;
	return 1;
fail:
	if (out) d_free(out);
	if (events) d_free(events);
	if (scheduled) d_free(scheduled);
	if (hmp) hmp_close(hmp);
	memset(info, 0, sizeof(*info));
	return 0;
}

int hmp2mid_playback_mem(const unsigned char *data, int len, int repeat,
                         unsigned char **midi, int *midi_len, struct hmp_playback_info *info)
{
	return hmp2mid_playback_device_mem(data, len, repeat, 0, midi, midi_len, info);
}
