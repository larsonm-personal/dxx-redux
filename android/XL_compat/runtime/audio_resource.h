#ifndef DXX_XL_AUDIO_RESOURCE_H
#define DXX_XL_AUDIO_RESOURCE_H
#include <string>
#include <optional>
#include <map>
#include <set>
namespace xl_compat
{
struct AudioResource {
	std::string path, bytes;
};
// Resolve only the selected HOG and its owned mission/level sound folders
AudioResource load_audio_resource(const std::string &authored, const char *level, int ordinal);
// Classic replacements use bank suppression and CFile folder/HOG fallback, without aliases
struct ClassicAudioScope {
	std::string mission, bank;
	int ordinal, rate;
	std::map<std::string, std::set<std::string>> directories;
	std::optional<AudioResource> selected_bank;
};
std::optional<ClassicAudioScope> prepare_classic_audio_scope(const char *level, int ordinal, int bank_rate);
std::optional<AudioResource> load_classic_audio_resource(const std::string &sample, ClassicAudioScope &scope);
} // namespace xl_compat
#endif
