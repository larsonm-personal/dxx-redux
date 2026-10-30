// Execute the pinned allocator with independent channel storage and playback
#include "audio_voice.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

namespace channel_reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETVAL(value) return value;
#define RETURN        return;
#include "xl_reference_audio_constants.h"
struct CFixVector {
};
int ChannelPersistent(int nSoundObj, int bLooping, int nVolume);
struct CAudioChannel {
	bool playing = false, persistent = false;
	int sound_class = 0, volume = 0;
	bool Playing() const
	{
		return playing;
	}
	int Persistent() const
	{
		return persistent;
	}
	int SoundClass() const
	{
		return sound_class;
	}
	int Volume() const
	{
		return volume;
	}
	int Start(int16_t, int sound_class_, fix volume_, int32_t, int32_t looping,
	          int32_t, int32_t, int32_t object, int32_t, const char *, CFixVector *);
};
struct Channels {
	std::array<CAudioChannel, MAX_SOUND_CHANNELS> values{};
	CAudioChannel *Buffer()
	{
		return values.data();
	}
	CAudioChannel *operator+(int index)
	{
		return &values.at(unsigned(index));
	}
	CAudioChannel &operator[](int index)
	{
		return values.at(unsigned(index));
	}
	int Index(CAudioChannel *channel)
	{
		return channel ? int(channel - values.data()) : -1;
	}
};
struct CAudio {
	struct {
		int nMaxChannels = MAX_SOUND_CHANNELS, nFreeChannel = 0;
		bool bAvailable = true;
	} m_info;
	Channels m_channels;
	bool fail_start = false;
	std::vector<int> operations;
	int SoundClassCount(int32_t);
	CAudioChannel *FindFreeChannel(int32_t);
	int StartSound(int16_t, int32_t, fix, int32_t, int32_t, int32_t, int32_t, int32_t, int32_t, const char *, CFixVector *);
	void StopAllChannels(bool);
	void StopSound(int slot)
	{
		m_channels[slot].playing = false;
	}
	void StopLoopingSound() {}
	void StopObjectSounds() {}
	void DestroyObjectSound(int object)
	{
		if (object != -1) throw std::runtime_error("Unexpected source owner cleanup request");
		operations.push_back(1);
	}
} audio;
struct {
	struct {
		bool bUseSound = true;
	} app;
} gameStates;
struct {
	struct {
		int bMoving = 0;
	} multiplayer;
	struct {
		struct {
			int bSound = 0;
		} firing[1];
	} weaponData;
} gameData;
struct {
	void Init() {}
} soundQueue;
struct {
	void Mute() {}
} lightningManager;
#include "xl_reference_audio_channels.h"

int CAudioChannel::Start(int16_t, int sound_class_, fix volume_, int32_t, int32_t looping,
                         int32_t, int32_t, int32_t object, int32_t, const char *, CFixVector *)
{
	audio.operations.push_back(2);
	if (audio.fail_start) return -1;
	sound_class = sound_class_;
	persistent = ChannelPersistent(object, looping, volume_) != 0;
	volume = int(std::int64_t(volume_) * 32768 / 65536);
	playing = true;
	return audio.m_info.nFreeChannel;
}
#undef ENTER
#undef RETVAL
#undef RETURN
} // namespace channel_reference
xl_compat::AudioPriority audio_channel_reference_priority(int sound_class, int volume, int pan, int looping, int owner)
{
	using namespace channel_reference;
	audio = {};
	const int slot = audio.StartSound(0, sound_class, volume, pan, looping, -1, -1, owner, 65536, nullptr, nullptr);
	if (slot < 0) throw std::runtime_error("Source priority fixture did not start");
	const auto &channel = audio.m_channels[slot];
	return { channel.sound_class, channel.volume, channel.persistent };
}

namespace
{
void require(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}

void reset_reference()
{
	channel_reference::audio = {};
	channel_reference::gameStates.app.bUseSound = true;
}

unsigned comparisons = 0;
int start_reference(int sound_class, int volume = 65536, int object = -1, bool looping = false)
{
	return channel_reference::audio.StartSound(0, sound_class, volume, 32767, looping, 0, 0, object, 65536, "prepared.wav", nullptr);
}

void compare(xl_compat::AudioVoices native, int sound_class)
{
	using namespace channel_reference;
	reset_reference();
	audio.m_info.nFreeChannel = int(native.cursor);
	for (unsigned i = 0; i < native.voices.size(); ++i) {
		const auto &voice = native.voices[i];
		audio.m_channels[int(i)] = { voice.remaining != 0, voice.priority.persistent, voice.priority.sound_class, voice.priority.volume };
	}
	const auto before = native.voices;
	const int expected = start_reference(sound_class);
	const auto priority = xl_compat::audio_priority(sound_class, 65536, 32768, -1, false);
	const int actual = native.start(0, 1, 500, priority);
	require(expected >= 0 && actual == expected && native.cursor == unsigned(audio.m_info.nFreeChannel), "XL source and owned channel selection or cursor differ");
	require(audio.operations == std::vector<int>{ 1, 2 }, "Source allocation did not clean disappeared owners before channel start");
	for (unsigned i = 0; i < native.voices.size(); ++i) {
		const auto &voice = native.voices[i];
		const auto &source = audio.m_channels[int(i)];
		require(bool(voice.remaining) == source.playing && (!voice.remaining ||
		                                                    (voice.priority.sound_class == source.sound_class && voice.priority.persistent == source.persistent && voice.priority.volume == source.volume)),
		        "Channel replacement changed unrelated priority or occupancy");
		if (i != unsigned(actual)) require(voice.serial == before[i].serial && voice.owner == before[i].owner, "Channel selection replaced another owner");
	}
	++comparisons;
}
} // namespace

void audio_channel_source_controls()
{
	using namespace xl_compat;
	using namespace channel_reference;
	static_assert(audio_voice_limit == MAX_SOUND_CHANNELS);
	for (int sound_class : { -1, 0, 1, 7 })
		for (int volume : { 0, 32768, 65536, 65537, 131072 })
			for (int object : { -1, 0, 17 })
				for (bool looping : { false, true }) {
					reset_reference();
					audio.m_info.nFreeChannel = 127;
					require(start_reference(sound_class, volume, object, looping) == 127 && audio.m_info.nFreeChannel == 0, "Source request did not wrap the free cursor");
					const auto expected = audio.m_channels[127];
					const auto actual = audio_priority(sound_class, volume, volume / 2, object, looping);
					require(actual.sound_class == expected.sound_class && actual.persistent == expected.persistent && actual.volume == expected.volume, "Source class normalization or persistence differs");
					++comparisons;
				}
	for (unsigned cursor : { 0u, 1u, 63u, 64u, 127u })
		for (unsigned matching : { 0u, 63u, 64u, 65u, 128u })
			for (int persistence : { 0, 1, 2 })
				for (int volumes : { 0, 1, 2, 3 }) {
					AudioVoices native;
					native.cursor = cursor;
					for (unsigned i = 0; i < audio_voice_limit; ++i) {
						const int volume = volumes == 0 ? 100 : volumes == 1 ? int(i)
						                                    : volumes == 2   ? int(128 - i)
						                                                     : int(i % 7);
						native.voices[i] = { i + 1, 0, 0, int(i), 1, { i < matching ? 0 : 1, volume, persistence == 1 || (persistence == 2 && i % 2) } };
					}
					native.next_serial = 129;
					compare(native, 0);
					// A free slot takes precedence even when a class occupies half the pool
					native.voices[(cursor + 19) % audio_voice_limit].remaining = 0;
					compare(native, 0);
				}
	std::mt19937 random(0x584c4155);
	for (unsigned test = 0; test < 5000; ++test) {
		AudioVoices native;
		native.cursor = random() % audio_voice_limit;
		native.next_serial = 129;
		for (unsigned i = 0; i < audio_voice_limit; ++i)
			native.voices[i] = { i + 1, 0, 0, int(i), test % 2 || random() % 7 ? 1 : 0, { int(random() % 4) - 1, int(random() % 65537), random() % 2 != 0 } };
		compare(native, int(random() % 4) - 1);
	}
	for (unsigned cursor : { 0u, 1u, 127u }) {
		reset_reference();
		AudioVoices native;
		native.cursor = cursor;
		audio.m_info.nFreeChannel = int(cursor);
		require(native.start(0, -1, 4) == start_reference(0, 65536, -1, true), "Source initial persistent allocation differs");
		audio.StopAllChannels(false);
		native.clear();
		require(native.cursor == unsigned(audio.m_info.nFreeChannel) && !audio.m_channels[int(cursor)].playing, "Stop-all reset the source allocation cursor");
		require(native.start(0, 0, 5) == start_reference(0), "Post-stop allocation lost the retained cursor");
		++comparisons;
	}
	reset_reference();
	audio.m_info.nFreeChannel = 5;
	audio.fail_start = true;
	require(start_reference(0) == -1 && audio.m_info.nFreeChannel == 5, "Failed source start advanced its selected cursor");
	audio.fail_start = false;
	audio.m_info.bAvailable = false;
	require(start_reference(0) == -1 && audio.m_info.nFreeChannel == 5, "Unavailable source backend allocated a channel");
	std::cout << "PASS: " << comparisons << " source channel comparisons, class threshold, persistence, volume ties, cursor wrap and stop-all continuation; object cleanup and playback are adapters\n";
}
