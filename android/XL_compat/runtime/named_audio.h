#ifndef DXX_XL_NAMED_AUDIO_H
#define DXX_XL_NAMED_AUDIO_H
#ifdef __cplusplus
extern "C" {
#endif
struct trigger;
int xl_named_audio_backend_open(int rate, unsigned format, int channels);
int xl_named_audio_backend_ready(void);
void xl_named_audio_slow_motion_reset(void);
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
#include "../slow_motion.h"
class XlResourceBanks;
namespace xl_compat
{
bool slow_motion_audio_event(SlowMotionEvent event);
struct Inventory;
struct AudioResources;
class TargetSources;
class PreparedAudioInstall
{
  public:
	explicit PreparedAudioInstall(std::shared_ptr<const AudioResources> resources);
	~PreparedAudioInstall();
	void validate() const;
	void install();

  private:
	struct State;
	std::unique_ptr<State> state;
};
std::shared_ptr<const AudioResources> prepare_named_audio(const char *level, const Inventory &inventory, int ordinal = 0, const XlResourceBanks *banks = nullptr, TargetSources *target = nullptr);
void install_named_audio(std::shared_ptr<const AudioResources> resources);
} // namespace xl_compat
#endif
#endif
