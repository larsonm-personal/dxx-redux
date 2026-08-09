#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <chromaprint.h>
#include "chromaprint_db.h"

static char *make_entry_json(const char *name, const char *disc_id, const char *encoded)
{
	size_t size = strlen(name) + strlen(disc_id) + strlen(encoded) + 128;
	char *json = (char *) malloc(size);
	assert(json);
	snprintf(json, size,
	         "[{\"name\":\"%s\",\"disc_id\":\"%s\",\"track\":7,"
	         "\"duration_ms\":123000,\"chromaprint\":\"%s\"}]",
	         name, disc_id, encoded);
	return json;
}

static char *make_ambiguous_json(const char *encoded, int reverse)
{
	const char *first = reverse ? "second" : "first";
	const char *second = reverse ? "first" : "second";
	size_t size = strlen(encoded) * 2 + 384;
	char *json = (char *) malloc(size);
	assert(json);
	snprintf(json, size,
	         "[{\"name\":\"%s\",\"disc_id\":\"disc-%s\",\"track\":1,"
	         "\"duration_ms\":123000,\"chromaprint\":\"%s\"},"
	         "{\"name\":\"%s\",\"disc_id\":\"disc-%s\",\"track\":2,"
	         "\"duration_ms\":124000,\"chromaprint\":\"%s\"}]",
	         first, first, encoded, second, second, encoded);
	return json;
}

int main(void)
{
	static const char empty_db[] = "[]";

	assert(chromaprint_db_load(empty_db, 2) == -1);
	assert(chromaprint_db_set_threshold(NAN) == 0);
	assert(chromaprint_db_load(empty_db, 2) == -1);
	assert(chromaprint_db_set_threshold(INFINITY) == 0);
	assert(chromaprint_db_set_threshold(0.0f) == 0);
	assert(chromaprint_db_set_threshold(1.01f) == 0);

	assert(chromaprint_db_set_threshold(0.40f) == 1);
	assert(chromaprint_db_load(empty_db, 2) == 0);
	assert(chromaprint_db_set_threshold(0.65f) == 1);
	assert(chromaprint_db_load(empty_db, 2) == 0);

	uint32_t raw_fp[20];
	for (int i = 0; i < 20; i++) raw_fp[i] = (uint32_t) (0x12345678U + i);
	char *encoded = NULL;
	int encoded_size = 0;
	assert(chromaprint_encode_fingerprint(
	           raw_fp, 20, CHROMAPRINT_ALGORITHM_DEFAULT,
	           &encoded, &encoded_size, 1) == 1);

	static const char escaped_name[] =
	    "A deliberately long fingerprint name that exceeds sixty-three bytes | "
	    "quoted: \\\" slash: \\\\ line:\\n caf\\u00e9";
	static const char expected_name[] =
	    "A deliberately long fingerprint name that exceeds sixty-three bytes | "
	    "quoted: \" slash: \\ line:\n caf\xc3\xa9";
	char *json = make_entry_json(escaped_name, "disc|id\\u2603", encoded);
	assert(chromaprint_db_load(json, (int) strlen(json)) == 1);
	free(json);

	chromaprint_db_match_t match = { 0 };
	assert(chromaprint_db_match(raw_fp, 20, 123000, &match) == 1);
	assert(strcmp(match.name, expected_name) == 0);
	assert(strcmp(match.disc_id, "disc|id\xe2\x98\x83") == 0);
	assert(match.track_num == 7);

	char *limit_name = (char *) malloc(CHROMAPRINT_DB_MAX_METADATA_BYTES + 2);
	assert(limit_name);
	memset(limit_name, 'x', CHROMAPRINT_DB_MAX_METADATA_BYTES);
	limit_name[CHROMAPRINT_DB_MAX_METADATA_BYTES] = '\0';
	json = make_entry_json(limit_name, "limit", encoded);
	assert(chromaprint_db_load(json, (int) strlen(json)) == 1);
	free(json);
	assert(strcmp(match.name, expected_name) == 0);
	chromaprint_db_match_free(&match);
	assert(chromaprint_db_match(raw_fp, 20, 123000, &match) == 1);
	assert(strlen(match.name) == CHROMAPRINT_DB_MAX_METADATA_BYTES);
	chromaprint_db_match_free(&match);
	unsigned char *limit_bytes = (unsigned char *) limit_name;
	for (int i = 0; i < CHROMAPRINT_DB_MAX_METADATA_BYTES; i += 2) {
		limit_bytes[i] = 0xc3;
		limit_bytes[i + 1] = 0xa9;
	}
	limit_name[CHROMAPRINT_DB_MAX_METADATA_BYTES] = '\0';
	json = make_entry_json(limit_name, "unicode-limit", encoded);
	assert(chromaprint_db_load(json, (int) strlen(json)) == 1);
	free(json);
	assert(chromaprint_db_match(raw_fp, 20, 123000, &match) == 1);
	assert(memcmp(match.name, limit_name, CHROMAPRINT_DB_MAX_METADATA_BYTES + 1) == 0);
	chromaprint_db_match_free(&match);

	for (int reverse = 0; reverse <= 1; reverse++) {
		json = make_ambiguous_json(encoded, reverse);
		assert(chromaprint_db_load(json, (int) strlen(json)) == 2);
		free(json);
		assert(chromaprint_db_match(raw_fp, 20, 123000, &match) ==
		       CHROMAPRINT_DB_MATCH_AMBIGUOUS);
		assert(match.name == NULL);
		assert(match.disc_id == NULL);
	}

	json = make_entry_json(limit_name, "unicode-limit", encoded);
	assert(chromaprint_db_load(json, (int) strlen(json)) == 1);
	free(json);
	assert(chromaprint_db_match(raw_fp, 20, 123000, &match) ==
	       CHROMAPRINT_DB_MATCH_FOUND);

	limit_name[CHROMAPRINT_DB_MAX_METADATA_BYTES] = 'x';
	limit_name[CHROMAPRINT_DB_MAX_METADATA_BYTES + 1] = '\0';
	json = make_entry_json(limit_name, "too-long", encoded);
	assert(chromaprint_db_load(json, (int) strlen(json)) == -1);
	assert(chromaprint_db_count() == 1);
	assert(memcmp(match.name, limit_name, CHROMAPRINT_DB_MAX_METADATA_BYTES) == 0);
	chromaprint_db_match_free(&match);
	free(json);
	free(limit_name);
	chromaprint_dealloc(encoded);

	assert(chromaprint_db_set_threshold(NAN) == 0);
	assert(chromaprint_db_load(empty_db, 2) == -1);
	chromaprint_db_free();
	printf("chromaprint DB configuration tests passed\n");
	return 0;
}
