#ifndef DXX_REDUX_SAF_MANIFEST_PARSER_H
#define DXX_REDUX_SAF_MANIFEST_PARSER_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	char *filename;
	char *content_uri;
	int64_t size_bytes;
} SafManifestEntry;

typedef struct {
	SafManifestEntry *entries;
	int count;
} SafManifestData;

SafManifestData *saf_manifest_parse(const char *json, size_t json_len);
void saf_manifest_free(SafManifestData *manifest);
int saf_manifest_name_equals(const char *left, const char *right);

#endif
