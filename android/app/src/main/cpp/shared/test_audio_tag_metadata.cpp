#include "audio_tag_metadata.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <fileref.h>
#include <tfile.h>
#include <tpropertymap.h>

namespace fs = std::filesystem;

static int failures;

static void check(bool condition, const char *message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	++failures;
}

static bool copy_and_tag(const fs::path &source, const fs::path &target)
{
	std::error_code error;
	fs::copy_file(source, target, fs::copy_options::overwrite_existing, error);
	if (error) return false;
	TagLib::FileRef file(target.string().c_str(), false);
	if (file.isNull() || !file.file()) return false;
	TagLib::PropertyMap properties = file.file()->properties();
	properties.replace("TITLE", TagLib::StringList(TagLib::String("Reactor Core", TagLib::String::UTF8)));
	properties.replace("COMPOSER", TagLib::StringList(TagLib::String("Jane Doe", TagLib::String::UTF8)));
	properties.replace("ARTIST", TagLib::StringList(TagLib::String("Test Performer", TagLib::String::UTF8)));
	properties.replace("ALBUM", TagLib::StringList(TagLib::String("DXX Test Album", TagLib::String::UTF8)));
	file.file()->setProperties(properties);
	return file.file()->save();
}

static void verify_tagged_file(const fs::path &path, const char *format)
{
	audio_tag_metadata metadata;
	audio_tag_metadata_init(&metadata);
	check(audio_tag_metadata_parse_path(path.string().c_str(), format, &metadata) != 0, "tagged file parses");
	check(std::string(metadata.title) == "Reactor Core", "title is normalized");
	check(std::string(metadata.composer) == "Jane Doe", "composer is normalized");
	check(std::string(metadata.artist) == "Test Performer", "artist remains distinct from composer");
	check(std::string(metadata.display_name) == "Reactor Core (Jane Doe)", "display name uses title and composer");
	check(metadata.property_count >= 4, "raw property list is retained");
	char *json = audio_tag_metadata_to_json(&metadata);
	check(json && std::string(json).find("\"display_name\":\"Reactor Core (Jane Doe)\"") != std::string::npos,
	      "JSON contains display name");
	free(json);
	audio_tag_metadata_free(&metadata);
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cerr << "usage: test_audio_tag_metadata <taglib-test-data> <temp-dir>\n";
		return 2;
	}
	const fs::path source_dir = argv[1];
	const fs::path temp_dir = argv[2];
	std::error_code error;
	fs::create_directories(temp_dir, error);
	check(!error, "temporary directory is available");

	const struct {
		const char *source;
		const char *target;
		const char *format;
	} fixtures[] = {
		{ "bladeenc.mp3", "tagged.mp3", "mp3" },
		{ "test.ogg", "tagged.ogg", "ogg" },
		{ "no-tags.flac", "tagged.flac", "flac" },
	};
	for (const auto &fixture : fixtures) {
		const fs::path target = temp_dir / fixture.target;
		check(copy_and_tag(source_dir / fixture.source, target), "fixture tags are written");
		verify_tagged_file(target, fixture.format);
	}

	const fs::path malformed = temp_dir / "malformed.mp3";
	{
		std::ofstream output(malformed, std::ios::binary | std::ios::trunc);
		output << "not an audio file";
	}
	audio_tag_metadata invalid;
	audio_tag_metadata_init(&invalid);
	check(audio_tag_metadata_parse_path(malformed.string().c_str(), "mp3", &invalid) == 0,
	      "malformed input is rejected");
	check(invalid.status == AUDIO_TAG_STATUS_INVALID, "malformed status is stable");
	audio_tag_metadata_free(&invalid);

	fs::remove_all(temp_dir, error);
	if (failures) {
		std::cerr << failures << " audio tag metadata checks failed\n";
		return 1;
	}
	std::cout << "audio tag metadata checks passed\n";
	return 0;
}
