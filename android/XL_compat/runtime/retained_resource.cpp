#include "retained_resource.h"
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace
{
// Adapt retained bytes to the existing PHYSFS-based native codec without a
// second resource parser or a temporary on-disk file. This is not an archive format:
// only the exact in-progress private mount name is recognized, and registration
// lasts for the synchronous decode. Nothing is published at a resource basename
std::mutex snapshot_mutex;
thread_local const char *snapshot_name;

void *snapshot_open(PHYSFS_Io *io, const char *name, int writing, int *claimed)
{
	if (writing || !snapshot_name || std::strcmp(name, snapshot_name)) return nullptr;
	*claimed = 1;
	return io->seek(io, 0) ? io : nullptr;
}

PHYSFS_EnumerateCallbackResult snapshot_enumerate(void *, const char *directory, PHYSFS_EnumerateCallback callback, const char *original, void *context)
{
	if (*directory) return PHYSFS_ENUM_OK;
	const auto result = callback(context, original, "data");
	if (result == PHYSFS_ENUM_ERROR) PHYSFS_setErrorCode(PHYSFS_ERR_APP_CALLBACK);
	return result;
}

PHYSFS_Io *snapshot_read(void *opaque, const char *name)
{
	if (std::strcmp(name, "data")) {
		PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
		return nullptr;
	}
	auto *source = static_cast<PHYSFS_Io *>(opaque);
	auto *copy = source->duplicate(source);
	if (copy && !copy->seek(copy, 0)) {
		copy->destroy(copy);
		return nullptr;
	}
	return copy;
}

PHYSFS_Io *snapshot_write(void *, const char *)
{
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return nullptr;
}
int snapshot_modify(void *, const char *)
{
	PHYSFS_setErrorCode(PHYSFS_ERR_READ_ONLY);
	return 0;
}
int snapshot_stat(void *opaque, const char *name, PHYSFS_Stat *stat)
{
	if (*name && std::strcmp(name, "data")) {
		PHYSFS_setErrorCode(PHYSFS_ERR_NOT_FOUND);
		return 0;
	}
	auto *source = static_cast<PHYSFS_Io *>(opaque);
	*stat = { *name ? source->length(source) : -1, -1, -1, -1,
		      *name ? PHYSFS_FILETYPE_REGULAR : PHYSFS_FILETYPE_DIRECTORY, 1 };
	return 1;
}
void snapshot_close(void *opaque)
{
	auto *source = static_cast<PHYSFS_Io *>(opaque);
	source->destroy(source);
}

const PHYSFS_Archiver snapshot_archiver = {
	0, { "XLRESSNAP", "Private retained resource bytes", "DXX Redux", "", 0 }, snapshot_open, snapshot_enumerate, snapshot_read, snapshot_write, snapshot_write, snapshot_modify, snapshot_modify, snapshot_stat, snapshot_close
};

} // namespace

void xl_read_retained_resource(const std::vector<unsigned char> &bytes, const std::function<void(PHYSFS_file *)> &reader)
{
	std::lock_guard<std::mutex> lock(snapshot_mutex);
	static unsigned long serial;
	std::string name;
	do {
		name = "__xl_resource_snapshot_" + std::to_string(++serial) + ".XLRESSNAP";
	} while (PHYSFS_exists(name.c_str()) || PHYSFS_getMountPoint(name.c_str()));
	if (!PHYSFS_registerArchiver(&snapshot_archiver)) throw std::runtime_error("Cannot register retained resource reader");
	struct Registration {
		bool active = true;
		~Registration()
		{
			snapshot_name = nullptr;
			if (active) PHYSFS_deregisterArchiver("XLRESSNAP");
		}
	} registration;
	void *memory = std::malloc(bytes.size());
	if (!memory) throw std::bad_alloc();
	std::memcpy(memory, bytes.data(), bytes.size());
	snapshot_name = name.c_str();
	const bool mounted = PHYSFS_mountMemory(memory, bytes.size(), std::free, name.c_str(), name.c_str(), 1) != 0;
	snapshot_name = nullptr;
	if (!mounted) {
		std::free(memory);
		throw std::runtime_error("Cannot mount retained resource bytes");
	}
	struct Mount {
		const std::string &name;
		bool active = true;
		~Mount()
		{
			if (active) PHYSFS_unmount(name.c_str());
		}
	} mount{ name };
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead((name + "/data").c_str()), PHYSFS_close);
	if (!file) throw std::runtime_error("Cannot open retained resource bytes");
	reader(file.get());
	if (!PHYSFS_close(file.get())) throw std::runtime_error("Cannot close retained resource reader");
	file.release();
	if (!PHYSFS_unmount(name.c_str())) throw std::runtime_error("Cannot release retained resource mount");
	mount.active = false;
	if (!PHYSFS_deregisterArchiver("XLRESSNAP")) throw std::runtime_error("Cannot release retained resource reader registration");
	registration.active = false;
}

std::vector<unsigned char> xl_retain_resource(PHYSFS_file *file)
{
	if (!file) throw std::runtime_error("Missing retained resource");
	const auto length = PHYSFS_fileLength(file);
	if (length <= 0 || length > 64 * 1024 * 1024) throw std::runtime_error("Resource exceeds retained source bounds");
	std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
	const auto position = PHYSFS_tell(file);
	if (position < 0 || !PHYSFS_seek(file, 0)) throw std::runtime_error("Cannot seek retained resource");
	const auto read = PHYSFS_readBytes(file, bytes.data(), length);
	const bool restored = PHYSFS_seek(file, position) != 0;
	if (read != length || !restored) throw std::runtime_error("Cannot retain resource bytes");
	return bytes;
}
