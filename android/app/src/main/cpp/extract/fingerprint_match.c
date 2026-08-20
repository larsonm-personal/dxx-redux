/*
 * fingerprint_match.c -- Find duplicate fingerprints in a JSON database.
 *
 * Reads a JSON file containing a flat array of fingerprint entries (the same
 * format used by chromaprint_db_load), loads them, and reports all pairs that
 * exceed the match threshold using proper XOR-popcount similarity.
 *
 * Usage: fingerprint_match <db.json> <threshold> <duration-tolerance>
 *
 * Output: one JSON line per duplicate pair:
 *   {"a_disc":"...","a_track":N,"b_disc":"...","b_track":N,"score":0.XX}
 *
 * The caller must pass the validated value from fingerprint_config.jsonc.
 */

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <chromaprint.h>
#include <nlohmann/json.hpp>

#include "json_writer.h"
#include "../shared/fingerprint_duration.h"

/* ── Matching parameters ──────────────────────────────────────────── */

#define MAX_OFFSET      15
#define MAX_ENTRIES     4096
#define MAX_NAME        128
#define MAX_DISC_ID     128
#define MAX_FINGERPRINT (1024 * 1024)

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

static float best_similarity(const entry_t *a, const entry_t *b,
                             float duration_tolerance)
{
	/* Duration pre-filter */
	if (!fingerprint_durations_compatible(a->duration_ms, b->duration_ms,
	                                      duration_tolerance))
		return 0.0f;
	float best = 0.0f;
	for (int off = -MAX_OFFSET; off <= MAX_OFFSET; off++) {
		float s = fp_similarity(a->raw_fp, a->fp_len, b->raw_fp, b->fp_len, off);
		if (s > best) best = s;
	}
	return best;
}

static int load_db(const char *data, int len)
{
	auto root = nlohmann::json::parse(data, data + len, NULL, false);
	if (!root.is_array() || root.empty() || root.size() > MAX_ENTRIES) {
		fprintf(stderr, "Expected a nonempty JSON array within the entry limit\n");
		return -1;
	}

	for (const auto &item : root) {
		if (!item.is_object() || !item.contains("name") || !item["name"].is_string() ||
		    !item.contains("disc_id") || !item["disc_id"].is_string() ||
		    !item.contains("chromaprint") || !item["chromaprint"].is_string() ||
		    !item.contains("track") || !item["track"].is_number_integer() ||
		    !item.contains("duration_ms") || !item["duration_ms"].is_number_integer())
			goto fail;
		try {
			const std::string name = item["name"].get<std::string>();
			const std::string disc_id = item["disc_id"].get<std::string>();
			const std::string fp_b64 = item["chromaprint"].get<std::string>();
			const int track = item["track"].get<int>();
			const int dur = item["duration_ms"].get<int>();
			uint32_t *raw = NULL;
			int raw_len = 0, alg = 0;

			if (name.size() >= MAX_NAME || disc_id.size() >= MAX_DISC_ID ||
			    fp_b64.empty() || fp_b64.size() > MAX_FINGERPRINT || dur <= 0 ||
			    chromaprint_decode_fingerprint(fp_b64.data(), (int) fp_b64.size(),
			                                   &raw, &raw_len, &alg, 1) != 1 ||
			    !raw || raw_len <= 0) {
				if (raw) chromaprint_dealloc(raw);
				goto fail;
			}
			entry_t *e = &s_entries[s_count];
			e->raw_fp = (uint32_t *) malloc((size_t) raw_len * sizeof(uint32_t));
			if (!e->raw_fp) {
				chromaprint_dealloc(raw);
				goto fail;
			}
			memcpy(e->raw_fp, raw, (size_t) raw_len * sizeof(uint32_t));
			chromaprint_dealloc(raw);
			e->fp_len = raw_len;
			e->duration_ms = dur;
			e->track_num = track;
			memcpy(e->name, name.c_str(), name.size() + 1);
			memcpy(e->disc_id, disc_id.c_str(), disc_id.size() + 1);
			s_count++;
		} catch (...) {
			goto fail;
		}
	}
	return s_count;

fail:
	for (int i = 0; i < s_count; i++) {
		free(s_entries[i].raw_fp);
		s_entries[i] = {};
	}
	s_count = 0;
	fprintf(stderr, "Invalid or incomplete fingerprint database\n");
	return -1;
}

/* ── JSON-escape a string ─────────────────────────────────────────── */

static void print_escaped(FILE *f, const char *s)
{
	json_write_string(f, s);
}

/* ── Main ─────────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
	if (argc != 4) {
		fprintf(stderr,
		        "Usage: fingerprint_match <db.json> <threshold> "
		        "<duration-tolerance>\n");
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

	errno = 0;
	char *tolerance_end = NULL;
	float duration_tolerance = strtof(argv[3], &tolerance_end);
	if (tolerance_end == argv[3] || *tolerance_end != '\0' || errno == ERANGE ||
	    !isfinite(duration_tolerance) || duration_tolerance < 0.0f ||
	    duration_tolerance > 1.0f) {
		fprintf(stderr, "Invalid duration tolerance: %s\n", argv[3]);
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		fprintf(stderr, "Cannot open %s\n", argv[1]);
		return 1;
	}
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return 1;
	}
	long sz = ftell(f);
	if (sz <= 0 || sz > INT_MAX || fseek(f, 0, SEEK_SET) != 0) {
		fclose(f);
		return 1;
	}
	char *data = (char *) malloc(sz + 1);
	if (!data) {
		fclose(f);
		return 1;
	}
	if (fread(data, 1, (size_t) sz, f) != (size_t) sz) {
		free(data);
		fclose(f);
		return 1;
	}
	data[sz] = '\0';
	fclose(f);

	int n = load_db(data, (int) sz);
	free(data);
	if (n <= 0) {
		fprintf(stderr, "No entries loaded\n");
		return 1;
	}
	fprintf(stderr, "Loaded %d entries, threshold=%.2f, duration tolerance=%.2f\n",
	        n, threshold, duration_tolerance);

	/* Compare all pairs, output duplicates */
	int dup_count = 0;
	printf("[\n");
	for (int i = 0; i < s_count; i++) {
		for (int j = i + 1; j < s_count; j++) {
			float score = best_similarity(&s_entries[i], &s_entries[j],
			                              duration_tolerance);
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
