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
#include <stdlib.h>
#include <string.h>
#include <strings.h>    /* strcasecmp */
#include <unistd.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "DXX-SAF-Archiver"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

/* ── JNI bridge (jni_saf.c) ───────────────────────────────────────── */
extern int saf_open_file(const char *content_uri);

/* ── In-memory file table ─────────────────────────────────────────── */

typedef struct {
    char *filename;
    char *content_uri;
    PHYSFS_sint64 size_bytes;
} SafEntry;

typedef struct {
    SafEntry *entries;
    int count;
} SafArchive;

/* ── Per-handle I/O data (pread-based) ────────────────────────────── */

typedef struct {
    int fd;
    PHYSFS_sint64 pos;    /* virtual seek position, independent per handle */
    PHYSFS_sint64 len;    /* cached file length */
    char *uri;            /* content URI string (for error messages) */
} SafIoData;

/* ────────────────────────────────────────────────────────────────────
 * Minimal JSON parser for .saf_manifest.json
 *
 * We only need to parse a very specific flat structure:
 *   { "files": [ { "filename": "...", "content_uri": "...", "size_bytes": N }, ... ] }
 *
 * This avoids adding a JSON library dependency to the C build.
 * ──────────────────────────────────────────────────────────────────── */

/* Skip whitespace, return pointer to next non-ws char */
static const char *skip_ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    return p;
}

/*
 * Extract a JSON string value.  p should point to the opening '"'.
 * Returns a malloc'd copy of the string contents (unescaped for \" and \\).
 * Advances *pp past the closing '"'.  Returns NULL on error.
 */
static char *parse_json_string(const char **pp)
{
    const char *p = *pp;
    if (*p != '"') return NULL;
    p++;

    /* First pass: measure length */
    const char *start = p;
    size_t len = 0;
    while (*p && *p != '"') {
        if (*p == '\\') { p++; if (!*p) return NULL; }
        p++;
        len++;
    }
    if (*p != '"') return NULL;

    /* Second pass: copy */
    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;
    p = start;
    size_t i = 0;
    while (*p != '"') {
        if (*p == '\\') {
            p++;
            switch (*p) {
                case '"': case '\\': case '/': result[i++] = *p; break;
                case 'n': result[i++] = '\n'; break;
                case 't': result[i++] = '\t'; break;
                case 'r': result[i++] = '\r'; break;
                default:  result[i++] = *p; break;
            }
        } else {
            result[i++] = *p;
        }
        p++;
    }
    result[i] = '\0';
    p++; /* skip closing quote */
    *pp = p;
    return result;
}

/* Parse a JSON integer (possibly negative). Advances *pp. */
static PHYSFS_sint64 parse_json_int(const char **pp)
{
    const char *p = *pp;
    PHYSFS_sint64 val = 0;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    while (*p >= '0' && *p <= '9') {
        val = val * 10 + (*p - '0');
        p++;
    }
    *pp = p;
    return neg ? -val : val;
}

/*
 * Parse the entire .saf_manifest.json from a memory buffer.
 * Returns a SafArchive with entries, or NULL on failure.
 */
static SafArchive *parse_manifest(const char *json, size_t json_len)
{
    /* Allocate archive */
    SafArchive *arch = (SafArchive *)calloc(1, sizeof(SafArchive));
    if (!arch) return NULL;

    /* Capacity management */
    int capacity = 16;
    arch->entries = (SafEntry *)calloc(capacity, sizeof(SafEntry));
    if (!arch->entries) { free(arch); return NULL; }

    /* Find "files" array */
    const char *p = json;
    const char *end = json + json_len;

    /* Search for "files" key */
    const char *files_key = strstr(p, "\"files\"");
    if (!files_key) { LOGE("parse_manifest: no \"files\" key"); goto fail; }
    p = files_key + 7; /* past "files" */
    p = skip_ws(p);
    if (*p == ':') p++;
    p = skip_ws(p);
    if (*p != '[') { LOGE("parse_manifest: expected '['"); goto fail; }
    p++; /* past '[' */

    /* Parse array entries */
    while (p < end) {
        p = skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') { p++; continue; }
        if (*p != '{') { LOGE("parse_manifest: expected '{'"); goto fail; }
        p++; /* past '{' */

        /* Parse object fields */
        char *filename = NULL;
        char *content_uri = NULL;
        PHYSFS_sint64 size_bytes = -1;

        while (p < end) {
            p = skip_ws(p);
            if (*p == '}') { p++; break; }
            if (*p == ',') { p++; continue; }

            /* Parse key */
            char *key = parse_json_string(&p);
            if (!key) { LOGE("parse_manifest: bad key"); goto obj_fail; }
            p = skip_ws(p);
            if (*p == ':') p++;
            p = skip_ws(p);

            /* Parse value */
            if (strcmp(key, "filename") == 0) {
                filename = parse_json_string(&p);
            } else if (strcmp(key, "content_uri") == 0) {
                content_uri = parse_json_string(&p);
            } else if (strcmp(key, "size_bytes") == 0) {
                size_bytes = parse_json_int(&p);
            } else {
                /* Skip unknown value (string or number) */
                if (*p == '"') {
                    char *tmp = parse_json_string(&p);
                    free(tmp);
                } else {
                    /* Skip number or literal */
                    while (p < end && *p != ',' && *p != '}') p++;
                }
            }
            free(key);
            continue;

        obj_fail:
            free(filename);
            free(content_uri);
            goto fail;
        }

        /* Validate entry */
        if (!filename || !content_uri || size_bytes < 0) {
            LOGE("parse_manifest: incomplete entry");
            free(filename);
            free(content_uri);
            continue; /* skip bad entries */
        }

        /* Grow array if needed */
        if (arch->count >= capacity) {
            capacity *= 2;
            SafEntry *tmp = (SafEntry *)realloc(arch->entries, capacity * sizeof(SafEntry));
            if (!tmp) { free(filename); free(content_uri); goto fail; }
            arch->entries = tmp;
        }

        arch->entries[arch->count].filename = filename;
        arch->entries[arch->count].content_uri = content_uri;
        arch->entries[arch->count].size_bytes = size_bytes;
        arch->count++;
    }

    LOGI("parse_manifest: loaded %d entries", arch->count);
    return arch;

fail:
    if (arch->entries) {
        for (int i = 0; i < arch->count; i++) {
            free(arch->entries[i].filename);
            free(arch->entries[i].content_uri);
        }
        free(arch->entries);
    }
    free(arch);
    return NULL;
}

/* ── Archive helpers ──────────────────────────────────────────────── */

static SafEntry *find_entry(SafArchive *arch, const char *name)
{
    for (int i = 0; i < arch->count; i++) {
        if (strcasecmp(arch->entries[i].filename, name) == 0)
            return &arch->entries[i];
    }
    return NULL;
}

/* ── PHYSFS_Io implementation (pread-based) ───────────────────────── */

static PHYSFS_sint64 safio_read(PHYSFS_Io *io, void *buf, PHYSFS_uint64 n)
{
    SafIoData *d = (SafIoData *)io->opaque;
    PHYSFS_uint64 remaining = (PHYSFS_uint64)(d->len - d->pos);
    if (n > remaining) n = remaining;
    if (n == 0) return 0;

    ssize_t got = pread(d->fd, buf, (size_t)n, (off_t)d->pos);
    if (got < 0) {
        LOGE("safio_read: pread failed, errno=%d (uri=%s)", errno, d->uri);
        return -1;
    }
    d->pos += got;
    return (PHYSFS_sint64)got;
}

static PHYSFS_sint64 safio_write(PHYSFS_Io *io, const void *buf, PHYSFS_uint64 n)
{
    (void)io; (void)buf; (void)n;
    PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
    return -1;
}

static int safio_seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
    SafIoData *d = (SafIoData *)io->opaque;
    if ((PHYSFS_sint64)offset > d->len) {
        PHYSFS_setErrorCode(PHYSFS_ERR_PAST_EOF);
        return 0;
    }
    d->pos = (PHYSFS_sint64)offset;
    return 1;
}

static PHYSFS_sint64 safio_tell(PHYSFS_Io *io)
{
    return ((SafIoData *)io->opaque)->pos;
}

static PHYSFS_sint64 safio_length(PHYSFS_Io *io)
{
    return ((SafIoData *)io->opaque)->len;
}

static void safio_destroy(PHYSFS_Io *io)
{
    SafIoData *d = (SafIoData *)io->opaque;
    if (d) {
        close(d->fd);
        free(d->uri);
        free(d);
    }
    free(io);
}

static PHYSFS_Io *safio_duplicate(PHYSFS_Io *io)
{
    SafIoData *orig = (SafIoData *)io->opaque;

    /* dup() creates a new fd to the same open file;
     * pread() is offset-independent so each handle's virtual position
     * is fully independent. */
    int newfd = dup(orig->fd);
    if (newfd < 0) {
        LOGE("safio_duplicate: dup() failed, errno=%d", errno);
        PHYSFS_setErrorCode(PHYSFS_ERR_OS_ERROR);
        return NULL;
    }

    SafIoData *data = (SafIoData *)malloc(sizeof(SafIoData));
    if (!data) { close(newfd); return NULL; }
    data->fd = newfd;
    data->pos = 0;
    data->len = orig->len;
    data->uri = strdup(orig->uri);

    PHYSFS_Io *newio = (PHYSFS_Io *)malloc(sizeof(PHYSFS_Io));
    if (!newio) { close(newfd); free(data->uri); free(data); return NULL; }
    memcpy(newio, io, sizeof(PHYSFS_Io));
    newio->opaque = data;
    return newio;
}

static int safio_flush(PHYSFS_Io *io)
{
    (void)io;
    return 1; /* read-only, nothing to flush */
}

/* Create a PHYSFS_Io from a native fd */
static PHYSFS_Io *create_saf_io(int fd, PHYSFS_sint64 len, const char *uri)
{
    SafIoData *data = (SafIoData *)malloc(sizeof(SafIoData));
    if (!data) { close(fd); return NULL; }
    data->fd = fd;
    data->pos = 0;
    data->len = len;
    data->uri = strdup(uri);

    PHYSFS_Io *io = (PHYSFS_Io *)malloc(sizeof(PHYSFS_Io));
    if (!io) { close(fd); free(data->uri); free(data); return NULL; }

    io->version   = 0;
    io->opaque    = data;
    io->read      = safio_read;
    io->write     = safio_write;
    io->seek      = safio_seek;
    io->tell      = safio_tell;
    io->length    = safio_length;
    io->duplicate = safio_duplicate;
    io->flush     = safio_flush;
    io->destroy   = safio_destroy;

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
    if (!basename) basename = name; else basename++;

    if (strcmp(basename, ".saf_manifest.json") != 0)
        return NULL;

    *claimed = 1;

    /* Read the entire file into memory for parsing */
    PHYSFS_sint64 file_len = io->length(io);
    if (file_len <= 0 || file_len > 1024 * 1024) {  /* max 1MB manifest */
        LOGE("SAF_openArchive: bad file length %lld", (long long)file_len);
        return NULL;
    }

    char *buf = (char *)malloc((size_t)file_len + 1);
    if (!buf) return NULL;

    io->seek(io, 0);
    PHYSFS_sint64 got = io->read(io, buf, (PHYSFS_uint64)file_len);
    if (got != file_len) {
        LOGE("SAF_openArchive: short read (%lld of %lld)", (long long)got, (long long)file_len);
        free(buf);
        return NULL;
    }
    buf[file_len] = '\0';

    SafArchive *arch = parse_manifest(buf, (size_t)file_len);
    free(buf);

    if (!arch) {
        LOGE("SAF_openArchive: parse_manifest failed for %s", name);
        return NULL;
    }

    LOGI("SAF_openArchive: mounted %s with %d files", name, arch->count);
    return arch;
}

static PHYSFS_EnumerateCallbackResult SAF_enumerate(
    void *opaque, const char *dirname,
    PHYSFS_EnumerateCallback cb, const char *origdir, void *callbackdata)
{
    SafArchive *arch = (SafArchive *)opaque;

    /* We present a flat virtual directory — all files live at the root.
     * Only enumerate for the root directory ("" or "/"). */
    if (dirname[0] != '\0' && strcmp(dirname, "/") != 0)
        return PHYSFS_ENUM_OK;

    for (int i = 0; i < arch->count; i++) {
        PHYSFS_EnumerateCallbackResult r =
            cb(callbackdata, origdir, arch->entries[i].filename);
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
    SafArchive *arch = (SafArchive *)opaque;
    SafEntry *entry = find_entry(arch, fnm);
    if (!entry) {
        PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
        return NULL;
    }

    int fd = saf_open_file(entry->content_uri);
    if (fd < 0) {
        LOGE("SAF_openRead: saf_open_file failed for %s", fnm);
        PHYSFS_setErrorCode(PHYSFS_ERR_OS_ERROR);
        return NULL;
    }

    return create_saf_io(fd, entry->size_bytes, entry->content_uri);
}

static PHYSFS_Io *SAF_openWrite(void *opaque, const char *fnm)
{
    (void)opaque; (void)fnm;
    PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
    return NULL;
}

static PHYSFS_Io *SAF_openAppend(void *opaque, const char *fnm)
{
    (void)opaque; (void)fnm;
    PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
    return NULL;
}

static int SAF_remove(void *opaque, const char *fnm)
{
    (void)opaque; (void)fnm;
    PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
    return 0;
}

static int SAF_mkdir(void *opaque, const char *fnm)
{
    (void)opaque; (void)fnm;
    PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
    return 0;
}

static int SAF_stat(void *opaque, const char *fn, PHYSFS_Stat *st)
{
    SafArchive *arch = (SafArchive *)opaque;
    SafEntry *entry = find_entry(arch, fn);
    if (!entry) {
        PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
        return 0;
    }

    st->filesize   = entry->size_bytes;
    st->modtime    = -1;
    st->createtime = -1;
    st->accesstime = -1;
    st->filetype   = PHYSFS_FILETYPE_REGULAR;
    st->readonly   = 1;
    return 1;
}

static void SAF_closeArchive(void *opaque)
{
    SafArchive *arch = (SafArchive *)opaque;
    if (!arch) return;

    for (int i = 0; i < arch->count; i++) {
        free(arch->entries[i].filename);
        free(arch->entries[i].content_uri);
    }
    free(arch->entries);
    free(arch);
}

/* ── Exported archiver struct ─────────────────────────────────────── */

const PHYSFS_Archiver SAF_Archiver = {
    .version = 0,
    .info = {
        .extension   = "json",
        .description = "SAF leave-in-place virtual directory",
        .author      = "dxx-redux",
        .url         = "https://github.com/dxx-redux",
        .supportsSymlinks = 0
    },
    .openArchive  = SAF_openArchive,
    .enumerate    = SAF_enumerate,
    .openRead     = SAF_openRead,
    .openWrite    = SAF_openWrite,
    .openAppend   = SAF_openAppend,
    .remove       = SAF_remove,
    .mkdir        = SAF_mkdir,
    .stat         = SAF_stat,
    .closeArchive = SAF_closeArchive
};
