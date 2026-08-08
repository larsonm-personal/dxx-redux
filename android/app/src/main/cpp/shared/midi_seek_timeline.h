#ifndef DXX_REDUX_MIDI_SEEK_TIMELINE_H
#define DXX_REDUX_MIDI_SEEK_TIMELINE_H

#include <stdint.h>

struct midi_seek_timeline_ops {
	unsigned int (*event_time_ms)(const void *event);
	const void *(*event_next)(const void *event);
	void (*dispatch_event)(void *context, const void *event);
	void (*render_frames)(void *context, short *output, int frames);
};

struct midi_seek_timeline {
	const void *event;
	uint64_t frame;
	int sample_rate;
	int channels;
	void *context;
	struct midi_seek_timeline_ops ops;
};

void midi_seek_timeline_init(struct midi_seek_timeline *timeline,
                             const void *first_event, int sample_rate,
                             int channels, void *context,
                             const struct midi_seek_timeline_ops *ops);
uint64_t midi_seek_timeline_frame_for_ms(double time_ms, int sample_rate);
double midi_seek_timeline_position_ms(const struct midi_seek_timeline *timeline);
int midi_seek_timeline_render(struct midi_seek_timeline *timeline,
                              short *output, int frames);
int midi_seek_timeline_reconstruct(struct midi_seek_timeline *timeline,
                                   uint64_t target_frame, short *block_buffer,
                                   int block_frames, int *prefill_frames);

#endif
