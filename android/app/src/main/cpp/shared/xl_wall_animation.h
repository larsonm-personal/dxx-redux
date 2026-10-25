#pragma once
/* Included by D2 OpenGL; reuse its texture pool and lifetime hooks. */
#include "android_mission_assets.h"
#include "xl_animation_phase.h"
#include "xl_static_texture_pixels.h"
#include "cntrlcen.h"
#include "wall.h"

struct xl_wall_strip {
	char path[256];
	grs_bitmap *frames;
	unsigned count;
	size_t bytes;
	int soft, pending;
};
struct xl_wall_path {
	char name[128], path[256];
	int static_lookup;
};
static struct xl_wall_strip xl_wall_strips[32];
static struct xl_wall_path xl_wall_paths[512];
static unsigned xl_wall_generation, xl_wall_strip_count, xl_wall_path_count, xl_wall_frame_count;
static size_t xl_wall_bytes;
static char xl_wall_hog[4096];

/* Keep applied phases across GPU resets: a paused wall still has this mapping.
 * Package generation and HOG identity prevent observations crossing missions. */
struct xl_wall_phase {
	int valid, bitmap, frames, frame_time, frame, time_left, stopped;
};
static struct xl_wall_phase xl_wall_phases[MAX_EFFECTS];
static unsigned xl_wall_phase_generation;
static char xl_wall_phase_hog[4096];

void dxx_xl_wall_mapped_phase(int effect, int bitmap, int frames, int frame_time,
                              int frame, int time_left, int stopped)
{
	if (effect < 0 || effect >= MAX_EFFECTS || !xl_resource_texture_scope_enabled()) return;
	const char *hog = mission_resource_archive();
	if (!hog || strlen(hog) >= sizeof(xl_wall_phase_hog)) return;
	const unsigned generation = android_mission_assets_generation();
	if (generation != xl_wall_phase_generation || strcmp(hog, xl_wall_phase_hog)) {
		memset(xl_wall_phases, 0, sizeof(xl_wall_phases));
		xl_wall_phase_generation = generation;
		strcpy(xl_wall_phase_hog, hog);
	}
	struct xl_wall_phase *phase = &xl_wall_phases[effect];
	phase->valid = 1;
	phase->bitmap = bitmap;
	phase->frames = frames;
	phase->frame_time = frame_time;
	phase->frame = frame;
	phase->time_left = time_left;
	phase->stopped = stopped;
}

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
		for (unsigned j = 0; j < strip->count; ++j) {
			xl_wall_release_texture(strip->frames[j].gltexture);
			xl_wall_release_texture(strip->frames[j].gltexture_mask);
		}
		free(strip->frames);
	}
	memset(xl_wall_strips, 0, sizeof(xl_wall_strips));
	xl_wall_strip_count = xl_wall_path_count = xl_wall_frame_count = 0;
	xl_wall_bytes = 0;
	xl_wall_hog[0] = 0;
	android_ogl_reset_texture_bindings(&ogl_bind_texture_state);
	xl_wall_generation = android_mission_assets_generation();
}

static struct xl_wall_strip *xl_wall_animation_source(const char *hog, const char *name, grs_bitmap *original, int allow_soft, int static_lookup)
{
	if (!name || strlen(name) >= sizeof(xl_wall_paths[0].name)) return NULL;
	unsigned path_index;
	for (path_index = 0; path_index < xl_wall_path_count; ++path_index)
		if (xl_wall_paths[path_index].static_lookup == static_lookup && !strcmp(xl_wall_paths[path_index].name, name)) break;
	if (path_index == xl_wall_path_count) {
		if (xl_wall_path_count == 512) return NULL;
		struct xl_wall_path *path = &xl_wall_paths[xl_wall_path_count++];
		strcpy(path->name, name);
		path->static_lookup = static_lookup;
		path->path[0] = 0;
		if (static_lookup) android_mission_assets_find_static_texture(hog, name, path->path, sizeof(path->path));
		else if (!android_mission_assets_find_animation_texture(hog, name, path->path, sizeof(path->path))) {
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
	struct xl_wall_strip *strip = NULL;
	for (unsigned i = 0; i < xl_wall_strip_count; ++i)
		if (!strcmp(xl_wall_strips[i].path, path)) {
			strip = &xl_wall_strips[i];
			if (strip->count) return !strip->soft || allow_soft ? strip : NULL;
			if (!strip->pending || !allow_soft) return NULL;
			strip->pending = 0; // one permitted retry; failures stay negative until reset
			break;
		}
	if (!strip && xl_wall_strip_count == 32) return NULL;
	/* Retain negative results too: unsupported artwork cannot decode every draw. */
	if (!strip) strip = &xl_wall_strips[xl_wall_strip_count++];
	strcpy(strip->path, path);
	png_data pixels = { 0 };
	if (!read_png_scoped_soft_animation(path, &pixels)) return NULL;
	const unsigned frames = pixels.height / pixels.width;
	strip->soft = pixels.scoped_soft;
	if (static_lookup && (frames != 1 || !strip->soft)) {
		free(pixels.data);
		return NULL;
	}
	if (strip->soft && !allow_soft) {
		strip->pending = 1;
		free(pixels.data);
		return NULL;
	}
	if (frames > 128 - xl_wall_frame_count) {
		free(pixels.data);
		return NULL;
	}
	unsigned free_slots = 0;
	for (int i = 0; i < OGL_TEXTURE_LIST_SIZE; ++i)
		free_slots += ogl_texture_list[i].handle <= 0 && !ogl_texture_list[i].w;
	/* Reserve worst-case color/mask pairs when any source frame has a key. */
	if (free_slots < frames * (pixels.scoped_super ? 2u : 1u)) {
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
		const int fit = strip->soft
		                    ? dxx_xl_fit_soft_coverage(source, NULL, pixels.width, pixels.width, pixels.channels, limit, 0, &fitted)
		                    : dxx_xl_fit_static_pixels(source, pixels.width, pixels.width, pixels.channels, limit, &fitted);
		if (maximum < 2 || !fit) {
			if (fit) free(fitted.data);
			complete = 0;
			break;
		}
		/* Allow full padded mip chains; retain at most32 MiB across all sources. */
		size_t width = 2;
		while (width < (size_t) fitted.width) width *= 2;
		const size_t bytes = width * width * 4 * 2 * (fitted.super ? 2 : 1);
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
		const int filter = android_ogl_effective_texfilt(GameCfg.TexFilt, ogl_aniso_level);
		int failed = ogl_loadtexture(fitted.data, 0, 0, frame->gltexture, frame->bm_flags, 4, filter, name);
		png_data coverage = { 0 };
		coverage.width = fitted.width;
		coverage.height = fitted.height;
		coverage.data = fitted.data;
		coverage.scoped_mask = fitted.mask;
		coverage.scoped_static = 1;
		coverage.scoped_super = fitted.super;
		coverage.scoped_soft = strip->soft;
		if (!failed) failed = !android_ogl_load_scoped_mask(&coverage, frame, filter, ogl_loadtexture);
		free(fitted.data);
		if (failed) {
			complete = 0;
			break;
		}
		frame->gltexture->is_png = 1;
		/* Color and mask travel together and bypass the merged-result cache. */
		frame->gltexture->scoped_static = 1;
		strip->bytes += bytes;
	}
	free(pixels.data);
	if (!complete) {
		for (unsigned i = 0; i < frames; ++i) {
			xl_wall_release_texture(strip->frames[i].gltexture);
			xl_wall_release_texture(strip->frames[i].gltexture_mask);
		}
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

static grs_bitmap *xl_wclip_animation_bitmap(grs_bitmap *original, int texture, int overlay, const char *hog, int allow_soft)
{
	const int seg = g_android_draw_face_ctx.seg, side = g_android_draw_face_ctx.side;
	if (Num_segments < 0 || Num_segments > MAX_SEGMENTS || seg < 0 || seg >= Num_segments ||
	    seg > Highest_segment_index || side < 0 || side >= 6 ||
	    Num_walls < 0 || Num_walls > MAX_WALLS || Num_wall_anims < 0 || Num_wall_anims > MAX_WALL_ANIMS) return original;
	const int wall_index = Segments[seg].sides[side].wall_num;
	if (wall_index < 0 || wall_index >= Num_walls) return original;
	const wall *w = &Walls[wall_index];
	if (w->segnum != seg || w->sidenum != side || (w->type != WALL_DOOR && w->type != WALL_BLASTABLE) ||
	    w->clip_num < 0 || w->clip_num >= Num_wall_anims) return original;
	const wclip *clip = &WallAnims[w->clip_num];
	if (clip->num_frames < 1 || clip->num_frames > MAX_CLIP_FRAMES ||
	    (!!(clip->flags & WCF_TMAP1)) == !!overlay) return original;
	const int mapped = overlay ? (Segments[seg].sides[side].tmap_num2 & 0x3fff) : Segments[seg].sides[side].tmap_num;
	if (mapped != texture || (overlay && !mapped)) return original;
	int stock_frame = -1;
	for (int i = 0; i < clip->num_frames; ++i) {
		if (clip->frames[i] < 0 || clip->frames[i] >= NumTextures) return original;
		if (clip->frames[i] != texture) continue;
		if (stock_frame != -1) return original; // repeated mappings cannot identify an authored frame
		stock_frame = i;
	}
	if (stock_frame < 0) return original;
	struct xl_wall_strip *strip = xl_wall_animation_source(hog, piggy_game_bitmap_name(original), original, allow_soft, 0);
	if (!strip) return original;
	const int frame = dxx_xl_wclip_frame(clip->num_frames, stock_frame, strip->count);
	if (frame < 0) return original;
	strip->frames[frame].bm_flags = piggy_bitmap_get_flags(original) & BM_FLAG_NO_LIGHTING;
	return &strip->frames[frame];
}

static grs_bitmap *xl_wall_animation_bitmap(grs_bitmap *original, int overlay, int allow_soft)
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
		if (xl_wall_phase_generation != android_mission_assets_generation() || strcmp(hog, xl_wall_phase_hog)) return original;
		/* Follow the actual mapping, including the inherited restore behavior for
		 * a stopped parent with a critical clip. stop_effect records its own
		 * first frame separately; neither path changes simulation semantics. */
		const struct xl_wall_phase phase = xl_wall_phases[i];
		if (!phase.valid || phase.bitmap != Textures[texture].index ||
		    dxx_xl_animation_frame(phase.frames, phase.frame_time, phase.frame, phase.time_left, phase.stopped, 1) < 0) return original;
		struct xl_wall_strip *strip = xl_wall_animation_source(hog, piggy_game_bitmap_name(original), original, allow_soft, 0);
		if (!strip) return original;
		const int frame = dxx_xl_animation_frame(phase.frames, phase.frame_time, phase.frame,
		                                         phase.time_left, phase.stopped, strip->count);
		if (frame < 0) return original;
		strip->frames[frame].bm_flags = piggy_bitmap_get_flags(original) & BM_FLAG_NO_LIGHTING;
		return &strip->frames[frame];
	}
	grs_bitmap *selected = xl_wclip_animation_bitmap(original, texture, overlay, hog, allow_soft);
	if (selected != original || !allow_soft) return selected;
	struct xl_wall_strip *strip = xl_wall_animation_source(hog, piggy_game_bitmap_name(original), original, 1, 1);
	if (!strip || !strip->soft || strip->count != 1) return original;
	strip->frames[0].bm_flags = piggy_bitmap_get_flags(original) & BM_FLAG_NO_LIGHTING;
	return &strip->frames[0];
}

static int xl_soft_overlay_base_opaque(grs_bitmap *base)
{
	if (!base || !base->gltexture) return 0;
	if (base->gltexture->scoped_static) return base->gltexture->scoped_opaque;
	const int texture = g_android_draw_face_ctx.tmap1;
	if (texture < 0 || texture >= NumTextures || Textures[texture].index >= MAX_BITMAP_FILES ||
	    base != &GameBitmaps[Textures[texture].index]) return 0;
	/* Uninspected cosmetic replacement alpha cannot establish opacity. */
	return !base->gltexture->is_png && !(piggy_bitmap_get_flags(base) & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT));
}
