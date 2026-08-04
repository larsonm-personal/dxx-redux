#include "saf_manifest_parser.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	const char *cursor;
	const char *end;
} JsonParser;

static void skip_ws(JsonParser *parser)
{
	while (parser->cursor < parser->end &&
	       (*parser->cursor == ' ' || *parser->cursor == '\t' ||
	        *parser->cursor == '\n' || *parser->cursor == '\r'))
		parser->cursor++;
}

static int consume(JsonParser *parser, char expected)
{
	skip_ws(parser);
	if (parser->cursor == parser->end || *parser->cursor != expected) return 0;
	parser->cursor++;
	return 1;
}

static int parse_hex4(JsonParser *parser, uint32_t *value)
{
	uint32_t result = 0;
	for (int i = 0; i < 4; i++) {
		if (parser->cursor == parser->end) return 0;
		const unsigned char c = (unsigned char) *parser->cursor++;
		uint32_t digit;
		if (c >= '0' && c <= '9') digit = c - '0';
		else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
		else return 0;
		result = result * 16 + digit;
	}
	*value = result;
	return 1;
}

static size_t append_utf8(char *output, size_t offset, uint32_t codepoint)
{
	if (codepoint <= 0x7f) {
		output[offset++] = (char) codepoint;
	} else if (codepoint <= 0x7ff) {
		output[offset++] = (char) (0xc0 | (codepoint >> 6));
		output[offset++] = (char) (0x80 | (codepoint & 0x3f));
	} else if (codepoint <= 0xffff) {
		output[offset++] = (char) (0xe0 | (codepoint >> 12));
		output[offset++] = (char) (0x80 | ((codepoint >> 6) & 0x3f));
		output[offset++] = (char) (0x80 | (codepoint & 0x3f));
	} else {
		output[offset++] = (char) (0xf0 | (codepoint >> 18));
		output[offset++] = (char) (0x80 | ((codepoint >> 12) & 0x3f));
		output[offset++] = (char) (0x80 | ((codepoint >> 6) & 0x3f));
		output[offset++] = (char) (0x80 | (codepoint & 0x3f));
	}
	return offset;
}

static char *parse_json_string(JsonParser *parser)
{
	skip_ws(parser);
	if (parser->cursor == parser->end || *parser->cursor++ != '"') return NULL;
	const size_t capacity = (size_t) (parser->end - parser->cursor) + 1;
	char *result = (char *) malloc(capacity);
	if (!result) return NULL;
	size_t length = 0;

	while (parser->cursor < parser->end) {
		const unsigned char c = (unsigned char) *parser->cursor++;
		if (c == '"') {
			result[length] = '\0';
			return result;
		}
		if (c < 0x20) goto fail;
		if (c != '\\') {
			result[length++] = (char) c;
			continue;
		}
		if (parser->cursor == parser->end) goto fail;
		switch (*parser->cursor++) {
			case '"': result[length++] = '"'; break;
			case '\\': result[length++] = '\\'; break;
			case '/': result[length++] = '/'; break;
			case 'b': result[length++] = '\b'; break;
			case 'f': result[length++] = '\f'; break;
			case 'n': result[length++] = '\n'; break;
			case 'r': result[length++] = '\r'; break;
			case 't': result[length++] = '\t'; break;
			case 'u': {
				uint32_t codepoint;
				if (!parse_hex4(parser, &codepoint) || codepoint == 0) goto fail;
				if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
					uint32_t low;
					if ((parser->end - parser->cursor) < 2 || parser->cursor[0] != '\\' ||
					    parser->cursor[1] != 'u')
						goto fail;
					parser->cursor += 2;
					if (!parse_hex4(parser, &low) || low < 0xdc00 || low > 0xdfff) goto fail;
					codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + (low - 0xdc00);
				} else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
					goto fail;
				}
				length = append_utf8(result, length, codepoint);
				break;
			}
			default: goto fail;
		}
	}

fail:
	free(result);
	return NULL;
}

static int parse_nonnegative_int64(JsonParser *parser, int64_t *value)
{
	skip_ws(parser);
	if (parser->cursor == parser->end || *parser->cursor < '0' || *parser->cursor > '9') return 0;
	uint64_t result = 0;
	if (*parser->cursor == '0' && parser->cursor + 1 < parser->end &&
	    parser->cursor[1] >= '0' && parser->cursor[1] <= '9')
		return 0;
	while (parser->cursor < parser->end && *parser->cursor >= '0' && *parser->cursor <= '9') {
		const uint64_t digit = (uint64_t) (*parser->cursor - '0');
		if (result > ((uint64_t) INT64_MAX - digit) / 10) return 0;
		result = result * 10 + digit;
		parser->cursor++;
	}
	*value = (int64_t) result;
	return 1;
}

int saf_manifest_name_equals(const char *left, const char *right)
{
	while (*left && *right) {
		unsigned char a = (unsigned char) *left++;
		unsigned char b = (unsigned char) *right++;
		if (a >= 'A' && a <= 'Z') a = (unsigned char) (a - 'A' + 'a');
		if (b >= 'A' && b <= 'Z') b = (unsigned char) (b - 'A' + 'a');
		if (a != b) return 0;
	}
	return *left == *right;
}

static int filename_is_duplicate(const SafManifestData *manifest, const char *filename)
{
	for (int i = 0; i < manifest->count; i++) {
		if (saf_manifest_name_equals(manifest->entries[i].filename, filename)) return 1;
	}
	return 0;
}

static int append_entry(SafManifestData *manifest, int *capacity, SafManifestEntry *entry)
{
	if (manifest->count >= *capacity) {
		if (*capacity > INT_MAX / 2) return 0;
		*capacity *= 2;
		SafManifestEntry *grown = (SafManifestEntry *) realloc(
		    manifest->entries, (size_t) *capacity * sizeof(SafManifestEntry));
		if (!grown) return 0;
		manifest->entries = grown;
	}
	manifest->entries[manifest->count++] = *entry;
	return 1;
}

static int parse_entry(JsonParser *parser, SafManifestData *manifest, int *capacity)
{
	SafManifestEntry entry = { NULL, NULL, -1 };
	int saw_filename = 0;
	int saw_content_uri = 0;
	int saw_size_bytes = 0;
	if (!consume(parser, '{')) return 0;

	skip_ws(parser);
	if (parser->cursor < parser->end && *parser->cursor != '}') {
		for (;;) {
			char *key = parse_json_string(parser);
			if (!key || !consume(parser, ':')) {
				free(key);
				goto fail;
			}
			if (strcmp(key, "filename") == 0 && !saw_filename) {
				saw_filename = 1;
				entry.filename = parse_json_string(parser);
				if (!entry.filename || entry.filename[0] == '\0') {
					free(key);
					goto fail;
				}
			} else if (strcmp(key, "content_uri") == 0 && !saw_content_uri) {
				saw_content_uri = 1;
				entry.content_uri = parse_json_string(parser);
				if (!entry.content_uri || entry.content_uri[0] == '\0') {
					free(key);
					goto fail;
				}
			} else if (strcmp(key, "size_bytes") == 0 && !saw_size_bytes) {
				saw_size_bytes = 1;
				if (!parse_nonnegative_int64(parser, &entry.size_bytes)) {
					free(key);
					goto fail;
				}
			} else {
				free(key);
				goto fail;
			}
			free(key);
			skip_ws(parser);
			if (parser->cursor == parser->end) goto fail;
			if (*parser->cursor == '}') break;
			if (*parser->cursor++ != ',') goto fail;
			skip_ws(parser);
			if (parser->cursor == parser->end || *parser->cursor == '}') goto fail;
		}
	}
	if (!consume(parser, '}') || !saw_filename || !saw_content_uri || !saw_size_bytes ||
	    filename_is_duplicate(manifest, entry.filename) || !append_entry(manifest, capacity, &entry))
		goto fail;
	return 1;

fail:
	free(entry.filename);
	free(entry.content_uri);
	return 0;
}

SafManifestData *saf_manifest_parse(const char *json, size_t json_len)
{
	SafManifestData *manifest = (SafManifestData *) calloc(1, sizeof(SafManifestData));
	if (!manifest) return NULL;
	int capacity = 16;
	manifest->entries = (SafManifestEntry *) calloc((size_t) capacity, sizeof(SafManifestEntry));
	if (!manifest->entries) {
		free(manifest);
		return NULL;
	}

	JsonParser parser = { json, json + json_len };
	if (!consume(&parser, '{')) goto fail;
	char *root_key = parse_json_string(&parser);
	if (!root_key || strcmp(root_key, "files") != 0 || !consume(&parser, ':') ||
	    !consume(&parser, '[')) {
		free(root_key);
		goto fail;
	}
	free(root_key);

	skip_ws(&parser);
	if (parser.cursor < parser.end && *parser.cursor != ']') {
		for (;;) {
			if (!parse_entry(&parser, manifest, &capacity)) goto fail;
			skip_ws(&parser);
			if (parser.cursor == parser.end) goto fail;
			if (*parser.cursor == ']') break;
			if (*parser.cursor++ != ',') goto fail;
			skip_ws(&parser);
			if (parser.cursor == parser.end || *parser.cursor == ']') goto fail;
		}
	}
	if (!consume(&parser, ']') || !consume(&parser, '}')) goto fail;
	skip_ws(&parser);
	if (parser.cursor != parser.end) goto fail;
	return manifest;

fail:
	saf_manifest_free(manifest);
	return NULL;
}

void saf_manifest_free(SafManifestData *manifest)
{
	if (!manifest) return;
	for (int i = 0; i < manifest->count; i++) {
		free(manifest->entries[i].filename);
		free(manifest->entries[i].content_uri);
	}
	free(manifest->entries);
	free(manifest);
}
