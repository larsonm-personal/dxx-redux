#ifndef DXX_MIDI_METADATA_H
#define DXX_MIDI_METADATA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_METADATA_MAX_INPUT_BYTES (64u * 1024u * 1024u)
#define MIDI_METADATA_MAX_EVENTS      256u
#define MIDI_METADATA_MAX_EVENT_BYTES 1024u
#define MIDI_METADATA_MAX_TEXT_BYTES  (64u * 1024u)
#define MIDI_METADATA_TITLE_BYTES     256u
#define MIDI_METADATA_COMPOSER_BYTES  256u
#define MIDI_METADATA_DISPLAY_BYTES   80u

enum midi_metadata_status {
	MIDI_METADATA_OK = 1,
	MIDI_METADATA_INVALID = -1,
	MIDI_METADATA_LIMIT = -2,
	MIDI_METADATA_ALLOCATION = -3,
	MIDI_METADATA_CONVERSION = -4
};

typedef struct midi_metadata_text_event {
	unsigned int track_index;
	unsigned int type;
	char *text;
} midi_metadata_text_event;

typedef struct midi_metadata {
	int status;
	unsigned int smf_format;
	unsigned int track_count;
	unsigned int time_division;
	int duration_ms;
	unsigned int event_count;
	unsigned int metadata_truncated;
	size_t retained_text_bytes;
	midi_metadata_text_event *events;
	char title[MIDI_METADATA_TITLE_BYTES];
	char composer[MIDI_METADATA_COMPOSER_BYTES];
	char display_name[MIDI_METADATA_DISPLAY_BYTES];
} midi_metadata;

void midi_metadata_init(midi_metadata *metadata);
void midi_metadata_free(midi_metadata *metadata);
int midi_metadata_parse(const unsigned char *data, size_t length, int is_hmp,
                        midi_metadata *metadata);
int midi_metadata_has_text(const midi_metadata *metadata);
int midi_metadata_has_useful_summary(const midi_metadata *metadata);
const char *midi_metadata_status_name(int status);
const char *midi_metadata_event_type_name(unsigned int type);
char *midi_metadata_to_json(const midi_metadata *metadata,
                            const char *metadata_source_filename,
                            int inherited_from_midi);

#ifdef __cplusplus
}
#endif

#endif
