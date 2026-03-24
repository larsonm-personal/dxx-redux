/*
 * test_fingerprint.c -- Unit tests for fingerprint_gen.c
 *
 * Verifies that our fingerprint pipeline produces correct results by
 * comparing against:
 *   1. Direct chromaprint API calls (raw PCM test files)
 *   2. fpcalc reference output (test.mp3)
 *   3. Expected duration values computed from file sizes
 *
 * Test data comes from chromaprint's own test suite, fetched by CMake
 * into _deps/chromaprint-src/tests/data/.
 *
 * Build via CMake:
 *   cmake --build android/tests/build --config Release --target test_fingerprint
 *
 * Run:
 *   cd android/tests/build && ctest -C Release -R fingerprint
 *   -- or --
 *   android/tests/build/Release/test_fingerprint.exe <path-to-chromaprint-test-data>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <chromaprint.h>
#include "fingerprint_gen.h"

/* ── Mini test framework ──────────────────────────────────────────── */

static int s_tests_run = 0;
static int s_tests_passed = 0;
static int s_tests_failed = 0;

#define TEST_BEGIN(name) \
	do { \
		s_tests_run++; \
		const char *_test_name = (name); \
		printf("  %-50s ", _test_name); \
		fflush(stdout);

#define TEST_END \
		s_tests_passed++; \
		printf("PASS\n"); \
	} while (0)

#define ASSERT_TRUE(cond, msg) \
	do { if (!(cond)) { \
		printf("FAIL: %s (line %d)\n", (msg), __LINE__); \
		s_tests_failed++; \
		goto _test_cleanup; \
	}} while (0)

#define ASSERT_EQ_INT(a, b, msg) \
	do { if ((a) != (b)) { \
		printf("FAIL: %s -- expected %d, got %d (line %d)\n", \
		       (msg), (int)(a), (int)(b), __LINE__); \
		s_tests_failed++; \
		goto _test_cleanup; \
	}} while (0)

#define ASSERT_NEAR(a, b, tol, msg) \
	do { if (fabs((double)(a) - (double)(b)) > (tol)) { \
		printf("FAIL: %s -- expected ~%.3f, got %.3f (line %d)\n", \
		       (msg), (double)(a), (double)(b), __LINE__); \
		s_tests_failed++; \
		goto _test_cleanup; \
	}} while (0)

/* ── Helpers ──────────────────────────────────────────────────────── */

static char s_data_dir[1024];

static void make_path(char *buf, int bufsz, const char *filename)
{
	snprintf(buf, bufsz, "%s/%s", s_data_dir, filename);
}

/* Load a raw PCM file into memory. Returns number of int16 values. */
static int load_raw_file(const char *filename, int16_t **out_data)
{
	char path[1024];
	make_path(path, sizeof(path), filename);

	FILE *f = fopen(path, "rb");
	if (!f) { fprintf(stderr, "Cannot open %s\n", path); return -1; }
	fseek(f, 0, SEEK_END);
	long sz = ftell(f);
	fseek(f, 0, SEEK_SET);

	*out_data = (int16_t *)malloc(sz);
	if (!*out_data) { fclose(f); return -1; }
	fread(*out_data, 1, sz, f);
	fclose(f);
	return (int)(sz / sizeof(int16_t));
}

/* Parse test.mp3.fpcalc.out: lines DURATION=N and FINGERPRINT=v1,v2,...
 * Returns number of raw values parsed. */
static int load_fpcalc_reference(const char *filename,
                                 int *out_duration,
                                 uint32_t *out_raw, int max_raw)
{
	char path[1024];
	make_path(path, sizeof(path), filename);

	FILE *f = fopen(path, "r");
	if (!f) return -1;

	char line[4096];
	int count = 0;
	*out_duration = 0;

	while (fgets(line, sizeof(line), f)) {
		if (strncmp(line, "DURATION=", 9) == 0) {
			*out_duration = atoi(line + 9);
		} else if (strncmp(line, "FINGERPRINT=", 12) == 0) {
			char *p = line + 12;
			while (*p && count < max_raw) {
				out_raw[count++] = (uint32_t)strtoul(p, &p, 10);
				if (*p == ',') p++;
			}
		}
	}
	fclose(f);
	return count;
}

/* XOR-popcount similarity between two raw fingerprint arrays */
#if defined(_MSC_VER)
#include <intrin.h>
#define POPCNT(x) __popcnt(x)
#else
#define POPCNT(x) __builtin_popcount(x)
#endif

static float fp_similarity(const uint32_t *a, int a_len,
                           const uint32_t *b, int b_len)
{
	int overlap = a_len < b_len ? a_len : b_len;
	if (overlap < 5) return 0.0f;
	long total = 0;
	for (int i = 0; i < overlap; i++)
		total += POPCNT(a[i] ^ b[i]);
	return 1.0f - (float)total / ((float)overlap * 32.0f);
}

/* ── Tests ────────────────────────────────────────────────────────── */

/*
 * Test: Feed test_stereo_44100.raw through our fingerprint_from_pcm() as
 * stereo 44100 Hz, and also through the direct chromaprint API. Both
 * must produce identical base64 fingerprints.
 */
static void test_stereo_raw_matches_direct_api(void)
{
	int16_t *data = NULL;
	fingerprint_result_t our_fp = {0};
	char *direct_encoded = NULL;
	ChromaprintContext *ctx = NULL;

	TEST_BEGIN("stereo_raw_matches_direct_api");

	int total_int16 = load_raw_file("test_stereo_44100.raw", &data);
	ASSERT_TRUE(total_int16 > 0 && data, "load test_stereo_44100.raw");

	/* Our pipeline: total_samples is per-channel frame count */
	int frames = total_int16 / 2; /* stereo */
	int rc = fingerprint_from_pcm(data, frames, 44100, 2, &our_fp);
	ASSERT_TRUE(rc == 0, "fingerprint_from_pcm stereo");
	ASSERT_TRUE(our_fp.encoded && our_fp.encoded[0], "encoded not empty");

	/* Direct chromaprint API: feed total int16 count */
	ctx = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
	ASSERT_TRUE(ctx, "chromaprint_new");
	ASSERT_TRUE(chromaprint_start(ctx, 44100, 2), "chromaprint_start");
	ASSERT_TRUE(chromaprint_feed(ctx, data, total_int16), "chromaprint_feed");
	ASSERT_TRUE(chromaprint_finish(ctx), "chromaprint_finish");
	ASSERT_TRUE(chromaprint_get_fingerprint(ctx, &direct_encoded), "get_fp");

	/* Must be identical */
	ASSERT_TRUE(strcmp(our_fp.encoded, direct_encoded) == 0,
	            "fingerprints must match direct API");

	TEST_END;

_test_cleanup:
	free(data);
	fingerprint_free(&our_fp);
	if (direct_encoded) chromaprint_dealloc(direct_encoded);
	if (ctx) chromaprint_free(ctx);
}

/*
 * Test: Same as above but with mono 44100 Hz data.
 */
static void test_mono_raw_matches_direct_api(void)
{
	int16_t *data = NULL;
	fingerprint_result_t our_fp = {0};
	char *direct_encoded = NULL;
	ChromaprintContext *ctx = NULL;

	TEST_BEGIN("mono_raw_matches_direct_api");

	int total_int16 = load_raw_file("test_mono_44100.raw", &data);
	ASSERT_TRUE(total_int16 > 0 && data, "load test_mono_44100.raw");

	/* Our pipeline: mono, so total_samples == total int16 values */
	int rc = fingerprint_from_pcm(data, total_int16, 44100, 1, &our_fp);
	ASSERT_TRUE(rc == 0, "fingerprint_from_pcm mono");
	ASSERT_TRUE(our_fp.encoded && our_fp.encoded[0], "encoded not empty");

	/* Direct chromaprint API */
	ctx = chromaprint_new(CHROMAPRINT_ALGORITHM_DEFAULT);
	ASSERT_TRUE(ctx, "chromaprint_new");
	ASSERT_TRUE(chromaprint_start(ctx, 44100, 1), "chromaprint_start");
	ASSERT_TRUE(chromaprint_feed(ctx, data, total_int16), "chromaprint_feed");
	ASSERT_TRUE(chromaprint_finish(ctx), "chromaprint_finish");
	ASSERT_TRUE(chromaprint_get_fingerprint(ctx, &direct_encoded), "get_fp");

	ASSERT_TRUE(strcmp(our_fp.encoded, direct_encoded) == 0,
	            "fingerprints must match direct API");

	TEST_END;

_test_cleanup:
	free(data);
	fingerprint_free(&our_fp);
	if (direct_encoded) chromaprint_dealloc(direct_encoded);
	if (ctx) chromaprint_free(ctx);
}

/*
 * Test: Duration from stereo raw PCM file.
 * test_stereo_44100.raw: 352800 bytes = 176400 int16 = 88200 frames stereo
 * Expected: 88200 / 44100 = 2.0 seconds = 2000 ms
 */
static void test_duration_stereo(void)
{
	int16_t *data = NULL;
	fingerprint_result_t fp = {0};

	TEST_BEGIN("duration_stereo");

	int total_int16 = load_raw_file("test_stereo_44100.raw", &data);
	ASSERT_TRUE(total_int16 > 0, "load file");
	/* 352800 bytes / 2 = 176400 int16 values */
	ASSERT_EQ_INT(176400, total_int16, "expected 176400 int16 values");

	int frames = total_int16 / 2;
	int rc = fingerprint_from_pcm(data, frames, 44100, 2, &fp);
	ASSERT_TRUE(rc == 0, "fingerprint_from_pcm");
	ASSERT_EQ_INT(2000, fp.duration_ms, "duration should be 2000ms");

	TEST_END;

_test_cleanup:
	free(data);
	fingerprint_free(&fp);
}

/*
 * Test: Duration from mono raw PCM file.
 * test_mono_44100.raw: 176400 bytes = 88200 int16 = 88200 frames mono
 * Expected: 88200 / 44100 = 2.0 seconds = 2000 ms
 */
static void test_duration_mono(void)
{
	int16_t *data = NULL;
	fingerprint_result_t fp = {0};

	TEST_BEGIN("duration_mono");

	int total_int16 = load_raw_file("test_mono_44100.raw", &data);
	ASSERT_TRUE(total_int16 > 0, "load file");
	ASSERT_EQ_INT(88200, total_int16, "expected 88200 int16 values");

	int rc = fingerprint_from_pcm(data, total_int16, 44100, 1, &fp);
	ASSERT_TRUE(rc == 0, "fingerprint_from_pcm");
	ASSERT_EQ_INT(2000, fp.duration_ms, "duration should be 2000ms");

	TEST_END;

_test_cleanup:
	free(data);
	fingerprint_free(&fp);
}

/*
 * Test: Fingerprint test.mp3 using our pipeline and compare against
 * the known-good fpcalc reference output (test.mp3.fpcalc.out).
 *
 * Different MP3 decoders may produce very slightly different PCM, so
 * we compare raw fingerprint arrays at similarity >= 0.95 rather than
 * requiring an exact match.
 */
static void test_mp3_matches_fpcalc_reference(void)
{
	fingerprint_result_t fp = {0};
	uint32_t ref_raw[512];

	TEST_BEGIN("mp3_matches_fpcalc_reference");

	/* Load fpcalc reference */
	int ref_duration = 0;
	int ref_count = load_fpcalc_reference("test.mp3.fpcalc.out",
	                                      &ref_duration, ref_raw, 512);
	ASSERT_TRUE(ref_count > 0, "load fpcalc reference");
	ASSERT_EQ_INT(10, ref_duration, "fpcalc ref duration should be 10s");

	/* Fingerprint the MP3 */
	char mp3_path[1024];
	make_path(mp3_path, sizeof(mp3_path), "test.mp3");
	int rc = fingerprint_from_audio_file(mp3_path, &fp);
	ASSERT_TRUE(rc == 0, "fingerprint_from_audio_file");

	/* Duration should be close to 10s (= 10000ms, within 500ms tolerance
	 * because MP3 frames have inherent padding) */
	ASSERT_NEAR(10000.0, fp.duration_ms, 500.0, "duration ~10s");

	/* Raw fingerprint similarity >= 0.95 (different decoders) */
	ASSERT_TRUE(fp.raw_fp && fp.fp_len > 0, "has raw fingerprint");
	float sim = fp_similarity(fp.raw_fp, fp.fp_len, ref_raw, ref_count);
	printf("(sim=%.4f) ", sim);
	ASSERT_TRUE(sim >= 0.95f, "similarity >= 0.95 vs fpcalc reference");

	TEST_END;

_test_cleanup:
	fingerprint_free(&fp);
}

/*
 * Test: Stereo and mono fingerprints of the same underlying audio should
 * differ (chromaprint processes them differently). This guards against
 * accidentally treating stereo data as mono or vice versa.
 */
static void test_stereo_mono_differ(void)
{
	int16_t *data = NULL;
	fingerprint_result_t fp_stereo = {0};
	fingerprint_result_t fp_mono = {0};

	TEST_BEGIN("stereo_mono_differ");

	int total_int16 = load_raw_file("test_stereo_44100.raw", &data);
	ASSERT_TRUE(total_int16 > 0, "load file");

	/* Fingerprint as stereo */
	int frames = total_int16 / 2;
	int rc = fingerprint_from_pcm(data, frames, 44100, 2, &fp_stereo);
	ASSERT_TRUE(rc == 0, "fingerprint stereo");

	/* Fingerprint same data buffer as mono (pretend it's mono) */
	rc = fingerprint_from_pcm(data, total_int16, 44100, 1, &fp_mono);
	ASSERT_TRUE(rc == 0, "fingerprint mono");

	/* Fingerprints should be different */
	ASSERT_TRUE(fp_stereo.encoded && fp_mono.encoded, "both have encoded");
	ASSERT_TRUE(strcmp(fp_stereo.encoded, fp_mono.encoded) != 0,
	            "stereo and mono fingerprints must differ");

	/* Durations also differ: stereo = 2s, mono = 4s (same data length) */
	ASSERT_EQ_INT(2000, fp_stereo.duration_ms, "stereo duration");
	ASSERT_EQ_INT(4000, fp_mono.duration_ms, "mono duration (same data as mono)");

	TEST_END;

_test_cleanup:
	free(data);
	fingerprint_free(&fp_stereo);
	fingerprint_free(&fp_mono);
}

/* ── Main ─────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: test_fingerprint <chromaprint-test-data-dir>\n");
		fprintf(stderr, "  e.g.: test_fingerprint path/to/_deps/chromaprint-src/tests/data\n");
		return 1;
	}

	strncpy(s_data_dir, argv[1], sizeof(s_data_dir) - 1);
	printf("Test data: %s\n\n", s_data_dir);

	test_stereo_raw_matches_direct_api();
	test_mono_raw_matches_direct_api();
	test_duration_stereo();
	test_duration_mono();
	test_mp3_matches_fpcalc_reference();
	test_stereo_mono_differ();

	printf("\n%d/%d passed", s_tests_passed, s_tests_run);
	if (s_tests_failed > 0)
		printf(", %d FAILED", s_tests_failed);
	printf("\n");

	return s_tests_failed > 0 ? 1 : 0;
}
