#ifndef PLAYSAVE_TEXT_H
#define PLAYSAVE_TEXT_H

#include <stddef.h>

struct playsave_text_entry {
	const char *key;
	const char *line;
};

int playsave_text_update_section(const char *path, const char *options_header,
                                 const char *section_header, const struct playsave_text_entry *entries,
                                 size_t entry_count);

#endif
