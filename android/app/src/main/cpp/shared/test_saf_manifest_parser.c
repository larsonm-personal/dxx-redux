#include "saf_manifest_parser.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                         \
	do {                                                                         \
		if (!(condition)) {                                                      \
			fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
			return 0;                                                            \
		}                                                                        \
	} while (0)

static int accepts(const char *json)
{
	SafManifestData *manifest = saf_manifest_parse(json, strlen(json));
	if (!manifest) return 0;
	saf_manifest_free(manifest);
	return 1;
}

static int test_valid_manifests(void)
{
	CHECK(accepts("{\"files\":[]}"));
	const char *json =
	    "{\"files\":[{\"filename\":\"quote-\\\"-slash-\\\\-snow-\\u2603.hog\","
	    "\"content_uri\":\"content:\\/\\/provider\\/tree\\/a\","
	    "\"size_bytes\":9223372036854775807}]}";
	SafManifestData *manifest = saf_manifest_parse(json, strlen(json));
	CHECK(manifest != NULL);
	CHECK(manifest->count == 1);
	CHECK(strcmp(manifest->entries[0].content_uri, "content://provider/tree/a") == 0);
	CHECK(manifest->entries[0].size_bytes == INT64_MAX);
	saf_manifest_free(manifest);
	return 1;
}

static int test_raw_and_escaped_unicode_uris_match(void)
{
	static const char expected[] = "content://provider/\xC3\xA9/\xF0\x9F\x98\x80";
	static const char raw[] =
	    "{\"files\":[{\"filename\":\"raw.hog\",\"content_uri\":\""
	    "content://provider/\xC3\xA9/\xF0\x9F\x98\x80\",\"size_bytes\":1}]}";
	static const char escaped[] =
	    "{\"files\":[{\"filename\":\"escaped.hog\",\"content_uri\":\""
	    "content://provider/\\u00E9/\\uD83D\\uDE00\",\"size_bytes\":1}]}";
	SafManifestData *raw_manifest = saf_manifest_parse(raw, strlen(raw));
	SafManifestData *escaped_manifest = saf_manifest_parse(escaped, strlen(escaped));
	CHECK(raw_manifest != NULL);
	CHECK(escaped_manifest != NULL);
	CHECK(strcmp(raw_manifest->entries[0].content_uri, expected) == 0);
	CHECK(strcmp(escaped_manifest->entries[0].content_uri, expected) == 0);
	saf_manifest_free(raw_manifest);
	saf_manifest_free(escaped_manifest);
	return 1;
}

static int test_every_truncated_prefix_is_rejected(void)
{
	const char *json =
	    "{\"files\":[{\"filename\":\"first.hog\",\"content_uri\":\"content://first\","
	    "\"size_bytes\":1},{\"filename\":\"second.hog\",\"content_uri\":\"content://second\","
	    "\"size_bytes\":2}]}";
	const size_t length = strlen(json);
	for (size_t prefix = 0; prefix < length; prefix++) {
		SafManifestData *manifest = saf_manifest_parse(json, prefix);
		CHECK(manifest == NULL);
	}
	CHECK(accepts(json));
	return 1;
}

static int test_invalid_schema_and_json_are_rejected(void)
{
	const char *invalid[] = {
		"{}",
		"{\"files\":null}",
		"{\"files\":[],}",
		"{\"files\":[,]}",
		"{\"files\":[{}]}",
		"{\"files\":[{\"filename\":\"a\",\"content_uri\":\"u\"}]}",
		"{\"files\":[{\"filename\":\"a\",\"content_uri\":\"u\",\"size_bytes\":-1}]}",
		"{\"files\":[{\"filename\":\"a\",\"content_uri\":\"u\",\"size_bytes\":01}]}",
		"{\"files\":[{\"filename\":\"a\",\"content_uri\":\"u\",\"size_bytes\":1.0}]}",
		"{\"files\":[{\"filename\":\"a\",\"content_uri\":\"u\",\"size_bytes\":9223372036854775808}]}",
		"{\"files\":[{\"filename\":\"a\",\"filename\":\"b\",\"content_uri\":\"u\",\"size_bytes\":1}]}",
		"{\"files\":[{\"filename\":\"a\",\"content_uri\":\"u\",\"size_bytes\":1,\"extra\":0}]}",
		"{\"files\":[{\"filename\":\"same.hog\",\"content_uri\":\"u1\",\"size_bytes\":1},"
		"{\"filename\":\"SAME.HOG\",\"content_uri\":\"u2\",\"size_bytes\":2}]}",
		"{\"files\":[{\"filename\":\"bad\\x\",\"content_uri\":\"u\",\"size_bytes\":1}]}",
		"{\"files\":[{\"filename\":\"bad\\uD800\",\"content_uri\":\"u\",\"size_bytes\":1}]}",
		"{\"files\":[{\"filename\":\"bad\\u0000\",\"content_uri\":\"u\",\"size_bytes\":1}]}",
	};
	for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) CHECK(!accepts(invalid[i]));
	return 1;
}

int main(void)
{
	CHECK(test_valid_manifests());
	CHECK(test_raw_and_escaped_unicode_uris_match());
	CHECK(test_every_truncated_prefix_is_rejected());
	CHECK(test_invalid_schema_and_json_are_rejected());
	printf("SAF manifest parser tests passed\n");
	return 0;
}
