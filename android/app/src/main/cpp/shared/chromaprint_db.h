/*
 * chromaprint_db.h -- In-memory fingerprint database for track identification.
 * Loaded from known_discs.json5 data at app startup via JNI.
 * Matching uses XOR-popcount similarity with offset alignment.
 */

#ifndef CHROMAPRINT_DB_H
#define CHROMAPRINT_DB_H

#include <stdint.h>

#define CHROMAPRINT_DB_MAX_NAME    64
#define CHROMAPRINT_DB_MAX_DISC_ID 64

typedef struct {
	uint32_t *raw_fp; /* decoded fingerprint (array of uint32) */
	int fp_len;       /* number of uint32 elements */
	int duration_ms;  /* track duration in milliseconds */
	char name[CHROMAPRINT_DB_MAX_NAME];
	char disc_id[CHROMAPRINT_DB_MAX_DISC_ID];
	int track_num;
} chromaprint_db_entry_t;

typedef struct {
	float confidence; /* 0.0 - 1.0, higher = better match */
	char name[CHROMAPRINT_DB_MAX_NAME];
	char disc_id[CHROMAPRINT_DB_MAX_DISC_ID];
	int track_num;
} chromaprint_db_match_t;

/* Load fingerprint entries from a JSON array.
 * Expected format (from known_discs.json5 flattened by Kotlin):
 *   [{"name":"Title","disc_id":"d2-gog","track":2,"duration_ms":187000,"chromaprint":"AQAA..."}]
 * Returns number of entries loaded, or -1 on error. */
int chromaprint_db_load(const char *json_data, int json_len);

/* Match a fingerprint against the loaded database.
 * raw_fp/fp_len: decoded fingerprint from chromaprint_get_raw_fingerprint().
 * duration_ms: track duration.
 * On match (confidence > threshold), fills out_match and returns 1.
 * Returns 0 if no match found. */
int chromaprint_db_match(const uint32_t *raw_fp, int fp_len, int duration_ms,
                         chromaprint_db_match_t *out_match);

/* Set match confidence threshold (0.0-1.0). Default 0.4.
 * Loaded from fingerprint_config.json5 at startup. */
void chromaprint_db_set_threshold(float threshold);

/* Set duration pre-filter tolerance (0.0-1.0 fraction). Default 0.10. */
void chromaprint_db_set_duration_tolerance(float tolerance);

/* Free all loaded database entries. */
void chromaprint_db_free(void);

/* Return current entry count. */
int chromaprint_db_count(void);

#endif /* CHROMAPRINT_DB_H */
