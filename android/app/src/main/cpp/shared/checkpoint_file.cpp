#include "checkpoint_file.h"
extern "C" {
#include "android_file_pair_transaction.h"
}
#include <cstdio>
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

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
	return ok;
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
	return ok;
}

static int pair_exists(void *, const char *path)
{
	std::error_code error;
	return std::filesystem::exists(std::filesystem::u8path(path), error);
}

static int pair_rename(void *, const char *from, const char *to)
{
	std::error_code error;
	std::filesystem::rename(std::filesystem::u8path(from), std::filesystem::u8path(to), error);
	return !error;
}

static int pair_delete(void *, const char *path)
{
	std::error_code error;
	return std::filesystem::remove(std::filesystem::u8path(path), error) && !error;
}

int checkpoint_file_publish_pair(const char *path, const void *data, size_t size,
                                 const char *companion_path, const void *companion, size_t companion_size)
{
	if (!path || !companion_path || !*companion_path ||
	    !std::filesystem::u8path(companion_path).is_absolute() ||
	    std::string(path) == companion_path || (!companion && companion_size)) return 0;
	const std::string primary_stage = std::string(path) + ".checkpoint.stage";
	const std::string companion_stage = std::string(companion_path) + ".checkpoint.stage";
	const std::string primary_backup = std::string(path) + ".bak";
	const std::string companion_backup = std::string(companion_path) + ".bak";
	if (!write_stage(path, data, size)) return 0;
	if (companion && !write_stage(companion_path, companion, companion_size)) {
		pair_delete(nullptr, primary_stage.c_str());
		return 0;
	}
	const android_file_pair_paths paths = {
		primary_stage.c_str(), path, primary_backup.c_str(),
		companion_stage.c_str(), companion_path, companion_backup.c_str(), companion != nullptr
	};
	const android_file_pair_ops ops = { nullptr, pair_exists, pair_rename, pair_delete };
	return android_file_pair_publish(&paths, &ops);
}
