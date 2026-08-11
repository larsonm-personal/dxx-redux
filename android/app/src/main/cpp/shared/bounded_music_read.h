#ifndef BOUNDED_MUSIC_READ_H
#define BOUNDED_MUSIC_READ_H

#include <stdint.h>
#include <stddef.h>

typedef int64_t (*bounded_music_read_callback)(void *context,
                                               unsigned char *destination, size_t maximum);

static inline int bounded_music_read_exact(bounded_music_read_callback callback,
                                           void *context, unsigned char *destination, size_t size)
{
	size_t offset = 0;

	if (!callback || !destination || !size)
		return 0;
	while (offset < size) {
		int64_t amount = callback(context, destination + offset, size - offset);

		if (amount <= 0 || (uint64_t) amount > size - offset)
			return 0;
		offset += (size_t) amount;
	}
	return 1;
}

#endif
