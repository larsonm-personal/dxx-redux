#include "audio_owners.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace xl_compat
{
bool AudioOwners::bound(unsigned index, const AudioVoices &mixer) const
{
	if (index >= count) return false;
	const auto &entry = entries[index];
	if (entry.channel < 0 || unsigned(entry.channel) >= mixer.voices.size()) return false;
	const auto &voice = mixer.voices[unsigned(entry.channel)];
	return voice.kind == AudioOwnerKind::object && voice.owner == int(index) && voice.serial == entry.channel_serial;
}

int AudioOwners::add(AudioOwner entry)
{
	if (count >= entries.size() || !next_serial || next_serial >= std::uint64_t(INT64_MAX)) return -1;
	entry.serial = next_serial++;
	entry.channel = -1;
	entry.channel_serial = 0;
	entries[count] = std::move(entry);
	return int(count++);
}

unsigned AudioOwners::active() const
{
	return unsigned(std::count_if(entries.begin(), entries.begin() + count, [](const auto &entry) { return entry.channel >= 0; }));
}

void AudioOwners::stop(unsigned index, AudioVoices &mixer)
{
	if (index >= count) return;
	if (bound(index, mixer)) {
		auto &voice = mixer.voices[unsigned(entries[index].channel)];
		mixer.stop(entries[index].channel);
		voice.owner = -1;
	}
	entries[index].channel = -1;
	entries[index].channel_serial = 0;
}

void AudioOwners::erase(unsigned index, AudioVoices &mixer)
{
	if (index >= count) return;
	stop(index, mixer);
	// The source container shifts entries. Repair every moved binding, not just
	// the last one: the source's swap-style repair loses unrelated live owners
	for (unsigned i = index + 1; i < count; ++i) {
		if (bound(i, mixer)) mixer.voices[unsigned(entries[i].channel)].owner = int(i - 1);
		entries[i - 1] = std::move(entries[i]);
	}
	entries[--count] = {};
}

void AudioOwners::end(unsigned index, AudioVoices &mixer)
{
	if (index >= count) return;
	if (entries[index].flags & sound_forever) stop(index, mixer);
	else erase(index, mixer);
}

bool AudioOwners::suspend(int threshold, AudioVoices &mixer)
{
	int selected = -1, minimum = 1000 * 65536;
	for (unsigned i = 0; i < count; ++i)
		if (entries[i].channel >= 0 && minimum > entries[i].volume) {
			minimum = entries[i].volume;
			selected = int(i);
		}
	if (selected < 0 || minimum > threshold) return false;
	stop(unsigned(selected), mixer);
	return true;
}

int AudioOwners::start_voice(unsigned sample, int repetitions, int owner, AudioPriority priority, AudioOutput output, AudioOwnerKind kind, AudioVoices &mixer)
{
	if (kind == AudioOwnerKind::object && (owner < 0 || unsigned(owner) >= count || entries[unsigned(owner)].channel >= 0)) return -1;
	AudioVoice retired;
	const int slot = mixer.start(sample, repetitions, owner, priority, output, kind, &retired);
	if (slot < 0) return slot;
	if (kind == AudioOwnerKind::object) {
		auto &entry = entries[unsigned(owner)];
		entry.channel = slot;
		entry.channel_serial = mixer.voices[unsigned(slot)].serial;
	}
	// The replacement already has a new serial. Retiring the old owner must not
	// stop it; ordered removal rebinds the replacement if its owner also shifts
	if (retired.serial && retired.kind == AudioOwnerKind::object && retired.owner >= 0 && unsigned(retired.owner) < count && entries[unsigned(retired.owner)].channel_serial == retired.serial)
		end(unsigned(retired.owner), mixer);
	return slot;
}

bool AudioOwners::start(unsigned index, int effects_volume, bool deferred, AudioVoices &mixer, bool available)
{
	if (index >= count || entries[index].channel >= 0 || deferred || effects_volume < 0 || effects_volume > 65536) return false;
	const auto entry = entries[index];
	if (entry.volume <= 0 || (entry.sound < 0 && !entry.external)) return false;
	const auto effective = std::int64_t(entry.volume) * effects_volume / 65536;
	const auto repetitions = (entry.flags & sound_forever) ? -1 : std::int64_t(entry.loop_end) - entry.loop_start + 1;
	if (effective > INT32_MAX / 2 || repetitions < INT32_MIN || repetitions > INT32_MAX) return false;
	if ((entry.flags & sound_permanent) && active() >= (std::max) (1u, 33u * unsigned(mixer.voices.size()) / 100u) && !suspend(entry.volume, mixer)) return false;
	if (!available) return false;
	const auto priority = audio_priority(entry.sound_class, entry.volume, int(effective), int(index), (entry.flags & sound_forever) != 0);
	return start_voice(entry.sample, int(repetitions), int(index), priority, audio_start_output(int(effective), entry.pan), AudioOwnerKind::object, mixer) >= 0;
}

bool AudioOwners::destroy_object(int object, int sound, AudioVoices &mixer)
{
	bool removed = false;
	for (unsigned i = count; i;) {
		const auto &entry = entries[--i];
		if ((entry.flags & (sound_used | sound_link_object)) == (sound_used | sound_link_object) && entry.object == object && (sound < 0 || entry.sound == sound)) {
			erase(i, mixer);
			removed = true;
		}
	}
	return removed;
}

bool AudioOwners::destroy_segment(int segment, int side, int sound, AudioVoices &mixer)
{
	bool removed = false;
	for (unsigned i = count; i;) {
		const auto &entry = entries[--i];
		if ((entry.flags & (sound_used | sound_link_position)) == (sound_used | sound_link_position) && entry.segment == segment && entry.side == side && (sound < 0 || entry.sound == sound)) {
			erase(i, mixer);
			removed = true;
		}
	}
	return removed;
}

bool AudioOwners::destroy_missing(const std::function<bool(int, int)> &matches, AudioVoices &mixer)
{
	bool removed = false;
	for (unsigned i = count; i;) {
		const auto &entry = entries[--i];
		if ((entry.flags & (sound_used | sound_link_object)) != (sound_used | sound_link_object) || matches(entry.object, entry.signature)) continue;
		if (entry.flags & sound_forever) stop(i, mixer);
		else {
			erase(i, mixer);
			removed = true;
		}
	}
	return removed;
}

void AudioOwners::reconcile(AudioVoices &mixer)
{
	for (unsigned i = count; i;) {
		--i;
		if (entries[i].channel >= 0 && (!bound(i, mixer) || !mixer.voices[unsigned(entries[i].channel)].remaining)) end(i, mixer);
	}
}

void AudioOwners::sync(const std::function<AudioOwnerUpdate(const AudioOwner &)> &update, AudioVoices &mixer,
                       std::array<int, 2> effects_volume, bool deferred, bool available)
{
	if (count > entries.size() || effects_volume[0] < 0 || effects_volume[0] > 65536 || effects_volume[1] < 0 || effects_volume[1] > 65536)
		throw std::runtime_error("Invalid XL sound-owner update state");
	const auto rounded_gain = [](int volume, int setting) {
		const auto effective = std::int64_t(volume) * setting / 65536;
		if (volume < 0 || setting < -1 || setting > 65536 || effective < INT32_MIN / 2 || effective > INT32_MAX / 2)
			throw std::runtime_error("XL linked gain exceeds the defined source range");
		const float scaled = float(2 * effective) / 65536.0f * 128.0f;
		return int(scaled < 0.0f ? scaled - 0.5f : scaled + 0.5f);
	};
	const auto find_serial = [&](std::uint64_t serial) {
		return std::lower_bound(entries.begin(), entries.begin() + count, serial,
		                        [](const AudioOwner &entry, std::uint64_t value) { return entry.serial < value; });
	};
	// Callback completions retire owners on the game thread, before frame updates
	reconcile(mixer);
	std::uint64_t cursor = next_serial;
	while (count) {
		auto found = find_serial(cursor);
		if (found == entries.begin()) break;
		--found;
		const auto previous = *found;
		cursor = previous.serial;
		if (!(previous.flags & sound_used)) continue;
		const unsigned index = unsigned(found - entries.begin());
		const int old_volume = rounded_gain(previous.volume, previous.audio_volume);
		if (!(previous.flags & sound_forever) && previous.channel < 0) {
			erase(index, mixer);
			continue;
		}
		const bool object_link = !(previous.flags & sound_link_position) && (previous.flags & sound_link_object);
		const auto next = previous.flags & (sound_link_position | sound_link_object)
		                      ? update(previous)
		                      : AudioOwnerUpdate{ true, true, previous.volume, previous.pan };
		if (object_link && !next.exists) {
			erase(index, mixer);
			continue;
		}
		if (object_link && !next.enabled) {
			stop(index, mixer);
			continue;
		}
		if (next.volume < 0 || next.pan < 0 || next.pan > 65536)
			throw std::runtime_error("Invalid XL linked spatial output");
		found->volume = next.volume;
		found->pan = next.pan;
		if (previous.custom) continue;
		const int setting = effects_volume[previous.ambient ? 1 : 0];
		const int new_volume = rounded_gain(next.volume, setting);
		if (old_volume != new_volume || ((new_volume <= 0) != (previous.channel < 0))) {
			found->audio_volume = setting;
			if (new_volume <= 0) {
				if (previous.channel >= 0) end(index, mixer);
			} else if (previous.channel < 0) start(index, effects_volume[0], deferred, mixer, available);
			else mixer.set_volume(previous.channel, next.volume, setting);
		}
		// Starting can retire an earlier owner and shift this owner's dense index
		found = find_serial(previous.serial);
		if (found != entries.begin() + count && found->serial == previous.serial && new_volume > 0 && previous.pan != next.pan && found->channel >= 0)
			mixer.set_pan(found->channel, next.pan);
	}
	reconcile(mixer);
}

void AudioOwners::clear(AudioVoices &mixer)
{
	while (count) erase(count - 1, mixer);
}

bool AudioOwners::valid(const AudioVoices &mixer) const
{
	if (count > entries.size() || !next_serial || next_serial > std::uint64_t(INT64_MAX)) return false;
	std::uint64_t previous = 0;
	for (unsigned i = 0; i < count; ++i) {
		const auto &entry = entries[i];
		if (entry.serial <= previous || entry.serial >= next_serial || (entry.flags & ~63u)) return false;
		if (entry.channel < 0 ? entry.channel != -1 || entry.channel_serial != 0 : !bound(i, mixer)) return false;
		previous = entry.serial;
	}
	for (unsigned slot = 0; slot < mixer.voices.size(); ++slot) {
		const auto &voice = mixer.voices[slot];
		if (voice.remaining && voice.kind == AudioOwnerKind::object && (voice.owner < 0 || unsigned(voice.owner) >= count || entries[unsigned(voice.owner)].channel != int(slot) || !bound(unsigned(voice.owner), mixer))) return false;
	}
	return true;
}
} // namespace xl_compat
