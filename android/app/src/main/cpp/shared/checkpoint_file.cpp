#include "checkpoint_file.h"
#include "android_save_set.h"
extern "C" {
#include "android_file_pair_transaction.h"
}
#include <cstdio>
#include <filesystem>
#include <string>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <vector>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#endif

static bool sync_parent(const char *path)
{
#ifdef _WIN32
	(void) path;
	return true;
#else
	const auto parent = std::filesystem::u8path(path).parent_path();
	const int fd = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (fd < 0) return false;
	const bool ok = fsync(fd) == 0;
	close(fd);
	return ok;
#endif
}

static int write_stage(const char *path, const void *data, size_t size)
{
	if (!path || !*path || !data || !size) return 0;
	const auto destination = std::filesystem::u8path(path);
	if (!destination.is_absolute()) return 0;
	const auto temporary = std::filesystem::u8path(std::string(path) + ".checkpoint.stage");
	std::error_code error;
	std::filesystem::create_directories(destination.parent_path(), error);
	if (error) return 0;
#ifdef _WIN32
	FILE *file = _wfopen(temporary.c_str(), L"wb");
#else
	FILE *file = std::fopen(temporary.c_str(), "wb");
#endif
	if (!file) return 0;
	bool ok = std::fwrite(data, 1, size, file) == size && std::fflush(file) == 0;
#ifdef _WIN32
	if (ok) ok = _commit(_fileno(file)) == 0;
#else
	if (ok) ok = fsync(fileno(file)) == 0;
#endif
	if (std::fclose(file)) ok = false;
	if (!ok) std::filesystem::remove(temporary, error);
	return ok && sync_parent(path);
}

/* Synchronous callers stage through PHYSFS. Flush those closed files before
 * recording a transaction that may commit them */
static bool sync_stage(const char *path)
{
	if (!path) return false;
#ifdef _WIN32
	FILE *file = _wfopen(std::filesystem::u8path(path).c_str(), L"r+b");
	if (!file) return false;
	const bool ok = _commit(_fileno(file)) == 0;
	return std::fclose(file) == 0 && ok;
#else
	const int fd = open(path, O_RDONLY | O_CLOEXEC);
	if (fd < 0) return false;
	const bool ok = fsync(fd) == 0;
	close(fd);
	return ok;
#endif
}

int checkpoint_file_publish(const char *path, const void *data, size_t size)
{
	if (!write_stage(path, data, size)) return 0;
	const auto destination = std::filesystem::u8path(path);
	const auto temporary = std::filesystem::u8path(std::string(path) + ".checkpoint.stage");
	bool ok;
#ifdef _WIN32
	ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	ok = std::rename(temporary.c_str(), destination.c_str()) == 0;
#endif
	std::error_code error;
	if (!ok) std::filesystem::remove(temporary, error);
	return ok && sync_parent(path);
}

static int pair_exists(void *, const char *path)
{
	std::error_code error;
	const bool exists = std::filesystem::exists(std::filesystem::u8path(path), error);
	return exists || !!error;
}

static int pair_rename(void *, const char *from, const char *to)
{
#ifdef _WIN32
	return MoveFileExW(std::filesystem::u8path(from).c_str(), std::filesystem::u8path(to).c_str(),
	                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	std::error_code error;
	std::filesystem::rename(std::filesystem::u8path(from), std::filesystem::u8path(to), error);
	return !error && sync_parent(to);
#endif
}

static int pair_delete(void *, const char *path)
{
	std::error_code error;
	return std::filesystem::remove(std::filesystem::u8path(path), error) && !error && sync_parent(path);
}

static int pair_write_state(void *, const char *path, unsigned state)
{
	const unsigned char bytes[] = { static_cast<unsigned char>(state), static_cast<unsigned char>(state >> 8),
		                            static_cast<unsigned char>(state >> 16), static_cast<unsigned char>(state >> 24) };
	return checkpoint_file_publish(path, bytes, sizeof(bytes));
}

static int pair_read_state(void *, const char *path, unsigned *state)
{
	std::ifstream stream(std::filesystem::u8path(path), std::ios::binary);
	unsigned char bytes[4];
	if (!stream.read(reinterpret_cast<char *>(bytes), sizeof(bytes)) || stream.peek() != EOF) return 0;
	*state = unsigned(bytes[0]) | unsigned(bytes[1]) << 8 | unsigned(bytes[2]) << 16 | unsigned(bytes[3]) << 24;
	return 1;
}

/* A filesystem lock also protects against launcher recovery through the other
 * engine library; a C++ mutex alone would not span those separately linked DSOs */
struct pair_lock {
#ifdef _WIN32
	HANDLE handle = INVALID_HANDLE_VALUE;
	explicit pair_lock(const std::string &path)
	{
		handle = CreateFileW(std::filesystem::u8path(path + ".pair.lock").c_str(), GENERIC_READ | GENERIC_WRITE,
		                     0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	}
	bool ok() const
	{
		return handle != INVALID_HANDLE_VALUE;
	}
	~pair_lock()
	{
		if (ok()) CloseHandle(handle);
	}
#else
	int fd;
	explicit pair_lock(const std::string &path) : fd(open((path + ".pair.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600))
	{
		if (fd >= 0) {
			int result;
			do {
				result = flock(fd, LOCK_EX);
			} while (result && errno == EINTR);
			if (result) {
				close(fd);
				fd = -1;
			}
		}
	}
	bool ok() const
	{
		return fd >= 0;
	}
	~pair_lock()
	{
		if (ok()) close(fd);
	}
#endif
};

static const android_file_pair_ops pair_ops = { nullptr, pair_exists, pair_rename, pair_delete, pair_write_state, pair_read_state };

static int pair_operation(const char *stage, const char *path, const char *companion_stage,
                          const char *companion_path, int present, bool recover)
{
	if (!path || !companion_path || !std::filesystem::u8path(path).is_absolute() ||
	    !std::filesystem::u8path(companion_path).is_absolute() || std::string(path) == companion_path) return 0;
	const std::string backup = std::string(path) + ".bak", companion_backup = std::string(companion_path) + ".bak";
	const std::string pending = std::string(path) + ".pair.pending", committed = std::string(path) + ".pair.committed";
	const android_file_pair_paths paths = { stage, path, backup.c_str(), companion_stage, companion_path,
		                                    companion_backup.c_str(), present, pending.c_str(), committed.c_str() };
	return recover ? android_file_pair_recover(&paths, &pair_ops) : android_file_pair_publish(&paths, &pair_ops);
}

int checkpoint_file_recover_pair(const char *path, const char *companion_path)
{
	if (!path || !companion_path) return 0;
	if (!pair_exists(nullptr, (std::string(path) + ".pair.pending").c_str()) &&
	    !pair_exists(nullptr, (std::string(path) + ".pair.committed").c_str())) return 1;
	pair_lock lock(path);
	return lock.ok() && pair_operation(nullptr, path, nullptr, companion_path, 0, true);
}

int checkpoint_file_publish_staged_pair(const char *stage, const char *path,
                                        const char *companion_stage, const char *companion_path, int present)
{
	if (!path) return 0;
	pair_lock lock(path);
	return lock.ok() && sync_stage(stage) && (!present || sync_stage(companion_stage)) &&
	       pair_operation(stage, path, companion_stage, companion_path, present, false);
}

int checkpoint_file_publish_pair(const char *path, const void *data, size_t size,
                                 const char *companion_path, const void *companion, size_t companion_size)
{
	if (!path || !companion_path || !*companion_path ||
	    !std::filesystem::u8path(companion_path).is_absolute() ||
	    std::string(path) == companion_path || (!companion && companion_size)) return 0;
	const std::string primary_stage = std::string(path) + ".checkpoint.stage";
	const std::string companion_stage = std::string(companion_path) + ".checkpoint.stage";
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::u8path(path).parent_path(), error);
	if (error) return 0;
	pair_lock lock(path);
	if (!lock.ok() || !pair_operation(nullptr, path, nullptr, companion_path, 0, true)) return 0;
	if (!write_stage(path, data, size)) return 0;
	if (companion && !write_stage(companion_path, companion, companion_size)) {
		pair_delete(nullptr, primary_stage.c_str());
		return 0;
	}
	return pair_operation(primary_stage.c_str(), path, companion_stage.c_str(), companion_path, companion != nullptr, false);
}

int checkpoint_file_recover_save(const char *path)
{
	if (!path) return 0;
	const auto primary = std::filesystem::u8path(path);
	const std::string name = primary.filename().u8string();
	if (name.size() < 4 || name.substr(name.size() - 4, 3) != ".sg" || name.back() < '0' || name.back() > '9') return 1;
	const auto companion = primary.parent_path() / (std::string(1, name.back()) + ANDROID_SAVE_SET_SECRET_SUFFIX);
	return checkpoint_file_recover_pair(path, companion.u8string().c_str());
}

void checkpoint_file_recover_directory(const char *directory)
{
	if (!directory) return;
	std::error_code error;
	std::vector<std::string> candidates;
	std::filesystem::directory_iterator iterator(std::filesystem::u8path(directory), error), end;
	for (; !error && iterator != end; iterator.increment(error)) {
		const std::string path = iterator->path().u8string();
		for (const char *suffix : { ".pair.pending", ".pair.committed" }) {
			const size_t length = std::strlen(suffix);
			if (path.size() > length && path.compare(path.size() - length, length, suffix) == 0)
				candidates.push_back(path.substr(0, path.size() - length));
		}
	}
	for (const auto &path : candidates) checkpoint_file_recover_save(path.c_str());
}
