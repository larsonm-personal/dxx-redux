#include "audio_tag_metadata.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include <fileref.h>
#include <id3v1tag.h>
#include <id3v2tag.h>
#include <mpegfile.h>
#include <tfile.h>
#include <tpropertymap.h>
#include <tstringlist.h>
#include <textidentificationframe.h>

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

static bool copy_fixture(const fs::path &source, const fs::path &target)
{
	std::error_code error;
	fs::copy_file(source, target, fs::copy_options::overwrite_existing, error);
	return !error;
}

static bool write_id3v1(const fs::path &source, const fs::path &target)
{
	if (!copy_fixture(source, target)) return false;
	TagLib::MPEG::File file(target.string().c_str(), false);
	if (!file.isValid()) return false;
	file.ID3v1Tag(true)->setTitle("ID3v1 Title");
	file.ID3v1Tag(true)->setArtist("Legacy Artist");
	return file.save(TagLib::MPEG::File::ID3v1, TagLib::File::StripOthers);
}

static bool write_id3v2(const fs::path &source, const fs::path &target, TagLib::ID3v2::Version version,
                        TagLib::String::Type encoding, const TagLib::String &title,
                        const TagLib::StringList &composers)
{
	if (!copy_fixture(source, target)) return false;
	TagLib::MPEG::File file(target.string().c_str(), false);
	if (!file.isValid()) return false;
	auto *title_frame = new TagLib::ID3v2::TextIdentificationFrame("TIT2", encoding);
	title_frame->setText(title);
	file.ID3v2Tag(true)->addFrame(title_frame);
	auto *composer_frame = new TagLib::ID3v2::TextIdentificationFrame("TCOM", encoding);
	composer_frame->setText(composers);
	file.ID3v2Tag(true)->addFrame(composer_frame);
	return file.save(TagLib::MPEG::File::ID3v2, TagLib::File::StripOthers, version);
}

static std::string parse_json(const fs::path &path, const char *format, audio_tag_metadata &metadata)
{
	audio_tag_metadata_init(&metadata);
	audio_tag_metadata_parse_path(path.string().c_str(), format, &metadata);
	char *json = audio_tag_metadata_to_json(&metadata);
	const std::string result = json ? json : "";
	free(json);
	return result;
}

static bool is_valid_utf8(const std::string &value)
{
	for (size_t index = 0; index < value.size();) {
		const auto first = static_cast<unsigned char>(value[index]);
		if (first < 0x80) {
			++index;
			continue;
		}
		size_t continuation_count;
		unsigned int codepoint;
		if ((first & 0xe0) == 0xc0) {
			continuation_count = 1;
			codepoint = first & 0x1f;
		} else if ((first & 0xf0) == 0xe0) {
			continuation_count = 2;
			codepoint = first & 0x0f;
		} else if ((first & 0xf8) == 0xf0) {
			continuation_count = 3;
			codepoint = first & 0x07;
		} else return false;
		if (index + continuation_count >= value.size()) return false;
		for (size_t offset = 1; offset <= continuation_count; ++offset) {
			const auto next = static_cast<unsigned char>(value[index + offset]);
			if ((next & 0xc0) != 0x80) return false;
			codepoint = (codepoint << 6) | (next & 0x3f);
		}
		if ((continuation_count == 1 && codepoint < 0x80) ||
		    (continuation_count == 2 && codepoint < 0x800) ||
		    (continuation_count == 3 && codepoint < 0x10000) || codepoint > 0x10ffff ||
		    (codepoint >= 0xd800 && codepoint <= 0xdfff)) return false;
		index += continuation_count + 1;
	}
	return true;
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
	check(metadata.duration_ms > 0, "audio duration is retained");
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

	const fs::path id3v1 = temp_dir / "id3v1.mp3";
	check(write_id3v1(source_dir / "bladeenc.mp3", id3v1), "ID3v1 fixture is written");
	audio_tag_metadata id3v1_metadata;
	parse_json(id3v1, "mp3", id3v1_metadata);
	check(std::string(id3v1_metadata.title) == "ID3v1 Title", "ID3v1 title is parsed");
	check(std::string(id3v1_metadata.composer).empty(), "ID3v1 artist is not treated as composer");
	check(std::string(id3v1_metadata.display_name) == "ID3v1 Title", "ID3v1 title is the fallback name");
	audio_tag_metadata_free(&id3v1_metadata);

	const fs::path id3v23 = temp_dir / "id3v23-utf16.mp3";
	check(write_id3v2(source_dir / "bladeenc.mp3", id3v23, TagLib::ID3v2::v3, TagLib::String::UTF16,
	                  TagLib::String("R\xC3\xA9"
	                                 "acteur",
	                                 TagLib::String::UTF8),
	                  TagLib::StringList(TagLib::String("Andr\xC3\xA9", TagLib::String::UTF8))),
	      "ID3v2.3 UTF-16 fixture is written");
	audio_tag_metadata id3v23_metadata;
	parse_json(id3v23, "mp3", id3v23_metadata);
	check(std::string(id3v23_metadata.title) == "R\xC3\xA9"
	                                            "acteur",
	      "ID3v2.3 UTF-16 title is converted to UTF-8");
	check(std::string(id3v23_metadata.composer) == "Andr\xC3\xA9", "ID3v2.3 UTF-16 composer is converted to UTF-8");
	audio_tag_metadata_free(&id3v23_metadata);

	const fs::path id3v24 = temp_dir / "id3v24-multiple.mp3";
	TagLib::StringList composers;
	composers.append("First Composer");
	composers.append("Second Composer");
	check(write_id3v2(source_dir / "bladeenc.mp3", id3v24, TagLib::ID3v2::v4, TagLib::String::UTF8,
	                  TagLib::String("Multiple", TagLib::String::UTF8), composers),
	      "ID3v2.4 multiple-composer fixture is written");
	audio_tag_metadata id3v24_metadata;
	const std::string first_json = parse_json(id3v24, "mp3", id3v24_metadata);
	check(std::string(id3v24_metadata.composer) == "First Composer; Second Composer",
	      "multiple composers retain order");
	check(std::string(id3v24_metadata.display_name) == "Multiple (First Composer; Second Composer)",
	      "multiple composers appear in the fallback name");
	audio_tag_metadata_free(&id3v24_metadata);
	audio_tag_metadata repeated_metadata;
	const std::string second_json = parse_json(id3v24, "mp3", repeated_metadata);
	check(first_json == second_json, "metadata JSON is deterministic");
	audio_tag_metadata_free(&repeated_metadata);

	const fs::path no_tags = temp_dir / "no-tags.flac";
	check(copy_fixture(source_dir / "no-tags.flac", no_tags), "no-tags fixture is copied");
	audio_tag_metadata no_tags_metadata;
	parse_json(no_tags, "flac", no_tags_metadata);
	check(no_tags_metadata.status == AUDIO_TAG_STATUS_NO_TAGS, "no-tags status is stable");
	check(no_tags_metadata.duration_ms > 0, "untagged audio duration is retained");
	audio_tag_metadata_free(&no_tags_metadata);

	const fs::path oversized = temp_dir / "oversized.ogg";
	check(copy_fixture(source_dir / "test.ogg", oversized), "oversized fixture is copied");
	{
		TagLib::FileRef file(oversized.string().c_str(), false);
		TagLib::PropertyMap properties = file.file()->properties();
		properties.replace("TITLE", TagLib::StringList(TagLib::String(std::string(4096, 'T'), TagLib::String::UTF8)));
		file.file()->setProperties(properties);
		check(file.file()->save(), "oversized fixture tag is written");
	}
	audio_tag_metadata oversized_metadata;
	parse_json(oversized, "ogg", oversized_metadata);
	check(oversized_metadata.status == AUDIO_TAG_STATUS_TRUNCATED, "oversized metadata reports truncation");
	check(strlen(oversized_metadata.title) < AUDIO_TAG_TEXT_BYTES, "oversized title respects the field bound");
	check(strlen(oversized_metadata.display_name) < AUDIO_TAG_DISPLAY_BYTES,
	      "oversized display name respects the field bound");
	audio_tag_metadata_free(&oversized_metadata);

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

	const fs::path malformed_length = temp_dir / "malformed-length.mp3";
	{
		const unsigned char bytes[] = { 'I', 'D', '3', 4, 0, 0, 0x7f, 0x7f, 0x7f, 0x7f };
		std::ofstream output(malformed_length, std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
	}
	audio_tag_metadata malformed_length_metadata;
	audio_tag_metadata_init(&malformed_length_metadata);
	check(audio_tag_metadata_parse_path(malformed_length.string().c_str(), "mp3", &malformed_length_metadata) == 0,
	      "malformed ID3 length is rejected");
	check(malformed_length_metadata.status == AUDIO_TAG_STATUS_INVALID,
	      "malformed ID3 length has invalid status");
	audio_tag_metadata_free(&malformed_length_metadata);

	const fs::path invalid_utf8 = temp_dir / "invalid-utf8.mp3";
	{
		const unsigned char tag[] = {
			'I',
			'D',
			'3',
			4,
			0,
			0,
			0,
			0,
			0,
			16,
			'T',
			'I',
			'T',
			'2',
			0,
			0,
			0,
			6,
			0,
			0,
			3,
			'B',
			'a',
			'd',
			0xc3,
			'(',
		};
		std::ofstream output(invalid_utf8, std::ios::binary | std::ios::trunc);
		output.write(reinterpret_cast<const char *>(tag), sizeof(tag));
		std::ifstream input(source_dir / "bladeenc.mp3", std::ios::binary);
		output << input.rdbuf();
	}
	audio_tag_metadata invalid_utf8_metadata;
	const std::string invalid_utf8_json = parse_json(invalid_utf8, "mp3", invalid_utf8_metadata);
	check(is_valid_utf8(invalid_utf8_metadata.title), "invalid UTF-8 is sanitized in normalized fields");
	check(is_valid_utf8(invalid_utf8_json), "invalid UTF-8 cannot escape into JSON");
	audio_tag_metadata_free(&invalid_utf8_metadata);

	fs::remove_all(temp_dir, error);
	if (failures) {
		std::cerr << failures << " audio tag metadata checks failed\n";
		return 1;
	}
	std::cout << "audio tag metadata checks passed\n";
	return 0;
}
