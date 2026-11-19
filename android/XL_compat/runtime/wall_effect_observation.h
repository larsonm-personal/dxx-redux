// Desktop diagnostic: read the texture selected by the ordinary effect clock
#include "pog_resources.h"
extern "C" {
#include "bm.h"
#include "effects.h"
#include "ogl_init.h"
#include "piggy.h"
#include "textures.h"
void ogl_bindbmtex(grs_bitmap *);
}

static unsigned wall_pixel_hash(const unsigned char *pixels, std::size_t count)
{
	unsigned value = 2166136261u;
	for (std::size_t i = 0; i < count; ++i) value = (value ^ pixels[i]) * 16777619u;
	return value;
}

static bool observe_texture_effect(int id, bool object, nlohmann::ordered_json &rows)
{
	if (id < 0 || id >= Num_effects) return false;
	const auto &effect = Effects[id];
	const int count = xl_pog_effect_frames(&effect);
	const int slot = object ? effect.changing_object_texture : effect.changing_wall_texture;
	if (slot < 0 || slot >= (object ? N_ObjBitmaps : NumTextures) || effect.vc.num_frames < 1 ||
	    effect.vc.num_frames > VCLIP_MAX_FRAMES || effect.frame_count < 0 || effect.frame_count >= count) return false;
	const auto key = std::to_string(effect.frame_count);
	if (rows.contains(key)) return true;
	const int bitmap = (object ? ObjBitmaps[slot] : Textures[slot]).index;
	if (bitmap < 0 || bitmap >= Num_bitmap_files) return false;
	auto &view = GameBitmaps[bitmap];
	if (view.bm_type != BM_RGBA || !view.bm_data || view.bm_w <= 0 || view.bm_h <= 0 || view.bm_w > 4096 || view.bm_h > 4096 ||
	    view.bm_rowsize != view.bm_w * 4) return false;
	const auto size = std::size_t(view.bm_rowsize) * view.bm_h;
	xl_pog_pixel_view physics{};
	if (!xl_pog_physics_view(bitmap, &physics)) return false;
	unsigned aliases = 0;
	for (int i = 0; i < effect.vc.num_frames; ++i) {
		const int alias = effect.vc.frames[i].index;
		xl_pog_pixel_view other{};
		if (alias >= Num_bitmap_files || GameBitmaps[alias].bm_data != view.bm_data || !xl_pog_physics_view(alias, &other) ||
		    other.pixels != physics.pixels || other.height != physics.height || other.width != physics.width) return false;
		++aliases;
	}
	const bool resident = view.gltexture && view.gltexture->handle;
	GLint previous = 0, width = 0, height = 0;
	glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous);
	ogl_bindbmtex(&view);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
	const bool full_texture = width == view.bm_w && height == view.bm_h && view.gltexture && view.gltexture->handle;
	std::vector<unsigned char> gpu;
	if (full_texture) {
		gpu.resize(size);
		glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, gpu.data());
	}
	glBindTexture(GL_TEXTURE_2D, previous);
	if (!full_texture || glGetError() != GL_NO_ERROR) return false;
	rows[key] = { { object ? "object_texture" : "wall_texture", slot }, { "bitmap", bitmap }, { "aliases", aliases }, { "frames", count }, { "frame_time", effect.vc.frame_time }, { "width", width }, { "height", height }, { "resident_before_probe", resident }, { "cpu_rgba_fnv1a", wall_pixel_hash(view.bm_data, size) }, { "gpu_rgba_fnv1a", wall_pixel_hash(gpu.data(), size) }, { "physics_height", physics.height }, { "physics_width", physics.width }, { "physics_rgba_fnv1a", wall_pixel_hash(physics.pixels, std::size_t(physics.row_size) * physics.height) } };
	return true;
}
