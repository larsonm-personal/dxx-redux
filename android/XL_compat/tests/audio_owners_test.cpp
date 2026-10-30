#include "audio_owners.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
using namespace xl_compat;
void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
AudioOwner owner(bool forever = false)
{
	AudioOwner result;
	result.flags = sound_used | sound_link_object | (forever ? sound_forever : 0);
	result.sound = 0;
	result.object = 17;
	result.signature = 101;
	result.volume = 65536;
	result.pan = 32767;
	return result;
}
void check_survivors(AudioVoices &actual, AudioVoices expected, const std::vector<AudioSample> &samples)
{
	std::array<std::int16_t, 32> a{}, b{};
	actual.mix(reinterpret_cast<unsigned char *>(a.data()), 16, samples);
	expected.mix(reinterpret_cast<unsigned char *>(b.data()), 16, samples);
	require(a == b, "Owner removal changed unrelated PCM output");
	for (unsigned i = 0; i < actual.voices.size(); ++i) {
		const auto &x = actual.voices[i], &y = expected.voices[i];
		require(x.serial == y.serial && x.position == y.position && x.remaining == y.remaining && x.priority.sound_class == y.priority.sound_class && x.priority.volume == y.priority.volume && x.output.volume == y.output.volume && x.output.pan == y.output.pan, "Owner removal changed a surviving voice lifetime, controls or playhead");
	}
}
} // namespace

void audio_owner_pcm_controls()
{
	using namespace xl_compat;
	const std::vector<AudioSample> samples{ { 44100, { 255, -255, 510, -510, 765, -765, 1020, -1020 } } };
	unsigned controls = 0;
	// A registry index and a named-action index can be numerically equal without
	// sharing ownership. Ordered compaction must not change either voice's PCM
	for (unsigned removed : { 0u, 1u, 2u }) {
		AudioOwners owners;
		AudioVoices mixer;
		for (unsigned i = 0; i < 3; ++i) {
			require(owners.add(owner(true)) == int(i) && owners.start(i, 32768, false, mixer), "Cannot prepare linked output controls");
		}
		const int named = owners.start_voice(0, -1, 0, audio_priority(0, 65536, 32768, -1, true), {}, AudioOwnerKind::action, mixer);
		const int unlinked = owners.start_voice(0, -1, -1, audio_priority(2, 65536, 32768, -1, true), {}, AudioOwnerKind::unlinked, mixer);
		require(named == 3 && unlinked == 4 && owners.valid(mixer), "Mixed owner types cannot share the pool");
		std::array<std::int16_t, 2> first{};
		mixer.mix(reinterpret_cast<unsigned char *>(first.data()), 1, samples);
		require(first[0] == 635 && first[1] == -635, "Mixed linked, named and unlinked first PCM frame differs");
		auto expected = mixer;
		expected.stop(int(removed));
		owners.erase(removed, mixer);
		require(owners.count == 2 && owners.valid(mixer), "Ordered removal invalidated surviving bindings");
		require(mixer.voices[unsigned(named)].owner == 0 && mixer.voices[unsigned(named)].kind == AudioOwnerKind::action && mixer.voices[unsigned(unlinked)].owner == -1, "Compaction changed an unrelated kind of owner");
		check_survivors(mixer, expected, samples);
		owners.clear(mixer);
		require(!owners.count && owners.valid(mixer) && mixer.voices[unsigned(named)].remaining && mixer.voices[unsigned(unlinked)].remaining, "Registry clear stopped unowned voices");
		++controls;
	}
	// Finite completion occurs inside the actual mixer. Reusing its slot before
	// the game-thread reconciliation must still retire exactly the old owner
	for (bool reuse_first : { false, true }) {
		AudioOwners owners;
		AudioVoices mixer;
		require(owners.add(owner()) == 0 && owners.start(0, 32768, false, mixer), "Cannot prepare completed owner");
		std::array<std::int16_t, 8> pcm{};
		mixer.mix(reinterpret_cast<unsigned char *>(pcm.data()), 4, samples);
		require(pcm == std::array<std::int16_t, 8>{ 127, -127, 254, -254, 381, -381, 508, -508 } && mixer.completions == 1 && !mixer.voices[0].remaining && owners.count == 1, "Real mixer completion changed PCM or mutated the game-thread registry");
		mixer.cursor = 0;
		if (reuse_first) require(owners.start_voice(0, -1, 0, audio_priority(0, 65536, 32768, -1, true), {}, AudioOwnerKind::action, mixer) == 0, "Cannot reuse a completed object channel");
		owners.reconcile(mixer);
		require(!owners.count && owners.valid(mixer), "Completed voice retained its finite owner");
		if (reuse_first) require(mixer.voices[0].remaining == -1 && mixer.voices[0].kind == AudioOwnerKind::action, "Old completion stopped the replacement action");
		++controls;
	}
	// A full pool can preempt a finite or forever owner. Exercise every kind of
	// replacement, including a new owner whose dense index shifts on retirement
	for (bool forever : { false, true })
		for (AudioOwnerKind kind : { AudioOwnerKind::action, AudioOwnerKind::object, AudioOwnerKind::unlinked }) {
			AudioOwners owners;
			AudioVoices mixer;
			for (unsigned i = 0; i < audio_owner_limit; ++i) {
				auto entry = owner(i ? true : forever);
				entry.volume = i ? 65536 : 1024;
				require(owners.add(entry) == int(i), "Registry did not reach source capacity");
			}
			const auto exhausted_serial = owners.next_serial;
			require(owners.add(owner()) == -1 && owners.next_serial == exhausted_serial, "Owner overflow mutated the registry");
			for (unsigned i = 0; i < audio_voice_limit; ++i) require(owners.start(i, 32768, false, mixer), "Cannot fill the shared pool");
			const auto first_serial = mixer.voices[0].serial;
			const auto last_owner_serial = owners.entries.back().serial;
			const auto preserved = mixer.voices;
			const int requested = kind == AudioOwnerKind::object ? 499 : kind == AudioOwnerKind::action ? 0
			                                                                                            : -1;
			require(owners.start_voice(0, -1, requested, audio_priority(0, 65536, 32768, kind == AudioOwnerKind::object ? requested : -1, true), {}, kind, mixer) == 0, "Full pool did not replace its quietest owner");
			require(owners.count == (forever ? 500u : 499u) && owners.valid(mixer) && mixer.voices[0].serial != first_serial && mixer.voices[0].kind == kind, "Preemption lost replacement identity or retained a finite owner");
			if (kind == AudioOwnerKind::object) require(owners.entries[owners.count - 1].serial == last_owner_serial && mixer.voices[0].owner == int(owners.count - 1) && owners.entries[owners.count - 1].channel == 0, "Finite retirement failed to rebind the newly started owner");
			if (forever) require(owners.entries[0].channel == -1, "Preempted forever owner remained attached");
			for (unsigned i = 1; i < audio_voice_limit; ++i) require(mixer.voices[i].serial == preserved[i].serial && mixer.voices[i].position == preserved[i].position && mixer.voices[i].remaining == preserved[i].remaining, "Full-pool replacement changed an unrelated lifetime");
			owners.reconcile(mixer);
			require(owners.valid(mixer) && mixer.voices[0].remaining == -1, "Reconciliation removed the new replacement");
			++controls;
		}
	// An owner detached by a backend stop remains restartable only if forever
	for (bool forever : { false, true }) {
		AudioOwners owners;
		AudioVoices mixer;
		owners.add(owner(forever));
		require(owners.start(0, 32768, false, mixer), "Cannot prepare backend retirement");
		mixer.stop(0);
		owners.reconcile(mixer);
		require(owners.count == unsigned(forever) && owners.valid(mixer), "Backend retirement changed finite/forever lifetime policy");
		if (forever) require(owners.start(0, 32768, false, mixer) && owners.valid(mixer) && owners.entries[0].channel == 1, "Detached forever owner cannot restart");
		++controls;
	}
	// Native object-slot reuse invalidates only the stale linked lifetime
	{
		AudioOwners owners;
		AudioVoices mixer;
		owners.add(owner());
		owners.add(owner(true));
		auto replacement = owner(true);
		replacement.signature = 102;
		owners.add(replacement);
		for (unsigned i = 0; i < 3; ++i) require(owners.start(i, 32768, false, mixer), "Cannot prepare native-lifetime retirement");
		require(owners.destroy_missing([](int object, int signature) { return object == 17 && signature == 102; }, mixer), "Missing finite owner was not removed");
		require(owners.count == 2 && owners.entries[0].signature == 101 && owners.entries[0].channel == -1 && owners.entries[1].signature == 102 && owners.entries[1].channel == 2 && owners.valid(mixer), "Object-slot reuse stopped the replacement lifetime");
		++controls;
	}

	// Sync can start the newest owner, retire the oldest finite owner, and then
	// compact hundreds of absent owners. Every surviving lifetime is visited once
	{
		AudioOwners owners;
		AudioVoices mixer;
		for (unsigned i = 0; i < audio_owner_limit; ++i) {
			auto entry = owner(i != 0);
			entry.volume = i ? 65536 : 1024;
			entry.audio_volume = 32768;
			if (i == audio_owner_limit - 1) entry.pan = 0;
			require(owners.add(entry) == int(i), "Cannot fill linked frame-update owners");
		}
		for (unsigned i = 0; i < audio_voice_limit; ++i) require(owners.start(i, 32768, false, mixer), "Cannot fill linked frame-update channels");
		std::array<std::int16_t, 2> pcm{};
		mixer.mix(reinterpret_cast<unsigned char *>(pcm.data()), 1, samples);
		const auto before = mixer.voices;
		const auto replacement_serial = mixer.next_serial;
		std::vector<std::uint64_t> visited;
		owners.sync([&](const AudioOwner &entry) {
			visited.push_back(entry.serial);
			return AudioOwnerUpdate{ entry.serial <= 128 || entry.serial == 500, true, 65536, entry.serial == 500 ? 65536 : 32767 };
		},
		            mixer, { 32768, 32768 });
		require(owners.count == 128 && owners.valid(mixer) && visited.size() == 499 && visited.front() == 500 && visited.back() == 2, "Linked update lost a surviving owner or visited an already removed lifetime");
		for (unsigned i = 1; i < visited.size(); ++i) require(visited[i - 1] > visited[i], "Linked update visited an owner twice or changed reverse order");
		require(owners.entries[127].serial == 500 && owners.entries[127].channel == 0 && mixer.voices[0].owner == 127 && mixer.voices[0].serial == replacement_serial && mixer.voices[0].output.pan == std::array<std::uint8_t, 2>{ 254, 0 }, "Linked update applied pan to the wrong owner after replacement/compaction");
		for (unsigned i = 1; i < audio_voice_limit; ++i) require(mixer.voices[i].serial == before[i].serial && mixer.voices[i].position == before[i].position && mixer.voices[i].remaining == before[i].remaining && mixer.voices[i].output.pan == before[i].output.pan, "Linked update changed an unrelated playing channel");
		pcm.fill(0);
		mixer.mix(reinterpret_cast<unsigned char *>(pcm.data()), 1, samples);
		// SetPan(65536) yields {254, 0}; 127 survivors each contribute {254, -254}
		require(pcm == std::array<std::int16_t, 2>{ 32512, -32258 }, "Linked update produced the wrong mixed PCM after full-pool replacement");
		++controls;
	}
	for (unsigned removed : { 0u, 1u }) {
		AudioOwners owners;
		AudioVoices mixer;
		for (unsigned i = 0; i < 3; ++i) {
			owners.add(owner(true));
			require(owners.start(i, 32768, false, mixer), "Cannot start linked sync removal controls");
		}
		const int named = owners.start_voice(0, -1, 0, audio_priority(0, 65536, 32768, -1, true), {}, AudioOwnerKind::action, mixer);
		const int unlinked = owners.start_voice(0, -1, -1, audio_priority(2, 65536, 32768, -1, true), {}, AudioOwnerKind::unlinked, mixer);
		auto expected = mixer;
		expected.stop(int(removed));
		owners.sync([&](const AudioOwner &entry) { return AudioOwnerUpdate{ entry.serial != removed + 1, true, entry.volume, entry.pan }; }, mixer, { 32768, 32768 });
		require(owners.count == 2 && owners.valid(mixer) && mixer.voices[unsigned(named)].remaining && mixer.voices[unsigned(unlinked)].remaining, "Missing linked lifetime removed unrelated named or unlinked playback");
		check_survivors(mixer, expected, samples);
		++controls;
	}
	std::cout << "PASS: " << controls << " mixed-pool PCM, 500-owner/128-channel exhaustion, preemption, compaction, actual completion and object-reuse controls\n";
}
