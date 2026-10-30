#include "linked_audio_state.h"
#include "shared_audio_internal.h"
#include "../audio_spatial.h"
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
}
namespace xl_compat
{
namespace
{
std::int64_t number(const nlohmann::json &value, std::int64_t low, std::int64_t high)
{
	if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<std::uint64_t>() > std::uint64_t(high)))
		throw std::runtime_error("Invalid linked-audio state integer");
	const auto result = value.get<std::int64_t>();
	if (result < low || result > high) throw std::runtime_error("Linked-audio integer is out of range");
	return result;
}
} // namespace
nlohmann::json encode_linked_audio(const AudioOwners &owners, bool deferred)
{
	nlohmann::json rows = nlohmann::json::array();
	for (unsigned i = 0; i < owners.count; ++i) {
		const auto &entry = owners.entries[i];
		rows.push_back({ { "serial", entry.serial }, { "channel_serial", entry.channel_serial }, { "flags", entry.flags }, { "sample", entry.sample }, { "sound", entry.sound }, { "class", entry.sound_class }, { "channel", entry.channel }, { "object", entry.object }, { "signature", entry.signature }, { "segment", entry.segment }, { "side", entry.side }, { "position", entry.position }, { "volume", entry.volume }, { "pan", entry.pan }, { "max_volume", entry.max_volume }, { "max_distance", entry.max_distance }, { "loop_start", entry.loop_start }, { "loop_end", entry.loop_end }, { "decay", entry.decay }, { "audio_volume", entry.audio_volume }, { "ambient", entry.ambient }, { "custom", entry.custom }, { "external", entry.external } });
	}
	return { { "next", owners.next_serial }, { "deferred", deferred }, { "owners", std::move(rows) } };
}
LinkedAudioState decode_linked_audio(const nlohmann::json &state, const AudioVoices &voices)
{
	if (!state.is_object() || state.size() != 3 || !state.at("deferred").is_boolean() || !state.at("owners").is_array() || state.at("owners").size() > audio_owner_limit)
		throw std::runtime_error("Invalid linked-audio state layout");
	LinkedAudioState restored;
	restored.deferred = state.at("deferred").get<bool>();
	restored.owners.next_serial = std::uint64_t(number(state.at("next"), 1, INT64_MAX));
	for (const auto &row : state.at("owners")) {
		if (!row.is_object() || row.size() != 23 || !row.at("ambient").is_boolean() || !row.at("custom").is_boolean() || !row.at("external").is_boolean())
			throw std::runtime_error("Invalid linked-audio owner layout");
		auto &entry = restored.owners.entries[restored.owners.count++];
		entry.serial = std::uint64_t(number(row.at("serial"), 1, std::int64_t(restored.owners.next_serial - 1)));
		entry.channel_serial = std::uint64_t(number(row.at("channel_serial"), 0, std::int64_t(voices.next_serial - 1)));
		entry.flags = unsigned(number(row.at("flags"), 0, 63));
		entry.sample = unsigned(number(row.at("sample"), 0, INT32_MAX));
		entry.sound = int(number(row.at("sound"), 0, INT32_MAX));
		entry.sound_class = int(number(row.at("class"), 0, 6));
		entry.channel = int(number(row.at("channel"), -1, audio_voice_limit - 1));
		entry.object = int(number(row.at("object"), -1, MAX_OBJECTS - 1));
		entry.signature = int(number(row.at("signature"), INT32_MIN, INT32_MAX));
		entry.segment = int(number(row.at("segment"), -1, Highest_segment_index));
		entry.side = int(number(row.at("side"), -1, 5));
		if (!row.at("position").is_array() || row.at("position").size() != 3) throw std::runtime_error("Invalid linked-audio position");
		for (unsigned axis = 0; axis < 3; ++axis) entry.position[axis] = int(number(row.at("position")[axis], INT32_MIN, INT32_MAX));
		entry.volume = int(number(row.at("volume"), 0, INT32_MAX));
		entry.pan = int(number(row.at("pan"), 0, 65536));
		entry.max_volume = int(number(row.at("max_volume"), 0, INT32_MAX));
		entry.max_distance = int(number(row.at("max_distance"), 1, INT32_MAX));
		entry.loop_start = int(number(row.at("loop_start"), INT32_MIN, INT32_MAX));
		entry.loop_end = int(number(row.at("loop_end"), INT32_MIN, INT32_MAX));
		entry.decay = int(number(row.at("decay"), 0, 6));
		entry.audio_volume = int(number(row.at("audio_volume"), -1, 65536));
		entry.ambient = row.at("ambient").get<bool>();
		entry.custom = row.at("custom").get<bool>();
		entry.external = row.at("external").get<bool>();
		const auto link = entry.flags & (sound_link_object | sound_link_position);
		if ((entry.flags & ~(sound_permanent | sound_link_object | sound_link_position)) != (sound_used | sound_forever) ||
		    (link != sound_link_object && link != sound_link_position) || entry.external ||
		    audio_runtime::classic_sample(entry.sound) != int(entry.sample))
			throw std::runtime_error("Linked-audio owner has an invalid type or sample");
		(void) audio_extended_range(entry.max_distance, entry.decay);
		if (link == sound_link_object) {
			if (entry.object < 0 || entry.segment != -1 || entry.side != -1 || entry.position != std::array<std::int32_t, 3>{} ||
			    entry.decay != entry.sound_class || entry.ambient != (entry.sound_class == 1))
				throw std::runtime_error("Invalid object-linked audio state");
			// Stopped owners may retain disappeared lifetimes until the next sync
			if (entry.channel >= 0 && (entry.object > Highest_object_index || Objects[entry.object].type == OBJ_NONE || Objects[entry.object].signature != entry.signature))
				throw std::runtime_error("Playing linked sound belongs to a different object lifetime");
		} else if (entry.segment < 0 || entry.object != -1 || entry.signature || entry.sound_class || entry.decay || entry.ambient || entry.custom || entry.loop_start != -1 || entry.loop_end != -1)
			throw std::runtime_error("Invalid segment-linked audio state");
		if (entry.channel >= 0) {
			const auto &voice = voices.voices[unsigned(entry.channel)];
			const auto expected = audio_priority(entry.sound_class, entry.volume, voice.priority.volume, int(restored.owners.count - 1), true);
			if (voice.remaining != -1 || voice.sample != entry.sample || voice.priority.sound_class != expected.sound_class || !voice.priority.persistent)
				throw std::runtime_error("Linked-audio voice differs from its owner");
		}
	}
	if (!restored.owners.valid(voices)) throw std::runtime_error("Linked-audio owner/channel bindings are inconsistent");
	return restored;
}
} // namespace xl_compat
