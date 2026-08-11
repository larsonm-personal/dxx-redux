#ifndef MUSIC_TRACK_JSON_H
#define MUSIC_TRACK_JSON_H

#include <stddef.h>

typedef struct music_track_json_writer {
	char *buffer;
	size_t capacity;
	size_t length;
	size_t count;
	int failed;
} music_track_json_writer;

void music_track_json_begin(music_track_json_writer *writer, char *buffer, size_t capacity);
void music_track_json_add(music_track_json_writer *writer, int index, const char *name);
int music_track_json_finish(music_track_json_writer *writer);

#endif
