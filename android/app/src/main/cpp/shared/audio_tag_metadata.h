#ifndef DXX_AUDIO_TAG_METADATA_H
#define DXX_AUDIO_TAG_METADATA_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_TAG_TEXT_BYTES       256
#define AUDIO_TAG_DISPLAY_BYTES    96
#define AUDIO_TAG_MAX_PROPERTIES   64
#define AUDIO_TAG_MAX_VALUES       8
#define AUDIO_TAG_MAX_VALUE_BYTES  1024
#define AUDIO_TAG_MAX_TOTAL_TEXT   (32 * 1024)
#define AUDIO_TAG_MAX_SOURCE_BYTES (256 * 1024 * 1024)

typedef enum audio_tag_status {
	AUDIO_TAG_STATUS_OK = 0,
	AUDIO_TAG_STATUS_NO_TAGS,
	AUDIO_TAG_STATUS_UNSUPPORTED,
	AUDIO_TAG_STATUS_TRUNCATED,
	AUDIO_TAG_STATUS_INVALID,
	AUDIO_TAG_STATUS_IO_ERROR
} audio_tag_status;

typedef struct audio_tag_property {
	char *key;
	char **values;
	unsigned int value_count;
} audio_tag_property;

typedef struct audio_tag_metadata {
	audio_tag_status status;
	char format[8];
	char title[AUDIO_TAG_TEXT_BYTES];
	char composer[AUDIO_TAG_TEXT_BYTES];
	char artist[AUDIO_TAG_TEXT_BYTES];
	char album_artist[AUDIO_TAG_TEXT_BYTES];
	char album[AUDIO_TAG_TEXT_BYTES];
	char date[AUDIO_TAG_TEXT_BYTES];
	char genre[AUDIO_TAG_TEXT_BYTES];
	char comment[AUDIO_TAG_TEXT_BYTES];
	char copyright[AUDIO_TAG_TEXT_BYTES];
	char track_number[32];
	char disc_number[32];
	char display_name[AUDIO_TAG_DISPLAY_BYTES];
	int duration_ms;
	audio_tag_property *properties;
	unsigned int property_count;
	int metadata_truncated;
} audio_tag_metadata;

void audio_tag_metadata_init(audio_tag_metadata *metadata);
void audio_tag_metadata_free(audio_tag_metadata *metadata);
int audio_tag_metadata_parse_path(const char *path, const char *extension,
                                  audio_tag_metadata *metadata);
int audio_tag_metadata_parse_physfs(const char *filename,
                                    audio_tag_metadata *metadata);
const char *audio_tag_status_name(audio_tag_status status);
char *audio_tag_metadata_to_json(const audio_tag_metadata *metadata);

#ifdef __cplusplus
}
#endif

#endif
