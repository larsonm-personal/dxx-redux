#include "boss_profile.h"
#include "../time_equipment.h"
#ifndef DXX_XL_ASSET_DEPENDENCIES_H
#define DXX_XL_ASSET_DEPENDENCIES_H
#include <set>
#include <stdexcept>
#include <string>
#include "resource_banks.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "robot.h"
#include "polyobj.h"
#include "powerup.h"
#include "weapon.h"
#include "laser.h"
#include "vclip.h"
#include "effects.h"
#include "textures.h"
#include "piggy.h"
#include "wall.h"
#include "digi.h"
#include "args.h"
}
namespace xl_assets
{
inline void asset_require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
struct ObjectDependencies {
	const XlResourceBanks *resource_banks = nullptr;
	bool require_owned_banks = false;
	bool xl_boss_spawning = false;
	bool xl_time_equipment = false;
	int robot_count = N_robot_types, joint_count = N_robot_joints, model_count = N_polygon_models;
	// Read-only views permit validating staged HXM definitions before installation
	const robot_info *robot_data = Robot_info;
	const jointpos *joint_data = Robot_joints;
	const polymodel *model_data = Polygon_models;
	const bitmap_index *object_bitmaps = ObjBitmaps;
	const ushort *object_bitmap_ptrs = ObjBitmapPtrs;
	const int *dying_models = Dying_modelnums, *dead_models = Dead_modelnums;
	int weapon_count = N_weapon_types, powerup_count = N_powerup_types;
	int clip_count = Num_vclips, effect_count = Num_effects, wall_clip_count = Num_wall_anims;
	int texture_count = NumTextures, object_bitmap_count = N_ObjBitmaps;
	const weapon_info *weapon_data = Weapon_info;
	const powerup_type_info *powerup_data = Powerup_info;
	const vclip *clip_data = Vclip;
	const eclip *effect_data = Effects;
	const wclip *wall_clip_data = WallAnims;
	const bitmap_index *texture_data = Textures;
	const tmap_info *texture_info = TmapInfo;
	const ubyte *sound_mappings = Sounds, *alternate_sounds = AltSounds;
	int low_memory = GameArg.SysLowMem;
	std::set<std::pair<int, bool>> robot_profiles;
	std::set<int> robots, weapons, powerups, models, clips, bitmaps, sounds, textures, effects, wall_clips;
	void sound(int id)
	{
		if (id == -1 || id == 255) return;
		asset_require(id >= 0 && id < MAX_SOUNDS, "dependency references invalid sound ID");
		// Pinned XlatSound treats the original table's 255 entries as silence
		const int logical = low_memory ? alternate_sounds[id] : id;
		if (logical == 255) return;
		asset_require(logical >= 0 && logical < MAX_SOUNDS, "dependency references invalid alternate sound ID");
		if (sound_mappings[logical] == 255) return;
		const int resolved = digi_xlat_sound_from_tables(id, low_memory, sound_mappings, alternate_sounds);
		if (resolved < 0 || resolved >= MAX_SOUND_FILES)
			throw std::runtime_error("dependency references unmapped sound " + std::to_string(id));
		if (resource_banks) {
			resource_banks->sound(resolved);
			sounds.insert(resolved);
			return;
		}
		asset_require(!require_owned_banks, "Incoming sound dependencies require prepared target banks");
		const auto &s = GameSounds[resolved];
		if (!s.data || s.data == reinterpret_cast<ubyte *>(-1) || s.length <= 0)
			throw std::runtime_error("dependency sound has no loaded sample: logical=" + std::to_string(id) +
			                         " sample=" + std::to_string(resolved) + " live_sample=" + std::to_string(digi_xlat_sound(id)) +
			                         " low_memory=" + std::to_string(low_memory));
		sounds.insert(resolved);
	}
	void bitmap(int id)
	{
		if (resource_banks) {
			resource_banks->bitmap(id);
			bitmaps.insert(id);
			return;
		}
		asset_require(!require_owned_banks, "Incoming bitmap dependencies require prepared target banks");
		asset_require(id > 0 && id < Num_bitmap_files, "object dependency references missing bitmap");
		if (!bitmaps.insert(id).second) return;
		bitmap_index index{};
		index.index = id;
		PIGGY_PAGE_IN(index);
		const auto &b = GameBitmaps[id];
		asset_require(!(b.bm_flags & BM_FLAG_PAGED_OUT) && b.bm_data && b.bm_w > 0 && b.bm_h > 0, "object dependency bitmap could not be paged in");
	}
	void clip(int id)
	{
		if (id == -1) return;
		asset_require(id >= 0 && id < clip_count, "object dependency references missing vclip");
		if (!clips.insert(id).second) return;
		const auto &v = clip_data[id];
		sound(v.sound_num);
		asset_require(v.num_frames > 0 && v.num_frames <= VCLIP_MAX_FRAMES, "object dependency has empty/invalid vclip");
		for (int i = 0; i < v.num_frames; ++i) bitmap(v.frames[i].index);
	}
	void effect(int id)
	{
		if (id == -1) return;
		asset_require(id >= 0 && id < effect_count, "dependency references missing effect");
		if (!effects.insert(id).second) return;
		const auto &e = effect_data[id];
		asset_require(e.vc.num_frames > 0 && e.vc.num_frames <= VCLIP_MAX_FRAMES, "effect frame count invalid");
		for (int i = 0; i < e.vc.num_frames; ++i) bitmap(e.vc.frames[i].index);
		sound(e.sound_num);
		// Effect playback uses e.sound_num, not the embedded vclip's sound field
		if (e.dest_bm_num >= 0) texture(e.dest_bm_num);
		clip(e.dest_vclip);
		effect(e.dest_eclip);
		effect(e.crit_clip);
	}
	void texture(int id)
	{
		asset_require(id >= 0 && id < texture_count, "dependency references missing texture");
		if (!textures.insert(id).second) return;
		bitmap(texture_data[id].index);
		effect(texture_info[id].eclip_num);
		if (texture_info[id].destroyed >= 0) texture(texture_info[id].destroyed);
	}
	void wall_clip(int id)
	{
		if (id == -1) return;
		asset_require(id >= 0 && id < wall_clip_count, "dependency references missing wall animation");
		if (!wall_clips.insert(id).second) return;
		const auto &w = wall_clip_data[id];
		asset_require(w.num_frames > 0 && w.num_frames <= MAX_CLIP_FRAMES, "wall animation frame count invalid");
		asset_require(w.play_time >= w.num_frames, "wall animation duration cannot represent every frame");
		for (int i = 0; i < w.num_frames; ++i) texture(w.frames[i]);
		sound(w.open_sound);
		sound(w.close_sound);
	}
	void model(int id)
	{
		if (id == -1) return;
		if (id < 0 || id >= model_count)
			throw std::runtime_error("object dependency references missing model " + std::to_string(id) +
			                         " (loaded " + std::to_string(model_count) + ")");
		if (!models.insert(id).second) return;
		const auto &p = model_data[id];
		asset_require(p.model_data && p.model_data_size > 0 && p.n_models > 0 && p.n_models <= MAX_SUBMODELS, "object dependency model has no valid payload");
		for (int child = 1; child < p.n_models; ++child) {
			int parent = child, remaining = p.n_models;
			while (parent) {
				asset_require(parent >= 0 && parent < p.n_models && remaining-- > 0, "model submodel parent chain is invalid or cyclic");
				parent = p.submodel_parents[parent];
			}
		}
		asset_require(int(p.first_texture) + p.n_textures <= MAX_OBJ_BITMAPS, "model texture pointer range invalid");
		for (int i = 0; i < p.n_textures; ++i) {
			const int index = object_bitmap_ptrs[p.first_texture + i];
			asset_require(index >= 0 && index < object_bitmap_count, "model texture references missing object bitmap");
			bitmap(object_bitmaps[index].index);
			for (int e = 0; e < effect_count; ++e)
				if (effect_data[e].changing_object_texture == index || effect_data[e].changing_object_texture == p.first_texture + i) effect(e);
		}
		model(dying_models[id]);
		model(dead_models[id]);
		if (p.simpler_model) model(p.simpler_model - 1);
	}
	void weapon(int id)
	{
		if (id == -1) return;
		asset_require(id >= 0 && id < weapon_count, "object dependency references missing weapon");
		if (!weapons.insert(id).second) return;
		const auto &w = weapon_data[id];
		sound(w.flash_sound);
		sound(w.robot_hit_sound);
		sound(w.wall_hit_sound);
		clip(w.flash_vclip);
		clip(w.robot_hit_vclip);
		clip(w.wall_hit_vclip);
		if (w.render_type == WEAPON_RENDER_POLYMODEL) {
			model(w.model_num);
			model(w.model_num_inner);
		} else if (w.render_type == WEAPON_RENDER_VCLIP) clip(w.weapon_vclip);
		else if (w.render_type == WEAPON_RENDER_BLOB || w.render_type == WEAPON_RENDER_LASER) bitmap(w.bitmap.index);
		else asset_require(w.render_type == WEAPON_RENDER_NONE, "unknown weapon rendering dependency");
		weapon(w.children);
	}
	void powerup(int id)
	{
		if (xl_time_equipment && xl_compat::time_equipment_powerup(id)) {
			powerups.insert(id);
			// Addon items use their owned image strips and the afterburner pickup sound
			asset_require(POW_AFTERBURNER < powerup_count, "time equipment requires the afterburner sound definition");
			sound(powerup_data[POW_AFTERBURNER].hit_sound);
			return;
		}
		asset_require(id >= 0 && id < powerup_count, "object dependency references missing powerup");
		if (powerups.insert(id).second) {
			clip(powerup_data[id].vclip_num);
			sound(powerup_data[id].hit_sound);
		}
	}
	void contents(int type, int id)
	{
		if (type == OBJ_ROBOT) {
			robot(id);
			asset_require(!robot_data[id].boss_flag, "contained bosses require dynamic boss lifecycle support");
		} else if (type == OBJ_POWERUP) powerup(id);
		else asset_require(false, "unknown contained object dependency");
	}
	void robot_behavior(int id, int behavior, bool static_control = false)
	{
		asset_require(id >= 0 && id < robot_count, "object dependency references missing robot");
		const bool inert = static_control && behavior == 135 && !robot_data[id].attack_type &&
		                   !robot_data[id].companion && !robot_data[id].thief;
		robot(id, !inert);
		// Source melee initialization changes static behavior to NORMAL before contact
		asset_require(behavior != 135 || robot_data[id].attack_type || (!robot_data[id].companion && !robot_data[id].thief),
		              "static companion/thief contact response requires source qualification");
	}
	void robot(int id, bool animated = true)
	{
		if (id < 0 || id >= robot_count) {
			throw std::runtime_error("Missing robot definition " + std::to_string(id) + " (loaded " + std::to_string(robot_count) + ")");
		}
		if (robot_data[id].model_num < 0 || robot_data[id].model_num >= model_count)
			throw std::runtime_error("Robot " + std::to_string(id) + " references unusable model " +
			                         std::to_string(robot_data[id].model_num) + " (loaded " + std::to_string(model_count) + ")");
		if (!robot_profiles.emplace(id, animated).second) return;
		robots.insert(id);
		const auto &r = robot_data[id];
		for (const int id : { r.exp1_sound_num, r.exp2_sound_num, r.see_sound, r.attack_sound, r.claw_sound, r.taunt_sound }) sound(id);
		if (r.death_roll) sound(r.deathroll_sound);
		model(r.model_num);
		weapon(r.weapon_type);
		weapon(r.weapon_type2);
		clip(r.exp1_vclip_num);
		clip(r.exp2_vclip_num);
		if (r.contains_count > 0) contents(r.contains_type, r.contains_id);
		if (r.smart_blobs || r.energy_blobs) weapon(ROBOT_SMART_HOMING_ID);
		if (r.boss_flag && !(xl_boss_spawning && xl_compat::supported_boss_profile(r.boss_flag)))
			throw std::runtime_error("boss profile " + std::to_string(int(r.boss_flag)) + " requires an implemented XL boss runtime");
		// Pinned AIB_STATIC installs CT_NONE/MT_NONE, so no AI joint animation runs
		if (!animated) return;
		asset_require(r.n_guns >= 0 && r.n_guns <= MAX_GUNS, "robot gun count invalid");
		for (int gun = 0; gun < r.n_guns; ++gun)
			asset_require(r.gun_submodels[gun] < model_data[r.model_num].n_models, "robot gun references missing submodel");
		for (int gun = 0; gun <= r.n_guns; ++gun)
			for (int state = 0; state < N_ANIM_STATES; ++state) {
				const auto &j = r.anim_states[gun][state];
				asset_require(j.n_joints >= 0 && (!j.n_joints || (j.offset >= 0 && j.offset + j.n_joints <= joint_count)), "robot joint range invalid");
				for (int i = 0; i < j.n_joints; ++i)
					asset_require(joint_data[j.offset + i].jointnum >= 0 && joint_data[j.offset + i].jointnum < MAX_SUBMODELS &&
					                  (joint_data[j.offset + i].jointnum < model_data[r.model_num].n_models || r.n_guns > 0),
					              "robot joint outside qualified animation storage");
			}
	}
};
} // namespace xl_assets
#endif
