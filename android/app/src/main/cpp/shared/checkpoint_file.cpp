#include "checkpoint_file.h"
#include <cstdio>
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

int checkpoint_file_publish(const char *path, const void *data, size_t size)
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
	if (ok) {
#ifdef _WIN32
		ok = MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		ok = std::rename(temporary.c_str(), destination.c_str()) == 0;
#endif
	}
	if (!ok) std::filesystem::remove(temporary, error);
	return ok;
}
