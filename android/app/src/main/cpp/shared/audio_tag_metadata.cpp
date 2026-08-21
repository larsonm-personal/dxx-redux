#include "audio_tag_metadata.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#ifndef AUDIO_TAG_NO_PHYSFS
#include <physfs.h>
#endif
#include <fileref.h>
#include <flacfile.h>
#include <mpegfile.h>
#include <tfile.h>
#include <tiostream.h>
#include <tpropertymap.h>
#include <vorbisfile.h>

namespace
{

class ReadOnlyStream : public TagLib::IOStream
{
  public:
	explicit ReadOnlyStream(std::string name) : name_(std::move(name)) {}
	TagLib::FileName name() const override
	{
		return TagLib::FileName(name_.c_str());
	}
	void writeBlock(const TagLib::ByteVector &) override {}
	void insert(const TagLib::ByteVector &, TagLib::offset_t, size_t) override {}
	void removeBlock(TagLib::offset_t, size_t) override {}
	bool readOnly() const override
	{
		return true;
	}
	void truncate(TagLib::offset_t) override {}

  protected:
	std::string name_;
};

class StdioStream final : public ReadOnlyStream
{
  public:
	explicit StdioStream(const char *path) : ReadOnlyStream(path ? path : ""), file_(path ? fopen(path, "rb") : nullptr)
	{
		if (!file_) return;
#ifdef _WIN32
		if (_fseeki64(file_, 0, SEEK_END) != 0) return;
		length_ = _ftelli64(file_);
		_fseeki64(file_, 0, SEEK_SET);
#else
		if (fseeko(file_, 0, SEEK_END) != 0) return;
		length_ = ftello(file_);
		fseeko(file_, 0, SEEK_SET);
#endif
	}
	~StdioStream() override
	{
		if (file_) fclose(file_);
	}
	TagLib::ByteVector readBlock(size_t length) override
	{
		if (!file_ || !length) return {};
		TagLib::ByteVector data(length, 0);
		const size_t read = fread(data.data(), 1, length, file_);
		data.resize(read);
		return data;
	}
	bool isOpen() const override
	{
		return file_ && length_ >= 0;
	}
	void seek(TagLib::offset_t offset, Position position) override
	{
		if (!file_) return;
		const int origin = position == Beginning ? SEEK_SET : position == Current ? SEEK_CUR
		                                                                          : SEEK_END;
#ifdef _WIN32
		_fseeki64(file_, offset, origin);
#else
		fseeko(file_, offset, origin);
#endif
	}
	TagLib::offset_t tell() const override
	{
		if (!file_) return -1;
#ifdef _WIN32
		return _ftelli64(file_);
#else
		return ftello(file_);
#endif
	}
	TagLib::offset_t length() override
	{
		return length_;
	}

  private:
	FILE *file_ = nullptr;
	TagLib::offset_t length_ = -1;
};

#ifndef AUDIO_TAG_NO_PHYSFS
class PhysFsStream final : public ReadOnlyStream
{
  public:
	explicit PhysFsStream(const char *path) : ReadOnlyStream(path ? path : ""), file_(path ? PHYSFS_openRead(path) : nullptr)
	{
		if (file_) length_ = PHYSFS_fileLength(file_);
	}
	~PhysFsStream() override
	{
		if (file_) PHYSFS_close(file_);
	}
	TagLib::ByteVector readBlock(size_t length) override
	{
		if (!file_ || !length || length > static_cast<size_t>(std::numeric_limits<PHYSFS_sint64>::max())) return {};
		TagLib::ByteVector data(length, 0);
		const PHYSFS_sint64 read = PHYSFS_readBytes(file_, data.data(), static_cast<PHYSFS_uint64>(length));
		if (read < 0) return {};
		data.resize(static_cast<size_t>(read));
		return data;
	}
	bool isOpen() const override
	{
		return file_ && length_ >= 0;
	}
	void seek(TagLib::offset_t offset, Position position) override
	{
		if (!file_) return;
		const PHYSFS_sint64 current = PHYSFS_tell(file_);
		const TagLib::offset_t base = position == Beginning ? 0 : position == Current ? current
		                                                                              : length_;
		if (base < 0 || (offset < 0 && base < -offset) || (offset > 0 && base > length_ - offset)) return;
		const TagLib::offset_t target = base + offset;
		if (target >= 0 && target <= length_) PHYSFS_seek(file_, static_cast<PHYSFS_uint64>(target));
	}
	TagLib::offset_t tell() const override
	{
		return file_ ? PHYSFS_tell(file_) : -1;
	}
	TagLib::offset_t length() override
	{
		return length_;
	}

  private:
	PHYSFS_file *file_ = nullptr;
	TagLib::offset_t length_ = -1;
};
#endif

static std::string lower_extension(const char *path, const char *extension)
{
	std::string value = extension ? extension : "";
	if (value.empty() && path) {
		const char *dot = strrchr(path, '.');
		if (dot) value = dot + 1;
	}
	if (!value.empty() && value[0] == '.') value.erase(0, 1);
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return value;
}

static std::string clean_text(const std::string &input)
{
	std::string output;
	bool whitespace = false;
	for (unsigned char ch : input) {
		if (ch < 0x20 || ch == 0x7f) {
			if (ch == '\t' || ch == '\r' || ch == '\n') whitespace = !output.empty();
			continue;
		}
		if (ch == ' ') {
			whitespace = !output.empty();
			continue;
		}
		if (whitespace) output.push_back(' ');
		whitespace = false;
		output.push_back(static_cast<char>(ch));
	}
	return output;
}

static bool is_placeholder(const std::string &value)
{
	std::string lower = value;
	std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
	return lower.empty() || lower == "unknown" || lower == "untitled" || lower == "[unknown]" || lower == "[untitled]";
}

static void copy_truncated(char *output, size_t output_size, const std::string &value, int *truncated)
{
	if (!output || !output_size) return;
	size_t length = value.size();
	if (length >= output_size) {
		length = output_size > 4 ? output_size - 4 : 0;
		while (length > 0 && (static_cast<unsigned char>(value[length]) & 0xc0) == 0x80) --length;
		memcpy(output, value.data(), length);
		if (output_size > 4) memcpy(output + length, "...", 4);
		else output[0] = '\0';
		if (truncated) *truncated = 1;
		return;
	}
	memcpy(output, value.data(), length);
	output[length] = '\0';
}

static std::vector<std::string> values_for(const TagLib::PropertyMap &properties, const char *key)
{
	std::vector<std::string> result;
	const auto iterator = properties.find(TagLib::String(key, TagLib::String::UTF8));
	if (iterator == properties.cend()) return result;
	for (const auto &value : iterator->second) {
		const std::string cleaned = clean_text(value.to8Bit(true));
		if (!cleaned.empty()) result.push_back(cleaned);
	}
	return result;
}

static std::string joined_values(const TagLib::PropertyMap &properties, const char *key)
{
	const auto values = values_for(properties, key);
	std::string joined;
	for (const auto &value : values) {
		if (!joined.empty()) joined += "; ";
		joined += value;
	}
	return joined;
}

static char *copy_allocated(const std::string &value)
{
	char *copy = static_cast<char *>(malloc(value.size() + 1));
	if (copy) memcpy(copy, value.c_str(), value.size() + 1);
	return copy;
}

static std::unique_ptr<TagLib::File> open_tag_file(TagLib::IOStream *stream, const std::string &format)
{
	if (format == "mp3") return std::make_unique<TagLib::MPEG::File>(stream, true, TagLib::AudioProperties::Fast);
	if (format == "ogg") return std::make_unique<TagLib::Ogg::Vorbis::File>(stream, true, TagLib::AudioProperties::Fast);
	if (format == "flac") return std::make_unique<TagLib::FLAC::File>(stream, true, TagLib::AudioProperties::Fast);
	return nullptr;
}

static bool has_supported_signature(TagLib::IOStream *stream, const std::string &format)
{
	stream->seek(0);
	const TagLib::ByteVector header = stream->readBlock(10);
	stream->seek(0);
	const bool has_id3 = header.size() >= 10 && memcmp(header.data(), "ID3", 3) == 0;
	bool valid_id3 = false;
	if (has_id3) {
		valid_id3 = static_cast<unsigned char>(header[3]) != 0xff &&
		            static_cast<unsigned char>(header[4]) != 0xff;
		TagLib::offset_t tag_size = 0;
		for (size_t index = 6; valid_id3 && index < 10; ++index) {
			const auto byte = static_cast<unsigned char>(header[index]);
			if (byte & 0x80) valid_id3 = false;
			else tag_size = (tag_size << 7) | byte;
		}
		const TagLib::offset_t footer_size =
		    static_cast<unsigned char>(header[3]) == 4 && (static_cast<unsigned char>(header[5]) & 0x10) ? 10 : 0;
		valid_id3 = valid_id3 && tag_size <= stream->length() - 10 - footer_size;
	}
	if (format == "ogg") return header.size() >= 4 && memcmp(header.data(), "OggS", 4) == 0;
	if (format == "flac")
		return (header.size() >= 4 && memcmp(header.data(), "fLaC", 4) == 0) ||
		       valid_id3;
	if (format == "mp3")
		return valid_id3 ||
		       (header.size() >= 2 && static_cast<unsigned char>(header[0]) == 0xff &&
		        (static_cast<unsigned char>(header[1]) & 0xe0) == 0xe0);
	return false;
}

static int parse_stream(TagLib::IOStream *stream, const std::string &format, audio_tag_metadata *metadata)
{
	if (!metadata) return 0;
	audio_tag_metadata_free(metadata);
	audio_tag_metadata_init(metadata);
	copy_truncated(metadata->format, sizeof(metadata->format), format, &metadata->metadata_truncated);
	if (format != "mp3" && format != "ogg" && format != "flac") {
		metadata->status = AUDIO_TAG_STATUS_UNSUPPORTED;
		return 0;
	}
	if (!stream || !stream->isOpen() || stream->length() < 0 || stream->length() > AUDIO_TAG_MAX_SOURCE_BYTES) {
		metadata->status = AUDIO_TAG_STATUS_IO_ERROR;
		return 0;
	}
	if (!has_supported_signature(stream, format)) {
		metadata->status = AUDIO_TAG_STATUS_INVALID;
		return 0;
	}
	auto file = open_tag_file(stream, format);
	if (!file || !file->isValid()) {
		metadata->status = AUDIO_TAG_STATUS_INVALID;
		return 0;
	}
	if (const TagLib::AudioProperties *audio = file->audioProperties()) {
		const long long duration = audio->lengthInMilliseconds();
		if (duration > 0 && duration <= std::numeric_limits<int>::max()) metadata->duration_ms = static_cast<int>(duration);
	}
	const TagLib::PropertyMap properties = file->properties();
	copy_truncated(metadata->title, sizeof(metadata->title), joined_values(properties, "TITLE"), &metadata->metadata_truncated);
	copy_truncated(metadata->composer, sizeof(metadata->composer), joined_values(properties, "COMPOSER"), &metadata->metadata_truncated);
	copy_truncated(metadata->artist, sizeof(metadata->artist), joined_values(properties, "ARTIST"), &metadata->metadata_truncated);
	copy_truncated(metadata->album_artist, sizeof(metadata->album_artist), joined_values(properties, "ALBUMARTIST"), &metadata->metadata_truncated);
	copy_truncated(metadata->album, sizeof(metadata->album), joined_values(properties, "ALBUM"), &metadata->metadata_truncated);
	copy_truncated(metadata->date, sizeof(metadata->date), joined_values(properties, "DATE"), &metadata->metadata_truncated);
	copy_truncated(metadata->genre, sizeof(metadata->genre), joined_values(properties, "GENRE"), &metadata->metadata_truncated);
	copy_truncated(metadata->comment, sizeof(metadata->comment), joined_values(properties, "COMMENT"), &metadata->metadata_truncated);
	copy_truncated(metadata->copyright, sizeof(metadata->copyright), joined_values(properties, "COPYRIGHT"), &metadata->metadata_truncated);
	copy_truncated(metadata->track_number, sizeof(metadata->track_number), joined_values(properties, "TRACKNUMBER"), &metadata->metadata_truncated);
	copy_truncated(metadata->disc_number, sizeof(metadata->disc_number), joined_values(properties, "DISCNUMBER"), &metadata->metadata_truncated);

	size_t total_text = 0;
	std::vector<audio_tag_property> retained;
	for (auto iterator = properties.cbegin(); iterator != properties.cend(); ++iterator) {
		if (retained.size() >= AUDIO_TAG_MAX_PROPERTIES) {
			metadata->metadata_truncated = 1;
			break;
		}
		const std::string key = clean_text(iterator->first.to8Bit(true));
		if (key.empty()) continue;
		audio_tag_property property{};
		property.key = copy_allocated(key);
		property.values = static_cast<char **>(calloc(AUDIO_TAG_MAX_VALUES, sizeof(char *)));
		if (!property.key || !property.values) {
			free(property.key);
			free(property.values);
			continue;
		}
		for (const auto &tag_value : iterator->second) {
			std::string value = clean_text(tag_value.to8Bit(true));
			if (value.empty()) continue;
			if (property.value_count >= AUDIO_TAG_MAX_VALUES || total_text >= AUDIO_TAG_MAX_TOTAL_TEXT) {
				metadata->metadata_truncated = 1;
				break;
			}
			if (value.size() >= AUDIO_TAG_MAX_VALUE_BYTES) {
				value.resize(AUDIO_TAG_MAX_VALUE_BYTES - 4);
				while (!value.empty() && (static_cast<unsigned char>(value.back()) & 0xc0) == 0x80) value.pop_back();
				value += "...";
				metadata->metadata_truncated = 1;
			}
			if (total_text + key.size() + value.size() > AUDIO_TAG_MAX_TOTAL_TEXT) {
				metadata->metadata_truncated = 1;
				break;
			}
			property.values[property.value_count] = copy_allocated(value);
			if (!property.values[property.value_count]) continue;
			++property.value_count;
			total_text += key.size() + value.size();
		}
		if (property.value_count) retained.push_back(property);
		else {
			free(property.key);
			free(property.values);
		}
	}
	if (!retained.empty()) {
		metadata->properties = static_cast<audio_tag_property *>(calloc(retained.size(), sizeof(audio_tag_property)));
		if (metadata->properties) {
			memcpy(metadata->properties, retained.data(), retained.size() * sizeof(audio_tag_property));
			metadata->property_count = static_cast<unsigned int>(retained.size());
		} else {
			for (auto &property : retained) {
				for (unsigned int index = 0; index < property.value_count; ++index) free(property.values[index]);
				free(property.values);
				free(property.key);
			}
		}
	}

	const std::string title = metadata->title;
	const std::string composer = metadata->composer;
	if (!is_placeholder(title)) {
		const std::string display = is_placeholder(composer) ? title : title + " (" + composer + ")";
		copy_truncated(metadata->display_name, sizeof(metadata->display_name), display, &metadata->metadata_truncated);
	}
	metadata->status = metadata->property_count == 0 ? AUDIO_TAG_STATUS_NO_TAGS : metadata->metadata_truncated ? AUDIO_TAG_STATUS_TRUNCATED
	                                                                                                           : AUDIO_TAG_STATUS_OK;
	return metadata->status == AUDIO_TAG_STATUS_OK || metadata->status == AUDIO_TAG_STATUS_TRUNCATED;
}

static void append_json_string(std::ostringstream &output, const char *value)
{
	output << '"';
	for (const unsigned char ch : std::string(value ? value : "")) {
		switch (ch) {
			case '"': output << "\\\""; break;
			case '\\': output << "\\\\"; break;
			case '\b': output << "\\b"; break;
			case '\f': output << "\\f"; break;
			case '\n': output << "\\n"; break;
			case '\r': output << "\\r"; break;
			case '\t': output << "\\t"; break;
			default:
				if (ch < 0x20) {
					const char hex[] = "0123456789abcdef";
					output << "\\u00" << hex[ch >> 4] << hex[ch & 15];
				} else output << static_cast<char>(ch);
		}
	}
	output << '"';
}

} // namespace

extern "C" void audio_tag_metadata_init(audio_tag_metadata *metadata)
{
	if (!metadata) return;
	memset(metadata, 0, sizeof(*metadata));
	metadata->status = AUDIO_TAG_STATUS_INVALID;
}

extern "C" void audio_tag_metadata_free(audio_tag_metadata *metadata)
{
	if (!metadata) return;
	for (unsigned int property_index = 0; property_index < metadata->property_count; ++property_index) {
		audio_tag_property *property = &metadata->properties[property_index];
		for (unsigned int value_index = 0; value_index < property->value_count; ++value_index) free(property->values[value_index]);
		free(property->values);
		free(property->key);
	}
	free(metadata->properties);
	metadata->properties = nullptr;
	metadata->property_count = 0;
}

extern "C" int audio_tag_metadata_parse_path(const char *path, const char *extension, audio_tag_metadata *metadata)
{
	StdioStream stream(path);
	return parse_stream(&stream, lower_extension(path, extension), metadata);
}

extern "C" int audio_tag_metadata_parse_physfs(const char *filename, audio_tag_metadata *metadata)
{
#ifdef AUDIO_TAG_NO_PHYSFS
	(void) filename;
	if (metadata) {
		audio_tag_metadata_free(metadata);
		audio_tag_metadata_init(metadata);
		metadata->status = AUDIO_TAG_STATUS_UNSUPPORTED;
	}
	return 0;
#else
	PhysFsStream stream(filename);
	return parse_stream(&stream, lower_extension(filename, nullptr), metadata);
#endif
}

extern "C" const char *audio_tag_status_name(audio_tag_status status)
{
	switch (status) {
		case AUDIO_TAG_STATUS_OK: return "ok";
		case AUDIO_TAG_STATUS_NO_TAGS: return "no_tags";
		case AUDIO_TAG_STATUS_UNSUPPORTED: return "unsupported";
		case AUDIO_TAG_STATUS_TRUNCATED: return "truncated";
		case AUDIO_TAG_STATUS_INVALID: return "invalid";
		case AUDIO_TAG_STATUS_IO_ERROR: return "io_error";
	}
	return "invalid";
}

extern "C" char *audio_tag_metadata_to_json(const audio_tag_metadata *metadata)
{
	if (!metadata) return nullptr;
	std::ostringstream output;
	output << "{\"parse_status\":";
	append_json_string(output, audio_tag_status_name(metadata->status));
#define AUDIO_TAG_JSON_FIELD(name) \
	output << ",\"" #name "\":";   \
	append_json_string(output, metadata->name)
	AUDIO_TAG_JSON_FIELD(format);
	AUDIO_TAG_JSON_FIELD(title);
	AUDIO_TAG_JSON_FIELD(composer);
	AUDIO_TAG_JSON_FIELD(artist);
	AUDIO_TAG_JSON_FIELD(album_artist);
	AUDIO_TAG_JSON_FIELD(album);
	AUDIO_TAG_JSON_FIELD(date);
	AUDIO_TAG_JSON_FIELD(genre);
	AUDIO_TAG_JSON_FIELD(comment);
	AUDIO_TAG_JSON_FIELD(copyright);
	AUDIO_TAG_JSON_FIELD(track_number);
	AUDIO_TAG_JSON_FIELD(disc_number);
	AUDIO_TAG_JSON_FIELD(display_name);
#undef AUDIO_TAG_JSON_FIELD
	output << ",\"duration_ms\":" << metadata->duration_ms;
	output << ",\"metadata_truncated\":" << (metadata->metadata_truncated ? "true" : "false") << ",\"properties\":[";
	for (unsigned int property_index = 0; property_index < metadata->property_count; ++property_index) {
		const audio_tag_property &property = metadata->properties[property_index];
		if (property_index) output << ',';
		output << "{\"key\":";
		append_json_string(output, property.key);
		output << ",\"values\":[";
		for (unsigned int value_index = 0; value_index < property.value_count; ++value_index) {
			if (value_index) output << ',';
			append_json_string(output, property.values[value_index]);
		}
		output << "]}";
	}
	output << "]}";
	const std::string text = output.str();
	return copy_allocated(text);
}
