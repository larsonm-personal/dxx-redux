/*
 * physfs_archiver_saf.c — Custom PhysFS archiver for Android SAF leave-in-place.
 *
 * This archiver presents a .saf_manifest.json file as a virtual directory
 * of game files.  When PhysFS opens a file by name, the archiver calls
 * back through JNI (via saf_open_file()) to get a native fd from Android's
 * ContentResolver, then wraps it in a pread()-based PHYSFS_Io.
 *
 * The manifest is a simple JSON file:
 * {
 *   "files": [
 *     { "filename": "descent2.hog", "content_uri": "content://...", "size_bytes": 5038817 },
 *     ...
 *   ]
 * }
 *
 * Build: Only compiled for Android (#ifdef ANDROID in CMakeLists).
 */

#include <physfs.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <android/log.h>

#include "android_profile.h"
#include "saf_io_contract.h"
#include "saf_manifest_parser.h"

#define LOG_TAG   "DXX-SAF-Archiver"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ── JNI bridge (jni_saf.c) ───────────────────────────────────────── */
extern int saf_open_file(const char *content_uri);

typedef SafManifestEntry SafEntry;

typedef struct {
	int fd;
	PHYSFS_sint64 len;
} SafSource;

typedef struct {
	SafManifestData *manifest;
	SafSource *sources;
	pthread_mutex_t source_mutex;
} SafArchive;

typedef struct {
	int fd;
	PHYSFS_sint64 pos;
	PHYSFS_sint64 len;
	char *filename;
	char *uri;
} SafIoData;

static long long saf_elapsed_us(const struct timespec *start, const struct timespec *end)
{
	return ((long long) end->tv_sec - (long long) start->tv_sec) * 1000000LL +
	       ((long long) end->tv_nsec - (long long) start->tv_nsec) / 1000LL;
}

/* ── Archive helpers ──────────────────────────────────────────────── */

static SafEntry *find_entry(SafArchive *arch, const char *name, int *entry_index)
{
	for (int i = 0; i < arch->manifest->count; i++) {
		if (saf_manifest_name_equals(arch->manifest->entries[i].filename, name)) {
			if (entry_index) *entry_index = i;
			return &arch->manifest->entries[i];
		}
	}
	return NULL;
}

static int verified_fd_length(int fd, PHYSFS_sint64 *length)
{
	struct stat stat_buffer;
	unsigned char probe;
	if (fstat(fd, &stat_buffer) != 0 || !S_ISREG(stat_buffer.st_mode) || stat_buffer.st_size < 0 ||
	    lseek(fd, 0, SEEK_CUR) < 0) {
		return 0;
	}
	if ((stat_buffer.st_size > 0 && pread(fd, &probe, 1, stat_buffer.st_size - 1) != 1) ||
	    pread(fd, &probe, 1, stat_buffer.st_size) != 0) {
		return 0;
	}
	*length = (PHYSFS_sint64) stat_buffer.st_size;
	return (off_t) *length == stat_buffer.st_size;
}

static int duplicate_source(SafArchive *arch, int entry_index, int *duplicate_fd,
                            PHYSFS_sint64 *length)
{
	SafSource *source = &arch->sources[entry_index];
	SafEntry *entry = &arch->manifest->entries[entry_index];
	int result = 0;
	pthread_mutex_lock(&arch->source_mutex);

	if (source->fd >= 0) {
		PHYSFS_sint64 current_length;
		if (!verified_fd_length(source->fd, &current_length) || current_length != source->len) {
			close(source->fd);
			source->fd = -1;
		}
	}
	if (source->fd < 0) {
		source->fd = saf_open_file(entry->content_uri);
		if (source->fd < 0 || !verified_fd_length(source->fd, &source->len)) {
			if (source->fd >= 0) close(source->fd);
			source->fd = -1;
			goto done;
		}
		if (source->len != entry->size_bytes) {
			LOGI("SAF source size changed for %s: manifest=%lld current=%lld", entry->filename,
			     (long long) entry->size_bytes, (long long) source->len);
		}
	}
	*length = source->len;
	if (duplicate_fd) {
		*duplicate_fd = dup(source->fd);
		if (*duplicate_fd < 0) goto done;
	}
	result = 1;

done:
	pthread_mutex_unlock(&arch->source_mutex);
	return result;
}

/* ── PHYSFS_Io implementation (pread-based) ───────────────────────── */

static PHYSFS_sint64 safio_read(PHYSFS_Io *io, void *buf, PHYSFS_uint64 n)
{
	SafIoData *d = (SafIoData *) io->opaque;
	struct timespec read_start, read_end;
	const PHYSFS_uint64 read_offset = (PHYSFS_uint64) d->pos;
	PHYSFS_uint64 remaining = (PHYSFS_uint64) (d->len - d->pos);
	if (n > remaining) n = remaining;
	if (n > SIZE_MAX) n = SIZE_MAX;
	if (n == 0) return 0;

	clock_gettime(CLOCK_MONOTONIC, &read_start);
	ssize_t got = pread(d->fd, buf, (size_t) n, (off_t) d->pos);
	clock_gettime(CLOCK_MONOTONIC, &read_end);
	if (got < 0) {
		LOGE("safio_read: pread failed, errno=%d (uri=%s)", errno, d->uri);
		return -1;
	}
	android_profile_storage_op(d->filename, "pread", read_offset,
	                           (unsigned long long) got, saf_elapsed_us(&read_start, &read_end));
	d->pos += got;
	return (PHYSFS_sint64) got;
}

static PHYSFS_sint64 safio_write(PHYSFS_Io *io, const void *buf, PHYSFS_uint64 n)
{
	(void) io;
	(void) buf;
	(void) n;
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return -1;
}

static int safio_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
	SafIoData *d = (SafIoData *) io->opaque;
	int64_t position;
	if (!saf_io_resolve_seek(offset, d->len, &position)) {
		PHYSFS_setErrorCode(PHYSFS_ERR_PAST_EOF);
		return 0;
	}
	d->pos = position;
	return 1;
}

static PHYSFS_sint64 safio_tell(PHYSFS_Io *io)
{
	return ((SafIoData *) io->opaque)->pos;
}

static PHYSFS_sint64 safio_length(PHYSFS_Io *io)
{
	return ((SafIoData *) io->opaque)->len;
}

static void safio_destroy(PHYSFS_Io *io)
{
	SafIoData *d = (SafIoData *) io->opaque;
	if (d) {
		close(d->fd);
		free(d->filename);
		free(d->uri);
		free(d);
	}
	free(io);
}

static PHYSFS_Io *safio_duplicate(PHYSFS_Io *io)
{
	SafIoData *orig = (SafIoData *) io->opaque;

	/* dup() creates a new fd to the same open file;
	 * pread() is offset-independent so each handle's virtual position
	 * is fully independent. */
	int newfd = dup(orig->fd);
	if (newfd < 0) {
		LOGE("safio_duplicate: dup() failed, errno=%d", errno);
		PHYSFS_setErrorCode(PHYSFS_ERR_OS_ERROR);
		return NULL;
	}

	SafIoData *data = (SafIoData *) malloc(sizeof(SafIoData));
	if (!data) {
		close(newfd);
		return NULL;
	}
	data->fd = newfd;
	data->pos = 0;
	data->len = orig->len;
	data->filename = orig->filename ? strdup(orig->filename) : NULL;
	data->uri = strdup(orig->uri);
	if (orig->filename && !data->filename) {
		close(newfd);
		free(data->uri);
		free(data);
		return NULL;
	}
	if (!data->uri) {
		close(newfd);
		free(data->filename);
		free(data);
		return NULL;
	}

	PHYSFS_Io *newio = (PHYSFS_Io *) malloc(sizeof(PHYSFS_Io));
	if (!newio) {
		close(newfd);
		free(data->filename);
		free(data->uri);
		free(data);
		return NULL;
	}
	memcpy(newio, io, sizeof(PHYSFS_Io));
	newio->opaque = data;
	return newio;
}

static int safio_flush(PHYSFS_Io *io)
{
	(void) io;
	return 1; /* read-only, nothing to flush */
}

/* Create a PHYSFS_Io from a native fd */
static PHYSFS_Io *create_saf_io(int fd, PHYSFS_sint64 len, const char *filename,
                                const char *uri)
{
	SafIoData *data = (SafIoData *) malloc(sizeof(SafIoData));
	if (!data) {
		close(fd);
		return NULL;
	}
	data->fd = fd;
	data->pos = 0;
	data->len = len;
	data->filename = filename ? strdup(filename) : NULL;
	data->uri = strdup(uri);
	if (filename && !data->filename) {
		close(fd);
		free(data);
		return NULL;
	}
	if (!data->uri) {
		close(fd);
		free(data->filename);
		free(data);
		return NULL;
	}

	PHYSFS_Io *io = (PHYSFS_Io *) malloc(sizeof(PHYSFS_Io));
	if (!io) {
		close(fd);
		free(data->filename);
		free(data->uri);
		free(data);
		return NULL;
	}

	io->version = 0;
	io->opaque = data;
	io->read = safio_read;
	io->write = safio_write;
	io->seek = safio_seek;
	io->tell = safio_tell;
	io->length = safio_length;
	io->duplicate = safio_duplicate;
	io->flush = safio_flush;
	io->destroy = safio_destroy;

	return io;
}

/* ── PHYSFS_Archiver vtable implementations ───────────────────────── */

static void *SAF_openArchive(PHYSFS_Io *io, const char *name,
                             int forWrite, int *claimed)
{
	/* We only support read-only access */
	if (forWrite) return NULL;

	/* We claim files ending in .saf_manifest.json */
	const char *basename = strrchr(name, '/');
	if (!basename) basename = strrchr(name, '\\');
	if (!basename) basename = name;
	else basename++;

	if (strcmp(basename, ".saf_manifest.json") != 0)
		return NULL;

	*claimed = 1;

	/* Read the entire file into memory for parsing */
	PHYSFS_sint64 file_len = io->length(io);
	if (file_len <= 0 || file_len > 1024 * 1024) { /* max 1MB manifest */
		LOGE("SAF_openArchive: bad file length %lld", (long long) file_len);
		return NULL;
	}

	char *buf = (char *) malloc((size_t) file_len + 1);
	if (!buf) return NULL;

	if (!io->seek(io, 0)) {
		LOGE("SAF_openArchive: could not seek manifest %s", name);
		free(buf);
		return NULL;
	}
	PHYSFS_sint64 got = io->read(io, buf, (PHYSFS_uint64) file_len);
	if (got != file_len) {
		LOGE("SAF_openArchive: short read (%lld of %lld)", (long long) got, (long long) file_len);
		free(buf);
		return NULL;
	}
	buf[file_len] = '\0';

	SafManifestData *manifest = saf_manifest_parse(buf, (size_t) file_len);
	free(buf);

	if (!manifest) {
		LOGE("SAF_openArchive: invalid or incomplete manifest %s", name);
		return NULL;
	}
	SafArchive *arch = (SafArchive *) calloc(1, sizeof(SafArchive));
	if (!arch) {
		saf_manifest_free(manifest);
		return NULL;
	}
	arch->manifest = manifest;
	if (manifest->count > 0) {
		arch->sources = (SafSource *) malloc((size_t) manifest->count * sizeof(SafSource));
		if (!arch->sources) {
			saf_manifest_free(manifest);
			free(arch);
			return NULL;
		}
		for (int i = 0; i < manifest->count; i++) arch->sources[i].fd = -1;
	}
	if (pthread_mutex_init(&arch->source_mutex, NULL) != 0) {
		free(arch->sources);
		saf_manifest_free(manifest);
		free(arch);
		return NULL;
	}

	LOGI("SAF_openArchive: mounted %s with %d files", name, manifest->count);
	io->destroy(io);
	return arch;
}

static PHYSFS_EnumerateCallbackResult SAF_enumerate(
    void *opaque, const char *dirname,
    PHYSFS_EnumerateCallback cb, const char *origdir, void *callbackdata)
{
	SafArchive *arch = (SafArchive *) opaque;

	/* We present a flat virtual directory — all files live at the root.
	 * Only enumerate for the root directory ("" or "/"). */
	if (dirname[0] != '\0' && strcmp(dirname, "/") != 0)
		return PHYSFS_ENUM_OK;

	for (int i = 0; i < arch->manifest->count; i++) {
		PHYSFS_EnumerateCallbackResult r =
		    cb(callbackdata, origdir, arch->manifest->entries[i].filename);
		if (r == PHYSFS_ENUM_ERROR) {
			PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
			return PHYSFS_ENUM_ERROR;
		}
		if (r == PHYSFS_ENUM_STOP)
			return PHYSFS_ENUM_STOP;
	}
	return PHYSFS_ENUM_OK;
}

static PHYSFS_Io *SAF_openRead(void *opaque, const char *fnm)
{
	SafArchive *arch = (SafArchive *) opaque;
	int entry_index;
	SafEntry *entry = find_entry(arch, fnm, &entry_index);
	struct timespec open_start, open_end;
	if (!entry) {
		PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
		return NULL;
	}

	int fd = -1;
	PHYSFS_sint64 length;
	clock_gettime(CLOCK_MONOTONIC, &open_start);
	int opened = duplicate_source(arch, entry_index, &fd, &length);
	clock_gettime(CLOCK_MONOTONIC, &open_end);
	if (!opened) {
		LOGE("SAF_openRead: saf_open_file failed for %s", fnm);
		PHYSFS_setErrorCode(PHYSFS_ERR_OS_ERROR);
		return NULL;
	}
	android_profile_storage_op(fnm, "open", 0, 0, saf_elapsed_us(&open_start, &open_end));

	return create_saf_io(fd, length, fnm, entry->content_uri);
}

static PHYSFS_Io *SAF_openWrite(void *opaque, const char *fnm)
{
	(void) opaque;
	(void) fnm;
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return NULL;
}

static PHYSFS_Io *SAF_openAppend(void *opaque, const char *fnm)
{
	(void) opaque;
	(void) fnm;
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return NULL;
}

static int SAF_remove(void *opaque, const char *fnm)
{
	(void) opaque;
	(void) fnm;
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return 0;
}

static int SAF_mkdir(void *opaque, const char *fnm)
{
	(void) opaque;
	(void) fnm;
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return 0;
}

static int SAF_stat(void *opaque, const char *fn, PHYSFS_Stat *st)
{
	SafArchive *arch = (SafArchive *) opaque;
	int entry_index;
	SafEntry *entry = find_entry(arch, fn, &entry_index);
	if (!entry) {
		PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
		return 0;
	}

	PHYSFS_sint64 length;
	if (!duplicate_source(arch, entry_index, NULL, &length)) {
		PHYSFS_setErrorCode(PHYSFS_ERR_OS_ERROR);
		return 0;
	}
	st->filesize = length;
	st->modtime = -1;
	st->createtime = -1;
	st->accesstime = -1;
	st->filetype = PHYSFS_FILETYPE_REGULAR;
	st->readonly = 1;
	return 1;
}

static void SAF_closeArchive(void *opaque)
{
	SafArchive *arch = (SafArchive *) opaque;
	if (!arch) return;
	for (int i = 0; i < arch->manifest->count; i++) {
		if (arch->sources[i].fd >= 0) close(arch->sources[i].fd);
	}
	pthread_mutex_destroy(&arch->source_mutex);
	free(arch->sources);
	saf_manifest_free(arch->manifest);
	free(arch);
}

/* ── Exported archiver struct ─────────────────────────────────────── */

const PHYSFS_Archiver SAF_Archiver = {
	.version = 0,
	.info = {
	    .extension = "json",
	    .description = "SAF leave-in-place virtual directory",
	    .author = "dxx-redux",
	    .url = "https://github.com/dxx-redux",
	    .supportsSymlinks = 0 },
	.openArchive = SAF_openArchive,
	.enumerate = SAF_enumerate,
	.openRead = SAF_openRead,
	.openWrite = SAF_openWrite,
	.openAppend = SAF_openAppend,
	.remove = SAF_remove,
	.mkdir = SAF_mkdir,
	.stat = SAF_stat,
	.closeArchive = SAF_closeArchive
};
