#include "object_birth.h"
#include "preview.h"
#include "camera_collision.h"
#include <array>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
}
namespace
{
struct Birth {
	int signature = -1;
	int64_t time = 0;
	uint64_t order = 0;
};
std::array<Birth, MAX_OBJECTS> births{};
uint64_t next_order = 0;
} // namespace
extern "C" void xl_object_birth_clear(void)
{
	births.fill({});
	next_order = 0;
	xl_camera_hit_state_clear();
}
extern "C" void xl_object_birth_created(int owner)
{
	if (owner < 0 || owner >= object_capacity()) return;
	xl_camera_hit_state_created(owner);
	births[owner] = xl_level_extensions_active() ? Birth{ Objects[owner].signature, GameTime64, ++next_order } : Birth{};
}
extern "C" int64_t xl_object_birth_time(int owner)
{
	if (!xl_level_extensions_active() || owner < 0 || owner > Highest_object_index || Objects[owner].type == OBJ_NONE) return 0;
	const auto &birth = births[owner];
	// Source-loaded objects have zero private creation time; save format omits it
	return birth.signature == Objects[owner].signature ? birth.time : 0;
}

extern "C" uint64_t xl_object_birth_order(int owner)
{
	if (!xl_level_extensions_active() || owner < 0 || owner > Highest_object_index || Objects[owner].type == OBJ_NONE) return 0;
	const auto &birth = births[owner];
	// Loaded and restored objects precede newly appended objects in source list order
	return birth.signature == Objects[owner].signature ? birth.order : 0;
}
