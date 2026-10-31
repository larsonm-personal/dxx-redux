#include "skybox_collision.h"
#include "../skybox_collision.h"
#include "particle_world.h"
#include "preview.h"
#include <algorithm>
#include <memory>
extern "C" {
#include "object.h"
#include "fvi.h"
#include "segment.h"
#include "dxxerror.h"
int check_trans_wall(vms_vector *, segment *, int, int);
}

namespace
{
struct World {
	xl_compat::SkyboxCollisionWorld policy;
	xl_compat::ParticleWorld geometry;
	explicit World(const xl_compat::Inventory &level) : policy(level), geometry(xl_compat::compile_particle_world(level)) {}
};
std::unique_ptr<World> world;
} // namespace

void xl_compat::bind_skybox_collision(const Inventory &level)
{
	if (std::none_of(level.segments.begin(), level.segments.end(), [](const auto &segment) { return segment.function == 10; })) {
		world.reset();
		return;
	}
	world = std::make_unique<World>(level);
}

extern "C" void xl_skybox_collision_clear(void)
{
	world.reset();
}

extern "C" int xl_skybox_wall_collision(object *owner, fvi_info *hit, const vms_vector *destination)
{
	if (!world || !xl_level_extensions_active() || hit->hit_type != HIT_WALL || owner->type != OBJ_WEAPON) return 0;
	try {
		const auto point = [](const vms_vector &v) { return std::array<int, 3>{ v.x, v.y, v.z }; };
		const auto result = world->policy.collide(true, owner->lifeleft, { hit->hit_seg, hit->hit_side, hit->hit_side_seg, hit->hit_face, point(hit->hit_pnt), point(*destination) }, [&](int segment_index, const auto &p, int side, int face) {
			    if (segment_index < 0 || segment_index > Highest_segment_index || side < 0 || side >= 6 || face < 0 || face >= 2)
				    throw std::runtime_error("XL skybox collision has an invalid pixel-query face");
			    vms_vector position{ p[0], p[1], p[2] };
			    return check_trans_wall(&position, &Segments[segment_index], side, face) != 0; }, [&](const auto &p, int start, bool, bool skybox) { return world->geometry.locate(p, skybox, start); });
		if (result.retire) {
			owner->lifeleft = 0;
			owner->flags |= OF_SHOULD_BE_DEAD;
		}
		if (!result.pass) return 0;
		hit->hit_seg = result.segment;
		hit->hit_type = HIT_NONE;
		return 1;
	} catch (const std::exception &error) {
		Error("XL skybox collision: %s", error.what());
		return 0;
	}
}
