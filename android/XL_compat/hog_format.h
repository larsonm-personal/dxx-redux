#ifndef DXX_HOG_FORMAT_H
#define DXX_HOG_FORMAT_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Binary format shared by the engine, launcher JNI and C music/resource readers
#define DXX_HOG_SIGNATURE_BYTES    3
#define DXX_HOG_SHORT_HEADER_BYTES 17
#define DXX_HOG_LONG_NAME_BYTES    256

struct dxx_hog_header {
	char name[DXX_HOG_LONG_NAME_BYTES];
	uint32_t size;
	unsigned int header_bytes;
	int extended;
};

typedef size_t (*dxx_hog_read_fn)(void *context, unsigned char *out, size_t count);

static inline int dxx_hog_signature(const unsigned char *bytes)
{
	return bytes[0] == 'D' && ((bytes[1] == 'H' && bytes[2] == 'F') || (bytes[1] == '2' && bytes[2] == 'X'));
}

static inline unsigned char dxx_hog_fold(unsigned char c)
{
	return c >= 'A' && c <= 'Z' ? (unsigned char) (c + ('a' - 'A')) : c;
}

static inline int dxx_hog_name_compare(const char *left, const char *right)
{
	while (*left && dxx_hog_fold((unsigned char) *left) == dxx_hog_fold((unsigned char) *right)) {
		++left;
		++right;
	}
	return (int) dxx_hog_fold((unsigned char) *left) - (int) dxx_hog_fold((unsigned char) *right);
}

static inline size_t dxx_hog_read_exact(dxx_hog_read_fn read, void *context, unsigned char *out, size_t count)
{
	size_t total = 0;
	while (total < count) {
		const size_t got = read(context, out + total, count - total);
		if (!got || got > count - total) break;
		total += got;
	}
	return total;
}

// Returns 1 for a complete header, 0 at a clean record boundary, -1 on invalid data
// The caller owns payload bounds, archive-wide name uniqueness and resource budgets
static inline int dxx_hog_read_header(dxx_hog_read_fn read, void *context, struct dxx_hog_header *entry, const char **error)
{
	unsigned char bytes[DXX_HOG_SHORT_HEADER_BYTES];
	unsigned char name[DXX_HOG_LONG_NAME_BYTES] = { 0 };
	size_t width, length, i;
	uint32_t raw;
	const size_t got = dxx_hog_read_exact(read, context, bytes, sizeof(bytes));
	memset(entry, 0, sizeof(*entry));
	*error = "Truncated HOG header";
	if (!got) return 0;
	if (got != sizeof(bytes)) return -1;
	raw = (uint32_t) bytes[13] | (uint32_t) bytes[14] << 8 | (uint32_t) bytes[15] << 16 | (uint32_t) bytes[16] << 24;
	if (raw == UINT32_C(0x80000000)) {
		*error = "Invalid HOG member size INT_MIN";
		return -1;
	}
	entry->extended = (raw & UINT32_C(0x80000000)) != 0;
	entry->header_bytes = sizeof(bytes) + (entry->extended ? sizeof(name) : 0);
	entry->size = entry->extended ? UINT32_C(0) - raw : raw;
	if (entry->extended) {
		if (dxx_hog_read_exact(read, context, name, sizeof(name)) != sizeof(name)) return -1;
	} else {
		memcpy(name, bytes, 13);
	}
	width = entry->extended ? sizeof(name) : 13;
	for (length = 0; length < width && name[length]; ++length) {}
	if (entry->extended && length == width) {
		*error = "Unterminated extended HOG member name";
		return -1;
	}
	*error = "Invalid HOG member name";
	if (!length || (length == 1 && name[0] == '.') || (length == 2 && name[0] == '.' && name[1] == '.')) return -1;
	for (i = 0; i < length; ++i) {
		if (name[i] == '/' || name[i] == '\\' || name[i] == ':') return -1;
		if (name[i] < 32 || name[i] == 127) {
			*error = "Invalid HOG member name control byte";
			return -1;
		}
	}
	memcpy(entry->name, name, length);
	*error = NULL;
	return 1;
}
#endif
