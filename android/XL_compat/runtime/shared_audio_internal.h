#ifndef DXX_XL_SHARED_AUDIO_INTERNAL_H
#define DXX_XL_SHARED_AUDIO_INTERNAL_H
#include "../audio_owners.h"
#include <string>
namespace xl_compat::audio_runtime
{
// Game-thread state shared by named, ordinary and linked consumers; the SDL
// callback only advances mixer voices while the game thread holds no audio lock
extern AudioVoices mixer;
extern AudioOwners owners;
extern bool backend_ready;
extern int priority_volume;
extern std::string failure;
int classic_sample(int physical_sample);
void clear_linked_scene();
struct AudioLock {
	bool locked;
	AudioLock();
	~AudioLock();
	AudioLock(const AudioLock &) = delete;
	AudioLock &operator=(const AudioLock &) = delete;
};
} // namespace xl_compat::audio_runtime
#endif
