#include "midi_seek_timeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tsf.h"

enum test_event_type {
	TEST_PROGRAM,
	TEST_NOTE_ON,
	TEST_NOTE_OFF,
	TEST_CONTROL,
	TEST_PITCH
};

struct test_event {
	unsigned int time_ms;
	enum test_event_type type;
	int channel;
	int first;
	int second;
	const struct test_event *next;
};

static unsigned int test_event_time_ms(const void *event)
{
	return ((const struct test_event *) event)->time_ms;
}

static const void *test_event_next(const void *event)
{
	return ((const struct test_event *) event)->next;
}

static void test_dispatch_event(void *context, const void *event)
{
	tsf *synth = (tsf *) context;
	const struct test_event *item = (const struct test_event *) event;

	switch (item->type) {
		case TEST_PROGRAM:
			tsf_channel_set_presetnumber(synth, item->channel, item->first,
			                             item->channel == 9);
			break;
		case TEST_NOTE_ON:
			tsf_channel_note_on(synth, item->channel, item->first,
			                    item->second / 127.0f);
			break;
		case TEST_NOTE_OFF:
			tsf_channel_note_off(synth, item->channel, item->first);
			break;
		case TEST_CONTROL:
			tsf_channel_midi_control(synth, item->channel, item->first, item->second);
			break;
		case TEST_PITCH:
			tsf_channel_set_pitchwheel(synth, item->channel, item->first);
			break;
	}
}

static void test_render_frames(void *context, short *output, int frames)
{
	tsf_render_short((tsf *) context, output, frames, 0);
}

static const struct midi_seek_timeline_ops test_ops = {
	test_event_time_ms,
	test_event_next,
	test_dispatch_event,
	test_render_frames
};

static unsigned char *read_file(const char *path, int *size_out)
{
	FILE *file = fopen(path, "rb");
	unsigned char *data;
	long size;

	if (!file || fseek(file, 0, SEEK_END) != 0 ||
	    (size = ftell(file)) <= 0 || size > 64 * 1024 * 1024 ||
	    fseek(file, 0, SEEK_SET) != 0) {
		if (file)
			fclose(file);
		return NULL;
	}
	data = (unsigned char *) malloc((size_t) size);
	if (!data || fread(data, 1, (size_t) size, file) != (size_t) size) {
		free(data);
		fclose(file);
		return NULL;
	}
	fclose(file);
	*size_out = (int) size;
	return data;
}

static void configure_synth(tsf *synth, int sample_rate)
{
	tsf_reset(synth);
	tsf_set_output(synth, TSF_STEREO_INTERLEAVED, sample_rate, -10.0f);
	tsf_set_max_voices(synth, 48);
}

static int render_linear_window(struct midi_seek_timeline *timeline,
                                uint64_t target_frame, short *window,
                                int window_frames)
{
	uint64_t required = target_frame + (uint64_t) window_frames;
	uint64_t end_frame = (required + 2047u) & ~(uint64_t) 2047u;
	short *all = (short *) malloc((size_t) end_frame * 2 * sizeof(short));
	int ok = 0;

	if (!all)
		return 0;
	while (timeline->frame < end_frame) {
		short *output = all + timeline->frame * 2;
		if (midi_seek_timeline_render(timeline, output, 2048) != 2048)
			goto done;
	}
	memcpy(window, all + target_frame * 2,
	       (size_t) window_frames * 2 * sizeof(short));
	ok = 1;
done:
	free(all);
	return ok;
}

static int render_seek_window(struct midi_seek_timeline *timeline,
                              uint64_t target_frame, short *window,
                              int window_frames)
{
	short block[2048 * 2];
	int prefill_frames;
	int copied;

	if (!midi_seek_timeline_reconstruct(timeline, target_frame, block, 2048,
	                                    &prefill_frames))
		return 0;
	copied = prefill_frames < window_frames ? prefill_frames : window_frames;
	if (copied > 0)
		memcpy(window, block, (size_t) copied * 2 * sizeof(short));
	while (copied < window_frames) {
		int got = midi_seek_timeline_render(timeline, block, 2048);
		int needed = window_frames - copied;

		if (got <= 0)
			return 0;
		if (needed > got)
			needed = got;
		memcpy(window + copied * 2, block,
		       (size_t) needed * 2 * sizeof(short));
		copied += needed;
	}
	return 1;
}

static int run_case(const unsigned char *soundfont, int soundfont_size,
                    const struct test_event *events, int sample_rate,
                    double target_ms)
{
	enum { WINDOW_FRAMES = 1024 };
	short linear_pcm[WINDOW_FRAMES * 2];
	short seek_pcm[WINDOW_FRAMES * 2];
	short event_only_pcm[WINDOW_FRAMES * 2];
	struct midi_seek_timeline linear;
	struct midi_seek_timeline seek;
	struct midi_seek_timeline event_only;
	const struct test_event *event;
	uint64_t target_frame = midi_seek_timeline_frame_for_ms(target_ms, sample_rate);
	tsf *linear_synth = tsf_load_memory(soundfont, soundfont_size);
	tsf *seek_synth = tsf_load_memory(soundfont, soundfont_size);
	tsf *event_only_synth = tsf_load_memory(soundfont, soundfont_size);
	int linear_voices;
	int seek_voices;
	int ok = 0;

	if (!linear_synth || !seek_synth || !event_only_synth)
		goto done;
	configure_synth(linear_synth, sample_rate);
	configure_synth(seek_synth, sample_rate);
	configure_synth(event_only_synth, sample_rate);
	midi_seek_timeline_init(&linear, events, sample_rate, 2, linear_synth, &test_ops);
	midi_seek_timeline_init(&seek, events, sample_rate, 2, seek_synth, &test_ops);
	if (!render_linear_window(&linear, target_frame, linear_pcm, WINDOW_FRAMES) ||
	    !render_seek_window(&seek, target_frame, seek_pcm, WINDOW_FRAMES))
		goto done;
	linear_voices = tsf_active_voice_count(linear_synth);
	seek_voices = tsf_active_voice_count(seek_synth);
	if (linear.frame != seek.frame || linear_voices != seek_voices ||
	    memcmp(linear_pcm, seek_pcm, sizeof(linear_pcm)) != 0)
		goto done;

	event = events;
	while (event && midi_seek_timeline_frame_for_ms(event->time_ms, sample_rate) <= target_frame) {
		test_dispatch_event(event_only_synth, event);
		event = event->next;
	}
	midi_seek_timeline_init(&event_only, event, sample_rate, 2,
	                        event_only_synth, &test_ops);
	event_only.frame = target_frame;
	if (midi_seek_timeline_render(&event_only, event_only_pcm, WINDOW_FRAMES) != WINDOW_FRAMES ||
	    memcmp(linear_pcm, event_only_pcm, sizeof(linear_pcm)) == 0) {
		fprintf(stderr, "event-only negative control did not diverge at %d Hz %.0f ms\n",
		        sample_rate, target_ms);
		goto done;
	}
	ok = 1;

done:
	if (!ok)
		fprintf(stderr, "MIDI seek parity failed at %d Hz %.0f ms\n", sample_rate, target_ms);
	if (linear_synth)
		tsf_close(linear_synth);
	if (seek_synth)
		tsf_close(seek_synth);
	if (event_only_synth)
		tsf_close(event_only_synth);
	return ok;
}

int main(int argc, char **argv)
{
	struct test_event events[] = {
		{ 0, TEST_PROGRAM, 0, 48, 0, NULL },
		{ 0, TEST_NOTE_ON, 0, 60, 110, NULL },
		{ 350, TEST_PITCH, 0, 10240, 0, NULL },
		{ 500, TEST_CONTROL, 0, 64, 127, NULL },
		{ 800, TEST_NOTE_OFF, 0, 60, 0, NULL },
		{ 900, TEST_NOTE_ON, 0, 60, 96, NULL },
		{ 1200, TEST_NOTE_OFF, 0, 60, 0, NULL },
		{ 1300, TEST_CONTROL, 0, 64, 0, NULL },
		{ 1500, TEST_NOTE_ON, 9, 38, 115, NULL },
		{ 1700, TEST_NOTE_OFF, 9, 38, 0, NULL },
		{ 5000, TEST_CONTROL, 0, 7, 100, NULL }
	};
	unsigned char *soundfont;
	int soundfont_size;
	int i;
	int rate;
	double target;

	if (argc != 2) {
		fprintf(stderr, "usage: test_midi_seek_timeline <soundfont>\n");
		return 2;
	}
	for (i = 0; i + 1 < (int) (sizeof(events) / sizeof(events[0])); i++)
		events[i].next = &events[i + 1];
	soundfont = read_file(argv[1], &soundfont_size);
	if (!soundfont) {
		fprintf(stderr, "could not read soundfont\n");
		return 2;
	}
	for (rate = 44100; rate <= 48000; rate += 3900) {
		for (target = 700.0; target <= 2100.0; target += 700.0) {
			if (!run_case(soundfont, soundfont_size, events, rate, target)) {
				free(soundfont);
				return 1;
			}
		}
	}
	free(soundfont);
	puts("MIDI seek timeline tests passed");
	return 0;
}
