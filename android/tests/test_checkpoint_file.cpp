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
	const auto companion = folder / "secret.bin";
	const auto companion_stage = folder / "secret.bin.checkpoint.stage";
	const auto publish_pair = [&](const char *primary, const char *secret) {
		return checkpoint_file_publish_pair(path.u8string().c_str(), primary, std::char_traits<char>::length(primary),
		                                    companion.u8string().c_str(), secret, secret ? std::char_traits<char>::length(secret) : 0);
	};
	CHECK(publish_pair("pair-one", "secret-one"));
	CHECK(read(path) == "pair-one" && read(companion) == "secret-one");
	CHECK(publish_pair("pair-two", "secret-two"));
	CHECK(read(path) == "pair-two" && read(companion) == "secret-two");
	std::filesystem::create_directory(companion_stage);
	CHECK(!publish_pair("failed", "failed-secret"));
	CHECK(read(path) == "pair-two" && read(companion) == "secret-two");
	CHECK(!std::filesystem::exists(stage));
	std::filesystem::remove(companion_stage);
	const auto backup = folder / "secret.bin.bak";
	CHECK(checkpoint_file_publish(backup.u8string().c_str(), "preserve", 8));
	CHECK(!publish_pair("blocked", "blocked-secret"));
	CHECK(read(path) == "pair-two" && read(companion) == "secret-two");
	CHECK(read(backup) == "preserve");
	CHECK(!std::filesystem::exists(stage) && !std::filesystem::exists(companion_stage));
	std::filesystem::remove(backup);
	CHECK(publish_pair("without-secret", nullptr));
	CHECK(read(path) == "without-secret" && !std::filesystem::exists(companion));
	std::filesystem::remove(path);
	std::filesystem::remove(folder);
	std::puts("PASS: checkpoint replacement and failed publication preserve existing data");
}
