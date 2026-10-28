#include "hog.h"
#include "hog_archiver.h"
#include <physfs.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <new>

namespace
{
struct Archive {
	PHYSFS_Io *io;
	std::vector<xl_compat::HogEntry> entries;
};
struct Stream {
	PHYSFS_Io *source;
	PHYSFS_uint64 offset, size, position;
};
const xl_compat::HogEntry *find(Archive *archive, const char *name)
{
	const auto key = xl_compat::hog_key(name);
	const auto at = std::lower_bound(archive->entries.begin(), archive->entries.end(), key,
	                                 [](const xl_compat::HogEntry &entry, const std::string &value) { return xl_compat::hog_key(entry.name) < value; });
	return at != archive->entries.end() && xl_compat::hog_key(at->name) == key ? &*at : nullptr;
}
PHYSFS_sint64 read(PHYSFS_Io *io, void *buffer, PHYSFS_uint64 length)
{
	auto &stream = *static_cast<Stream *>(io->opaque);
	length = (std::min) (length, stream.size - stream.position);
	const auto count = stream.source->read(stream.source, buffer, length);
	if (count > 0) stream.position += static_cast<PHYSFS_uint64>(count);
	return count;
}
int seek(PHYSFS_Io *io, PHYSFS_uint64 offset)
{
	auto &stream = *static_cast<Stream *>(io->opaque);
	if (offset > stream.size) {
		PHYSFS_setErrorCode(PHYSFS_ERR_PAST_EOF);
		return 0;
	}
	if (!stream.source->seek(stream.source, stream.offset + offset)) return 0;
	stream.position = offset;
	return 1;
}
PHYSFS_sint64 tell(PHYSFS_Io *io)
{
	return static_cast<PHYSFS_sint64>(static_cast<Stream *>(io->opaque)->position);
}
PHYSFS_sint64 length(PHYSFS_Io *io)
{
	return static_cast<PHYSFS_sint64>(static_cast<Stream *>(io->opaque)->size);
}
void destroy(PHYSFS_Io *io)
{
	auto *stream = static_cast<Stream *>(io->opaque);
	stream->source->destroy(stream->source);
	delete stream;
	delete io;
}
PHYSFS_Io *make_stream(PHYSFS_Io *source, PHYSFS_uint64 offset, PHYSFS_uint64 size, PHYSFS_uint64 position);
PHYSFS_Io *duplicate(PHYSFS_Io *io)
{
	const auto &stream = *static_cast<Stream *>(io->opaque);
	return make_stream(stream.source, stream.offset, stream.size, stream.position);
}
PHYSFS_Io *make_stream(PHYSFS_Io *source, PHYSFS_uint64 offset, PHYSFS_uint64 size, PHYSFS_uint64 position)
{
	auto *copy = source->duplicate(source);
	if (!copy) return nullptr;
	try {
		auto state = std::make_unique<Stream>(Stream{ copy, offset, size, position });
		auto io = std::make_unique<PHYSFS_Io>(PHYSFS_Io{ 0, state.get(), read, nullptr, seek, tell, length, duplicate, nullptr, destroy });
		if (!copy->seek(copy, offset + position)) {
			copy->destroy(copy);
			return nullptr;
		}
		state.release();
		return io.release();
	} catch (...) {
		copy->destroy(copy);
		PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
		return nullptr;
	}
}
void *open_archive(PHYSFS_Io *io, const char *, int writing, int *claimed)
{
	*claimed = 0;
	unsigned char signature[3];
	if (!io->seek(io, 0) || io->read(io, signature, sizeof(signature)) != sizeof(signature) || !xl_compat::hog_signature(signature)) return nullptr;
	*claimed = 1;
	if (writing) {
		PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
		return nullptr;
	}
	try {
		const auto size = io->length(io);
		if (size < 0) return nullptr;
		auto archive = std::make_unique<Archive>();
		archive->io = io;
		archive->entries = xl_compat::index_hog(static_cast<std::uint64_t>(size), [io](std::uint64_t offset, unsigned char *buffer, std::size_t count) {
			if (!io->seek(io, offset) || io->read(io, buffer, count) != static_cast<PHYSFS_sint64>(count)) throw std::runtime_error("Cannot read HOG header");
		});
		std::sort(archive->entries.begin(), archive->entries.end(), [](const xl_compat::HogEntry &a, const xl_compat::HogEntry &b) { return xl_compat::hog_key(a.name) < xl_compat::hog_key(b.name); });
		return archive.release();
	} catch (const std::bad_alloc &) {
		PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
	} catch (...) {
		PHYSFS_setErrorCode(PHYSFS_ERR_CORRUPT);
	}
	return nullptr;
}
PHYSFS_EnumerateCallbackResult enumerate(void *opaque, const char *directory, PHYSFS_EnumerateCallback callback, const char *original, void *data)
{
	if (*directory) return PHYSFS_ENUM_OK;
	for (const auto &entry : static_cast<Archive *>(opaque)->entries) {
		const auto result = callback(data, original, entry.name.c_str());
		if (result != PHYSFS_ENUM_OK) {
			if (result == PHYSFS_ENUM_ERROR) PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
			return result;
		}
	}
	return PHYSFS_ENUM_OK;
}
PHYSFS_Io *open_read(void *opaque, const char *name)
{
	try {
		auto *archive = static_cast<Archive *>(opaque);
		if (const auto *entry = find(archive, name)) return make_stream(archive->io, entry->offset, entry->size, 0);
		PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
	} catch (...) {
		PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
	}
	return nullptr;
}
PHYSFS_Io *open_write(void *, const char *)
{
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return nullptr;
}
int reject_write(void *, const char *)
{
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return 0;
}
int stat(void *opaque, const char *name, PHYSFS_Stat *out)
{
	try {
		const auto *entry = find(static_cast<Archive *>(opaque), name);
		if (!entry && *name) {
			PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
			return 0;
		}
		*out = PHYSFS_Stat{ entry ? static_cast<PHYSFS_sint64>(entry->size) : 0, -1, -1, -1, entry ? PHYSFS_FILETYPE_REGULAR : PHYSFS_FILETYPE_DIRECTORY, 1 };
		return 1;
	} catch (...) {
		PHYSFS_setErrorCode(PHYSFS_ERR_OUT_OF_MEMORY);
		return 0;
	}
}
void close_archive(void *opaque)
{
	auto *archive = static_cast<Archive *>(opaque);
	archive->io->destroy(archive->io);
	delete archive;
}
const PHYSFS_Archiver archiver{ 0, { "HOG", "Descent and D2X-XL HOG archives", "DXX Redux", "https://github.com/larsonm-personal/dxx-redux", 0 }, open_archive, enumerate, open_read, open_write, open_write, reject_write, reject_write, stat, close_archive };
} // namespace
extern "C" int xl_register_hog_archiver(void)
{
	return PHYSFS_deregisterArchiver("HOG") && PHYSFS_registerArchiver(&archiver);
}
