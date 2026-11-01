#include "explosion_motion.h"
#include "../explosion_motion.h"
#include "preview.h"
#include <array>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "vclip.h"
#include "dxxerror.h"
}
namespace
{
struct Motion {
	int signature = -1, distance = 0, duration = 0;
	xl_compat::FixedLightPoint origin{};
};
std::array<Motion, MAX_OBJECTS> motions{};
} // namespace
extern "C" void xl_explosion_motion_clear(void)
{
	motions.fill({});
}
extern "C" void xl_explosion_motion_begin(object *effect)
{
	if (!effect || !xl_level_extensions_active()) return;
	const auto owner = effect - Objects;
	if (owner < 0 || owner > Highest_object_index || effect->type != OBJ_FIREBALL || effect->control_type != CT_EXPLOSION || effect->render_type != RT_FIREBALL)
		Error("Invalid XL explosion motion owner");
	auto &motion = motions[owner];
	motion = {};
	if (effect->id == VCLIP_MORPHING_ROBOT || effect->id == VCLIP_POWERUP_DISAPPEARANCE || effect->id == VCLIP_AFTERBURNER_BLOB) return;
	try {
		motion = { effect->signature, xl_compat::splash_fixed(2.0 * effect->size), effect->lifeleft, { effect->pos.x, effect->pos.y, effect->pos.z } };
	} catch (const std::exception &error) {
		Error("Cannot initialize XL explosion motion: %s", error.what());
	}
}
extern "C" void xl_explosion_motion_frame(object *effect)
{
	if (!effect || !xl_level_extensions_active()) return;
	const auto owner = effect - Objects;
	if (owner < 0 || owner > Highest_object_index || effect->type != OBJ_FIREBALL || effect->control_type != CT_EXPLOSION || effect->render_type != RT_FIREBALL) return;
	const auto &motion = motions[owner];
	if (motion.signature != effect->signature || !motion.duration) return;
	if (!Viewer) Error("XL explosion motion requires the current viewer");
	try {
		const auto position = xl_compat::explosion_motion(motion.origin, { Viewer->pos.x, Viewer->pos.y, Viewer->pos.z }, motion.distance, motion.duration, effect->lifeleft);
		effect->pos = { position[0], position[1], position[2] };
	} catch (const std::exception &error) {
		Error("Cannot advance XL explosion motion: %s", error.what());
	}
}
