#include "midi_seek_timeline.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

static uint64_t midi_seek_event_frame(const struct midi_seek_timeline *timeline)
{
	uint64_t time_ms = timeline->ops.event_time_ms(timeline->event);
	return (time_ms * (uint64_t) timeline->sample_rate + 999u) / 1000u;
}

static void midi_seek_timeline_dispatch_due(struct midi_seek_timeline *timeline)
{
	while (timeline->event && midi_seek_event_frame(timeline) <= timeline->frame) {
		const void *event = timeline->event;
		timeline->ops.dispatch_event(timeline->context, event);
		timeline->event = timeline->ops.event_next(event);
	}
}

void midi_seek_timeline_init(struct midi_seek_timeline *timeline,
                             const void *first_event, int sample_rate,
                             int channels, void *context,
                             const struct midi_seek_timeline_ops *ops)
{
	if (!timeline)
		return;
	timeline->event = first_event;
	timeline->frame = 0;
	timeline->sample_rate = sample_rate;
	timeline->channels = channels;
	timeline->context = context;
	if (ops)
		timeline->ops = *ops;
}

uint64_t midi_seek_timeline_frame_for_ms(double time_ms, int sample_rate)
{
	double exact_frames;
	uint64_t frames;

	if (time_ms <= 0.0 || sample_rate <= 0)
		return 0;
	exact_frames = time_ms * (double) sample_rate / 1000.0;
	frames = (uint64_t) exact_frames;
	if ((double) frames < exact_frames)
		frames++;
	return frames;
}

double midi_seek_timeline_position_ms(const struct midi_seek_timeline *timeline)
{
	if (!timeline || timeline->sample_rate <= 0)
		return 0.0;
	return (double) timeline->frame * 1000.0 / (double) timeline->sample_rate;
}

int midi_seek_timeline_render(struct midi_seek_timeline *timeline,
                              short *output, int frames)
{
	int rendered = 0;

	if (!timeline || frames < 0 || timeline->sample_rate <= 0 ||
	    timeline->channels <= 0 || !timeline->ops.event_time_ms ||
	    !timeline->ops.event_next || !timeline->ops.dispatch_event ||
	    !timeline->ops.render_frames)
		return 0;

	while (frames > 0) {
		uint64_t next_frame;
		uint64_t available;
		int block;

		midi_seek_timeline_dispatch_due(timeline);
		if (!timeline->event)
			break;
		next_frame = midi_seek_event_frame(timeline);
		available = next_frame - timeline->frame;
		block = available > (uint64_t) INT_MAX ? INT_MAX : (int) available;
		if (block > frames)
			block = frames;
		if (block <= 0)
			continue;
		timeline->ops.render_frames(timeline->context, output, block);
		timeline->frame += (uint64_t) block;
		if (output)
			output += block * timeline->channels;
		frames -= block;
		rendered += block;
	}
	midi_seek_timeline_dispatch_due(timeline);
	return rendered;
}

int midi_seek_timeline_reconstruct(struct midi_seek_timeline *timeline,
                                   uint64_t target_frame, short *block_buffer,
                                   int block_frames, int *prefill_frames)
{
	if (!timeline || !block_buffer || block_frames <= 0 || !prefill_frames)
		return 0;
	*prefill_frames = 0;
	while (timeline->frame < target_frame) {
		uint64_t remaining = target_frame - timeline->frame;
		int required = remaining < (uint64_t) block_frames ? (int) remaining : block_frames;
		int got = midi_seek_timeline_render(timeline, block_buffer, block_frames);

		if (got < required)
			return 0;
		if (remaining < (uint64_t) block_frames) {
			*prefill_frames = got - required;
			if (*prefill_frames > 0)
				memmove(block_buffer,
				        block_buffer + required * timeline->channels,
				        (size_t) *prefill_frames * timeline->channels * sizeof(*block_buffer));
			return 1;
		}
	}
	return 1;
}
