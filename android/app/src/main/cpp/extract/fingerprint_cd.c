/*
 * fingerprint_cd.c -- Standalone tool to fingerprint audio tracks from BIN/CUE
 *                     CD images using Chromaprint.
 *
 * Usage: fingerprint_cd <cue_file>
 *
 * Parses the CUE sheet, reads raw CD-DA audio sectors from BIN file(s),
 * and outputs per-track JSON lines to stdout with SHA-1 hash, Chromaprint
 * base64 fingerprint, and duration in milliseconds.
 *
 * Data tracks get SHA-1 only (no fingerprint).  Audio tracks get all three.
 *
 * Build (via CMake from repo root):
 *   cmake -S android/app/src/main/cpp/extract -B android/tests/build
 *   cmake --build android/tests/build --config Release --target fingerprint_cd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define open_bin(p)        _open(p, _O_RDONLY | _O_BINARY)
#define close_fd(fd)       _close(fd)
#define read_fd(fd, b, n)  _read(fd, b, (unsigned int) (n))
#define lseek_fd(fd, o, w) _lseeki64(fd, o, w)
#define stat_file          _stat64
#define stat_t             struct __stat64
#else
#include <fcntl.h>
#include <unistd.h>
#define open_bin(p)        open(p, O_RDONLY)
#define close_fd(fd)       close(fd)
#define read_fd(fd, b, n)  read(fd, b, n)
#define lseek_fd(fd, o, w) lseek(fd, o, w)
#define stat_file          stat
#define stat_t             struct stat
#endif

#include "cue_parser.h"
#include "cd_read_contract.h"
#include "fingerprint_gen.h"
#include "json_writer.h"

/* ── Minimal SHA-1 (same implementation as extract_cd.c) ──────────── */

typedef struct {
	unsigned int state[5];
	unsigned int count[2];
	unsigned char buffer[64];
} SHA1_CTX;

#define SHA1_ROL(v, b) (((v) << (b)) | ((v) >> (32 - (b))))
#define SHA1_BLK0(i)                                              \
	(block[i] = (block[i] << 24) | (((block[i]) & 0xFF00) << 8) | \
	            (((block[i]) >> 8) & 0xFF00) | ((block[i]) >> 24))
#define SHA1_BLK(i)                                                        \
	(block[i & 15] = SHA1_ROL(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ \
	                              block[(i + 2) & 15] ^ block[i & 15],     \
	                          1))
#define SHA1_R0(v, w, x, y, z, i)                                          \
	z += ((w & (x ^ y)) ^ y) + SHA1_BLK0(i) + 0x5A827999 + SHA1_ROL(v, 5); \
	w = SHA1_ROL(w, 30);
#define SHA1_R1(v, w, x, y, z, i)                                         \
	z += ((w & (x ^ y)) ^ y) + SHA1_BLK(i) + 0x5A827999 + SHA1_ROL(v, 5); \
	w = SHA1_ROL(w, 30);
#define SHA1_R2(v, w, x, y, z, i)                                 \
	z += (w ^ x ^ y) + SHA1_BLK(i) + 0x6ED9EBA1 + SHA1_ROL(v, 5); \
	w = SHA1_ROL(w, 30);
#define SHA1_R3(v, w, x, y, z, i)                                               \
	z += (((w | x) & y) | (w & x)) + SHA1_BLK(i) + 0x8F1BBCDC + SHA1_ROL(v, 5); \
	w = SHA1_ROL(w, 30);
#define SHA1_R4(v, w, x, y, z, i)                                 \
	z += (w ^ x ^ y) + SHA1_BLK(i) + 0xCA62C1D6 + SHA1_ROL(v, 5); \
	w = SHA1_ROL(w, 30);

static void sha1_transform(unsigned int state[5], const unsigned char buf[64])
{
	unsigned int a, b, c, d, e;
	unsigned int block[16];
	memcpy(block, buf, 64);
	a = state[0];
	b = state[1];
	c = state[2];
	d = state[3];
	e = state[4];
	SHA1_R0(a, b, c, d, e, 0);
	SHA1_R0(e, a, b, c, d, 1);
	SHA1_R0(d, e, a, b, c, 2);
	SHA1_R0(c, d, e, a, b, 3);
	SHA1_R0(b, c, d, e, a, 4);
	SHA1_R0(a, b, c, d, e, 5);
	SHA1_R0(e, a, b, c, d, 6);
	SHA1_R0(d, e, a, b, c, 7);
	SHA1_R0(c, d, e, a, b, 8);
	SHA1_R0(b, c, d, e, a, 9);
	SHA1_R0(a, b, c, d, e, 10);
	SHA1_R0(e, a, b, c, d, 11);
	SHA1_R0(d, e, a, b, c, 12);
	SHA1_R0(c, d, e, a, b, 13);
	SHA1_R0(b, c, d, e, a, 14);
	SHA1_R0(a, b, c, d, e, 15);
	SHA1_R1(e, a, b, c, d, 16);
	SHA1_R1(d, e, a, b, c, 17);
	SHA1_R1(c, d, e, a, b, 18);
	SHA1_R1(b, c, d, e, a, 19);
	SHA1_R2(a, b, c, d, e, 20);
	SHA1_R2(e, a, b, c, d, 21);
	SHA1_R2(d, e, a, b, c, 22);
	SHA1_R2(c, d, e, a, b, 23);
	SHA1_R2(b, c, d, e, a, 24);
	SHA1_R2(a, b, c, d, e, 25);
	SHA1_R2(e, a, b, c, d, 26);
	SHA1_R2(d, e, a, b, c, 27);
	SHA1_R2(c, d, e, a, b, 28);
	SHA1_R2(b, c, d, e, a, 29);
	SHA1_R2(a, b, c, d, e, 30);
	SHA1_R2(e, a, b, c, d, 31);
	SHA1_R3(d, e, a, b, c, 32);
	SHA1_R3(c, d, e, a, b, 33);
	SHA1_R3(b, c, d, e, a, 34);
	SHA1_R3(a, b, c, d, e, 35);
	SHA1_R3(e, a, b, c, d, 36);
	SHA1_R3(d, e, a, b, c, 37);
	SHA1_R3(c, d, e, a, b, 38);
	SHA1_R3(b, c, d, e, a, 39);
	SHA1_R3(a, b, c, d, e, 40);
	SHA1_R3(e, a, b, c, d, 41);
	SHA1_R3(d, e, a, b, c, 42);
	SHA1_R3(c, d, e, a, b, 43);
	SHA1_R3(b, c, d, e, a, 44);
	SHA1_R3(a, b, c, d, e, 45);
	SHA1_R3(e, a, b, c, d, 46);
	SHA1_R3(d, e, a, b, c, 47);
	SHA1_R4(c, d, e, a, b, 48);
	SHA1_R4(b, c, d, e, a, 49);
	SHA1_R4(a, b, c, d, e, 50);
	SHA1_R4(e, a, b, c, d, 51);
	SHA1_R4(d, e, a, b, c, 52);
	SHA1_R4(c, d, e, a, b, 53);
	SHA1_R4(b, c, d, e, a, 54);
	SHA1_R4(a, b, c, d, e, 55);
	SHA1_R4(e, a, b, c, d, 56);
	SHA1_R4(d, e, a, b, c, 57);
	SHA1_R4(c, d, e, a, b, 58);
	SHA1_R4(b, c, d, e, a, 59);
	SHA1_R4(a, b, c, d, e, 60);
	SHA1_R4(e, a, b, c, d, 61);
	SHA1_R4(d, e, a, b, c, 62);
	SHA1_R4(c, d, e, a, b, 63);
	SHA1_R4(b, c, d, e, a, 64);
	SHA1_R4(a, b, c, d, e, 65);
	SHA1_R4(e, a, b, c, d, 66);
	SHA1_R4(d, e, a, b, c, 67);
	SHA1_R4(c, d, e, a, b, 68);
	SHA1_R4(b, c, d, e, a, 69);
	SHA1_R4(a, b, c, d, e, 70);
	SHA1_R4(e, a, b, c, d, 71);
	SHA1_R4(d, e, a, b, c, 72);
	SHA1_R4(c, d, e, a, b, 73);
	SHA1_R4(b, c, d, e, a, 74);
	SHA1_R4(a, b, c, d, e, 75);
	SHA1_R4(e, a, b, c, d, 76);
	SHA1_R4(d, e, a, b, c, 77);
	SHA1_R4(c, d, e, a, b, 78);
	SHA1_R4(b, c, d, e, a, 79);
	state[0] += a;
	state[1] += b;
	state[2] += c;
	state[3] += d;
	state[4] += e;
}

static void sha1_init(SHA1_CTX *ctx)
{
	ctx->state[0] = 0x67452301;
	ctx->state[1] = 0xEFCDAB89;
	ctx->state[2] = 0x98BADCFE;
	ctx->state[3] = 0x10325476;
	ctx->state[4] = 0xC3D2E1F0;
	ctx->count[0] = ctx->count[1] = 0;
}

static void sha1_update(SHA1_CTX *ctx, const unsigned char *data, unsigned int len)
{
	unsigned int i, j;
	j = (ctx->count[0] >> 3) & 63;
	if ((ctx->count[0] += len << 3) < (len << 3)) ctx->count[1]++;
	ctx->count[1] += (len >> 29);
	i = 64 - j;
	if (len >= i) {
		memcpy(&ctx->buffer[j], data, i);
		sha1_transform(ctx->state, ctx->buffer);
		for (; i + 63 < len; i += 64)
			sha1_transform(ctx->state, &data[i]);
		j = 0;
	} else i = 0;
	memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void sha1_final(unsigned char digest[20], SHA1_CTX *ctx)
{
	unsigned int i;
	unsigned char finalcount[8], c;
	for (i = 0; i < 8; i++)
		finalcount[i] = (unsigned char) ((ctx->count[(i >= 4) ? 0 : 1] >> ((3 - (i & 3)) * 8)) & 255);
	c = 0200;
	sha1_update(ctx, &c, 1);
	while ((ctx->count[0] & 504) != 448) {
		c = 0;
		sha1_update(ctx, &c, 1);
	}
	sha1_update(ctx, finalcount, 8);
	for (i = 0; i < 20; i++)
		digest[i] = (unsigned char) ((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
}

static void sha1_hex(const unsigned char digest[20], char hex[41])
{
	int i;
	for (i = 0; i < 20; i++) sprintf(hex + i * 2, "%02x", digest[i]);
	hex[40] = '\0';
}

/* ── File helpers ────────────────────────────────────────────────────── */

static long long get_file_size(const char *path)
{
	stat_t st;
	if (stat_file(path, &st) != 0) return -1;
	return (long long) st.st_size;
}

static void path_dir(const char *path, char *dir, int dir_len)
{
	const char *last_sep = NULL, *p;
	for (p = path; *p; p++)
		if (*p == '/' || *p == '\\') last_sep = p;
	if (last_sep) {
		int len = (int) (last_sep - path);
		if (len >= dir_len) len = dir_len - 1;
		memcpy(dir, path, len);
		dir[len] = '\0';
	} else {
		dir[0] = '.';
		dir[1] = '\0';
	}
}

static void path_join(char *out, int out_len, const char *a, const char *b)
{
	snprintf(out, out_len, "%s/%s", a, b);
}

/* Escape a string for JSON output (handles quotes and backslashes) */
static void json_escape(FILE *output, const char *src)
{
	json_write_string(output, src);
}

/* ── Main ────────────────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
	char cue_dir[1024];
	char *cue_text;
	cue_disc_t disc;
	long long bin_sizes[CUE_MAX_FILES];
	int i, num_tracks;
	int errors = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: fingerprint_cd <cue_file>\n");
		return 1;
	}

	{
		if (!cd_read_file_exact(argv[1], CD_CUE_MAX_BYTES, &cue_text, NULL)) {
			fprintf(stderr, "ERROR: Cannot read complete CUE file: %s\n", argv[1]);
			return 1;
		}
	}
	path_dir(argv[1], cue_dir, sizeof(cue_dir));

	/* First pass: parse without sizes to get filenames */
	memset(&disc, 0, sizeof(disc));
	num_tracks = cue_parse(cue_text, NULL, 0, &disc);
	if (num_tracks <= 0) {
		fprintf(stderr, "ERROR: Failed to parse CUE file: %s\n", argv[1]);
		free(cue_text);
		return 1;
	}

	/* Get BIN file sizes and re-parse */
	for (i = 0; i < disc.num_files; i++) {
		char bin_path[1024];
		path_join(bin_path, sizeof(bin_path), cue_dir, disc.files[i].filename);
		bin_sizes[i] = get_file_size(bin_path);
		if (bin_sizes[i] < 0) {
			fprintf(stderr, "ERROR: Cannot stat BIN file: %s\n", bin_path);
			free(cue_text);
			return 1;
		}
	}
	{
		int nfiles = disc.num_files;
		memset(&disc, 0, sizeof(disc));
		num_tracks = cue_parse(cue_text, bin_sizes, nfiles, &disc);
	}
	free(cue_text);

	fprintf(stderr, "Parsed %d tracks from %d file(s)\n", num_tracks, disc.num_files);

	/* Process each track */
	for (i = 0; i < disc.num_tracks; i++) {
		cue_track_info_t *t = &disc.tracks[i];
		char bin_path[1024];
		int bin_fd;
		SHA1_CTX sha_ctx;
		unsigned char sha_digest[20];
		char sha_hex[41];
		unsigned char sector_buf[CUE_SECTOR_SIZE];
		fingerprint_stream_t *audio_stream = NULL;
		long long track_offset;
		int s;
		int read_failed = 0;
		const char *track_type = t->type == CUE_TRACK_AUDIO ? "audio" : "data";

		if (t->file_index < 0 || t->file_index >= disc.num_files ||
		    !cd_track_span(t->start_sector, t->num_sectors,
		                   t->file_index >= 0 && t->file_index < disc.num_files
		                       ? bin_sizes[t->file_index]
		                       : -1,
		                   &track_offset, NULL)) {
			fprintf(stderr, "ERROR: Invalid or incomplete span for track %d\n", t->track_num);
			printf("{\"track\": %d, \"type\": \"%s\", \"error\": \"invalid track span\"}\n",
			       t->track_num, track_type);
			errors++;
			continue;
		}

		path_join(bin_path, sizeof(bin_path), cue_dir, disc.files[t->file_index].filename);
		bin_fd = open_bin(bin_path);
		if (bin_fd < 0) {
			fprintf(stderr, "ERROR: Cannot open BIN: %s\n", bin_path);
			errors++;
			printf("{\"track\": %d, \"type\": \"%s\", \"error\": \"cannot open BIN\"}\n",
			       t->track_num, t->type == CUE_TRACK_AUDIO ? "audio" : "data");
			continue;
		}

		/* Hash all raw sectors (redump convention) */
		if (t->type == CUE_TRACK_AUDIO && t->num_sectors > 0) {
			audio_stream = fingerprint_stream_new(44100, 2);
		}
		sha1_init(&sha_ctx);
		if (lseek_fd(bin_fd, track_offset, SEEK_SET) != track_offset) {
			fprintf(stderr, "ERROR: Seek failed for track %d\n", t->track_num);
			printf("{\"track\": %d, \"type\": \"%s\", \"error\": \"seek failed\"}\n",
			       t->track_num, track_type);
			fingerprint_stream_free(audio_stream);
			close_fd(bin_fd);
			errors++;
			continue;
		}

		for (s = 0; s < t->num_sectors; s++) {
			int n = read_fd(bin_fd, sector_buf, CUE_SECTOR_SIZE);
			if (n != CUE_SECTOR_SIZE) {
				fprintf(stderr, "ERROR: Short read on track %d sector %d\n",
				        t->track_num, s);
				read_failed = 1;
				break;
			}
			sha1_update(&sha_ctx, sector_buf, CUE_SECTOR_SIZE);
			if (audio_stream &&
			    fingerprint_stream_feed(audio_stream,
			                            (const int16_t *) sector_buf, 588) != 0) {
				read_failed = 1;
				break;
			}
		}
		if (read_failed || s != t->num_sectors) {
			printf("{\"track\": %d, \"type\": \"%s\", \"error\": \"incomplete track read\"}\n",
			       t->track_num, track_type);
			fingerprint_stream_free(audio_stream);
			close_fd(bin_fd);
			errors++;
			continue;
		}
		sha1_final(sha_digest, &sha_ctx);
		sha1_hex(sha_digest, sha_hex);

		if (t->type == CUE_TRACK_AUDIO && t->num_sectors > 0) {
			if (!audio_stream) {
				fprintf(stderr, "ERROR: Could not initialize fingerprint for track %d\n",
				        t->track_num);
				printf("{\"track\": %d, \"type\": \"audio\", \"sha1\": \"%s\", "
				       "\"error\": \"fingerprint initialization failed\"}\n",
				       t->track_num, sha_hex);
				close_fd(bin_fd);
				errors++;
				continue;
			}
			fingerprint_result_t fp = { 0 };
			int rc = fingerprint_stream_finish(audio_stream, &fp);
			fingerprint_stream_free(audio_stream);

			if (rc == 0 && fp.encoded) {
				printf("{\"track\": %d, \"type\": \"audio\", \"sha1\": \"%s\", "
				       "\"chromaprint\": ",
				       t->track_num, sha_hex);
				json_escape(stdout, fp.encoded);
				printf(", \"duration_ms\": %d", fp.duration_ms);
				if (t->title[0]) {
					printf(", \"title\": ");
					json_escape(stdout, t->title);
				}
				printf("}\n");

				fprintf(stderr, "  Track %d: audio  sha1=%s  fp_len=%d  duration=%dms\n",
				        t->track_num, sha_hex, fp.fp_len, fp.duration_ms);
			} else {
				printf("{\"track\": %d, \"type\": \"audio\", \"sha1\": \"%s\", "
				       "\"error\": \"fingerprint failed\"}\n",
				       t->track_num, sha_hex);
				fprintf(stderr, "  Track %d: fingerprint FAILED\n", t->track_num);
				errors++;
			}
			fingerprint_free(&fp);
		} else {
			/* Data track -- SHA-1 only */
			printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\"}\n",
			       t->track_num, sha_hex);
			fprintf(stderr, "  Track %d: data   sha1=%s\n", t->track_num, sha_hex);
		}

		close_fd(bin_fd);
	}

	fprintf(stderr, "\nDone: %d tracks processed, %d errors\n",
	        disc.num_tracks, errors);
	return errors > 0 ? 1 : 0;
}
