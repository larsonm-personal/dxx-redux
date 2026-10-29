#ifndef DXX_XL_NAMED_AUDIO_H
#define DXX_XL_NAMED_AUDIO_H
#ifdef __cplusplus
extern "C" {
#endif
struct trigger;
int xl_named_audio_backend_open(int rate, unsigned format, int channels);
void xl_named_audio_backend_close(void);
void xl_named_audio_set_volume(int volume);
void xl_named_audio_pause(int paused);
void xl_named_audio_resume(void);
void xl_named_audio_frame(void);
void xl_named_audio_stop(void);
void xl_named_audio_clear(void);
int xl_named_audio_action(struct trigger *action, int owner);
void xl_named_audio_wall_frame(int owner);
const char *xl_named_audio_error(void);
#ifdef __cplusplus
}
#include <memory>
#include <string>
namespace xl_compat
{
struct Inventory;
struct NamedAudioResources;
std::shared_ptr<const NamedAudioResources> prepare_named_audio(const char *level, const Inventory &inventory);
void install_named_audio(std::shared_ptr<const NamedAudioResources> resources);
} // namespace xl_compat
#endif
#endif
