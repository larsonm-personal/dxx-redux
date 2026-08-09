/*
 * fingerprint_match.c -- Find duplicate fingerprints in a JSON database.
 *
 * Reads a JSON file containing a flat array of fingerprint entries (the same
 * format used by chromaprint_db_load), loads them, and reports all pairs that
 * exceed the match threshold using proper XOR-popcount similarity.
 *
 * Usage: fingerprint_match <db.json> <threshold>
 *
 * Output: one JSON line per duplicate pair:
 *   {"a_disc":"...","a_track":N,"b_disc":"...","b_track":N,"score":0.XX}
 *
 * The caller must pass the validated value from fingerprint_config.json5.
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chromaprint.h>

#include "json_writer.h"

/* ── Matching parameters ──────────────────────────────────────────── */

#define MAX_OFFSET         15
#define DURATION_TOLERANCE 0.10f
#define MAX_ENTRIES        4096
#define MAX_NAME           128
#define MAX_DISC_ID        128

/* ── Data ─────────────────────────────────────────────────────────── */

typedef struct {
	uint32_t *raw_fp;
	int fp_len;
	int duration_ms;
	int track_num;
	char name[MAX_NAME];
	char disc_id[MAX_DISC_ID];
} entry_t;

static entry_t s_entries[MAX_ENTRIES];
static int s_count = 0;

/* ── Popcount ─────────────────────────────────────────────────────── */

#if defined(__GNUC__) || defined(__clang__)
#define POPCNT(x) __builtin_popcount(x)
#elif defined(_MSC_VER)
#include <intrin.h>
#define POPCNT(x) __popcnt(x)
#else
static int sw_popcount(uint32_t x)
{
	x = x - ((x >> 1) & 0x55555555u);
	x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
	return (int) ((((x + (x >> 4)) & 0x0F0F0F0Fu) * 0x01010101u) >> 24);
}
#define POPCNT(x) sw_popcount(x)
#endif

/* ── Similarity ───────────────────────────────────────────────────── */

static float fp_similarity(const uint32_t *a, int a_len,
                           const uint32_t *b, int b_len, int offset)
{
	const uint32_t *pa = a, *pb = b;
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
	if (overlap < 10) return 0.0f;
	long total = 0;
	for (int i = 0; i < overlap; i++)
		total += POPCNT(pa[i] ^ pb[i]);
	return 1.0f - (float) total / ((float) overlap * 32.0f);
}

static float best_similarity(const entry_t *a, const entry_t *b)
{
	/* Duration pre-filter */
	if (a->duration_ms > 0 && b->duration_ms > 0) {
		float ratio = (float) a->duration_ms / (float) b->duration_ms;
		if (ratio < 1.0f - DURATION_TOLERANCE || ratio > 1.0f + DURATION_TOLERANCE)
			return 0.0f;
	}
	float best = 0.0f;
	for (int off = -MAX_OFFSET; off <= MAX_OFFSET; off++) {
		float s = fp_similarity(a->raw_fp, a->fp_len, b->raw_fp, b->fp_len, off);
		if (s > best) best = s;
	}
	return best;
}

/* ── Minimal JSON parsing (reused pattern from chromaprint_db.c) ── */

static const char *skip_ws(const char *p, const char *end)
{
	while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
		p++;
	return p;
}

static const char *parse_str(const char *p, const char *end, char *buf, int blen)
{
	if (p >= end || *p != '"') return NULL;
	p++;
	int i = 0;
	while (p < end && *p != '"') {
		if (*p == '\\' && p + 1 < end) p++;
		if (i < blen - 1) buf[i++] = *p;
		p++;
	}
	buf[i] = '\0';
	if (p < end) p++;
	return p;
}

static const char *parse_int(const char *p, const char *end, int *out)
{
	int neg = 0, v = 0;
	if (p < end && *p == '-') {
		neg = 1;
		p++;
	}
	while (p < end && *p >= '0' && *p <= '9') {
		v = v * 10 + (*p - '0');
		p++;
	}
	*out = neg ? -v : v;
	return p;
}

static const char *skip_val(const char *p, const char *end)
{
	p = skip_ws(p, end);
	if (p >= end) return p;
	if (*p == '"') {
		p++;
		while (p < end && *p != '"') {
			if (*p == '\\') p++;
			p++;
		}
		if (p < end) p++;
		return p;
	}
	if (*p == '{' || *p == '[') {
		char open = *p, close = (*p == '{') ? '}' : ']';
		int depth = 1;
		p++;
		while (p < end && depth > 0) {
			if (*p == open) depth++;
			else if (*p == close) depth--;
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
	/* Handles numbers, true, false, null */
	while (p < end && *p != ',' && *p != '}' && *p != ']' &&
	       *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r')
		p++;
	return p;
}

/* Parse a JSON string value, or handle null by leaving buf empty */
static const char *parse_str_or_null(const char *p, const char *end,
                                     char *buf, int blen)
{
	p = skip_ws(p, end);
	if (p >= end) return NULL;
	if (*p == 'n' && p + 3 < end && p[1] == 'u' && p[2] == 'l' && p[3] == 'l') {
		buf[0] = '\0';
		return p + 4;
	}
	return parse_str(p, end, buf, blen);
}

static int load_db(const char *data, int len)
{
	const char *p = data, *end = data + len;
	p = skip_ws(p, end);
	if (p >= end || *p != '[') {
		fprintf(stderr, "Expected JSON array\n");
		return -1;
	}
	p++;

	while (s_count < MAX_ENTRIES) {
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
		p++;

		char name[MAX_NAME] = { 0 };
		char disc_id[MAX_DISC_ID] = { 0 };
		char fp_b64[8192] = { 0 };
		int track = 0, dur = 0;

		while (p < end && *p != '}') {
			p = skip_ws(p, end);
			if (p >= end || *p == '}') break;
			if (*p == ',') {
				p++;
				continue;
			}

			char key[32] = { 0 };
			p = parse_str(p, end, key, sizeof(key));
			if (!p) break;
			p = skip_ws(p, end);
			if (p >= end || *p != ':') break;
			p++;
			p = skip_ws(p, end);

			if (strcmp(key, "name") == 0)
				p = parse_str_or_null(p, end, name, sizeof(name));
			else if (strcmp(key, "disc_id") == 0)
				p = parse_str_or_null(p, end, disc_id, sizeof(disc_id));
			else if (strcmp(key, "chromaprint") == 0)
				p = parse_str_or_null(p, end, fp_b64, sizeof(fp_b64));
			else if (strcmp(key, "track") == 0)
				p = parse_int(p, end, &track);
			else if (strcmp(key, "duration_ms") == 0)
				p = parse_int(p, end, &dur);
			else
				p = skip_val(p, end);
			if (!p) break;
		}
		if (p < end && *p == '}') p++;

		if (fp_b64[0] && dur > 0) {
			uint32_t *raw = NULL;
			int raw_len = 0, alg = 0;
			if (chromaprint_decode_fingerprint(fp_b64, (int) strlen(fp_b64),
			                                   &raw, &raw_len, &alg, 1) == 1 &&
			    raw) {
				entry_t *e = &s_entries[s_count];
				e->raw_fp = (uint32_t *) malloc((size_t) raw_len * sizeof(uint32_t));
				if (e->raw_fp) {
					memcpy(e->raw_fp, raw, (size_t) raw_len * sizeof(uint32_t));
					e->fp_len = raw_len;
					e->duration_ms = dur;
					e->track_num = track;
					strncpy(e->name, name, MAX_NAME - 1);
					strncpy(e->disc_id, disc_id, MAX_DISC_ID - 1);
					s_count++;
				}
				chromaprint_dealloc(raw);
			}
		}
	}
	return s_count;
}

/* ── JSON-escape a string ─────────────────────────────────────────── */

static void print_escaped(FILE *f, const char *s)
{
	json_write_string(f, s);
}

/* ── Main ─────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: fingerprint_match <db.json> <threshold>\n");
		return 1;
	}

	errno = 0;
	char *threshold_end = NULL;
	float threshold = strtof(argv[2], &threshold_end);
	if (threshold_end == argv[2] || *threshold_end != '\0' || errno == ERANGE ||
	    !isfinite(threshold) || threshold <= 0.0f || threshold > 1.0f) {
		fprintf(stderr, "Invalid threshold: %s\n", argv[2]);
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		fprintf(stderr, "Cannot open %s\n", argv[1]);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *data = (char *) malloc(sz + 1);
	if (!data) {
		fclose(f);
		return 1;
	}
	fread(data, 1, sz, f);
	data[sz] = '\0';
	fclose(f);

	int n = load_db(data, (int) sz);
	free(data);
	if (n <= 0) {
		fprintf(stderr, "No entries loaded\n");
		return 1;
	}
	fprintf(stderr, "Loaded %d entries, threshold=%.2f\n", n, threshold);

	/* Compare all pairs, output duplicates */
	int dup_count = 0;
	printf("[\n");
	for (int i = 0; i < s_count; i++) {
		for (int j = i + 1; j < s_count; j++) {
			float score = best_similarity(&s_entries[i], &s_entries[j]);
			if (score >= threshold) {
				if (dup_count > 0) printf(",\n");
				printf("  {\"a_disc\": ");
				print_escaped(stdout, s_entries[i].disc_id);
				printf(", \"a_track\": %d, \"a_name\": ", s_entries[i].track_num);
				print_escaped(stdout, s_entries[i].name);
				printf(", \"a_duration_ms\": %d", s_entries[i].duration_ms);
				printf(", \"b_disc\": ");
				print_escaped(stdout, s_entries[j].disc_id);
				printf(", \"b_track\": %d, \"b_name\": ", s_entries[j].track_num);
				print_escaped(stdout, s_entries[j].name);
				printf(", \"b_duration_ms\": %d", s_entries[j].duration_ms);
				printf(", \"score\": %.4f}", score);
				dup_count++;
			}
		}
	}
	printf("\n]\n");
	fprintf(stderr, "Found %d duplicate pairs\n", dup_count);

	for (int i = 0; i < s_count; i++)
		free(s_entries[i].raw_fp);
	return 0;
}
