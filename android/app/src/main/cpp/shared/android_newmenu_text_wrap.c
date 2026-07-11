#include "android_newmenu_text_wrap.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "strutil.h"
#include "u_mem.h"

static void trim_copy(char *dst, size_t dst_size, const char *src, size_t src_len)
{
	while (src_len > 0 && isspace((unsigned char) *src)) {
		src++;
		src_len--;
	}
	while (src_len > 0 && isspace((unsigned char) src[src_len - 1]))
		src_len--;
	if (src_len >= dst_size)
		src_len = dst_size - 1;
	memcpy(dst, src, src_len);
	dst[src_len] = 0;
}

void android_newmenu_free_text_items(newmenu_item *items, int count)
{
	int i;

	for (i = 0; i < count; i++)
		d_free(items[i].text);
	d_free(items);
}

static int append_wrapped_line(newmenu_item **items, int *count,
                               int *capacity, const char *text)
{
	newmenu_item *grown;

	if (*count >= *capacity) {
		*capacity *= 2;
		grown = (newmenu_item *) d_realloc(*items, sizeof(newmenu_item) * *capacity);
		if (!grown)
			return 0;
		*items = grown;
	}

	memset(&(*items)[*count], 0, sizeof(newmenu_item));
	(*items)[*count].type = NM_TYPE_TEXT;
	(*items)[*count].text = d_strdup((char *) text);
	if (!(*items)[*count].text)
		return 0;
	(*count)++;
	return 1;
}

static int wrap_words(newmenu_item **items, int *count, int *capacity,
                      const char *first_prefix, const char *next_prefix, const char *text,
                      int wrap_width, android_newmenu_measure_text measure_text)
{
	const char *p = text;
	const char *prefix = first_prefix;
	char line[NM_MAX_TEXT_LEN + 1];
	char candidate[NM_MAX_TEXT_LEN + 1];
	char word[NM_MAX_TEXT_LEN + 1];

	if (!text || !*text)
		return append_wrapped_line(items, count, capacity, first_prefix);

	snprintf(line, sizeof(line), "%s", prefix);
	while (*p) {
		size_t len = 0;
		while (*p && isspace((unsigned char) *p))
			p++;
		while (p[len] && !isspace((unsigned char) p[len]) && len < sizeof(word) - 1) {
			word[len] = p[len];
			len++;
		}
		word[len] = 0;
		p += len;
		if (!word[0])
			break;

		snprintf(candidate, sizeof(candidate), "%s%s%s", line,
		         strlen(line) > strlen(prefix) ? " " : "", word);
		if (measure_text(candidate) <= wrap_width || strlen(line) == strlen(prefix)) {
			snprintf(line, sizeof(line), "%s", candidate);
		} else {
			if (!append_wrapped_line(items, count, capacity, line))
				return 0;
			prefix = next_prefix;
			snprintf(line, sizeof(line), "%s%s", prefix, word);
		}
	}

	return append_wrapped_line(items, count, capacity, line);
}

static int wrap_text_item(newmenu_item **items, int *count, int *capacity,
                          const char *text, int wrap_width, android_newmenu_measure_text measure_text)
{
	char key[NM_MAX_TEXT_LEN + 1];
	char body[NM_MAX_TEXT_LEN + 1];
	char first_prefix[NM_MAX_TEXT_LEN + 1];
	const char *tab;

	if (!text || !*text)
		return append_wrapped_line(items, count, capacity, "");

	tab = strchr(text, '\t');
	if (!tab)
		return wrap_words(items, count, capacity, "", "  ", text,
		                  wrap_width, measure_text);

	trim_copy(key, sizeof(key), text, tab - text);
	trim_copy(body, sizeof(body), tab + 1, strlen(tab + 1));
	if (!body[0])
		return append_wrapped_line(items, count, capacity, key);

	snprintf(first_prefix, sizeof(first_prefix), "%s  ", key);
	return wrap_words(items, count, capacity, first_prefix, "    ", body,
	                  wrap_width, measure_text);
}

int android_newmenu_wrap_text_items(const newmenu_item *source, int source_count,
                                    int wrap_width, android_newmenu_measure_text measure_text,
                                    newmenu_item **wrapped, int *wrapped_count)
{
	int i, count = 0;
	int capacity = source_count * 2 + 8;
	newmenu_item *items;

	if (!source || source_count <= 0 || !measure_text || !wrapped || !wrapped_count)
		return 0;

	items = (newmenu_item *) d_malloc(sizeof(newmenu_item) * capacity);
	if (!items)
		return 0;

	for (i = 0; i < source_count; i++) {
		if (!wrap_text_item(&items, &count, &capacity, source[i].text,
		                    wrap_width, measure_text)) {
			android_newmenu_free_text_items(items, count);
			return 0;
		}
	}

	*wrapped = items;
	*wrapped_count = count;
	return 1;
}
