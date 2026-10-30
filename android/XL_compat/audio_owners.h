#ifndef DXX_XL_AUDIO_OWNERS_H
#define DXX_XL_AUDIO_OWNERS_H
#include "audio_voice.h"
#include <functional>

namespace xl_compat
{
constexpr unsigned audio_owner_limit = 500;
constexpr unsigned sound_used = 1, sound_link_object = 4, sound_link_position = 8,
                   sound_forever = 16, sound_permanent = 32;
struct AudioOwner {
	std::uint64_t serial = 0, channel_serial = 0;
	unsigned flags = sound_used, sample = 0;
	int sound = -1, sound_class = 0, channel = -1;
	int object = -1, signature = 0, segment = -1, side = -1;
	std::array<std::int32_t, 3> position{};
	int volume = 0, pan = 0, max_volume = 65536, max_distance = 0;
	int loop_start = -1, loop_end = -1, decay = 0, audio_volume = -1;
	bool ambient = false, custom = false, external = false;
};

struct AudioOwnerUpdate {
	bool exists = true, enabled = true;
	int volume = 0, pan = 0;
};

// The game thread owns this registry; callers hold the mixer lock while changing
// bindings. The audio callback only changes voice playheads and completion state
class AudioOwners
{
  public:
	std::array<AudioOwner, audio_owner_limit> entries{};
	unsigned count = 0;
	std::uint64_t next_serial = 1;
	int add(AudioOwner entry);
	unsigned active() const;
	void stop(unsigned index, AudioVoices &mixer);
	void erase(unsigned index, AudioVoices &mixer);
	void end(unsigned index, AudioVoices &mixer);
	bool suspend(int threshold, AudioVoices &mixer);
	bool start(unsigned index, int effects_volume, bool deferred, AudioVoices &mixer, bool available = true);
	int start_voice(unsigned sample, int repetitions, int owner, AudioPriority priority, AudioOutput output, AudioOwnerKind kind, AudioVoices &mixer);
	bool destroy_object(int object, int sound, AudioVoices &mixer);
	bool destroy_segment(int segment, int side, int sound, AudioVoices &mixer);
	bool destroy_missing(const std::function<bool(int, int)> &matches, AudioVoices &mixer);
	void reconcile(AudioVoices &mixer);
	void sync(const std::function<AudioOwnerUpdate(const AudioOwner &)> &update, AudioVoices &mixer,
	          std::array<int, 2> effects_volume, bool deferred = false, bool available = true);
	void clear(AudioVoices &mixer);
	bool valid(const AudioVoices &mixer) const;

  private:
	bool bound(unsigned index, const AudioVoices &mixer) const;
};
} // namespace xl_compat
#endif
