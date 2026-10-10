#include "checkpoint_file.h"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#define CHECK(condition)                                                         \
	do {                                                                         \
		if (!(condition)) {                                                      \
			std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			std::exit(1);                                                        \
		}                                                                        \
	} while (0)

static std::string read(const std::filesystem::path &path)
{
	std::ifstream stream(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(stream), {});
}

int main()
{
	const auto folder = std::filesystem::absolute("checkpoint-file-test");
	const auto path = folder / "checkpoint.bin";
	const auto stage = folder / "checkpoint.bin.checkpoint.stage";
	std::filesystem::create_directories(folder);
	CHECK(checkpoint_file_publish(path.u8string().c_str(), "first", 5));
	CHECK(read(path) == "first");
	CHECK(checkpoint_file_publish(path.u8string().c_str(), "second", 6));
	CHECK(read(path) == "second" && !std::filesystem::exists(stage));
	CHECK(!checkpoint_file_publish(path.u8string().c_str(), nullptr, 0));
	CHECK(read(path) == "second");
	/* Force staging failure with a directory where the temporary file belongs */
	std::filesystem::create_directory(stage);
	CHECK(!checkpoint_file_publish(path.u8string().c_str(), "bad", 3));
	CHECK(read(path) == "second");
	std::filesystem::remove(stage);
	/* Force publication failure after a successful staged write */
	const auto blocked = folder / "directory-target";
	std::filesystem::create_directory(blocked);
	CHECK(!checkpoint_file_publish(blocked.u8string().c_str(), "bad", 3));
	CHECK(std::filesystem::is_directory(blocked));
	CHECK(!std::filesystem::exists(folder / "directory-target.checkpoint.stage"));
	std::filesystem::remove(blocked);
	std::filesystem::remove(path);
	std::filesystem::remove(folder);
	std::puts("PASS: checkpoint replacement and failed publication preserve existing data");
}
