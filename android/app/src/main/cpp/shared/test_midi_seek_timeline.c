#include "midi_seek_timeline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tsf.h"
#include "hmp_tsf_state.h"

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

struct loop_probe {
	int volume, note_count, frames;
	int note_volume[8];
};

static void loop_dispatch(void *context, const void *event)
{
	struct loop_probe *probe = context;
	const struct test_event *e = event;
	if (e->type == TEST_CONTROL && e->first == 7)
		probe->volume = e->second;
	if (e->type == TEST_NOTE_ON && probe->note_count < 8)
		probe->note_volume[probe->note_count++] = probe->volume;
}

static void loop_render(void *context, short *output, int frames)
{
	struct loop_probe *probe = context;
	(void) output;
	probe->frames += frames;
}

static int test_loop_range(void)
{
	struct test_event events[] = {
		{ 0, TEST_CONTROL, 4, 7, 0, NULL },
		{ 100, TEST_NOTE_ON, 4, 60, 100, NULL },
		{ 200, TEST_CONTROL, 4, 7, 127, NULL },
		{ 300, TEST_NOTE_ON, 4, 62, 100, NULL },
		{ 400, TEST_PROGRAM, 4, 0, 0, NULL },
		{ 500, TEST_NOTE_ON, 4, 60, 100, NULL }
	};
	const struct midi_seek_timeline_ops ops = {
		test_event_time_ms, test_event_next, loop_dispatch, loop_render
	};
	struct midi_seek_timeline timeline;
	struct loop_probe probe = { 0 };
	int i;
	for (i = 0; i < 5; i++) events[i].next = &events[i + 1];
	midi_seek_timeline_init(&timeline, events, 1000, 2, &probe, &ops);
	if (!midi_seek_timeline_set_range(&timeline, &events[4], 400, 800) ||
	    midi_seek_timeline_render(&timeline, NULL, 1600) != 1600 ||
	    probe.frames != 1600 || probe.note_count != 5 || probe.note_volume[0] != 0)
		return 0;
	for (i = 1; i < 5; i++)
		if (probe.note_volume[i] != 127) return 0;
	/* The last event is at 500 ms; the finite range must still render to 800 */
	memset(&probe, 0, sizeof(probe));
	midi_seek_timeline_init(&timeline, events, 1000, 2, &probe, &ops);
	if (!midi_seek_timeline_set_range(&timeline, NULL, 0, 800) ||
	    midi_seek_timeline_render(&timeline, NULL, 1000) != 800 ||
	    midi_seek_timeline_render(&timeline, NULL, 1000) != 0 || probe.frames != 800)
		return 0;
	return !midi_seek_timeline_set_range(&timeline, events, 800, 800);
}

static int test_hmp_song_state(const unsigned char *soundfont, int size)
{
	tsf *synth = tsf_load_memory(soundfont, size);
	struct hmp_tsf_state state = { 0 };
	int preset, ok;
	float pan;
	if (!synth) return 0;
	configure_synth(synth, 48000);
	tsf_channel_set_presetnumber(synth, 4, 39, 0);
	tsf_channel_midi_control(synth, 4, 10, 47);
	tsf_channel_midi_control(synth, 4, 42, 31);
	tsf_channel_midi_control(synth, 4, 7, 127);
	preset = tsf_channel_get_preset_index(synth, 4);
	pan = tsf_channel_get_pan(synth, 4);
	hmp_tsf_capture(synth, &state);
	tsf_reset(synth);
	hmp_tsf_begin(synth, &state);
	ok = tsf_channel_get_preset_index(synth, 4) == preset &&
	     tsf_channel_get_pan(synth, 4) == pan &&
	     tsf_channel_get_volume(synth, 4) < 0.0001f;
	if (!ok)
		fprintf(stderr, "HMP state preset %d/%d pan %.9f/%.9f raw %u volume %.9f\n", preset,
		        tsf_channel_get_preset_index(synth, 4), pan, tsf_channel_get_pan(synth, 4),
		        state.pan[4], tsf_channel_get_volume(synth, 4));
	/* A subsequent LSB update must combine with the retained pan MSB */
	tsf_channel_midi_control(synth, 4, 42, 32);
	ok = ok && tsf_channel_get_pan(synth, 4) > pan && tsf_channel_get_pan(synth, 4) < pan + 0.0001f;
	/* Cover the pinned getter's endpoint and unallocated-channel behavior */
	tsf_channel_midi_control(synth, 0, 10, 0);
	tsf_channel_midi_control(synth, 0, 42, 0);
	tsf_channel_midi_control(synth, 15, 10, 127);
	tsf_channel_midi_control(synth, 15, 42, 127);
	hmp_tsf_capture(synth, &state);
	ok = ok && state.pan[0] == 0 && state.pan[15] == 16383;
	tsf_close(synth);
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
	if (!test_loop_range()) {
		fprintf(stderr, "MIDI loop state/end range test failed\n");
		return 1;
	}

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
	if (!test_hmp_song_state(soundfont, soundfont_size)) {
		fprintf(stderr, "HMP transition state test failed\n");
		free(soundfont);
		return 1;
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
