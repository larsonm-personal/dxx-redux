/* Offline listening clips using Android's pinned synth, soundfont and scheduler */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tsf.h"
#include "tml.h"
#include "hmp_tsf_state.h"
#include "midi_seek_timeline.h"

struct render_context {
	tsf *synth;
	int channel;
};

static void *read_file(const char *path, int *size)
{
	FILE *f = fopen(path, "rb");
	long length;
	void *data = NULL;
	if (!f) return NULL;
	if (fseek(f, 0, SEEK_END) || (length = ftell(f)) <= 0 || length > 64 * 1024 * 1024 || fseek(f, 0, SEEK_SET))
		goto done;
	data = malloc((size_t) length);
	if (data && fread(data, 1, (size_t) length, f) != (size_t) length) {
		free(data);
		data = NULL;
	}
	if (data) *size = (int) length;
done:
	fclose(f);
	return data;
}

static tml_message *load_midi(const char *path)
{
	int size;
	void *data = read_file(path, &size);
	tml_message *m = data ? tml_load_memory(data, size) : NULL;
	free(data);
	return m;
}

static unsigned int event_time(const void *event)
{
	return ((const tml_message *) event)->time;
}
static const void *event_next(const void *event)
{
	return ((const tml_message *) event)->next;
}

static void dispatch(void *opaque, const void *event)
{
	struct render_context *ctx = opaque;
	const tml_message *m = event;
	tsf *synth = ctx->synth;
	if (ctx->channel >= 0 && m->channel != ctx->channel) return;
	switch (m->type) {
		case TML_NOTE_ON: tsf_channel_note_on(synth, m->channel, m->key, m->velocity / 127.0f); break;
		case TML_NOTE_OFF: tsf_channel_note_off(synth, m->channel, m->key); break;
		case TML_PROGRAM_CHANGE: tsf_channel_set_presetnumber(synth, m->channel, m->program, m->channel == 9); break;
		case TML_CONTROL_CHANGE: hmp_tsf_control(synth, m->channel, m->control, m->control_value); break;
		case TML_PITCH_BEND: tsf_channel_set_pitchwheel(synth, m->channel, m->pitch_bend); break;
		default: break;
	}
}

static void render(void *opaque, short *out, int frames)
{
	tsf_render_short(((struct render_context *) opaque)->synth, out, frames, 0);
}

/* Fast prefix reconstruction excludes voices from preceding songs */
static tml_message *prefix(struct render_context *ctx, tml_message *m, int end_ms)
{
	for (; m && m->time < (unsigned int) end_ms; m = m->next)
		if (m->type != TML_NOTE_ON && m->type != TML_NOTE_OFF) dispatch(ctx, m);
	return m;
}

static void le32(unsigned char *p, uint32_t v)
{
	int i;
	for (i = 0; i < 4; i++) p[i] = (unsigned char) (v >> (i * 8));
}

int main(int argc, char **argv)
{
	const struct midi_seek_timeline_ops ops = { event_time, event_next, dispatch, render };
	struct midi_seek_timeline timeline;
	struct render_context ctx = { NULL, -1 };
	struct hmp_tsf_state retained = { 0 };
	tml_message *messages = NULL, *context = NULL, *first;
	unsigned char header[44] = "RIFF\0\0\0\0WAVEfmt ";
	short block[2048 * 2];
	void *sf;
	FILE *out = NULL;
	int size, start, duration, frames, i, ok = 0;
	if (argc != 6 && argc != 7 && argc != 9) {
		fprintf(stderr, "Usage: midi_tsf_render soundfont.sf2 input.mid output.wav start_ms duration_ms [channel|-1 [context.mid context_end_ms]]\n");
		return 2;
	}
	start = atoi(argv[4]);
	duration = atoi(argv[5]);
	if (argc >= 7) ctx.channel = atoi(argv[6]);
	if (start < 0 || start > 3600000 || duration <= 0 || duration > 600000 || ctx.channel < -1 || ctx.channel > 15)
		return 2;
	sf = read_file(argv[1], &size);
	ctx.synth = sf ? tsf_load_memory(sf, size) : NULL;
	free(sf);
	if (!ctx.synth || !(messages = load_midi(argv[2]))) goto done;
	tsf_set_output(ctx.synth, TSF_STEREO_INTERLEAVED, 48000, -10.0f);
	tsf_set_max_voices(ctx.synth, 48);
	if (argc == 9) {
		if (!(context = load_midi(argv[7])) || atoi(argv[8]) < 0) goto done;
		prefix(&ctx, context, atoi(argv[8]));
		hmp_tsf_capture(ctx.synth, &retained);
		tsf_reset(ctx.synth);
		hmp_tsf_begin(ctx.synth, &retained);
	}
	first = prefix(&ctx, messages, start);
	midi_seek_timeline_init(&timeline, first, 48000, 2, &ctx, &ops);
	timeline.frame = (uint64_t) start * 48;
	if (!midi_seek_timeline_set_range(&timeline, NULL, 0, start + duration)) goto done;
	frames = duration * 48;
	le32(header + 4, (uint32_t) frames * 4 + 36);
	le32(header + 16, 16);
	header[20] = 1;
	header[22] = 2;
	le32(header + 24, 48000);
	le32(header + 28, 48000 * 4);
	header[32] = 4;
	header[34] = 16;
	memcpy(header + 36, "data", 4);
	le32(header + 40, (uint32_t) frames * 4);
	out = fopen(argv[3], "wb");
	if (!out || fwrite(header, 1, sizeof(header), out) != sizeof(header)) goto done;
	while (frames > 0) {
		unsigned char bytes[sizeof(block)];
		int n = frames > 2048 ? 2048 : frames;
		if (midi_seek_timeline_render(&timeline, block, n) != n) goto done;
		for (i = 0; i < n * 2; i++) {
			bytes[i * 2] = (unsigned char) block[i];
			bytes[i * 2 + 1] = (unsigned char) ((uint16_t) block[i] >> 8);
		}
		if (fwrite(bytes, 4, (size_t) n, out) != (size_t) n) goto done;
		frames -= n;
	}
	ok = 1;
done:
	if (out && fclose(out)) ok = 0;
	if (context) tml_free(context);
	if (messages) tml_free(messages);
	if (ctx.synth) tsf_close(ctx.synth);
	if (!ok) fprintf(stderr, "MIDI clip render failed\n");
	return ok ? 0 : 1;
}
