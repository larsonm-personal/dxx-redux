#ifndef DXX_XL_SCOPED_TEXTURE_PLAYBACK_H
#define DXX_XL_SCOPED_TEXTURE_PLAYBACK_H
#include "scoped_texture_resources.h"

namespace xl_compat
{
struct ScopedTextureEffect {
	int effect = -1, frame_time = 0;
	std::size_t image = 0;
	std::vector<int> bitmaps;
};
struct ScopedTextureClip {
	int clip = -1;
	std::size_t image = 0;
	std::vector<int> bitmaps;
};
struct ScopedTextureDoor {
	int clip = -1;
	int frames = 0;
	std::size_t image = 0;
	std::vector<int> bitmaps;
};

// Shared-strip source mode, resolved against retained definitions before install
struct ScopedTexturePlayback {
	std::vector<ScopedTextureBinding> bindings;
	std::vector<ScopedTextureEffect> effects;
	std::vector<ScopedTextureClip> clips;
	std::vector<ScopedTextureDoor> doors;
	static ScopedTexturePlayback prepare(const ScopedTextureResources &resources,
	                                     const xl_assets::ObjectDependencies &graph)
	{
		if (!graph.require_owned_banks || !graph.resource_banks || graph.effect_count < 0 || graph.effect_count > MAX_EFFECTS ||
		    graph.clip_count < 0 || graph.clip_count > VCLIP_MAXNUM || graph.wall_clip_count < 0 || graph.wall_clip_count > MAX_WALL_ANIMS ||
		    graph.texture_count < 0 || graph.texture_count > MAX_TEXTURES)
			throw std::runtime_error("Scoped playback requires bounded retained definitions");
		std::map<int, std::size_t> selected;
		for (const auto &binding : resources.bindings) {
			if (binding.bitmap <= 0 || binding.bitmap >= graph.resource_banks->bitmap_count() || binding.image >= resources.images.size() ||
			    !selected.emplace(binding.bitmap, binding.image).second)
				throw std::runtime_error("Scoped playback has an invalid or duplicate bitmap binding");
		}
		ScopedTexturePlayback result;
		std::map<int, int> clock_owner;
		// Source effects run globally; inspect every effect that references a loaded image
		for (int id = 0; id < graph.effect_count; ++id) {
			const auto &effect = graph.effect_data[id];
			if (effect.changing_wall_texture < 0 && effect.changing_object_texture < 0) continue;
			if (effect.vc.num_frames < 1 || effect.vc.num_frames > VCLIP_MAX_FRAMES)
				throw std::runtime_error("Scoped playback effect frame count exceeds native bounds");
			// FindAnimBaseTex selects the first override, even if it has only one frame
			auto first = selected.end();
			for (int frame = 0; frame < effect.vc.num_frames && first == selected.end(); ++frame)
				first = selected.find(effect.vc.frames[frame].index);
			if (first == selected.end() || resources.images[first->second].bitmap.frames < 2) continue;
			if (effect.changing_object_texture >= 0 || effect.changing_wall_texture >= graph.texture_count || effect.vc.frame_time <= 0)
				throw std::runtime_error("Scoped replacement requires a qualified wall effect clock");
			ScopedTextureEffect clock;
			clock.effect = id;
			clock.frame_time = effect.vc.frame_time;
			clock.image = first->second;
			for (int frame = 0; frame < effect.vc.num_frames; ++frame) {
				const int bitmap = effect.vc.frames[frame].index;
				if (resources.pog_indices.count(bitmap))
					throw std::runtime_error("Scoped effect strip would replace a POG-owned bitmap");
				if (bitmap <= 0 || bitmap >= graph.resource_banks->bitmap_count())
					throw std::runtime_error("Scoped effect references a missing retained bitmap");
				const auto owner = clock_owner.emplace(bitmap, id);
				if (!owner.second && owner.first->second != id)
					throw std::runtime_error("Scoped bitmap is driven by conflicting effect clocks");
				clock.bitmaps.push_back(bitmap);
			}
			result.effects.push_back(std::move(clock));
		}
		// SetupHiresAnim assigns the selected strip to every native frame alias
		for (int id = 0; id < graph.effect_count; ++id) {
			const auto &effect = graph.effect_data[id];
			if (effect.changing_wall_texture < 0 && effect.changing_object_texture < 0) continue;
			for (int frame = 0; frame < effect.vc.num_frames; ++frame) {
				const auto owner = clock_owner.find(effect.vc.frames[frame].index);
				if (owner != clock_owner.end() && owner->second != id)
					throw std::runtime_error("Scoped strip aliases an independently ticking effect");
			}
		}
		for (const auto &clock : result.effects)
			for (int bitmap : clock.bitmaps) selected[bitmap] = clock.image;
		const auto has_strip = [&](int bitmap) {
			const auto found = selected.find(bitmap);
			return found != selected.end() && resources.images[found->second].bitmap.frames > 1;
		};
		std::map<int, std::size_t> door_images;
		for (int id : graph.wall_clips) {
			if (id < 0 || id >= graph.wall_clip_count) throw std::runtime_error("Scoped wall clock is outside retained definitions");
			const auto &clip = graph.wall_clip_data[id];
			if (clip.num_frames < 1 || clip.num_frames > MAX_CLIP_FRAMES) throw std::runtime_error("Scoped wall clock has invalid frames");
			bool strip = false;
			for (int frame = 0; frame < clip.num_frames; ++frame) {
				const int texture = clip.frames[frame];
				if (texture < 0 || texture >= graph.texture_count) throw std::runtime_error("Scoped wall clock has a missing texture");
				strip = strip || has_strip(graph.texture_data[texture].index);
			}
			if (!strip) continue;
			const auto first = selected.find(graph.texture_data[clip.frames[0]].index);
			if (!graph.door_clips.count(id) || graph.other_wall_clips.count(id) || first == selected.end() ||
			    resources.images[first->second].bitmap.frames < 2)
				throw std::runtime_error("Scoped wall strip lacks a qualified first-frame door binding");
			const auto frames = resources.images[first->second].bitmap.frames;
			if (frames > 128 || clip.play_time < int(frames))
				throw std::runtime_error("Scoped door period cannot represent every replacement frame");
			ScopedTextureDoor door;
			door.clip = id;
			door.frames = int(frames);
			door.image = first->second;
			for (int frame = 0; frame < clip.num_frames; ++frame) {
				const int bitmap = graph.texture_data[clip.frames[frame]].index;
				const auto binding = selected.find(bitmap);
				if (bitmap <= 0 || bitmap >= graph.resource_banks->bitmap_count() || resources.pog_indices.count(bitmap) ||
				    clock_owner.count(bitmap) || (binding != selected.end() && binding->second != door.image))
					throw std::runtime_error("Scoped door alias conflicts with retained image ownership");
				const auto owner = door_images.emplace(bitmap, door.image);
				if (!owner.second && owner.first->second != door.image)
					throw std::runtime_error("Scoped doors select conflicting images");
				door.bitmaps.push_back(bitmap);
			}
			result.doors.push_back(std::move(door));
		}
		for (int id = 0; id < graph.effect_count; ++id) {
			const auto &effect = graph.effect_data[id];
			if (effect.changing_wall_texture < 0 && effect.changing_object_texture < 0) continue;
			for (int frame = 0; frame < effect.vc.num_frames; ++frame)
				if (door_images.count(effect.vc.frames[frame].index))
					throw std::runtime_error("Scoped door aliases an independently ticking effect");
		}
		std::map<int, std::size_t> clip_images;
		for (int id : graph.clips) {
			if (id < 0 || id >= graph.clip_count) throw std::runtime_error("Scoped vclip is outside retained definitions");
			const auto &clip = graph.clip_data[id];
			if (clip.num_frames < 1 || clip.num_frames > VCLIP_MAX_FRAMES) throw std::runtime_error("Scoped vclip has invalid frames");
			for (int frame = 0; frame < clip.num_frames; ++frame)
				if (door_images.count(clip.frames[frame].index))
					throw std::runtime_error("Scoped vclip references a door-owned strip");
			auto first = selected.end();
			for (int frame = 0; frame < clip.num_frames && first == selected.end(); ++frame)
				first = selected.find(clip.frames[frame].index);
			if (first == selected.end() || resources.images[first->second].bitmap.frames < 2) continue;
			if (clip.frame_time <= 0 || clip.play_time <= 0)
				throw std::runtime_error("Scoped vclip has an invalid authored period");
			ScopedTextureClip animation;
			animation.clip = id;
			animation.image = first->second;
			for (int frame = 0; frame < clip.num_frames; ++frame) {
				const int bitmap = clip.frames[frame].index;
				if (bitmap <= 0 || bitmap >= graph.resource_banks->bitmap_count() || resources.pog_indices.count(bitmap))
					throw std::runtime_error("Scoped vclip has a missing or POG-owned bitmap");
				if (clock_owner.count(bitmap) || door_images.count(bitmap)) throw std::runtime_error("Scoped vclip aliases a wall animation owner");
				const auto binding = clip_images.emplace(bitmap, animation.image);
				if (!binding.second && binding.first->second != animation.image)
					throw std::runtime_error("Scoped vclips select conflicting strip images");
				animation.bitmaps.push_back(bitmap);
			}
			result.clips.push_back(std::move(animation));
		}
		for (const auto &binding : clip_images) selected[binding.first] = binding.second;
		for (const auto &binding : door_images) selected[binding.first] = binding.second;
		for (const auto &binding : selected) result.bindings.push_back({ binding.first, binding.second });
		return result;
	}
};
} // namespace xl_compat
#endif
