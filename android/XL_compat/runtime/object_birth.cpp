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
};
std::array<Birth, MAX_OBJECTS> births{};
} // namespace
extern "C" void xl_object_birth_clear(void)
{
	births.fill({});
	xl_camera_hit_state_clear();
}
extern "C" void xl_object_birth_created(int owner)
{
	if (owner < 0 || owner >= object_capacity()) return;
	xl_camera_hit_state_created(owner);
	births[owner] = xl_level_extensions_active() ? Birth{ Objects[owner].signature, GameTime64 } : Birth{};
}
extern "C" int64_t xl_object_birth_time(int owner)
{
	if (!xl_level_extensions_active() || owner < 0 || owner > Highest_object_index || Objects[owner].type == OBJ_NONE) return 0;
	const auto &birth = births[owner];
	// Source-loaded objects have zero private creation time; save format omits it
	return birth.signature == Objects[owner].signature ? birth.time : 0;
}
