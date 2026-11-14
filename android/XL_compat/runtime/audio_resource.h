#ifndef DXX_XL_AUDIO_RESOURCE_H
#define DXX_XL_AUDIO_RESOURCE_H
#include <string>
#include <optional>
#include <map>
#include <set>
namespace xl_compat
{
class TargetSources;
struct AudioResource {
	std::string path, bytes;
};
// Selected HOG, owned mission/level folders, then owned startup addon sounds
AudioResource load_audio_resource(const std::string &authored, const char *level, int ordinal, TargetSources *target = nullptr);
// Classic replacements use bank suppression and CFile folder/HOG fallback, without aliases
struct ClassicAudioScope {
	std::string mission, bank;
	int ordinal, rate;
	std::map<std::string, std::set<std::string>> directories;
	std::optional<AudioResource> selected_bank;
	TargetSources *target = nullptr;
};
std::optional<ClassicAudioScope> prepare_classic_audio_scope(const char *level, int ordinal, int bank_rate, TargetSources *target = nullptr);
std::optional<AudioResource> load_classic_audio_resource(const std::string &sample, ClassicAudioScope &scope);
} // namespace xl_compat
#endif
