#ifndef DXX_XL_LINKED_AUDIO_STATE_H
#define DXX_XL_LINKED_AUDIO_STATE_H
#include "../audio_owners.h"
#include <nlohmann/json.hpp>
namespace xl_compat
{
struct LinkedAudioState {
	AudioOwners owners;
	bool deferred = false;
};
nlohmann::json encode_linked_audio(const AudioOwners &owners, bool deferred);
LinkedAudioState decode_linked_audio(const nlohmann::json &state, const AudioVoices &voices);
} // namespace xl_compat
#endif
