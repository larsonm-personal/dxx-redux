// Execute actual source channel controls against an independent SDL-call adapter
#include "audio_voice.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
namespace output_reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETURN         return;
#define USE_SDL_MIXER  1
#define DBG            0
#define MIX_MAX_VOLUME 128
#include "xl_reference_audio_constants.h"
struct Output {
	int volume = 128;
	std::array<std::uint8_t, 2> pan{ 127, 127 };
	unsigned changes = 0;
};
std::array<Output, 2> outputs;
int Mix_Volume(int channel, int volume)
{
	auto &output = outputs.at(unsigned(channel));
	const int previous = output.volume;
	if (volume >= 0) output.volume = (std::min) (128, volume);
	++output.changes;
	return previous;
}
int Mix_SetPanning(int channel, std::uint8_t left, std::uint8_t right)
{
	auto &output = outputs.at(unsigned(channel));
	output.pan = { left, right };
	++output.changes;
	return 1;
}
struct CAudioChannel {
	struct {
		int bPlaying = 1, bAmbient = 0, nVolume = 32768, nPan = 32767, nChannel = 0;
	} m_info;
	void SetVolume(int32_t);
	void SetPan(int32_t);
};
struct CAudio {
	std::array<CAudioChannel, 2> channels;
	std::array<int, 2> volumes{ 32768, 16384 };
	bool available = true;
	bool Available()
	{
		return available;
	}
	CAudioChannel *Channel()
	{
		return channels.data();
	}
	int Volume(int ambient)
	{
		return volumes.at(unsigned(ambient));
	}
} audio;
struct Options {
	struct {
		int bUseSDLMixer = 1;
	} sound;
} options;
Options *gameOpts = &options;
#include "xl_reference_audio_controls.h"
} // namespace output_reference

void audio_output_source_controls()
{
	using namespace output_reference;
	const auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
	unsigned cases = 0;
	const auto start = [&](int volume, int pan) {
		outputs[0] = { 13, { 33, 221 }, 0 };
		Mix_VolPan(0, volume, pan);
		const auto actual = xl_compat::audio_start_output(volume, pan, { 13, { 33, 221 } });
		require(actual.volume == outputs[0].volume && actual.pan == outputs[0].pan, "Startup output differs from actual XL Mix_VolPan");
		++cases;
	};
	for (int pan = 0; pan <= 65536; ++pan) start(32768, pan);
	for (int volume = 0; volume <= 65536; ++volume) start(volume, 32767);
	for (int volume : { 0, 1, 127, 128, 129, 16384, 32768, 65536, 131072, INT32_MAX / 2 })
		for (int pan : { INT32_MIN, -65536, -1, 0, 1, 128, 129, 257, 258, 32638, 32767, 32768, 32900, 65535, 65536, 65537, INT32_MAX }) start(volume, pan);
	xl_compat::AudioVoices native;
	const int slot = native.start(0, 1, 7);
	require(slot == 0, "Output reference requires the first slot");
	auto &channel = audio.channels[0];
	channel.m_info.nChannel = 0;
	for (int pan = -1024; pan <= 66560; ++pan) {
		channel.SetPan(pan);
		require(native.set_pan(slot, pan) && native.voices[0].output.pan == outputs[0].pan, "Channel update differs from actual XL SetPan");
		++cases;
	}
	for (int pan : { INT32_MIN, INT32_MAX }) {
		channel.SetPan(pan);
		require(native.set_pan(slot, pan) && native.voices[0].output.pan == outputs[0].pan, "Endpoint pan byte conversion differs");
		++cases;
	}
	for (int playing : { 0, 1 })
		for (int ambient : { 0, 1 })
			for (int global : { 0, 1, 257, 16384, 32768, 65536 })
				for (int requested : { 0, 1, 129, 255, 512, 32768, 65536, 65537, 131072, INT32_MAX / 2 })
					for (bool equal : { false, true }) {
						const int effective = int(std::int64_t(global) * requested / 65536);
						const int previous = equal ? effective : effective == 1 ? 2
						                                                        : 1;
						outputs[0] = { 13, { 69, 185 }, 0 };
						channel.m_info = { playing, ambient, previous, 1234, 0 };
						audio.volumes[unsigned(ambient)] = global;
						native.voices[0] = { 17, 0, 23, 7, playing, { 4, previous, true }, { 13, { 69, 185 } } };
						channel.SetVolume(requested);
						require(native.set_volume(0, requested, global) == bool(playing), "Inactive channel update result differs");
						const auto &voice = native.voices[0];
						require(voice.priority.volume == channel.m_info.nVolume && voice.output.volume == outputs[0].volume && voice.output.pan == outputs[0].pan, "Volume update differs in priority, gain or retained panning");
						require(voice.serial == 17 && voice.position == 23 && voice.owner == 7 && voice.remaining == playing && voice.priority.sound_class == 4 && voice.priority.persistent, "Channel update disturbed playback identity or unrelated priority");
						++cases;
					}
	channel.m_info.bPlaying = 0;
	outputs[0] = { 17, { 25, 229 }, 0 };
	native.voices[0].remaining = 0;
	native.voices[0].output = { 17, { 25, 229 } };
	channel.SetPan(32767);
	require(!native.set_pan(0, 32767) && !outputs[0].changes && native.voices[0].output.pan == outputs[0].pan, "Inactive pan update changed channel state");
	std::cout << "PASS: " << cases << " pinned startup/update gain and pan comparisons, quantization, priority and stopped channels\n";
}
