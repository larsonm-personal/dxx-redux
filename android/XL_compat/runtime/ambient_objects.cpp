#include "ambient_objects.h"
#include "../ambient_sound.h"
#include "linked_audio.h"
#include "shared_audio_internal.h"
#include "preview.h"
#include <nlohmann/json.hpp>
#include <map>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "object.h"
}
namespace
{
struct Entry {
	xl_compat::AmbientSound sound;
	int signature;
	bool alive = true;
};
std::map<int, Entry> entries;
bool native_sound(int index)
{
	return index >= 0 && index <= Highest_object_index && Objects[index].type == 20 && Objects[index].id == 2 &&
	       Objects[index].control_type == CT_NONE && Objects[index].movement_type == MT_NONE && Objects[index].render_type == RT_NONE;
}
bool bound(int index, const Entry &entry)
{
	return entry.alive && native_sound(index) && Objects[index].signature == entry.signature;
}
} // namespace

void xl_compat::bind_ambient_objects(const Inventory &level)
{
	std::map<int, Entry> prepared;
	for (unsigned index = 0; index < level.objects.size(); ++index) {
		const auto &object = level.objects[index];
		if (object.render != 13) continue;
		if (!native_sound(int(index))) throw std::runtime_error("XL ambient object identity changed during native loading");
		auto sound = decode_ambient_sound(object);
		if (sound.volume < 0 || sound.filename.empty()) throw std::runtime_error("XL ambient sound has an invalid playback definition");
		prepared.emplace(int(index), Entry{ std::move(sound), Objects[index].signature });
	}
	entries = std::move(prepared);
}

extern "C" void xl_ambient_objects_clear(void)
{
	entries.clear();
}
extern "C" void xl_ambient_objects_start(void)
{
	if (!xl_level_extensions_active()) return;
	for (const auto &[index, entry] : entries) {
		if (!bound(index, entry)) continue;
		const auto filename = entry.sound.filename + ".wav";
		// Disabled objects still own a deferred loop; SyncSounds gates playback
		xl_audio_link_external(filename.c_str(), 1, index, 1, entry.sound.volume, 256 * F1_0, 0, 0, 0, 0);
	}
}
extern "C" int xl_ambient_object_enabled(int index)
{
	const auto found = entries.find(index);
	return found == entries.end() || !bound(index, found->second) || found->second.sound.enabled;
}
extern "C" int xl_ambient_object_set_enabled(int index, int enabled)
{
	if (!xl_level_extensions_active()) return 0;
	const auto found = entries.find(index);
	if (found == entries.end() || !bound(index, found->second)) return 0;
	found->second.sound.enabled = enabled != 0;
	return 1;
}
extern "C" int xl_ambient_object_noncolliding(int type)
{
	return type == 20 && xl_level_extensions_active();
}

nlohmann::json xl_compat::ambient_objects_snapshot()
{
	auto state = nlohmann::json::array();
	for (const auto &[index, entry] : entries)
		state.push_back({ index, entry.signature, entry.sound.enabled, bound(index, entry) });
	return state;
}
bool xl_compat::restore_ambient_objects(const nlohmann::json &state, bool apply)
{
	try {
		if (!state.is_array() || state.size() != entries.size()) return false;
		auto prepared = entries;
		unsigned ordinal = 0;
		for (auto &[index, entry] : prepared) {
			const auto &row = state[ordinal++];
			if (!row.is_array() || row.size() != 4 || !row[0].is_number_integer() || row[0] != index || !row[1].is_number_integer() ||
			    (row[1].is_number_unsigned() && row[1].get<std::uint64_t>() > INT32_MAX) ||
			    row[1].get<std::int64_t>() < 0 || row[1].get<std::int64_t>() > INT32_MAX ||
			    !row[2].is_boolean() || !row[3].is_boolean()) return false;
			entry.signature = row[1].get<int>();
			entry.sound.enabled = row[2].get<bool>();
			entry.alive = row[3].get<bool>();
			const bool native = native_sound(index) && Objects[index].signature == entry.signature;
			if (entry.alive ? !native : native_sound(index)) return false;
		}
		if (apply) entries = std::move(prepared);
		return true;
	} catch (const std::exception &) {
		return false;
	}
}

bool xl_compat::ambient_audio_matches(const nlohmann::json &audio)
{
	try {
		// Called after ordinary audio-state validation. Authored sound objects have
		// one immutable resource definition even while their loop is stopped
		for (const auto &owner : audio.at("linked").at("owners")) {
			const int index = owner.at("object").get<int>();
			const auto found = entries.find(index);
			if (found == entries.end() || !native_sound(index) || owner.at("signature") != Objects[index].signature) continue;
			const auto &sound = found->second.sound;
			if (!owner.at("external").get<bool>() || owner.at("sample") != audio_runtime::external_sample((sound.filename + ".wav").c_str()) ||
			    owner.at("class") != 1 || owner.at("max_volume") != sound.volume || owner.at("max_distance") != 256 * F1_0 ||
			    owner.at("loop_start") != 0 || owner.at("loop_end") != 0 || owner.at("custom").get<bool>()) return false;
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}
