#ifndef CLASSIC_DEMO_STRING_H
#define CLASSIC_DEMO_STRING_H

#include <stddef.h>
#include <string.h>

typedef int (*classic_demo_string_read_fn)(void *context, void *buffer, size_t size);

typedef enum classic_demo_string_status {
	CLASSIC_DEMO_STRING_OK = 0,
	CLASSIC_DEMO_STRING_INVALID_LENGTH,
	CLASSIC_DEMO_STRING_TOO_LONG,
	CLASSIC_DEMO_STRING_UNTERMINATED,
	CLASSIC_DEMO_STRING_TRUNCATED
} classic_demo_string_status;

static inline classic_demo_string_status classic_demo_read_counted_string(
    classic_demo_string_read_fn read_fn, void *context,
    char *destination, size_t destination_size, unsigned int *encoded_length)
{
	unsigned char length;
	unsigned char payload[255];

	if (encoded_length)
		*encoded_length = 0;
	if (!read_fn || !destination || !destination_size)
		return CLASSIC_DEMO_STRING_INVALID_LENGTH;
	if (!read_fn(context, &length, 1))
		return CLASSIC_DEMO_STRING_TRUNCATED;
	if (encoded_length)
		*encoded_length = length;
	if (!length)
		return CLASSIC_DEMO_STRING_INVALID_LENGTH;
	if (!read_fn(context, payload, length))
		return CLASSIC_DEMO_STRING_TRUNCATED;
	if ((size_t) length > destination_size)
		return CLASSIC_DEMO_STRING_TOO_LONG;
	if (payload[length - 1] != '\0')
		return CLASSIC_DEMO_STRING_UNTERMINATED;
	memcpy(destination, payload, length);
	return CLASSIC_DEMO_STRING_OK;
}

#endif
