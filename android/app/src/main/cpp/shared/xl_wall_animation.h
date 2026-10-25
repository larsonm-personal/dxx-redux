#pragma once
/* Included by D2 OpenGL; reuse its texture pool and lifetime hooks. */
#include "android_mission_assets.h"
#include "xl_animation_phase.h"
#include "xl_static_texture_pixels.h"
#include "cntrlcen.h"

struct xl_wall_strip {
	char path[256];
	grs_bitmap *frames;
	unsigned count;
	size_t bytes;
};
struct xl_wall_path {
	char name[128], path[256];
};
static struct xl_wall_strip xl_wall_strips[32];
static struct xl_wall_path xl_wall_paths[512];
static unsigned xl_wall_generation, xl_wall_strip_count, xl_wall_path_count, xl_wall_frame_count;
static size_t xl_wall_bytes;
static char xl_wall_hog[4096];

static void xl_wall_release_texture(ogl_texture *texture)
{
	if (!texture) return;
	ogl_freetexture(texture);
	ogl_reset_texture(texture);
}

static void xl_wall_animation_clear(void)
{
	for (unsigned i = 0; i < xl_wall_strip_count; ++i) {
		struct xl_wall_strip *strip = &xl_wall_strips[i];
		for (unsigned j = 0; j < strip->count; ++j)
			xl_wall_release_texture(strip->frames[j].gltexture);
		free(strip->frames);
	}
	memset(xl_wall_strips, 0, sizeof(xl_wall_strips));
	xl_wall_strip_count = xl_wall_path_count = xl_wall_frame_count = 0;
	xl_wall_bytes = 0;
	xl_wall_hog[0] = 0;
	android_ogl_reset_texture_bindings(&ogl_bind_texture_state);
	xl_wall_generation = android_mission_assets_generation();
}

static struct xl_wall_strip *xl_wall_animation_source(const char *hog, const char *name, grs_bitmap *original)
{
	if (!name || strlen(name) >= sizeof(xl_wall_paths[0].name)) return NULL;
	unsigned path_index;
	for (path_index = 0; path_index < xl_wall_path_count; ++path_index)
		if (!strcmp(xl_wall_paths[path_index].name, name)) break;
	if (path_index == xl_wall_path_count) {
		if (xl_wall_path_count == 512) return NULL;
		struct xl_wall_path *path = &xl_wall_paths[xl_wall_path_count++];
		strcpy(path->name, name);
		path->path[0] = 0;
		if (!android_mission_assets_find_animation_texture(hog, name, path->path, sizeof(path->path))) {
			const char *marker = strchr(name, '#');
			if (marker && marker - name < 125) {
				char first[128];
				memcpy(first, name, marker - name);
				strcpy(first + (marker - name), "#0");
				android_mission_assets_find_animation_texture(hog, first, path->path, sizeof(path->path));
			}
		}
	}
	const char *path = xl_wall_paths[path_index].path;
	if (!*path) return NULL;
	for (unsigned i = 0; i < xl_wall_strip_count; ++i)
		if (!strcmp(xl_wall_strips[i].path, path)) return xl_wall_strips[i].count ? &xl_wall_strips[i] : NULL;
	if (xl_wall_strip_count == 32) return NULL;
	/* Retain negative results too: unsupported artwork cannot decode every draw. */
	struct xl_wall_strip *strip = &xl_wall_strips[xl_wall_strip_count++];
	strcpy(strip->path, path);
	png_data pixels = { 0 };
	if (!read_png_scoped_animation(path, &pixels)) return NULL;
	const unsigned frames = pixels.height / pixels.width;
	if (frames > 128 - xl_wall_frame_count) {
		free(pixels.data);
		return NULL;
	}
	unsigned free_slots = 0;
	for (int i = 0; i < OGL_TEXTURE_LIST_SIZE; ++i)
		free_slots += ogl_texture_list[i].handle <= 0 && !ogl_texture_list[i].w;
	if (free_slots < frames) {
		free(pixels.data);
		return NULL;
	}
	strip->frames = (grs_bitmap *) calloc(frames, sizeof(grs_bitmap));
	if (!strip->frames) {
		free(pixels.data);
		return NULL;
	}
	int complete = 1;
	for (unsigned i = 0; i < frames; ++i) {
		dxx_xl_static_pixels fitted = { 0 };
		const unsigned char *source = pixels.data + (size_t) i * pixels.width * pixels.width * pixels.channels;
		int limit = 2;
		const int maximum = ogl_max_texture_size > 0 ? ogl_max_texture_size : 2048;
		while (limit <= maximum / 2 && limit < 16384) limit *= 2;
		if (maximum < 2 || !dxx_xl_fit_static_pixels(source, pixels.width, pixels.width, pixels.channels, limit, &fitted)) {
			complete = 0;
			break;
		}
		/* Allow full padded mip chains; retain at most32 MiB across all sources. */
		size_t width = 2;
		while (width < (size_t) fitted.width) width *= 2;
		const size_t bytes = width * width * 4 * 2;
		if (bytes > 32u * 1024u * 1024u - xl_wall_bytes - strip->bytes) {
			free(fitted.data);
			complete = 0;
			break;
		}
		grs_bitmap *frame = &strip->frames[i];
		*frame = *original;
		frame->bm_parent = NULL;
		frame->bm_data = NULL;
		frame->bm_flags = piggy_bitmap_get_flags(original) & BM_FLAG_NO_LIGHTING;
		frame->gltexture_mask = NULL;
		frame->gltexture = ogl_get_free_texture();
		ogl_init_texture(frame->gltexture, fitted.width, fitted.height, OGL_FLAG_ALPHA);
		const int failed = ogl_loadtexture(fitted.data, 0, 0, frame->gltexture, frame->bm_flags, 4,
		                                   android_ogl_effective_texfilt(GameCfg.TexFilt, ogl_aniso_level), name);
		free(fitted.data);
		if (failed) {
			complete = 0;
			break;
		}
		frame->gltexture->is_png = 1;
		/* Opaque source also bypasses the static merged-result cache. */
		frame->gltexture->scoped_static = 1;
		strip->bytes += bytes;
	}
	free(pixels.data);
	if (!complete) {
		for (unsigned i = 0; i < frames; ++i)
			xl_wall_release_texture(strip->frames[i].gltexture);
		free(strip->frames);
		strip->frames = NULL;
		strip->bytes = 0;
		android_ogl_reset_texture_bindings(&ogl_bind_texture_state);
		return NULL;
	}
	strip->count = frames;
	xl_wall_bytes += strip->bytes;
	xl_wall_frame_count += frames;
	return strip;
}

static grs_bitmap *xl_wall_animation_bitmap(grs_bitmap *original, int overlay)
{
	if (Num_effects < 0 || Num_effects > MAX_EFFECTS || NumTextures < 0 || NumTextures > MAX_TEXTURES ||
	    !g_android_draw_face_ctx.valid || !xl_resource_texture_scope_enabled() || d1_in_d2_use_d1_gameplay() ||
	    (Game_mode & GM_MULTI) || !ogl_allow_png() || piggy_bitmap_is_custom(original)) return original;
	const char *hog = mission_resource_archive();
	if (!hog || strlen(hog) >= sizeof(xl_wall_hog)) return original;
	if (xl_wall_generation != android_mission_assets_generation() || strcmp(hog, xl_wall_hog)) {
		xl_wall_animation_clear();
		strcpy(xl_wall_hog, hog);
	}
	const int texture = overlay ? (g_android_draw_face_ctx.tmap2 & 0x3fff) : g_android_draw_face_ctx.tmap1;
	if (texture < 0 || texture >= NumTextures || (overlay && !g_android_draw_face_ctx.tmap2) ||
	    Textures[texture].index >= MAX_BITMAP_FILES || original != &GameBitmaps[Textures[texture].index]) return original;
	for (int i = 0; i < Num_effects; ++i) {
		const eclip *effect = &Effects[i];
		if (effect->changing_wall_texture != texture || (effect->flags & EF_CRITICAL)) continue;
		if (effect->crit_clip != -1 && Control_center_destroyed) {
			if (effect->crit_clip < 0 || effect->crit_clip >= Num_effects) return original;
			effect = &Effects[effect->crit_clip];
		}
		if (dxx_xl_animation_frame(effect->vc.num_frames, effect->vc.frame_time, effect->frame_count,
		                           effect->time_left, effect->flags & EF_STOPPED, 1) < 0 ||
		    effect->vc.frames[effect->frame_count].index != Textures[texture].index) return original;
		struct xl_wall_strip *strip = xl_wall_animation_source(mission_resource_archive(), piggy_game_bitmap_name(original), original);
		if (!strip) return original;
		const int frame = dxx_xl_animation_frame(effect->vc.num_frames, effect->vc.frame_time, effect->frame_count,
		                                         effect->time_left, effect->flags & EF_STOPPED, strip->count);
		if (frame < 0) return original;
		strip->frames[frame].bm_flags = piggy_bitmap_get_flags(original) & BM_FLAG_NO_LIGHTING;
		return &strip->frames[frame];
	}
	return original;
}
