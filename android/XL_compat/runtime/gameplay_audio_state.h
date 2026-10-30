#ifndef DXX_XL_GAMEPLAY_AUDIO_STATE_H
#define DXX_XL_GAMEPLAY_AUDIO_STATE_H
#include "../audio_voice.h"
namespace xl_compat::audio_runtime
{
struct LoopingAudioState {
	int channel = -1;
	std::uint64_t serial = 0;
	bool matches(const AudioVoices &mixer) const
	{
		if (channel < 0 || unsigned(channel) >= mixer.voices.size() || !serial) return false;
		const auto &voice = mixer.voices[unsigned(channel)];
		return voice.remaining == -1 && voice.serial == serial && voice.kind == AudioOwnerKind::unlinked &&
		       voice.owner == -1 && voice.priority.sound_class == -1 && voice.priority.persistent;
	}
};
extern LoopingAudioState looping;
} // namespace xl_compat::audio_runtime
#endif
