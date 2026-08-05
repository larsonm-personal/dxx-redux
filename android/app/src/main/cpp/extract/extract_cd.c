/*
 * extract_cd.c — Standalone tool to extract game files from BIN/CUE CD images
 *                and compute per-track SHA-1 hashes.
 *
 * Usage: extract_cd <cue_file> [output_dir]
 *
 * Parses the CUE sheet, opens BIN file(s), extracts game files from ISO 9660
 * data tracks to <output_dir>/data_tracks/ (default: alongside the CUE file),
 * and prints per-track SHA-1 hashes as JSON lines to stdout.
 *
 * Build (via CMake from repo root):
 *   cmake -S android/app/src/main/cpp/extract -B android/tests/build
 *   cmake --build android/tests/build --config Release --target extract_cd
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#define strcasecmp         _stricmp
#define open_bin(p)        _open(p, _O_RDONLY | _O_BINARY)
#define close_fd(fd)       _close(fd)
#define read_fd(fd, b, n)  _read(fd, b, (unsigned int) (n))
#define lseek_fd(fd, o, w) _lseeki64(fd, o, w)
#define mkdir_p(d)         _mkdir(d)
#define stat_file          _stat64
#define stat_t             struct __stat64
#else
#include <fcntl.h>
#include <unistd.h>
#define open_bin(p)        open(p, O_RDONLY)
#define close_fd(fd)       close(fd)
#define read_fd(fd, b, n)  read(fd, b, n)
#define lseek_fd(fd, o, w) lseek(fd, o, w)
#define mkdir_p(d)         mkdir(d, 0755)
#define stat_file          stat
#define stat_t             struct stat
#endif

#include "cue_parser.h"
#include "cd_read_contract.h"
#include "hfs_reader.h"
#include "iso9660_reader.h"
#include "mac_hfs_extract.h"
#include "sow_extract.h"
#include "game_file_extensions.h"
#include "json_writer.h"

/* ── Minimal SHA-1 (RFC 3174) ──────────────────────────────────────── */

typedef struct {
	unsigned int state[5];
	unsigned int count[2];
	unsigned char buffer[64];
} SHA1_CTX;

#define SHA1_ROL(v, b) (((v) << (b)) | ((v) >> (32 - (b))))
#define SHA1_BLK0(i)   (block[i] = (block[i] << 24) | (((block[i]) & 0xFF00) << 8) | (((block[i]) >> 8) & 0xFF00) | ((block[i]) >> 24))
#define SHA1_BLK(i)    (block[i & 15] = SHA1_ROL(block[(i + 13) & 15] ^ block[(i + 8) & 15] ^ block[(i + 2) & 15] ^ block[i & 15], 1))
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
	unsigned char finalcount[8];
	unsigned char c;
	for (i = 0; i < 8; i++)
		finalcount[i] = (unsigned char) ((ctx->count[(i >= 4) ? 0 : 1] >> ((3 - (i & 3)) * 8)) & 255);
	c = 0200;
	sha1_update(ctx, &c, 1);
	while ((ctx->count[0] & 504) != 448) {
		c = 0000;
		sha1_update(ctx, &c, 1);
	}
	sha1_update(ctx, finalcount, 8);
	for (i = 0; i < 20; i++)
		digest[i] = (unsigned char) ((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
}

static void sha1_hex(const unsigned char digest[20], char hex[41])
{
	int i;
	for (i = 0; i < 20; i++)
		sprintf(hex + i * 2, "%02x", digest[i]);
	hex[40] = '\0';
}

/* ── File helpers ────────────────────────────────────────────────────── */

static long long file_size(const char *path)
{
	stat_t st;
	if (stat_file(path, &st) != 0) return -1;
	return (long long) st.st_size;
}

/* Get directory component of a path */
static void path_dir(const char *path, char *dir, int dir_len)
{
	const char *last_sep = NULL, *p;
	for (p = path; *p; p++) {
		if (*p == '/' || *p == '\\') last_sep = p;
	}
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

/* Join two path components */
static void path_join(char *out, int out_len, const char *a, const char *b)
{
	snprintf(out, out_len, "%s/%s", a, b);
}

static int path_has_extension(const char *path, const char *ext)
{
	const char *dot = strrchr(path, '.');

	if (!dot || !ext) return 0;
	return strcasecmp(dot, ext) == 0;
}

static int progress_cb(const char *filename, long long done, long long total, void *ud);

static int hash_file_bytes(const char *path, char sha_hex[41])
{
	SHA1_CTX sha_ctx;
	unsigned char sha_digest[20];
	unsigned char buf[64 * 1024];
	int fd, n;

	fd = open_bin(path);
	if (fd < 0)
		return -1;

	sha1_init(&sha_ctx);
	for (;;) {
		n = read_fd(fd, buf, sizeof(buf));
		if (n < 0) {
			close_fd(fd);
			return -1;
		}
		if (n == 0)
			break;
		sha1_update(&sha_ctx, buf, (unsigned int) n);
	}
	close_fd(fd);

	sha1_final(sha_digest, &sha_ctx);
	sha1_hex(sha_digest, sha_hex);
	return 0;
}

static void print_iso_json_line(const char *sha_hex, int files_extracted, const char *error)
{
	if (error) {
		if (sha_hex && *sha_hex) {
			printf("{\"track\": 1, \"type\": \"data\", \"sha1\": ");
			json_write_string(stdout, sha_hex);
			printf(", \"source_format\": \"iso\", \"error\": ");
		} else {
			printf("{\"track\": 1, \"type\": \"data\", \"source_format\": \"iso\", \"error\": ");
		}
		json_write_string(stdout, error);
		printf("}\n");
		return;
	}

	printf("{\"track\": 1, \"type\": \"data\", \"sha1\": ");
	json_write_string(stdout, sha_hex);
	printf(", \"files_extracted\": %d, \"source_format\": \"iso\"}\n",
	       files_extracted);
}

static void extract_iso_image(const char *iso_path,
                              const char *out_dir,
                              int *data_tracks_extracted,
                              int *total_files_extracted,
                              int *errors)
{
	char sha_hex[41] = "";
	iso_file_list_t *file_list;
	int iso_fd, nf;

	if (hash_file_bytes(iso_path, sha_hex) < 0) {
		fprintf(stderr, "ERROR: Cannot hash ISO: %s\n", iso_path);
		print_iso_json_line(NULL, 0, "cannot hash ISO");
		(*errors)++;
		return;
	}

	iso_fd = open_bin(iso_path);
	if (iso_fd < 0) {
		fprintf(stderr, "ERROR: Cannot open ISO: %s\n", iso_path);
		print_iso_json_line(sha_hex, 0, "cannot open ISO");
		(*errors)++;
		return;
	}

	file_list = iso_file_list_create();
	if (!file_list) {
		fprintf(stderr, "ERROR: Cannot allocate ISO catalog\n");
		print_iso_json_line(sha_hex, 0, "cannot allocate ISO catalog");
		(*errors)++;
		close_fd(iso_fd);
		return;
	}

	nf = iso_list_image_files(iso_fd, file_list);
	if (nf > 0) {
		int extracted;

		mkdir_p(out_dir);
		extracted = iso_extract_image_files(iso_fd, file_list, out_dir,
		                                    NULL, progress_cb, NULL);
		if (extracted >= 0) {
			if (extracted > 0) {
				(*data_tracks_extracted)++;
				(*total_files_extracted) += extracted;
			}
			fprintf(stderr, "  ISO image: extracted %d files to %s\n",
			        extracted, out_dir);
			print_iso_json_line(sha_hex, extracted, NULL);
		} else {
			fprintf(stderr, "  ISO image: extraction failed\n");
			print_iso_json_line(sha_hex, 0, "ISO extraction failed");
			(*errors)++;
		}
	} else if (nf == 0) {
		fprintf(stderr, "  ISO image: no files found\n");
		print_iso_json_line(sha_hex, 0, NULL);
	} else {
		fprintf(stderr, "  ISO image: listing failed\n");
		print_iso_json_line(sha_hex, 0, "ISO listing failed");
		(*errors)++;
	}

	iso_file_list_destroy(file_list);
	close_fd(iso_fd);
}

/* ── Main ────────────────────────────────────────────────────────────── */

static int progress_cb(const char *filename, long long done, long long total, void *ud)
{
	(void) ud;
	(void) done;
	(void) total;
	(void) filename;
	return 0; /* continue */
}

int main(int argc, char *argv[])
{
	char source_dir[1024], out_dir[1024];
	char *cue_text;
	cue_disc_t disc;
	long long bin_sizes[CUE_MAX_FILES];
	int i, num_tracks, data_tracks_extracted = 0, total_files_extracted = 0;
	int errors = 0;
	int is_iso;

	if (argc < 2) {
		fprintf(stderr, "Usage: extract_cd <cue_or_iso_file> [output_dir]\n");
		return 1;
	}

	is_iso = path_has_extension(argv[1], ".iso");
	path_dir(argv[1], source_dir, sizeof(source_dir));

	/* Determine output directory */
	if (argc >= 3) {
		snprintf(out_dir, sizeof(out_dir), "%s", argv[2]);
	} else {
		path_join(out_dir, sizeof(out_dir), source_dir, "data_tracks");
	}

	if (is_iso) {
		fprintf(stderr, "Processing ISO image: %s\n", argv[1]);
		extract_iso_image(argv[1], out_dir,
		                  &data_tracks_extracted,
		                  &total_files_extracted,
		                  &errors);
	} else {

		/* Read CUE sheet */
		{
			if (!cd_read_file_exact(argv[1], CD_CUE_MAX_BYTES, &cue_text, NULL)) {
				fprintf(stderr, "ERROR: Cannot read complete CUE file: %s\n", argv[1]);
				return 1;
			}
		}
		/* First pass: parse without sizes to get file count and filenames */
		memset(&disc, 0, sizeof(disc));
		num_tracks = cue_parse(cue_text, NULL, 0, &disc);
		if (num_tracks <= 0) {
			fprintf(stderr, "ERROR: Failed to parse CUE file: %s\n", argv[1]);
			free(cue_text);
			return 1;
		}

		/* Get BIN file sizes */
		{
			int nfiles = disc.num_files;
			for (i = 0; i < nfiles; i++) {
				char bin_path[1024];
				path_join(bin_path, sizeof(bin_path), source_dir, disc.files[i].filename);
				bin_sizes[i] = file_size(bin_path);
				if (bin_sizes[i] < 0) {
					fprintf(stderr, "ERROR: Cannot stat BIN file: %s\n", bin_path);
					free(cue_text);
					return 1;
				}
			}

			/* Re-parse with sizes for correct sector counts */
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
			long long track_offset;
			int s;
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

			path_join(bin_path, sizeof(bin_path), source_dir, disc.files[t->file_index].filename);
			bin_fd = open_bin(bin_path);
			if (bin_fd < 0) {
				fprintf(stderr, "ERROR: Cannot open BIN: %s\n", bin_path);
				errors++;
				printf("{\"track\": %d, \"type\": \"%s\", \"error\": \"cannot open BIN\"}\n",
				       t->track_num, t->type == CUE_TRACK_AUDIO ? "audio" : "data");
				continue;
			}

			/* Hash the full raw sectors for this track (redump convention) */
			sha1_init(&sha_ctx);

			/* Seek to track start */
			if (lseek_fd(bin_fd, track_offset, SEEK_SET) != track_offset) {
				fprintf(stderr, "ERROR: Seek failed for track %d\n", t->track_num);
				printf("{\"track\": %d, \"type\": \"%s\", \"error\": \"seek failed\"}\n",
				       t->track_num, track_type);
				close_fd(bin_fd);
				errors++;
				continue;
			}

			for (s = 0; s < t->num_sectors; s++) {
				int n = read_fd(bin_fd, sector_buf, CUE_SECTOR_SIZE);
				if (n != CUE_SECTOR_SIZE) {
					fprintf(stderr, "ERROR: Short read on track %d sector %d (got %d)\n",
					        t->track_num, s, n);
					break;
				}
				sha1_update(&sha_ctx, sector_buf, CUE_SECTOR_SIZE);
			}

			if (s != t->num_sectors) {
				printf("{\"track\": %d, \"type\": \"%s\", \"error\": \"short read\"}\n",
				       t->track_num, track_type);
				close_fd(bin_fd);
				errors++;
				continue;
			}
			sha1_final(sha_digest, &sha_ctx);
			sha1_hex(sha_digest, sha_hex);

			/* Extract ISO files from data tracks */
			if (t->type == CUE_TRACK_DATA) {
				iso_file_list_t *file_list;
				int nf;

				/* Reopen for ISO reader (it uses its own seeking) */
				close_fd(bin_fd);
				bin_fd = open_bin(bin_path);
				if (bin_fd < 0) {
					fprintf(stderr, "ERROR: Cannot reopen BIN for ISO: %s\n", bin_path);
					errors++;
					printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\", \"error\": \"cannot reopen for ISO\"}\n",
					       t->track_num, sha_hex);
					continue;
				}

				file_list = iso_file_list_create();
				if (!file_list) {
					fprintf(stderr, "ERROR: Cannot allocate ISO catalog\n");
					errors++;
					printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\", \"error\": \"cannot allocate ISO catalog\"}\n",
					       t->track_num, sha_hex);
					close_fd(bin_fd);
					continue;
				}

				nf = iso_list_files(bin_fd, t->start_sector, t->num_sectors, file_list);
				if (nf > 0) {
					int extracted;
					mkdir_p(out_dir);
					extracted = iso_extract_files(bin_fd, t->start_sector, t->num_sectors,
					                              file_list, out_dir, NULL, progress_cb, NULL);
					if (extracted > 0) {
						data_tracks_extracted++;
						total_files_extracted += extracted;
						fprintf(stderr, "  Track %d: extracted %d files to %s\n",
						        t->track_num, extracted, out_dir);
						printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\", \"files_extracted\": %d}\n",
						       t->track_num, sha_hex, extracted);
					} else {
						fprintf(stderr, "  Track %d: ISO extraction failed\n", t->track_num);
						printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\", \"files_extracted\": 0}\n",
						       t->track_num, sha_hex);
					}
				} else if (nf == 0) {
					fprintf(stderr, "  Track %d: no files found in ISO\n", t->track_num);
					printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\", \"files_extracted\": 0}\n",
					       t->track_num, sha_hex);
				} else {
					hfs_partition_info_t hfs_info;

					if (hfs_find_partition(bin_fd, t->start_sector, t->num_sectors, &hfs_info) == 0) {
						int extracted;

						fprintf(stderr,
						        "  Track %d: detected HFS volume '%s' (%u blocks)\n",
						        t->track_num,
						        hfs_info.volume_name[0] ? hfs_info.volume_name : hfs_info.partition_name,
						        hfs_info.partition_block_count);

						extracted = mac_extract_files_from_hfs_track(bin_fd, t->start_sector, t->num_sectors,
						                                             out_dir,
						                                             NULL,
						                                             dxx_android_mac_disc_extract_extensions,
						                                             progress_cb,
						                                             NULL);
						if (extracted > 0) {
							data_tracks_extracted++;
							total_files_extracted += extracted;
							fprintf(stderr, "  Track %d: extracted %d files from HFS to %s\n",
							        t->track_num, extracted, out_dir);
							printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\", \"filesystem\": \"hfs\", \"files_extracted\": %d}\n",
							       t->track_num, sha_hex, extracted);
						} else {
							fprintf(stderr, "  Track %d: HFS extraction failed\n", t->track_num);
							printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\", \"filesystem\": \"hfs\", \"error\": \"HFS extraction failed\"}\n",
							       t->track_num, sha_hex);
							errors++;
						}
					} else {
						fprintf(stderr, "  Track %d: ISO listing failed (not ISO 9660?)\n", t->track_num);
						printf("{\"track\": %d, \"type\": \"data\", \"sha1\": \"%s\", \"error\": \"ISO listing failed\"}\n",
						       t->track_num, sha_hex);
						errors++;
					}
				}
				iso_file_list_destroy(file_list);
			} else {
				/* Audio track — just output the hash */
				printf("{\"track\": %d, \"type\": \"audio\", \"sha1\": \"%s\"}\n",
				       t->track_num, sha_hex);
			}

			close_fd(bin_fd);
		}
	}

	fprintf(stderr, "\nDone: %d data tracks extracted, %d total files, %d errors\n",
	        data_tracks_extracted, total_files_extracted, errors);

	/* ── Post-ISO: scan for and extract .sow archives ────────────── */
	if (data_tracks_extracted > 0) {
		sow_file_list_t sow_list;
		int nsow = sow_scan_dir(out_dir, &sow_list);
		if (nsow > 0) {
			fprintf(stderr, "\nFound %d .sow archive(s), extracting...\n", nsow);
			for (int si = 0; si < sow_list.count; si++) {
				/* Extract to the directory containing the .sow file */
				char sow_dir[1024];
				path_dir(sow_list.paths[si], sow_dir, sizeof(sow_dir));

				/* Relative path for JSON output (forward slashes for valid JSON) */
				const char *rel = sow_list.paths[si];
				if (strncmp(rel, out_dir, strlen(out_dir)) == 0)
					rel += strlen(out_dir) + 1; /* skip out_dir + separator */
				char rel_json[1024];
				snprintf(rel_json, sizeof(rel_json), "%s", rel);
				for (char *p = rel_json; *p; p++)
					if (*p == '\\') *p = '/';

				fprintf(stderr, "  Extracting %s...\n", rel);
				int sow_count = sow_extract(sow_list.paths[si], sow_dir,
				                            NULL, progress_cb, NULL);
				if (sow_count >= 0) {
					printf("{\"sow\": ");
					json_write_string(stdout, rel_json);
					printf(", \"files_extracted\": %d}\n", sow_count);
					total_files_extracted += sow_count;
					fprintf(stderr, "  Extracted %d files from %s\n",
					        sow_count, rel);
				} else {
					printf("{\"sow\": ");
					json_write_string(stdout, rel_json);
					printf(", \"error\": \"extraction failed\"}\n");
					errors++;
				}
			}
		}
	}

	return errors > 0 ? 1 : 0;
}
