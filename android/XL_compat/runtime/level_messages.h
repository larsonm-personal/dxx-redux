#ifndef DXX_XL_RUNTIME_LEVEL_MESSAGES_H
#define DXX_XL_RUNTIME_LEVEL_MESSAGES_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
typedef struct xl_message_state {
	int64_t elapsed_ms;
	int32_t id, duration;
} xl_message_state;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
struct trigger;
int xl_message_action(const struct trigger *action);
void xl_message_clear(void);
void xl_message_draw(void);
void xl_message_frame(void);
void xl_message_state_get(xl_message_state *state);
int xl_message_restore(const xl_message_state *state, int apply);
const char *xl_message_identity(void);
const char *xl_message_text(void);
float xl_message_alpha(void);
uint64_t xl_message_draws(void);
uint64_t xl_message_modal_shows(void);
int xl_message_drawn_id(void);
float xl_message_drawn_alpha(void);
#ifdef __cplusplus
}
#include <memory>
namespace xl_compat
{
struct Inventory;
struct MessageResources;
std::shared_ptr<const MessageResources> prepare_messages(const char *level, const Inventory &inventory);
void install_messages(std::shared_ptr<const MessageResources> resources);
} // namespace xl_compat
#endif
#endif
