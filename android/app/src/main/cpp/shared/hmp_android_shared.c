/* Shared Android HMP memory conversion for D1 and D2 */

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

static hmp_file *hmp_android_open_mem(const unsigned char *buf, int buf_len)
{
	int i, data, num_tracks, tempo, offset;
	hmp_file *hmp;
	unsigned char *p;

	if (!buf || buf_len < 0x308 + 12)
		return NULL;
	if (memcmp(buf, "HMIMIDIP", 8) != 0)
		return NULL;

	hmp = d_calloc(1, sizeof(*hmp));
	if (!hmp)
		return NULL;

	memcpy(&num_tracks, buf + 0x30, 4);
	if ((num_tracks < 1) || (num_tracks > HMP_TRACKS)) {
		hmp_close(hmp);
		return NULL;
	}
	hmp->num_trks = num_tracks;

	memcpy(&tempo, buf + 0x38, 4);
	hmp->tempo = INTEL_INT(tempo);

	offset = 0x308;
	for (i = 0; i < num_tracks; i++) {
		if (offset + 8 > buf_len) {
			hmp_close(hmp);
			return NULL;
		}
		memcpy(&data, buf + offset + 4, 4);
		data -= 12;
		if (data < 0 || offset + 12 + data > buf_len) {
			hmp_close(hmp);
			return NULL;
		}
		hmp->trks[i].len = data;

		p = d_malloc(data);
		if (!(hmp->trks[i].data = p)) {
			hmp_close(hmp);
			return NULL;
		}
		memcpy(p, buf + offset + 12, data);
		hmp->trks[i].loop_set = 0;
		offset += 12 + data;
	}
	hmp->filesize = buf_len;
	return hmp;
}

int hmp_android_convert_mem(
    const unsigned char *hmp_data, int hmp_len,
    unsigned char **out_midi, int *out_len,
    hmp_android_track_converter convert_track,
    const unsigned char *tempo_track, unsigned int tempo_track_len)
{
	int mi, i;
	short ms, time_div = 0xC0;
	hmp_file *hmp;
	unsigned int midlen = 0;
	unsigned char *midbuf = NULL;

	hmp = hmp_android_open_mem(hmp_data, hmp_len);
	if (hmp == NULL)
		return 0;

	time_div = hmp->tempo * 1.6;

	/* Write MIDI header */
	midbuf = d_realloc(midbuf, midlen + 4);
	memcpy(&midbuf[midlen], "MThd", 4);
	midlen += 4;
	mi = HMP_MIDI_INT(6);
	midbuf = d_realloc(midbuf, midlen + sizeof(mi));
	memcpy(&midbuf[midlen], &mi, sizeof(mi));
	midlen += sizeof(mi);
	ms = HMP_MIDI_SHORT(1);
	midbuf = d_realloc(midbuf, midlen + sizeof(ms));
	memcpy(&midbuf[midlen], &ms, sizeof(ms));
	midlen += sizeof(ms);
	ms = HMP_MIDI_SHORT(hmp->num_trks);
	midbuf = d_realloc(midbuf, midlen + sizeof(ms));
	memcpy(&midbuf[midlen], &ms, sizeof(ms));
	midlen += sizeof(ms);
	ms = HMP_MIDI_SHORT(time_div);
	midbuf = d_realloc(midbuf, midlen + sizeof(ms));
	memcpy(&midbuf[midlen], &ms, sizeof(ms));
	midlen += sizeof(ms);
	midbuf = d_realloc(midbuf, midlen + tempo_track_len);
	memcpy(&midbuf[midlen], tempo_track, tempo_track_len);
	midlen += tempo_track_len;

	/* Convert HMP tracks */
	for (i = 1; i < hmp->num_trks; i++) {
		int midtrklenpos = 0;

		midbuf = d_realloc(midbuf, midlen + 4);
		memcpy(&midbuf[midlen], "MTrk", 4);
		midlen += 4;
		midtrklenpos = midlen;
		mi = 0;
		midbuf = d_realloc(midbuf, midlen + sizeof(mi));
		midlen += sizeof(mi);
		mi = convert_track(hmp->trks[i].data, hmp->trks[i].len, &midbuf, &midlen);
		mi = HMP_MIDI_INT(mi);
		memcpy(&midbuf[midtrklenpos], &mi, 4);
	}

	hmp_close(hmp);
	*out_midi = midbuf;
	*out_len = (int) midlen;
	return 1;
}
