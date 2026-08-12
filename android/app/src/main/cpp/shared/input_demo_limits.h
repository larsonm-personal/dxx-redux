#ifndef INPUT_DEMO_LIMITS_H
#define INPUT_DEMO_LIMITS_H

#include <stdint.h>

#define INPUT_DEMO_CHECKPOINT_MAX_BYTES         (2u * 1024u * 1024u)
#define INPUT_DEMO_CHECKPOINT_MAX_ENCODED_BYTES ((((INPUT_DEMO_CHECKPOINT_MAX_BYTES) + 2u) / 3u) * 4u)
#define INPUT_DEMO_CHECKPOINT_MAX_EXPANSION     1024u
#define INPUT_DEMO_FILE_MAX_BYTES               (128u * 1024u * 1024u)

static inline int input_demo_checkpoint_size_supported(uint64_t size)
{
	return size <= INPUT_DEMO_CHECKPOINT_MAX_BYTES;
}

static inline int input_demo_checkpoint_encoded_size_supported(uint64_t size)
{
	return size <= INPUT_DEMO_CHECKPOINT_MAX_ENCODED_BYTES;
}

static inline int input_demo_checkpoint_expansion_supported(uint64_t expanded_size,
                                                            uint64_t compressed_size)
{
	if (!expanded_size || !compressed_size)
		return 0;
	return expanded_size / compressed_size < INPUT_DEMO_CHECKPOINT_MAX_EXPANSION ||
	       (expanded_size / compressed_size == INPUT_DEMO_CHECKPOINT_MAX_EXPANSION &&
	        expanded_size % compressed_size == 0);
}

static inline int input_demo_file_size_supported(uint64_t size)
{
	return size <= INPUT_DEMO_FILE_MAX_BYTES;
}

static inline int input_demo_level_in_mission(int level,
                                              int last_level,
                                              int last_secret_level)
{
	if (level > 0)
		return level <= last_level;
	return level < 0 && level >= last_secret_level;
}

#endif
