/*
 * chromaprint_db.c -- Fingerprint database loading and matching.
 *
 * The database is loaded from a JSON array passed from Kotlin (flattened
 * from the physical-disc and album fingerprint assets).  Matching uses XOR-popcount bit similarity
 * with offset alignment to handle silence/pregap differences.
 *
 * Chromaprint base64 decoding is done by chromaprint_decode_fingerprint()
 * from the Chromaprint library.
 */

#include "chromaprint_db.h"
#include "fingerprint_duration.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <android/log.h>
#include <chromaprint.h>
#include <nlohmann/json.hpp>
#include <mutex>

#define LOG_TAG   "chromaprint_db"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* Single source of truth for the match threshold is fingerprint_config.json5.
 * Database loading fails until Kotlin supplies its validated value. */
#define DEFAULT_DURATION_TOLERANCE 0.10f

/* Max offset alignment shift (in fingerprint frames) */
#define MAX_OFFSET 15

/* Max entries in the database */
#define MAX_DB_ENTRIES 1024

static float s_match_threshold = 0.0f;
static int s_match_threshold_configured = 0;
static float s_duration_tolerance = DEFAULT_DURATION_TOLERANCE;

static chromaprint_db_entry_t s_entries[MAX_DB_ENTRIES];
static int s_entry_count = 0;
static std::mutex s_db_mutex;

int chromaprint_db_set_threshold(float threshold)
{
	const std::lock_guard<std::mutex> lock(s_db_mutex);
	if (!isfinite(threshold) || threshold <= 0.0f || threshold > 1.0f) {
		s_match_threshold = 0.0f;
		s_match_threshold_configured = 0;
		return 0;
	}
	s_match_threshold = threshold;
	s_match_threshold_configured = 1;
	return 1;
}

void chromaprint_db_set_duration_tolerance(float tolerance)
{
	const std::lock_guard<std::mutex> lock(s_db_mutex);
	if (tolerance > 0.0f && tolerance <= 1.0f)
		s_duration_tolerance = tolerance;
}

static char *copy_string(const std::string &value)
{
	char *copy = (char *) malloc(value.size() + 1);
	if (!copy) return NULL;
	memcpy(copy, value.data(), value.size());
	copy[value.size()] = '\0';
	return copy;
}

static char *copy_string(const char *value)
{
	size_t length = strlen(value);
	char *copy = (char *) malloc(length + 1);
	if (copy) memcpy(copy, value, length + 1);
	return copy;
}

static void free_entries(chromaprint_db_entry_t *entries, int count)
{
	for (int i = 0; i < count; i++) {
		free(entries[i].raw_fp);
		free(entries[i].name);
		free(entries[i].disc_id);
		entries[i] = {};
	}
}

int chromaprint_db_load(const char *json_data, int json_len)
{
	const std::lock_guard<std::mutex> lock(s_db_mutex);
	if (!s_match_threshold_configured) {
		LOGE("Fingerprint match threshold is not configured");
		return -1;
	}
	if (!json_data || json_len <= 0) return -1;

	auto root = nlohmann::json::parse(json_data, json_data + json_len, NULL, false);
	if (!root.is_array() || root.size() > MAX_DB_ENTRIES) {
		LOGE("Expected JSON array");
		return -1;
	}

	chromaprint_db_entry_t pending[MAX_DB_ENTRIES] = {};
	int count = 0;
	for (const auto &item : root) {
		if (!item.is_object() || !item.contains("name") || !item["name"].is_string() ||
		    !item.contains("disc_id") || !item["disc_id"].is_string() ||
		    !item.contains("chromaprint") || !item["chromaprint"].is_string() ||
		    !item.contains("track") || !item["track"].is_number_integer() ||
		    !item.contains("duration_ms") || !item["duration_ms"].is_number_integer()) {
			free_entries(pending, count);
			return -1;
		}
		const std::string &name = item["name"].get_ref<const std::string &>();
		const std::string &disc_id = item["disc_id"].get_ref<const std::string &>();
		const std::string &fp_b64 = item["chromaprint"].get_ref<const std::string &>();
		if (name.empty() || name.size() > CHROMAPRINT_DB_MAX_METADATA_BYTES ||
		    disc_id.size() > CHROMAPRINT_DB_MAX_METADATA_BYTES ||
		    fp_b64.empty() || fp_b64.size() > CHROMAPRINT_DB_MAX_FINGERPRINT_BYTES ||
		    name.find('\0') != std::string::npos || disc_id.find('\0') != std::string::npos ||
		    fp_b64.find('\0') != std::string::npos ||
		    !item["track"].is_number_integer() || !item["duration_ms"].is_number_integer()) {
			free_entries(pending, count);
			return -1;
		}

		int track;
		int duration_ms;
		try {
			track = item["track"].get<int>();
			duration_ms = item["duration_ms"].get<int>();
		} catch (...) {
			free_entries(pending, count);
			return -1;
		}
		if (duration_ms <= 0) {
			free_entries(pending, count);
			return -1;
		}

		uint32_t *raw_fp = NULL;
		int raw_fp_len = 0;
		int algorithm = 0;
		if (chromaprint_decode_fingerprint(fp_b64.data(), (int) fp_b64.size(),
		                                   &raw_fp, &raw_fp_len,
		                                   &algorithm, 1) != 1 ||
		    !raw_fp || raw_fp_len <= 0) {
			if (raw_fp) chromaprint_dealloc(raw_fp);
			free_entries(pending, count);
			return -1;
		}

		chromaprint_db_entry_t *entry = &pending[count];
		entry->raw_fp = (uint32_t *) malloc((size_t) raw_fp_len * sizeof(uint32_t));
		entry->name = copy_string(name);
		entry->disc_id = copy_string(disc_id);
		if (!entry->raw_fp || !entry->name || !entry->disc_id) {
			chromaprint_dealloc(raw_fp);
			free_entries(pending, count + 1);
			return -1;
		}
		memcpy(entry->raw_fp, raw_fp, (size_t) raw_fp_len * sizeof(uint32_t));
		chromaprint_dealloc(raw_fp);
		entry->fp_len = raw_fp_len;
		entry->duration_ms = duration_ms;
		entry->track_num = track;
		count++;
	}

	free_entries(s_entries, s_entry_count);
	memcpy(s_entries, pending, (size_t) count * sizeof(pending[0]));
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
	const std::lock_guard<std::mutex> lock(s_db_mutex);
	if (!raw_fp || fp_len <= 0 || !out_match || s_entry_count == 0)
		return 0;
	*out_match = {};

	float best_score = 0.0f;
	int best_idx = -1;
	int ambiguous = 0;

	for (int i = 0; i < s_entry_count; i++) {
		const chromaprint_db_entry_t *e = &s_entries[i];

		/* Duration pre-filter */
		if (!fingerprint_durations_compatible(duration_ms, e->duration_ms,
		                                      s_duration_tolerance))
			continue;

		/* Try several offset alignments */
		float entry_score = 0.0f;
		for (int off = -MAX_OFFSET; off <= MAX_OFFSET; off++) {
			float score = fp_similarity(raw_fp, fp_len,
			                            e->raw_fp, e->fp_len, off);
			if (score > entry_score) entry_score = score;
		}

		if (entry_score > best_score + 1.0e-6f) {
			best_score = entry_score;
			best_idx = i;
			ambiguous = 0;
		} else if (best_idx >= 0 && fabsf(entry_score - best_score) <= 1.0e-6f &&
		           (s_entries[best_idx].track_num != e->track_num ||
		            strcmp(s_entries[best_idx].disc_id, e->disc_id) != 0 ||
		            strcmp(s_entries[best_idx].name, e->name) != 0)) {
			ambiguous = 1;
		}
	}

	if (best_idx >= 0 && best_score >= s_match_threshold) {
		if (ambiguous) {
			LOGW("Ambiguous fingerprint match at confidence %.6f", best_score);
			return CHROMAPRINT_DB_MATCH_AMBIGUOUS;
		}
		out_match->confidence = best_score;
		out_match->name = copy_string(s_entries[best_idx].name);
		out_match->disc_id = copy_string(s_entries[best_idx].disc_id);
		if (!out_match->name || !out_match->disc_id) {
			chromaprint_db_match_free(out_match);
			return CHROMAPRINT_DB_MATCH_NONE;
		}
		out_match->track_num = s_entries[best_idx].track_num;
		return CHROMAPRINT_DB_MATCH_FOUND;
	}

	return CHROMAPRINT_DB_MATCH_NONE;
}

void chromaprint_db_match_free(chromaprint_db_match_t *match)
{
	if (!match) return;
	free(match->name);
	free(match->disc_id);
	*match = {};
}

void chromaprint_db_free(void)
{
	const std::lock_guard<std::mutex> lock(s_db_mutex);
	free_entries(s_entries, s_entry_count);
	s_entry_count = 0;
}

int chromaprint_db_count(void)
{
	const std::lock_guard<std::mutex> lock(s_db_mutex);
	return s_entry_count;
}
