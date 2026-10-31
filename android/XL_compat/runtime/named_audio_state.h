#ifndef DXX_XL_NAMED_AUDIO_STATE_H
#define DXX_XL_NAMED_AUDIO_STATE_H
#include <nlohmann/json.hpp>
namespace xl_compat
{
nlohmann::json named_audio_snapshot();
bool restore_named_audio(const nlohmann::json &state, bool apply);
nlohmann::json named_audio_diagnostics();
// Keep consecutive synchronous captures at one audio epoch without changing
// the caller's pause/resume policy or holding SDL's lock across serialization
class AudioSnapshotPause
{
	bool active = false, paused = false, pending = false;

  public:
	AudioSnapshotPause();
	~AudioSnapshotPause();
	AudioSnapshotPause(const AudioSnapshotPause &) = delete;
	AudioSnapshotPause &operator=(const AudioSnapshotPause &) = delete;
};
} // namespace xl_compat
#endif
