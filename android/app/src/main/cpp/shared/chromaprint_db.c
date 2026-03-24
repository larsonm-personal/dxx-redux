/*
 * chromaprint_db.c -- Fingerprint database loading and matching.
 *
 * The database is loaded from a JSON array passed from Kotlin (flattened
 * from known_discs.json5).  Matching uses XOR-popcount bit similarity
 * with offset alignment to handle silence/pregap differences.
 *
 * Chromaprint base64 decoding is done by chromaprint_decode_fingerprint()
 * from the Chromaprint library.
 */

#include "chromaprint_db.h"

#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <chromaprint.h>

#define LOG_TAG   "chromaprint_db"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Match threshold: minimum similarity to consider a positive match */
#define MATCH_THRESHOLD 0.45f

/* Duration tolerance: +/- percentage for duration pre-filter */
#define DURATION_TOLERANCE 0.10f

/* Max offset alignment shift (in fingerprint frames) */
#define MAX_OFFSET 15

/* Max entries in the database */
#define MAX_DB_ENTRIES 1024

static chromaprint_db_entry_t s_entries[MAX_DB_ENTRIES];
static int s_entry_count = 0;

/* ---------------------------------------------------------------------- */
/* Simple JSON parser helpers (avoids pulling in a full JSON library)      */
/* We parse a flat JSON array of objects with known string/int fields.     */
/* ---------------------------------------------------------------------- */

static const char *skip_ws(const char *p, const char *end)
{
	while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	return p;
}

/* Parse a JSON string value (between quotes).  Returns pointer past closing quote.
 * Writes at most buf_len-1 chars to buf. */
static const char *parse_json_string(const char *p, const char *end,
                                     char *buf, int buf_len)
{
	if (p >= end || *p != '"') return NULL;
	p++; /* skip opening quote */
	int i = 0;
	while (p < end && *p != '"') {
		if (*p == '\\' && p + 1 < end) {
			p++; /* skip escape */
		}
		if (i < buf_len - 1)
			buf[i++] = *p;
		p++;
	}
	buf[i] = '\0';
	if (p < end) p++; /* skip closing quote */
	return p;
}

/* Parse a JSON integer value.  Returns pointer past the number. */
static const char *parse_json_int(const char *p, const char *end, int *out)
{
	int neg = 0, val = 0;
	if (p < end && *p == '-') {
		neg = 1;
		p++;
	}
	while (p < end && *p >= '0' && *p <= '9') {
		val = val * 10 + (*p - '0');
		p++;
	}
	*out = neg ? -val : val;
	return p;
}

/* Skip a JSON value (string, number, object, array, bool, null). */
static const char *skip_json_value(const char *p, const char *end)
{
	p = skip_ws(p, end);
	if (p >= end) return p;
	if (*p == '"') {
		p++;
		while (p < end && *p != '"') {
			if (*p == '\\' && p + 1 < end) p++;
			p++;
		}
		if (p < end) p++;
		return p;
	}
	if (*p == '{') {
		int depth = 1;
		p++;
		while (p < end && depth > 0) {
			if (*p == '{') depth++;
			else if (*p == '}') depth--;
			else if (*p == '"') {
				p++;
				while (p < end && *p != '"') {
					if (*p == '\\') p++;
					p++;
				}
			}
			p++;
		}
		return p;
	}
	if (*p == '[') {
		int depth = 1;
		p++;
		while (p < end && depth > 0) {
			if (*p == '[') depth++;
			else if (*p == ']') depth--;
			else if (*p == '"') {
				p++;
				while (p < end && *p != '"') {
					if (*p == '\\') p++;
					p++;
				}
			}
			p++;
		}
		return p;
	}
	/* number, bool, null */
	while (p < end && *p != ',' && *p != '}' && *p != ']' &&
	       *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
		p++;
	return p;
}

int chromaprint_db_load(const char *json_data, int json_len)
{
	if (!json_data || json_len <= 0) return -1;

	/* Free any previous data */
	chromaprint_db_free();

	const char *p = json_data;
	const char *end = json_data + json_len;

	p = skip_ws(p, end);
	if (p >= end || *p != '[') {
		LOGE("Expected JSON array");
		return -1;
	}
	p++; /* skip '[' */

	int count = 0;
	while (count < MAX_DB_ENTRIES) {
		p = skip_ws(p, end);
		if (p >= end || *p == ']') break;
		if (*p == ',') {
			p++;
			continue;
		}
		if (*p != '{') {
			p++;
			continue;
		}
		p++; /* skip '{' */

		/* Parse object fields */
		char name[CHROMAPRINT_DB_MAX_NAME] = { 0 };
		char disc_id[CHROMAPRINT_DB_MAX_DISC_ID] = { 0 };
		char fp_b64[4096] = { 0 };
		int track = 0, duration_ms = 0;

		while (p < end && *p != '}') {
			p = skip_ws(p, end);
			if (p >= end || *p == '}') break;
			if (*p == ',') {
				p++;
				continue;
			}

			/* Parse key */
			char key[32] = { 0 };
			p = parse_json_string(p, end, key, sizeof(key));
			if (!p) break;
			p = skip_ws(p, end);
			if (p >= end || *p != ':') break;
			p++;
			p = skip_ws(p, end);

			/* Parse value based on key */
			if (strcmp(key, "name") == 0) {
				p = parse_json_string(p, end, name, sizeof(name));
			} else if (strcmp(key, "disc_id") == 0) {
				p = parse_json_string(p, end, disc_id, sizeof(disc_id));
			} else if (strcmp(key, "chromaprint") == 0) {
				p = parse_json_string(p, end, fp_b64, sizeof(fp_b64));
			} else if (strcmp(key, "track") == 0) {
				p = parse_json_int(p, end, &track);
			} else if (strcmp(key, "duration_ms") == 0) {
				p = parse_json_int(p, end, &duration_ms);
			} else {
				p = skip_json_value(p, end);
			}
			if (!p) break;
		}
		if (p < end && *p == '}') p++;

		/* Validate and decode fingerprint */
		if (fp_b64[0] && duration_ms > 0 && name[0]) {
			uint32_t *raw_fp = NULL;
			int raw_fp_len = 0;
			int algorithm = 0;
			if (chromaprint_decode_fingerprint(fp_b64, strlen(fp_b64),
			                                   (void **) &raw_fp, &raw_fp_len,
			                                   &algorithm, 1) == 1 &&
			    raw_fp) {
				chromaprint_db_entry_t *e = &s_entries[count];
				/* Copy the raw fingerprint -- chromaprint_dealloc will free
				 * the original, so we must own our copy */
				e->raw_fp = (uint32_t *) malloc((size_t) raw_fp_len * sizeof(uint32_t));
				if (e->raw_fp) {
					memcpy(e->raw_fp, raw_fp, (size_t) raw_fp_len * sizeof(uint32_t));
					e->fp_len = raw_fp_len;
					e->duration_ms = duration_ms;
					e->track_num = track;
					strncpy(e->name, name, CHROMAPRINT_DB_MAX_NAME - 1);
					strncpy(e->disc_id, disc_id, CHROMAPRINT_DB_MAX_DISC_ID - 1);
					count++;
				}
				chromaprint_dealloc(raw_fp);
			}
		}
	}

	s_entry_count = count;
	LOGI("Loaded %d fingerprint entries", count);
	return count;
}

/* ---------------------------------------------------------------------- */
/* XOR-popcount similarity matching                                        */
/* ---------------------------------------------------------------------- */

#if defined(__GNUC__) || defined(__clang__)
#define POPCOUNT32(x) __builtin_popcount(x)
#else
static int popcount32(uint32_t x)
{
	x = x - ((x >> 1) & 0x55555555);
	x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
	return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}
#define POPCOUNT32(x) popcount32(x)
#endif

/* Compare two fingerprints at a given offset.
 * Returns similarity as 0.0 - 1.0 (1.0 = identical). */
static float fp_similarity(const uint32_t *a, int a_len,
                           const uint32_t *b, int b_len,
                           int offset)
{
	/* Adjust pointers for offset */
	const uint32_t *pa = a;
	const uint32_t *pb = b;
	int la = a_len, lb = b_len;

	if (offset > 0) {
		if (offset >= lb) return 0.0f;
		pb += offset;
		lb -= offset;
	} else if (offset < 0) {
		if (-offset >= la) return 0.0f;
		pa += (-offset);
		la -= (-offset);
	}

	int overlap = la < lb ? la : lb;
	if (overlap < 10) return 0.0f; /* too short to be meaningful */

	long total_bits = 0;
	for (int i = 0; i < overlap; i++)
		total_bits += POPCOUNT32(pa[i] ^ pb[i]);

	/* Max bits per frame = 32; perfect match = 0 bits different */
	float max_bits = (float) overlap * 32.0f;
	return 1.0f - (float) total_bits / max_bits;
}

int chromaprint_db_match(const uint32_t *raw_fp, int fp_len, int duration_ms,
                         chromaprint_db_match_t *out_match)
{
	if (!raw_fp || fp_len <= 0 || !out_match || s_entry_count == 0)
		return 0;

	float best_score = 0.0f;
	int best_idx = -1;

	for (int i = 0; i < s_entry_count; i++) {
		const chromaprint_db_entry_t *e = &s_entries[i];

		/* Duration pre-filter */
		if (duration_ms > 0 && e->duration_ms > 0) {
			float ratio = (float) duration_ms / (float) e->duration_ms;
			if (ratio < (1.0f - DURATION_TOLERANCE) ||
			    ratio > (1.0f + DURATION_TOLERANCE))
				continue;
		}

		/* Try several offset alignments */
		for (int off = -MAX_OFFSET; off <= MAX_OFFSET; off++) {
			float score = fp_similarity(raw_fp, fp_len,
			                            e->raw_fp, e->fp_len, off);
			if (score > best_score) {
				best_score = score;
				best_idx = i;
			}
		}
	}

	if (best_idx >= 0 && best_score >= MATCH_THRESHOLD) {
		out_match->confidence = best_score;
		strncpy(out_match->name, s_entries[best_idx].name,
		        CHROMAPRINT_DB_MAX_NAME - 1);
		out_match->name[CHROMAPRINT_DB_MAX_NAME - 1] = '\0';
		strncpy(out_match->disc_id, s_entries[best_idx].disc_id,
		        CHROMAPRINT_DB_MAX_DISC_ID - 1);
		out_match->disc_id[CHROMAPRINT_DB_MAX_DISC_ID - 1] = '\0';
		out_match->track_num = s_entries[best_idx].track_num;
		return 1;
	}

	return 0;
}

void chromaprint_db_free(void)
{
	for (int i = 0; i < s_entry_count; i++) {
		free(s_entries[i].raw_fp);
		s_entries[i].raw_fp = NULL;
	}
	s_entry_count = 0;
}

int chromaprint_db_count(void)
{
	return s_entry_count;
}
