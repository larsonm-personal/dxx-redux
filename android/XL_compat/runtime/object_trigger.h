#ifndef XL_OBJECT_TRIGGER_H
#define XL_OBJECT_TRIGGER_H
#ifdef __cplusplus
#include <nlohmann/json_fwd.hpp>
#include "../inventory.h"
namespace xl_compat
{
// Pure raw ownership/action state, without retiring stale bindings
nlohmann::json object_trigger_snapshot();
} // namespace xl_compat
void xl_object_trigger_load(const xl_compat::Inventory &level);
struct trigger;
extern "C" int xl_object_teleport_action(const trigger *action, int object, int *destination);
extern "C" {
#endif
typedef struct xl_object_trigger_state {
	int owner, signature, type, flags, value, operated, destination;
} xl_object_trigger_state;
void xl_object_trigger_clear(void);
void xl_object_trigger_event(int object, int damage);
void xl_object_trigger_frame(void);
int xl_object_trigger_count(void);
int xl_object_trigger_has_spawn(int object);
int xl_object_trigger_has_attachment(int object);
int xl_object_trigger_state_get(int index, xl_object_trigger_state *state);
int xl_object_trigger_restore(const xl_object_trigger_state *states, int count, int apply);
#ifdef __cplusplus
}
#endif
#endif
