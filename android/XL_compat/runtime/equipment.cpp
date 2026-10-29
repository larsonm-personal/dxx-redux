#include "equipment.h"
#include "../producer_layout.h"
#include "../inventory.h"
#include "preview.h"
#include <array>
#include <cstdint>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "gameseg.h"
#include "fuelcen.h"
#include "game.h"
#include "player.h"
#include "powerup.h"
#include "vclip.h"
#include "effects.h"
#include "fireball.h"
#include "digi.h"
#include "bm.h"
#include "rng_shared.h"
#include "save_epoch_format.h"
}

namespace
{
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
struct Maker {
	xl_equipment_state state;
	std::array<std::uint32_t, 2> flags;
	int owner;
};
#ifdef _MSC_VER
#pragma pack(pop)
#endif
std::vector<Maker> makers;
std::vector<std::array<int, 2>> station_links;

bool has_target(const xl_compat::Inventory &level, int segment)
{
	const auto targets = [segment](const xl_compat::Trigger &trigger) {
		for (const auto &target : trigger.targets)
			if (target[0] == segment) return true;
		return false;
	};
	for (std::size_t i = 0; i < level.triggers.size(); ++i) {
		for (const auto &wall : level.walls) {
			if (wall.trigger != static_cast<int>(i)) continue;
			const int overlay = level.segments[wall.segment].sides[wall.side].overlay & 0x3fff;
			if (overlay > 0) {
				const auto &texture = TmapInfo[overlay];
				const int effect = texture.eclip_num;
				const bool destructible = effect < 0 ? texture.destroyed != -1 : !(Effects[effect].flags & EF_ONE_SHOT) && Effects[effect].dest_bm_num >= 0;
				if (destructible && targets(level.triggers[i])) return true;
			}
			break; // FindTriggerWall uses the first wall only
		}
	}
	for (const auto &entry : level.object_trigger_records)
		if (entry.object >= 0 && entry.object <= Highest_object_index && Objects[entry.object].type != OBJ_NONE && targets(entry.action)) return true;
	return false;
}

int select_type(const Maker &maker)
{
	int types[50], count = 0;
	// XL ignores bits above MAX_POWERUP_TYPES, including the unused third word
	for (int id = 0; id < 50; ++id)
		if (maker.flags[id / 32] & (std::uint32_t(1) << (id % 32))) types[count++] = id;
	return !count ? -1 : types[count == 1 ? 0 : (d_rand() * count) / 32768];
}
} // namespace

void xl_equipment_load(const xl_compat::Inventory &level)
{
	makers.clear();
	station_links.clear();
	if (level.equipment.empty()) return;
	for (int i = 0; i < Num_fuelcenters; ++i) station_links.push_back({ Station[i].Type, Station[i].segnum });
	const auto producers = xl_compat::producer_layout(level);
	for (std::size_t i = 0; i < producers.equipment.size(); ++i) {
		const int segment_index = producers.equipment[i];
		const auto &segment = level.segments[segment_index];
		const auto &row = level.equipment.at(i);
		// SetEquipmentMakerStates indexes the combined list by equipment row
		const int enabled = !has_target(level, producers.stations.at(i));
		makers.push_back({ { -1, segment_index, enabled, 0 }, { std::uint32_t(row[0]), std::uint32_t(row[1]) }, segment.owner });
	}
}
extern "C" void xl_equipment_clear(void)
{
	makers.clear();
	station_links.clear();
}
extern "C" int xl_equipment_count(void)
{
	return xl_level_extensions_active() ? static_cast<int>(makers.size()) : 0;
}
extern "C" int xl_equipment_state_get(int index, xl_equipment_state *state)
{
	if (!state || index < 0 || index >= xl_equipment_count()) return 0;
	*state = makers[index].state;
	return 1;
}
extern "C" int xl_equipment_restore(const xl_equipment_state *states, int count, int apply)
{
	if (count != xl_equipment_count() || (!states && count)) return 0;
	if (count) {
		if (Num_fuelcenters != static_cast<int>(station_links.size())) return 0;
		for (int i = 0; i < Num_fuelcenters; ++i)
			if (Station[i].Type != station_links[i][0] || Station[i].segnum != station_links[i][1]) return 0;
	}
	for (int i = 0; i < count; ++i)
		if (states[i].segment != makers[i].state.segment || states[i].timer < -1 || states[i].timer > SAVE_EPOCH_MAX ||
		    states[i].enabled < 0 || states[i].enabled > 1 || states[i].pending < 0 || states[i].pending > 1 || (states[i].pending && states[i].timer < 0)) return 0;
	if (apply)
		for (int i = 0; i < count; ++i) makers[i].state = states[i];
	return 1;
}
extern "C" int xl_equipment_trigger(int segment)
{
	if (!xl_level_extensions_active()) return 0;
	for (auto &maker : makers)
		if (maker.state.segment == segment) {
			maker.state.enabled = !maker.state.enabled;
			return maker.state.enabled ? 1 : 2;
		}
	return 0;
}
extern "C" void xl_equipment_frame(int first_segment, int end_segment)
{
	// XL's SUSP_POWERUPS bit is separate from Redux's robot suspension
	if (!xl_level_extensions_active() || (Game_suspended & 4)) return;
	for (auto &maker : makers) {
		auto &state = maker.state;
		if (state.segment < first_segment || state.segment >= end_segment) continue;
		if (!state.enabled) continue;
		state.timer += FrameTime;
		if (!state.pending) {
			if (state.timer < i2f(3 * (Difficulty_level + 1))) continue;
			bool occupied = false;
			for (int index = Segments[state.segment].objects; index >= 0; index = Objects[index].next)
				// Retain the reference's ID comparison, not a player-type test
				if (Objects[index].type == OBJ_POWERUP || Objects[index].id == OBJ_PLAYER) {
					occupied = true;
					break;
				}
			if (occupied) {
				state.timer = 0;
				continue;
			}
			vms_vector center;
			compute_segment_center(&center, &Segments[state.segment]);
			auto *effect = object_create_explosion(state.segment, &center, i2f(10), VCLIP_MORPHING_ROBOT);
			if (effect) {
				extract_orient_from_segment(&effect->orient, &Segments[state.segment]);
				const int sound = Vclip[VCLIP_MORPHING_ROBOT].sound_num;
				if (sound >= 0) digi_link_sound_to_pos(sound, state.segment, 0, &center, 0, F1_0);
				state.pending = 1;
				state.timer = 0;
			}
		} else {
			if (state.timer < Vclip[VCLIP_MORPHING_ROBOT].play_time / 2) continue;
			state.pending = 0;
			state.timer = 0;
			const int type = select_type(maker);
			if (type < 0) continue;
			vms_vector center;
			compute_segment_center(&center, &Segments[state.segment]);
			const int index = obj_create(OBJ_POWERUP, type, state.segment, &center, &vmd_identity_matrix, Powerup_info[type].size, CT_POWERUP, MT_PHYSICS, RT_POWERUP);
			if (index < 0) continue;
			auto &object = Objects[index];
			if (type == POW_VULCAN_AMMO) object.ctype.powerup_info.count = VULCAN_AMMO_AMOUNT;
			object.rtype.vclip_info.vclip_num = Powerup_info[type].vclip_num;
			object.rtype.vclip_info.frametime = Vclip[object.rtype.vclip_info.vclip_num].frame_time;
			object.rtype.vclip_info.framenum = 0;
			object.matcen_creator = maker.owner;
			object.lifeleft = IMMORTAL_TIME;
		}
	}
}
