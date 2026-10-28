#include "fluids.h"
#include "preview.h"
#include "speed_boost.h"
#include "geometry.h"
#include <cstdint>
#include <limits>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "segment.h"
#include "wall.h"
#include "textures.h"
#include "gameseg.h"
#include "collide.h"
#include "palette.h"
#include "sounds.h"
#include "digi.h"
#include "automap.h"
}

namespace
{
fix64 last_scrape = 0;
fix multiply(fix value, fix scale)
{
	return static_cast<fix>(double(value) * scale / 65536.0);
}
bool thrusting(const object &actor)
{
	const auto &thrust = actor.mtype.phys_info.thrust;
	return thrust.x || thrust.y || thrust.z;
}
int volume(int segment, fix &damage)
{
	if (xl_lava_segment(segment)) {
		damage = TmapInfo[404].damage / 2;
		return 1;
	}
	if (xl_preview_water_segment(segment)) {
		damage = 0;
		return 2;
	}
	return 0;
}
int surface(int segment, int side, fix &damage)
{
	const auto &face = Segments[segment].sides[side];
	const bool solid = Segments[segment].children[side] == -1;
	if (!solid && (face.wall_num < 0 || Walls[face.wall_num].type != WALL_ILLUSION || (Walls[face.wall_num].flags & WALL_ILLUSION_OFF))) return 0;
	const auto &texture = TmapInfo[face.tmap_num];
	if (texture.damage) {
		damage = texture.damage;
		return 3;
	}
	return (texture.flags & TMI_WATER) ? (solid ? 2 : 4) : 0;
}
void turbulence(object &actor, int divisor)
{
	actor.mtype.phys_info.rotvel.x = (16383 - d_rand()) / divisor;
	actor.mtype.phys_info.rotvel.z = (16383 - d_rand()) / divisor;
}
fix hurt(object &actor, fix damage)
{
	if (Difficulty_level == 0) damage /= 2;
	if (!(Players[Player_num].flags & PLAYER_FLAGS_INVULNERABLE)) apply_damage_to_player(&actor, &actor, damage, 0);
	PALETTE_FLASH_ADD(f2i(damage * 4), 0, 0);
	return damage;
}
vms_vector normalize(const vms_vector &value)
{
	const auto result = xl_compat::geometry_fixed_direction({ value.x, value.y, value.z });
	return { result[0], result[1], result[2] };
}
vms_vector random_direction()
{
	vms_vector result;
	const int first = d_rand() % 3;
	fix *parts[] = { &result.x, &result.y, &result.z };
	const int axis = first == 2 ? 0 : first == 1 ? 1
	                                             : 2;
	for (int i = 0; i < 3; ++i) *parts[(axis + i) % 3] = (16383 - d_rand()) | (i == 0 ? 1 : 0);
	return normalize(result);
}
} // namespace

extern "C" void xl_fluid_clear(void)
{
	last_scrape = 0;
}
extern "C" int64_t xl_fluid_scrape_elapsed(void)
{
	if (last_scrape < 0 && GameTime64 > (std::numeric_limits<fix64>::max)() + last_scrape) return (std::numeric_limits<fix64>::max)();
	if (last_scrape > 0 && GameTime64 < (std::numeric_limits<fix64>::min)() + last_scrape) return (std::numeric_limits<fix64>::min)();
	return GameTime64 - last_scrape;
}
extern "C" int xl_fluid_restore(int64_t elapsed, int apply)
{
	if ((elapsed > 0 && GameTime64 < (std::numeric_limits<fix64>::min)() + elapsed) ||
	    (elapsed < 0 && GameTime64 > (std::numeric_limits<fix64>::max)() + elapsed)) return 0;
	if (apply) last_scrape = GameTime64 - elapsed;
	return 1;
}

// -1 selects Redux; nonnegative results are XL's 0/1/2/3/4 contact types
extern "C" int xl_fluid_wall(int index, int segment, int side)
{
	if (!xl_level_extensions_active()) return -1;
	if (index < 0 || index > Highest_object_index || segment < 0 || segment >= Num_segments || side < 0 || side >= 6) return 0;
	auto &actor = Objects[index];
	fix damage = 0;
	int type = surface(segment, side, damage);
	if (!type) type = volume(segment, damage);
	if (!type) return 0;
	if (xl_preview_no_damage_segment(segment)) damage = 0;
	if (actor.type == OBJ_PLAYER && actor.id == Player_num) {
		if (damage > 0) damage = hurt(actor, multiply(damage, FrameTime));
		if (damage || thrusting(actor)) turbulence(actor, 2);
	}
	return type;
}

extern "C" int xl_fluid_frame(int index, int *playing)
{
	if (!xl_level_extensions_active()) return 0;
	if (index < 0 || index > Highest_object_index || !playing) return 1;
	auto &actor = Objects[index];
	if (actor.type != OBJ_PLAYER || actor.id != Player_num) return 1;
	int type = 0;
	const auto mask = get_seg_masks(&actor.pos, actor.segnum, actor.size, const_cast<char *>(__FILE__), __LINE__).sidemask;
	for (int side = 0; side < 6 && !type; ++side)
		if (mask & (1 << side)) type = xl_fluid_wall(index, actor.segnum, side);
	if (!type) {
		fix damage = 0;
		type = volume(actor.segnum, damage);
		if (damage > 0) {
			hurt(actor, multiply(damage, FrameTime) / 2);
			turbulence(actor, 4);
		} else if (type && thrusting(actor)) turbulence(actor, 8);
	}
	const int sound = !type || (type == 2 && !thrusting(actor)) ? -1 : type == 2 ? SOUND_SHIP_IN_WATER
	                                                               : (type & 1)  ? SOUND_LAVAFALL_HISS
	                                                                             : SOUND_SHIP_IN_WATERFALL;
	*playing = xl_preview_fluid_loop(index, sound, *playing);
	return 1;
}

extern "C" int xl_fluid_scrape(int index, int segment, int side, const int32_t point[3])
{
	if (!xl_level_extensions_active()) return 0;
	if (index < 0 || index > Highest_object_index || Objects[index].type != OBJ_PLAYER || Objects[index].id != Player_num) return 1;
	auto &actor = Objects[index];
	const int type = xl_fluid_wall(index, segment, side);
	const fix delay = type ? F1_0 / (d_rand() % 4 + 1) : F1_0 + i2f(2) / (d_rand() % 8 + 1);
	if (xl_fluid_scrape_elapsed() > delay || GameTime64 < last_scrape) {
		last_scrape = GameTime64;
		// Optional addon dry-scrape sounds are disabled in this runtime profile
		if (type) {
			vms_vector hit{ point[0], point[1], point[2] };
			digi_link_sound_to_pos((type & 1) ? SOUND_VOLATILE_WALL_HISS : SOUND_SHIP_IN_WATER, segment, 0, &hit, 0, F1_0);
			auto direction = Segments[segment].sides[side].normals[0];
			const auto random = random_direction();
			direction.x += multiply(random.x, F1_0 / 8);
			direction.y += multiply(random.y, F1_0 / 8);
			direction.z += multiply(random.z, F1_0 / 8);
			direction = normalize(direction);
			if (actor.mtype.phys_info.mass && actor.movement_type == MT_PHYSICS && !Automap_active && !xl_speed_boost_active(index)) {
				const float scale = 1.0f / (float(actor.mtype.phys_info.mass) / F1_0);
				actor.mtype.phys_info.velocity.x += static_cast<fix>(float(multiply(direction.x, i2f(8))) * scale);
				actor.mtype.phys_info.velocity.y += static_cast<fix>(float(multiply(direction.y, i2f(8))) * scale);
				actor.mtype.phys_info.velocity.z += static_cast<fix>(float(multiply(direction.z, i2f(8))) * scale);
			}
		}
	}
	return 1;
}
