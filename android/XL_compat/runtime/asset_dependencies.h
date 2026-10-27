#ifndef DXX_XL_ASSET_DEPENDENCIES_H
#define DXX_XL_ASSET_DEPENDENCIES_H
#include <set>
#include <stdexcept>
#include <string>
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
}
namespace xl_assets
{
inline void asset_require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
struct ObjectDependencies {
	bool xl_boss_spawning = false;
	// Read-only views permit validating staged HXM definitions before installation
	const robot_info *robot_data = Robot_info;
	const jointpos *joint_data = Robot_joints;
	const polymodel *model_data = Polygon_models;
	const bitmap_index *object_bitmaps = ObjBitmaps;
	const ushort *object_bitmap_ptrs = ObjBitmapPtrs;
	const int *dying_models = Dying_modelnums, *dead_models = Dead_modelnums;
	std::set<int> robots, weapons, powerups, models, clips, bitmaps, sounds, textures, effects, wall_clips;
	void sound(int id)
	{
		if (id == -1 || id == 255) return;
		asset_require(id >= 0 && id < MAX_SOUNDS, "dependency references invalid sound ID");
		const int resolved = digi_xlat_sound(id);
		if (resolved < 0 || resolved >= MAX_SOUND_FILES)
			throw std::runtime_error("dependency references unmapped sound " + std::to_string(id));
		const auto &s = GameSounds[resolved];
		asset_require(s.data && s.data != reinterpret_cast<ubyte *>(-1) && s.length > 0,
		              "dependency sound has no loaded sample");
		sounds.insert(resolved);
	}
	void bitmap(int id)
	{
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
		asset_require(id >= 0 && id < Num_vclips, "object dependency references missing vclip");
		if (!clips.insert(id).second) return;
		const auto &v = Vclip[id];
		sound(v.sound_num);
		asset_require(v.num_frames > 0 && v.num_frames <= VCLIP_MAX_FRAMES, "object dependency has empty/invalid vclip");
		for (int i = 0; i < v.num_frames; ++i) bitmap(v.frames[i].index);
	}
	void effect(int id)
	{
		if (id == -1) return;
		asset_require(id >= 0 && id < Num_effects, "dependency references missing effect");
		if (!effects.insert(id).second) return;
		const auto &e = Effects[id];
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
		asset_require(id >= 0 && id < NumTextures, "dependency references missing texture");
		if (!textures.insert(id).second) return;
		bitmap(Textures[id].index);
		effect(TmapInfo[id].eclip_num);
		if (TmapInfo[id].destroyed >= 0) texture(TmapInfo[id].destroyed);
	}
	void wall_clip(int id)
	{
		if (id == -1) return;
		asset_require(id >= 0 && id < Num_wall_anims, "dependency references missing wall animation");
		if (!wall_clips.insert(id).second) return;
		const auto &w = WallAnims[id];
		asset_require(w.num_frames > 0 && w.num_frames <= MAX_CLIP_FRAMES, "wall animation frame count invalid");
		for (int i = 0; i < w.num_frames; ++i) texture(w.frames[i]);
		sound(w.open_sound);
		sound(w.close_sound);
	}
	void model(int id)
	{
		if (id == -1) return;
		asset_require(id >= 0 && id < N_polygon_models, "object dependency references missing model");
		if (!models.insert(id).second) return;
		const auto &p = model_data[id];
		asset_require(p.model_data && p.model_data_size > 0 && p.n_models > 0 && p.n_models <= MAX_SUBMODELS, "object dependency model has no valid payload");
		asset_require(int(p.first_texture) + p.n_textures <= MAX_OBJ_BITMAPS, "model texture pointer range invalid");
		for (int i = 0; i < p.n_textures; ++i) {
			const int index = object_bitmap_ptrs[p.first_texture + i];
			asset_require(index >= 0 && index < N_ObjBitmaps, "model texture references missing object bitmap");
			bitmap(object_bitmaps[index].index);
			for (int e = 0; e < Num_effects; ++e)
				if (Effects[e].changing_object_texture == index || Effects[e].changing_object_texture == p.first_texture + i) effect(e);
		}
		model(dying_models[id]);
		model(dead_models[id]);
		if (p.simpler_model) model(p.simpler_model - 1);
	}
	void weapon(int id)
	{
		if (id == -1) return;
		asset_require(id >= 0 && id < N_weapon_types, "object dependency references missing weapon");
		if (!weapons.insert(id).second) return;
		const auto &w = Weapon_info[id];
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
		asset_require(id >= 0 && id < N_powerup_types, "object dependency references missing powerup");
		if (powerups.insert(id).second) {
			clip(Powerup_info[id].vclip_num);
			sound(Powerup_info[id].hit_sound);
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
	void robot(int id)
	{
		asset_require(id >= 0 && id < N_robot_types && robot_data[id].model_num >= 0 &&
		                  robot_data[id].model_num < N_polygon_models,
		              "object dependency references unusable robot");
		if (!robots.insert(id).second) return;
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
		asset_require(!r.boss_flag || (xl_boss_spawning && r.boss_flag >= 21 && r.boss_flag < 29), "boss profile requires an implemented XL boss runtime");
		asset_require(r.n_guns >= 0 && r.n_guns <= MAX_GUNS, "robot gun count invalid");
		for (int gun = 0; gun <= r.n_guns; ++gun)
			for (int state = 0; state < N_ANIM_STATES; ++state) {
				const auto &j = r.anim_states[gun][state];
				asset_require(j.n_joints >= 0 && j.offset >= 0 && j.offset + j.n_joints <= N_robot_joints, "robot joint range invalid");
				for (int i = 0; i < j.n_joints; ++i)
					asset_require(joint_data[j.offset + i].jointnum >= 0 && joint_data[j.offset + i].jointnum < model_data[r.model_num].n_models, "robot joint references missing submodel");
			}
	}
};
} // namespace xl_assets
#endif
