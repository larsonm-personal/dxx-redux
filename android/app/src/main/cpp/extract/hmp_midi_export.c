/* Host export of the same HMP conversion and MIDI loader used by Android */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hmp.h"
#include "hmp_android_shared.h"
#include "tml.h"

void *test_hmp_calloc(size_t n, size_t size)
{
	return calloc(n, size);
}
void *test_hmp_malloc(size_t size)
{
	return malloc(size);
}
void *test_hmp_realloc(void *p, size_t size)
{
	return realloc(p, size);
}
void test_hmp_free(void *p)
{
	free(p);
}
void hmp_close(hmp_file *hmp)
{
	int i;
	for (i = 0; i < HMP_TRACKS; i++) free(hmp->trks[i].data);
	free(hmp);
}

static void write_delta(FILE *file, unsigned int delta)
{
	unsigned char bytes[5];
	int n = 5;
	bytes[--n] = delta & 127;
	while ((delta >>= 7) != 0) bytes[--n] = (delta & 127) | 128;
	fwrite(bytes + n, 1, (size_t) (5 - n), file);
}

static int export_loaded_events(const char *path, const tml_message *message, double end_ms)
{
	static const unsigned char header[] = {
		'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 0, 0, 1, 1, 0xf4,
		'M', 'T', 'r', 'k', 0, 0, 0, 0
	};
	unsigned int last = 0, end = (unsigned int) (end_ms + 0.999999);
	long bytes;
	int ok;
	char *name = malloc(strlen(path) + 9);
	FILE *file;
	if (!name) return 0;
	sprintf(name, "%s.tml.mid", path);
	file = fopen(name, "wb");
	free(name);
	if (!file) return 0;
	fwrite(header, 1, sizeof(header), file);
	for (; message; message = message->next) {
		if (message->type < 0x80 || message->type >= 0xf0) continue;
		write_delta(file, message->time - last);
		last = message->time;
		fputc(message->type | message->channel, file);
		if (message->type == TML_PITCH_BEND) {
			fputc(message->pitch_bend & 127, file);
			fputc(message->pitch_bend >> 7, file);
		} else {
			fputc((unsigned char) message->key, file);
			if ((message->type & 0xe0) != 0xc0)
				fputc((unsigned char) message->velocity, file);
		}
	}
	write_delta(file, end > last ? end - last : 0);
	fwrite("\xff\x2f\0", 1, 3, file);
	bytes = ftell(file) - 22;
	ok = !ferror(file) && bytes > 0 && !fseek(file, 18, SEEK_SET);
	if (ok) {
		fputc((bytes >> 24) & 255, file);
		fputc((bytes >> 16) & 255, file);
		fputc((bytes >> 8) & 255, file);
		fputc(bytes & 255, file);
		ok = !ferror(file);
	}
	if (fclose(file)) ok = 0;
	return ok;
}

int main(int argc, char **argv)
{
	FILE *file;
	long size;
	unsigned char *data, *midi = NULL;
	int midi_len = 0, ok;
	struct hmp_playback_info info = { 0 };
	tml_message *messages;
	if (argc < 3 || argc > 4) {
		fprintf(stderr, "Usage: hmp_midi_export input.hmp output.mid [once|repeat|legacy]\n");
		return 2;
	}
	if (argc == 4 && strcmp(argv[3], "once") && strcmp(argv[3], "repeat") && strcmp(argv[3], "legacy"))
		return 2;
	file = fopen(argv[1], "rb");
	if (!file) return 2;
	if (fseek(file, 0, SEEK_END) || (size = ftell(file)) <= 0 || size > 16 * 1024 * 1024 || fseek(file, 0, SEEK_SET)) {
		fclose(file);
		return 2;
	}
	data = malloc((size_t) size);
	if (!data || fread(data, 1, (size_t) size, file) != (size_t) size) {
		fclose(file);
		free(data);
		return 2;
	}
	fclose(file);
	if (argc == 4 && !strcmp(argv[3], "legacy"))
		ok = hmp2mid_mem(data, (int) size, &midi, &midi_len);
	else
		ok = hmp2mid_playback_mem(data, (int) size, argc == 4 && !strcmp(argv[3], "repeat"), &midi, &midi_len, &info);
	free(data);
	if (!ok) {
		fprintf(stderr, "HMP playback conversion failed\n");
		return 1;
	}
	messages = tml_load_memory(midi, midi_len);
	if (!messages) {
		free(midi);
		return 1;
	}
	file = fopen(argv[2], "wb");
	ok = file && fwrite(midi, 1, (size_t) midi_len, file) == (size_t) midi_len;
	if (file && fclose(file)) ok = 0;
	if (ok) ok = export_loaded_events(argv[2], messages, info.end_ms);
	tml_free(messages);
	free(midi);
	printf("{\"repeat_ms\":%.6f,\"end_ms\":%.6f,\"branch_loop\":%d,\"unsupported_branches\":%d}\n",
	       info.repeat_ms, info.end_ms, info.branch_loop, info.unsupported_branches);
	return ok ? 0 : 1;
}
