#ifdef ANDROID

#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "physfs.h"

#include "android_graphics_options.h"
#include "android_log.h"
#include "config.h"
#include "graphics_config_transaction.h"
#include "palette.h"
#include "playsave.h"
#include "render.h"

static int clamp_bool(int value)
{
	return value ? 1 : 0;
}

static int clamp_texfilt(int value)
{
	if (value < 0)
		return 0;
	if (value > 2)
		return 2;
	return value;
}

static int clamp_gamma(int value)
{
	if (value < 0)
		return 0;
	if (value > 16)
		return 16;
	return value;
}

static int clamp_corner_text_inset(int value)
{
	if (value < 0)
		return 0;
	if (value > 2)
		return 2;
	return value;
}

static int clamp_main_view_fov(int value)
{
	if (value == 100 || value == 110 || value == 120)
		return value;
	return 0;
}

static int g_android_default_alpha_effects;
static int g_android_default_dynlight_color;
static int g_corner_text_surface_width_px;
static int g_corner_text_surface_height_px;
static int g_corner_text_top_left_px;
static int g_corner_text_bottom_left_px;
static int g_corner_text_top_right_px;
static int g_corner_text_bottom_right_px;
static pthread_mutex_t g_config_transaction_mutex = PTHREAD_MUTEX_INITIALIZER;

void android_graphics_apply_pilot_defaults(void)
{
	PlayerCfg.AlphaEffects = g_android_default_alpha_effects;
	PlayerCfg.DynLightColor = g_android_default_dynlight_color;
}

static int android_files_root(char *root, size_t root_size)
{
	const char *write_dir = PHYSFS_getWriteDir();
	char *slash;
	char *backslash;
	char *leaf;
	size_t len;

	if (!write_dir || !*write_dir)
		return 0;
	snprintf(root, root_size, "%s", write_dir);
	len = strlen(root);
	while (len > 0 && (root[len - 1] == '/' || root[len - 1] == '\\'))
		root[--len] = 0;
	slash = strrchr(root, '/');
	backslash = strrchr(root, '\\');
	if (backslash > slash)
		slash = backslash;
	leaf = slash ? slash + 1 : root;
	if (slash && (!strcmp(leaf, "d1x-redux") || !strcmp(leaf, "d2x-redux")))
		*slash = 0;
	return 1;
}

static int path_is_dir(const char *path)
{
	struct stat st;
	return !stat(path, &st) && S_ISDIR(st.st_mode);
}

static int ends_with_plx(const char *name)
{
	size_t len = strlen(name);
	return len > 4 && !strcmp(name + len - 4, ".plx");
}

static enum graphics_config_transaction_result
mirror_config_key(const char *key, int value, int mirror_d1, int mirror_d2)
{
	char root[1024];
	char paths[GRAPHICS_CONFIG_MAX_TARGETS][1200];
	const char *targets[GRAPHICS_CONFIG_MAX_TARGETS];
	size_t target_count = 0;

	if (!android_files_root(root, sizeof(root)))
		return GRAPHICS_CONFIG_TRANSACTION_INVALID;
	snprintf(paths[target_count], sizeof(paths[target_count]), "%s/descent.cfg", root);
	targets[target_count] = paths[target_count];
	target_count++;
	if (mirror_d1) {
		snprintf(paths[target_count], sizeof(paths[target_count]), "%s/d1x-redux", root);
		if (path_is_dir(paths[target_count])) {
			snprintf(paths[target_count], sizeof(paths[target_count]),
			         "%s/d1x-redux/descent.cfg", root);
			targets[target_count] = paths[target_count];
			target_count++;
		}
	}
	if (mirror_d2) {
		snprintf(paths[target_count], sizeof(paths[target_count]), "%s/d2x-redux", root);
		if (path_is_dir(paths[target_count])) {
			snprintf(paths[target_count], sizeof(paths[target_count]),
			         "%s/d2x-redux/descent.cfg", root);
			targets[target_count] = paths[target_count];
			target_count++;
		}
	}
	return graphics_config_patch_files(targets, target_count, key, value);
}

static void patch_visual_dir(const char *dir, int alpha_effects, int dynlight_color)
{
	DIR *d = opendir(dir);
	struct dirent *entry;
	char path[1200];

	if (!d)
		return;
	while ((entry = readdir(d))) {
		if (!ends_with_plx(entry->d_name))
			continue;
		snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
		plx_write_visual_prefs(path, alpha_effects, dynlight_color);
	}
	closedir(d);
}

static void mirror_visual_prefs(void)
{
	char root[1024];
	char dir[1200];

	if (!android_files_root(root, sizeof(root)))
		return;
	snprintf(dir, sizeof(dir), "%s/d1x-redux", root);
	patch_visual_dir(dir, PlayerCfg.AlphaEffects, PlayerCfg.DynLightColor);
	snprintf(dir, sizeof(dir), "%s/d1x-redux/Players", root);
	patch_visual_dir(dir, PlayerCfg.AlphaEffects, PlayerCfg.DynLightColor);
	snprintf(dir, sizeof(dir), "%s/d2x-redux", root);
	patch_visual_dir(dir, PlayerCfg.AlphaEffects, PlayerCfg.DynLightColor);
	snprintf(dir, sizeof(dir), "%s/d2x-redux/Players", root);
	patch_visual_dir(dir, PlayerCfg.AlphaEffects, PlayerCfg.DynLightColor);
}

static int persist_config_if_needed(int persist, const char *key, int value,
                                    int mirror_d1, int mirror_d2)
{
	enum graphics_config_transaction_result result;
	if (!persist)
		return ANDROID_GRAPHICS_OPTION_OK;
	pthread_mutex_lock(&g_config_transaction_mutex);
	result = mirror_config_key(key, value, mirror_d1, mirror_d2);
	pthread_mutex_unlock(&g_config_transaction_mutex);
	if (result != GRAPHICS_CONFIG_TRANSACTION_OK) {
		debug_log(DLOG_GRAPHICS, "graphics config transaction failed: key=%s result=%s",
		          key, graphics_config_transaction_result_name(result));
		return ANDROID_GRAPHICS_OPTION_PERSIST_FAILED;
	}
	return ANDROID_GRAPHICS_OPTION_OK;
}

static void persist_player_if_needed(int persist)
{
	if (!persist)
		return;
	write_player_file();
	mirror_visual_prefs();
}

int android_graphics_set_aniso_level(int value, int persist)
{
	extern int ogl_aniso_level;
	extern volatile int g_aniso_pending_apply;

	if (value < 0)
		value = 0;
	debug_log(DLOG_GRAPHICS,
	          "graphics option request: aniso_level=%d persist=%d",
	          value, persist);
	GameCfg.AnisoLevel = value;
	ogl_aniso_level = value;
	__sync_synchronize();
	g_aniso_pending_apply = 1;
	return persist_config_if_needed(persist, "AnisoLevel", GameCfg.AnisoLevel, 1, 1);
}

int android_graphics_set_msaa_level(int value, int persist)
{
	extern int ogl_msaa_samples;
	extern volatile int g_msaa_pending_apply;

	if (value < 0)
		value = 0;
	debug_log(DLOG_GRAPHICS,
	          "graphics option request: msaa_level=%d persist=%d",
	          value, persist);
	GameCfg.MsaaLevel = value;
	ogl_msaa_samples = value;
	__sync_synchronize();
	g_msaa_pending_apply = 1;
	return persist_config_if_needed(persist, "MsaaLevel", GameCfg.MsaaLevel, 1, 1);
}

int android_graphics_set_texfilt(int value, int persist)
{
	extern int g_texfilt_level;
	extern volatile int g_texfilt_pending_apply;

	value = clamp_texfilt(value);
	debug_log(DLOG_GRAPHICS,
	          "graphics option request: tex_filt=%d persist=%d",
	          value, persist);
	GameCfg.TexFilt = value;
	g_texfilt_level = value;
	__sync_synchronize();
	g_texfilt_pending_apply = 1;
	return persist_config_if_needed(persist, "TexFilt", GameCfg.TexFilt, 1, 1);
}

int android_graphics_set_gamma_level(int value, int persist)
{
	gr_palette_set_gamma(clamp_gamma(value));
	GameCfg.GammaLevel = gr_palette_get_gamma();
	return persist_config_if_needed(persist, "GammaLevel", GameCfg.GammaLevel, 1, 1);
}

int android_graphics_set_menu_texfilt(int value, int persist)
{
	GameCfg.MenuTexFilt = clamp_bool(value);
	return persist_config_if_needed(persist, "MenuTexFilt", GameCfg.MenuTexFilt, 1, 1);
}

int android_graphics_set_hud_texfilt(int value, int persist)
{
	GameCfg.HudTexFilt = clamp_bool(value);
	return persist_config_if_needed(persist, "HudTexFilt", GameCfg.HudTexFilt, 1, 1);
}

int android_graphics_set_main_view_fov(int value, int persist)
{
	GameCfg.MainViewFov = clamp_main_view_fov(value);
	android_render_set_main_view_fov(GameCfg.MainViewFov);
	debug_log(DLOG_GRAPHICS,
	          "graphics option request: main_view_fov=%d persist=%d",
	          GameCfg.MainViewFov, persist);
	return persist_config_if_needed(persist, "MainViewFov", GameCfg.MainViewFov, 1, 1);
}

int android_graphics_set_main_view_fov_locked(int value, int persist)
{
	(void) persist;
	android_render_set_main_view_fov_locked(value);
	debug_log(DLOG_GRAPHICS,
	          "graphics option request: main_view_fov_locked=%d",
	          value ? 1 : 0);
	return ANDROID_GRAPHICS_OPTION_OK;
}

int android_graphics_set_corner_text_inset(int value, int persist)
{
	GameCfg.CornerTextInset = clamp_corner_text_inset(value);
	return persist_config_if_needed(persist, "CornerTextInset", GameCfg.CornerTextInset, 1, 1);
}

int android_graphics_set_classic_depth(int value, int persist)
{
	GameCfg.ClassicDepth = clamp_bool(value);
	return persist_config_if_needed(persist, "ClassicDepth", GameCfg.ClassicDepth, 1, 1);
}

int android_graphics_set_alpha_effects(int value, int persist)
{
	PlayerCfg.AlphaEffects = clamp_bool(value);
	g_android_default_alpha_effects = PlayerCfg.AlphaEffects;
	persist_player_if_needed(persist);
	return ANDROID_GRAPHICS_OPTION_OK;
}

int android_graphics_set_dynlight_color(int value, int persist)
{
	PlayerCfg.DynLightColor = clamp_bool(value);
	g_android_default_dynlight_color = PlayerCfg.DynLightColor;
	persist_player_if_needed(persist);
	return ANDROID_GRAPHICS_OPTION_OK;
}

int android_graphics_set_movie_texfilt(int value, int persist)
{
#ifdef DXX_BUILD_DESCENT_II
	GameCfg.MovieTexFilt = clamp_bool(value);
	return persist_config_if_needed(persist, "MovieTexFilt", GameCfg.MovieTexFilt, 0, 1);
#else
	(void) value;
	(void) persist;
	return ANDROID_GRAPHICS_OPTION_OK;
#endif
}

void android_graphics_set_rounded_corner_text_insets(int surface_width, int surface_height,
                                                     int top_left_px, int bottom_left_px,
                                                     int top_right_px, int bottom_right_px)
{
	g_corner_text_surface_width_px = surface_width > 0 ? surface_width : 0;
	g_corner_text_surface_height_px = surface_height > 0 ? surface_height : 0;
	g_corner_text_top_left_px = top_left_px > 0 ? top_left_px : 0;
	g_corner_text_bottom_left_px = bottom_left_px > 0 ? bottom_left_px : 0;
	g_corner_text_top_right_px = top_right_px > 0 ? top_right_px : 0;
	g_corner_text_bottom_right_px = bottom_right_px > 0 ? bottom_right_px : 0;
}

static int isqrt_int64(long long value)
{
	long long root = 0;
	long long bit = 1LL << 62;

	while (bit > value)
		bit >>= 2;
	while (bit) {
		if (value >= root + bit) {
			value -= root + bit;
			root = (root >> 1) + bit;
		} else {
			root >>= 1;
		}
		bit >>= 2;
	}
	return (int) root;
}

static int scale_corner_text_x(int canvas_width, int inset_px)
{
	if (inset_px <= 0 && g_corner_text_surface_width_px > 0)
		inset_px = g_corner_text_surface_width_px / 20;
	if (inset_px <= 0 || g_corner_text_surface_width_px <= 0)
		return canvas_width / 20;
	return (inset_px * canvas_width + g_corner_text_surface_width_px / 2) /
	       g_corner_text_surface_width_px;
}

static int scale_corner_text_y(int canvas_width, int canvas_height, int inset_px)
{
	if (inset_px <= 0 && g_corner_text_surface_width_px > 0)
		inset_px = g_corner_text_surface_width_px / 20;
	if (inset_px <= 0 || g_corner_text_surface_height_px <= 0)
		return canvas_width / 20;
	return (inset_px * canvas_height + g_corner_text_surface_height_px / 2) /
	       g_corner_text_surface_height_px;
}

static int rounded_corner_inset_for_depth(int x_edge, int y_radius, int depth)
{
	int root;
	long long dy;
	long long inside;

	if (x_edge <= 0 || y_radius <= 0)
		return 0;
	if (depth <= 0)
		return x_edge;
	if (depth >= y_radius)
		return 0;

	dy = y_radius - depth;
	inside = (long long) y_radius * y_radius - dy * dy;
	root = isqrt_int64(inside);
	return (x_edge * (y_radius - root) + y_radius / 2) / y_radius;
}

static int android_graphics_scale_corner_text_inset(int canvas_width, int canvas_height,
                                                    int y, int h, int top_px, int bottom_px)
{
	int top_x;
	int top_y;
	int bottom_x;
	int bottom_y;
	int bottom_depth;
	int full_inset;

	int scaled;

	if (GameCfg.CornerTextInset <= 0 || canvas_width <= 0 || canvas_height <= 0)
		return 0;
	if (h <= 0)
		h = 1;

	top_x = scale_corner_text_x(canvas_width, top_px);
	top_y = scale_corner_text_y(canvas_width, canvas_height, top_px);
	bottom_x = scale_corner_text_x(canvas_width, bottom_px);
	bottom_y = scale_corner_text_y(canvas_width, canvas_height, bottom_px);
	bottom_depth = canvas_height - (y + h);
	full_inset = rounded_corner_inset_for_depth(top_x, top_y, y);
	scaled = rounded_corner_inset_for_depth(bottom_x, bottom_y, bottom_depth);
	if (scaled > full_inset)
		full_inset = scaled;

	if (GameCfg.CornerTextInset == 1)
		return (full_inset + 1) / 2;
	return full_inset;
}

int android_graphics_get_corner_text_left_inset(int canvas_width, int canvas_height, int y, int h)
{
	return android_graphics_scale_corner_text_inset(canvas_width, canvas_height, y, h,
	                                                g_corner_text_top_left_px,
	                                                g_corner_text_bottom_left_px);
}

int android_graphics_get_corner_text_right_inset(int canvas_width, int canvas_height, int y, int h)
{
	return android_graphics_scale_corner_text_inset(canvas_width, canvas_height, y, h,
	                                                g_corner_text_top_right_px,
	                                                g_corner_text_bottom_right_px);
}

int android_graphics_set_option(const char *name, int value, int persist)
{
	if (!name)
		return ANDROID_GRAPHICS_OPTION_UNKNOWN;
	if (!strcmp(name, "aniso_level"))
		return android_graphics_set_aniso_level(value, persist);
	else if (!strcmp(name, "msaa_level"))
		return android_graphics_set_msaa_level(value, persist);
	else if (!strcmp(name, "tex_filt"))
		return android_graphics_set_texfilt(value, persist);
	else if (!strcmp(name, "gamma_level"))
		return android_graphics_set_gamma_level(value, persist);
	else if (!strcmp(name, "menu_tex_filt"))
		return android_graphics_set_menu_texfilt(value, persist);
	else if (!strcmp(name, "hud_tex_filt"))
		return android_graphics_set_hud_texfilt(value, persist);
	else if (!strcmp(name, "main_view_fov"))
		return android_graphics_set_main_view_fov(value, persist);
	else if (!strcmp(name, "main_view_fov_locked"))
		return android_graphics_set_main_view_fov_locked(value, persist);
	else if (!strcmp(name, "corner_text_inset"))
		return android_graphics_set_corner_text_inset(value, persist);
	else if (!strcmp(name, "classic_depth"))
		return android_graphics_set_classic_depth(value, persist);
	else if (!strcmp(name, "alpha_effects"))
		return android_graphics_set_alpha_effects(value, persist);
	else if (!strcmp(name, "dynlight_color"))
		return android_graphics_set_dynlight_color(value, persist);
	else if (!strcmp(name, "movie_tex_filt"))
		return android_graphics_set_movie_texfilt(value, persist);
	return ANDROID_GRAPHICS_OPTION_UNKNOWN;
}

#endif
