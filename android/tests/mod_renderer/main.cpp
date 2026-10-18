#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <fcntl.h>
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <EGL/egl.h>
#include "input_demo_codec.h"
#include "android_mod_bitmaps.h"
extern "C" {
#include "args.h"
#include "3d.h"
#include "bm.h"
#include "digi.h"
#include "dxxerror.h"
#include "ogl_init.h"
#include "palette.h"
#include "physfsx.h"
#include "piggy.h"
#include "pngfile.h"
#include "u_mem.h"
#include "vclip.h"
#include "textures.h"
#include "android_texture_usage.h"
extern vms_vector View_position;
extern vms_matrix View_matrix;
extern int GL_TEXTURE_2D_enabled;
void ogl_init_texture_list_internal(void);
void ogl_smash_texture_list_internal(void);
void ogl_init_pixel_buffers(int, int);
void ogl_close_pixel_buffers(void);
void ogl_init_prog(void);
void ogl_loadbmtexture_f(grs_bitmap *, int, const char *);
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#include "d1_in_d2/d1_in_d2_assets.h"
void piggy_init_pigfile(char *);
#else
#include "custom.h"
void custom_remove(void);
#endif
}
using nlohmann::json;
static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}
#pragma push_macro("glDrawArrays")
#undef glDrawArrays
static GLuint shader(GLenum type, const char *source)
{
	GLuint result = glCreateShader(type);
	glShaderSource(result, 1, &source, nullptr);
	glCompileShader(result);
	GLint ok;
	glGetShaderiv(result, GL_COMPILE_STATUS, &ok);
	require(ok, "compile sampling shader");
	return result;
}
static std::string sample(GLuint program, GLuint texture, int *transparent_pixels = nullptr)
{
	require(texture != 0, "texture has a GPU allocation");
	glViewport(0, 0, 64, 64);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	gles3_shim_bind_program(program);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);
	glUniform1i(glGetUniformLocation(program, "tex"), 0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	std::vector<unsigned char> pixels(64 * 64 * 4);
	glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	require(glGetError() == GL_NO_ERROR, "sample renderer texture and mask");
	if (transparent_pixels) {
		*transparent_pixels = 0;
		for (size_t i = 3; i < pixels.size(); i += 4)
			*transparent_pixels += pixels[i] < 255;
	}
	std::string hash, error;
	require(input_demo_sha256_hex(pixels.data(), pixels.size(), &hash, &error), "hash actual GPU pixels");
	return hash;
}
#pragma pop_macro("glDrawArrays")
static void mount(const char *archive, const char *point)
{
	if (!PHYSFS_mount(archive, point, 0)) {
		std::fprintf(stderr, "Mount failed: %s (%s)\n", archive, PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()));
		require(false, "mount pack first");
	}
	ogl_smash_texture_list_internal();
}
static void unmount(const char *archive)
{
	require(PHYSFS_unmount(archive), "unmount pack");
	ogl_smash_texture_list_internal();
}
static json render(GLuint program, grs_bitmap *bitmap)
{
	ogl_freebmtexture(bitmap);
	ogl_loadbmtexture_f(bitmap, 0, nullptr);
	require(bitmap->gltexture && bitmap->gltexture->handle, "actual renderer loaded texture");
	int transparent_pixels = 0;
	json result = { { "image", sample(program, bitmap->gltexture->handle, &transparent_pixels) }, { "source", bitmap->gltexture->replacement_source }, { "width", bitmap->gltexture->w } };
	result["transparent_pixels"] = transparent_pixels;
	result["mask"] = bitmap->gltexture_mask ? sample(program, bitmap->gltexture_mask->handle) : "";
	return result;
}
static void write_mission(int index, const char *name)
{
	std::string bytes(32, '\0');
	std::memcpy(bytes.data(), "DPOG", 4);
	bytes[4] = bytes[8] = 1;
	bytes[12] = index & 255;
	bytes[13] = index >> 8;
	bytes[23] = bytes[24] = 64;
	bytes += std::string(4096, '\x2a');
	auto *file = PHYSFS_openWrite(name);
	require(file && PHYSFS_writeBytes(file, bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()) && PHYSFS_close(file), "write authored mission replacement");
}
static void copy_asset(const std::string &source, const std::string &target)
{
	auto *input = PHYSFSX_openReadBuffered(source.c_str());
	require(input != nullptr, "open real pack image or mask");
	std::string bytes(static_cast<size_t>(PHYSFS_fileLength(input)), '\0');
	require(PHYSFS_readBytes(input, bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()), "read real pack image or mask");
	PHYSFS_close(input);
	require(PHYSFS_mkdir(target.substr(0, target.find_last_of('/')).c_str()), "create paired overlay");
	auto *output = PHYSFS_openWrite(target.c_str());
	require(output && PHYSFS_writeBytes(output, bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()) && PHYSFS_close(output), "write paired overlay");
}

static void write_blend(const std::string &path, const char *value)
{
	require(PHYSFS_mkdir(path.substr(0, path.find_last_of('/')).c_str()), "create sprite material directory");
	auto *file = PHYSFS_openWrite(path.c_str());
	require(file && PHYSFS_writeBytes(file, value, std::strlen(value)) == static_cast<PHYSFS_sint64>(std::strlen(value)) && PHYSFS_close(file), "write sprite material");
}

static std::vector<unsigned char> draw_sprite(grs_bitmap *bitmap, int background, int fade, int blend)
{
	glViewport(0, 0, 64, 64);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glClearColor(background / 255.0f, background / 255.0f, background / 255.0f, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrthof(-1, 1, -1, 1, -2, 2);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	View_position = {};
	View_matrix = vmd_identity_matrix;
	vm_vec_make(&Matrix_scale, F1_0, F1_0, F1_0);
	gr_settransblend(fade, blend);
	GL_TEXTURE_2D_enabled = -1;
	vms_vector position = { 0, 0, F1_0 };
	g3_draw_bitmap(&position, F1_0, F1_0, bitmap);
	require(grd_curcanv->cv_blend_func == blend && grd_curcanv->cv_fade_level == fade, "sprite restores canvas blend and fade");
	GLint destination;
	glGetIntegerv(GL_BLEND_DST_RGB, &destination);
	require(destination == (blend == GR_BLEND_NORMAL ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE), "sprite restores actual GPU blend state");
	std::vector<unsigned char> pixels(64 * 64 * 4);
	glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	require(glGetError() == GL_NO_ERROR, "draw actual engine sprite");
	return pixels;
}

static void test_sprite_material(json &trace, GLuint program, int content_game, const char *point)
{
	/* D1 uses the same renderer hook, exercised with the original D2 artwork */
	const std::string prefix = content_game == 1 ? "textures/d1/" : "textures/d2/";
	const char *pack = "d2-hires-256-textures-ktx2.dxa";
	mount(pack, nullptr);
	std::vector<std::string> names;
	for (int group : { 2, 3, 5, 6, 8, 9 }) {
		if (content_game == 1 && group > 6) continue;
		for (int frame = 0; frame < 3; ++frame) {
			char name[16];
			std::snprintf(name, sizeof(name), "muzl%02d#%d", group, frame);
			names.emplace_back(name);
			copy_asset(std::string("textures/d2/") + name + ".ktx2", "sprites/" + prefix + name + ".ktx2");
			PHYSFS_delete(("sprites/" + prefix + name + ".ktx2.blend").c_str());
		}
	}
	unmount(pack);
	mount("sprites", point);
	char first_name[] = "muzl02#0";
	auto *first = &GameBitmaps[piggy_find_bitmap(first_name).index];
	require(render(program, first)["source"] == 0, "RGB sprite without authored material falls back to stock");
	const auto marker = prefix + "muzl02#0.ktx2.blend";
	write_blend("foreign/" + marker, "additive\n");
	mount("foreign", point);
	require(render(program, first)["source"] == 0, "another archive cannot lend a blend marker");
	unmount("foreign");
	for (const char *invalid : { "additive", "multiply\n", "additive\nextra" }) {
		write_blend("sprites/" + marker, invalid);
		ogl_smash_texture_list_internal();
		require(render(program, first)["source"] == 0, "malformed sprite material falls back to stock");
	}
	json frames = json::array();
	for (const auto &name : names) {
		write_blend("sprites/" + prefix + name + ".ktx2.blend", "additive\n");
		ogl_smash_texture_list_internal();
		char lookup[16];
		std::strcpy(lookup, name.c_str());
		const auto index = piggy_find_bitmap(lookup);
		require(index.index != 0, "find original muzzle frame");
		auto *bitmap = &GameBitmaps[index.index];
		const auto loaded = render(program, bitmap);
		if (loaded["source"] == 0 || !bitmap->gltexture->sprite_additive) {
			std::fprintf(stderr, "Sprite rejected: %s index=%d render=%s clips=%d textures=%d\n", lookup, index.index, loaded.dump().c_str(), Num_vclips, NumTextures);
			for (int i = 0; i < Num_vclips; ++i)
				for (int f = 0; f < Vclip[i].num_frames; ++f)
					if (Vclip[i].frames[f].index == index.index) std::fprintf(stderr, "clip=%d frame=%d flags=%d\n", i, f, Vclip[i].flags);
			for (int i = 0; i < NumTextures; ++i)
				if (Textures[i].index == index.index) std::fprintf(stderr, "wall=%d\n", i);
			const std::string image_path = (point ? std::string(point) + "/" : "") + prefix + name + ".ktx2";
			std::fprintf(stderr, "image=%s owner=%s marker=%s\n", image_path.c_str(), android_texture_source(image_path.c_str()), android_texture_source((image_path + ".blend").c_str()));
		}
		require(loaded["source"] != 0 && loaded["width"] == 256 && bitmap->gltexture->sprite_additive, "load authored RGB sprite material");
		require(bitmap->gltexture->additive_draws == 0, "upload alone is not counted as a sprite draw");
		const auto black = draw_sprite(bitmap, 0, GR_FADE_OFF, GR_BLEND_NORMAL);
		const auto gray = draw_sprite(bitmap, 64, GR_FADE_OFF, GR_BLEND_NORMAL);
		const auto faded = draw_sprite(bitmap, 64, 15, GR_BLEND_ADDITIVE_C);
		require(bitmap->gltexture->additive_draws == 3, "count actual sprite draw submissions");
		int dark = 0, bright = 0;
		for (size_t i = 0; i < black.size(); i += 4) {
			if (black[i] < 4 && black[i + 1] < 4 && black[i + 2] < 4) ++dark;
			if (black[i] > 64 || black[i + 1] > 64 || black[i + 2] > 64) ++bright;
			for (size_t c = 0; c < 3; ++c) {
				const int expected = (std::min) (255, black[i + c] + 64);
				require(std::abs(gray[i + c] - expected) <= 2, "additive muzzle preserves background and adds source light");
				const float alpha = 1.0f - 15.0f / (GR_FADE_LEVELS - 1.0f);
				const int fade_expected = (std::min) (255, static_cast<int>(black[i + c] * alpha + 64));
				require(std::abs(faded[i + c] - fade_expected) <= 2, "authored additive sprite respects fade");
			}
		}
		require(dark > 100 && bright > 5, "actual sprite has black corners and visible flare");
		frames.push_back({ { "name", name }, { "render", loaded }, { "draws", bitmap->gltexture->additive_draws }, { "dark_pixels", dark }, { "bright_pixels", bright } });
		ogl_smash_texture_list_internal();
		ogl_loadbmtexture_f(bitmap, 0, nullptr);
		require(bitmap->gltexture->sprite_additive && bitmap->gltexture->additive_draws == 0, "reupload resets draw history for the new allocation");
	}
	/* A valid marker on a transparency-flagged wall must not permit RGB */
	copy_asset((point ? std::string(point) + "/" : "") + prefix + "muzl02#0.ktx2", "sprites/" + prefix + "rock156.ktx2");
	write_blend("sprites/" + prefix + "rock156.ktx2.blend", "additive\n");
	ogl_smash_texture_list_internal();
	char rock_name[] = "rock156";
	auto *rock = &GameBitmaps[piggy_find_bitmap(rock_name).index];
	require(render(program, rock)["source"] == 0, "wall material cannot bypass the alpha guard");
	unmount("sprites");
	const auto normal = render(program, first);
	require(normal["source"] == 0 && !first->gltexture->sprite_additive, "unmount clears sprite material");
	const auto normal_black = draw_sprite(first, 0, GR_FADE_OFF, GR_BLEND_NORMAL);
	const auto normal_gray = draw_sprite(first, 64, GR_FADE_OFF, GR_BLEND_NORMAL);
	int solid = 0;
	for (size_t i = 0; i < normal_black.size(); i += 4)
		if (normal_black[i] > 64 && normal_black[i] == normal_gray[i]) ++solid;
	require(solid > 5, "subsequent ordinary sprite uses normal blending");
	trace.push_back({ { "case", "additive_sprite_material" }, { "game", content_game }, { "frames", frames } });
}
static void test_mode(json &trace, GLuint program, int content_game)
{
#ifdef DXX_BUILD_DESCENT_II
	const char *point = content_game == 1 ? "d1-mods" : nullptr;
#else
	const char *point = nullptr;
#endif
	const char *uud = content_game == 1 ? "uud1tp-textures.dxa" : "uud2tp-textures.dxa";
	const char *hires = content_game == 1 ? "d1-hires-256-textures-ktx2.dxa" : "d2-hires-256-textures-ktx2.dxa";
	char name[16];
	std::strcpy(name, content_game == 1 ? "metl127" : "misc037");
	mount(uud, point);
#ifdef DXX_BUILD_DESCENT_II
	if (content_game == 1) {
		const char *error = nullptr;
		auto *assets = d1_in_d2_read_assets("DESCENT.PIG", "palette.256", &error);
		require(assets && d1_in_d2_publish_assets(assets, &error), error ? error : "publish D1 assets");
	} else {
		properties_init();
		char pig[] = "groupa.pig";
		piggy_init_pigfile(pig);
	}
#else
	properties_init();
	require(android_mod_bitmaps_apply(1, nullptr) == 120, "apply indexed D1 bank");
#endif
	char palette[16];
	std::strcpy(palette, content_game == 1 ? "palette.256" : "groupa.256");
	gr_use_palette_table(palette);
	/* Check corrected opaque artwork without UUD clearing the original stock flags */
	android_mod_bitmaps_reset();
	unmount(uud);
	json rocks = json::array();
	for (const char *rock_name : { "rock156", "rock178", "rock260" }) {
		char lookup_name[16];
		std::strcpy(lookup_name, rock_name);
		const auto rock = piggy_find_bitmap(lookup_name);
		require(rock.index > 0, "find stock rock");
		auto *rock_bitmap = &GameBitmaps[rock.index];
		require(piggy_bitmap_get_flags(rock_bitmap) & BM_FLAG_TRANSPARENT, "original rock transparency flag remains");
		const auto stock_rock = render(program, rock_bitmap);
		require(stock_rock["source"] == 0 && stock_rock["transparent_pixels"] > 0, "stock rock has isolated transparent pixels");
		mount(hires, point);
		const auto replacement_rock = render(program, rock_bitmap);
		require(replacement_rock["source"] != 0 && replacement_rock["width"] == 256 && replacement_rock["transparent_pixels"] == 0, "authored opaque rock renders without stock holes");
		rocks.push_back({ { "name", rock_name }, { "stock", stock_rock }, { "replacement", replacement_rock } });
		unmount(hires);
	}
	trace.push_back({ { "case", "opaque_rock_fixes" }, { "game", content_game }, { "rocks", rocks } });
	test_sprite_material(trace, program, content_game, point);
	mount(uud, point);
	require(android_mod_bitmaps_apply(content_game, content_game == 1 ? nullptr : "groupa.pig") > 0, "restore indexed UUD bank after stock rock check");
	const auto index = piggy_find_bitmap(name);
	require(index.index > 0, "find real overlap bitmap");
	auto *bitmap = &GameBitmaps[index.index];
	const auto only_uud = render(program, bitmap);
	require(only_uud["source"] != 0 && only_uud["width"] == 64 && only_uud["mask"] != "", "UUD image and mask actually loaded");
	trace.push_back({ { "case", "uud_alone" }, { "game", content_game }, { "render", only_uud } });
	unmount(uud);
	mount(hires, point);
	const auto only_hires = render(program, bitmap);
	require(only_hires["source"] != 0 && only_hires["width"] == 256 && only_hires["mask"] != "", "high-res image and mask actually loaded");
	require(only_hires["image"] != only_uud["image"], "overlap has distinguishable authored pixels");
	trace.push_back({ { "case", "hires_alone" }, { "game", content_game }, { "render", only_hires } });
	{
		std::atomic<bool> stop{ false }, failed{ false };
		std::atomic<unsigned> writes{ 0 };
		std::thread metadata([&]() {
			const char *path = ".route_metadata_ingame_cpu_duty.fixture.tmp";
			while (!stop.load(std::memory_order_relaxed)) {
				const int file = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0600);
				if (file < 0 || close(file) || unlink(path)) {
					failed.store(true, std::memory_order_relaxed);
					break;
				}
				writes.fetch_add(1, std::memory_order_relaxed);
			}
		});
		for (int i = 0; i < 64; ++i) {
			clear_texture_lookup_cache();
			require(render(program, bitmap) == only_hires, "transient root metadata preserves indexed replacement pixels and mask");
		}
		stop.store(true, std::memory_order_relaxed);
		metadata.join();
		require(!failed.load() && writes.load() > 0, "exercise concurrent temporary metadata writes");
		trace.push_back({ { "case", "transient_root_metadata" }, { "game", content_game }, { "iterations", 64 }, { "render", only_hires } });
	}
#ifdef DXX_BUILD_DESCENT_II
	if (content_game == 2) {
		json doors = json::array();
		for (const auto &clip : { std::pair<int, int>{ 33, 7 }, { 48, 10 }, { 51, 10 }, { 52, 8 } }) {
			for (int frame = 1; frame <= clip.second; ++frame) {
				char door_name[16];
				std::snprintf(door_name, sizeof(door_name), "door%d#%d", clip.first, frame);
				const auto door = piggy_find_bitmap(door_name);
				require(door.index != 0, "find stock door with unused super flag");
				const auto rendered_door = render(program, &GameBitmaps[door.index]);
				require(rendered_door["source"] != 0 && rendered_door["width"] == 256 && rendered_door["mask"] != "", "unused stock super flag does not disable valid high-res animation frames");
				doors.push_back({ { "name", door_name }, { "render", rendered_door } });
				ogl_freebmtexture(&GameBitmaps[door.index]);
			}
		}
		trace.push_back({ { "case", "unused_super_flag" }, { "game", content_game }, { "frames", doors } });
	}
#endif
	mount(uud, point);
	const auto uud_first = render(program, bitmap);
	if (uud_first != only_uud) {
		std::fprintf(stderr, "UUD priority mismatch for content game %d: expected %s, actual %s\n", content_game, only_uud.dump().c_str(), uud_first.dump().c_str());
#ifdef DXX_BUILD_DESCENT_II
		std::fprintf(stderr, "UUD bitmap classification: custom=%d, stored_flags=%d, current_flags=%d\n", piggy_bitmap_is_custom(bitmap), piggy_bitmap_get_flags(bitmap), bitmap->bm_flags);
#endif
	}
	require(uud_first == only_uud, "higher priority UUD PNG beats lower priority KTX2, with its own mask");
	trace.push_back({ { "case", "uud_first" }, { "game", content_game }, { "render", uud_first } });
	unmount(hires);
	mount(hires, point);
	const auto hires_first = render(program, bitmap);
	require(hires_first == only_hires, "higher priority KTX2 beats lower priority UUD PNG, with its own mask");
	trace.push_back({ { "case", "hires_first" }, { "game", content_game }, { "render", hires_first } });
	const std::string stem = std::string("textures/") + (content_game == 1 ? "d1/" : "d2/") + name;
	const std::string source = (point ? std::string(point) + "/" : "") + (content_game == 1 ? std::string(name) : std::string("textures/d2/sets/groupa/") + name);
	PHYSFS_delete(("paired/" + stem + "_mask.png").c_str());
	copy_asset(source + ".png", "paired/" + stem + ".png");
	mount("paired", point);
	const auto missing_mask = render(program, bitmap);
	require(missing_mask == only_hires, "incomplete higher-priority PNG cannot borrow another pack's mask");
	trace.push_back({ { "case", "unpaired_mask_rejected" }, { "game", content_game }, { "render", missing_mask } });
	copy_asset(source + "_mask.png", "paired/" + stem + "_mask.png");
	ogl_smash_texture_list_internal();
	const auto paired = render(program, bitmap);
	require(paired["image"] == only_uud["image"] && paired["mask"] == only_uud["mask"] && paired["source"] != only_hires["source"], "matching image and mask from one owner win together");
	trace.push_back({ { "case", "paired_image_and_mask" }, { "game", content_game }, { "render", paired } });
	unmount("paired");
	unmount(hires);
	unmount(uud);
	const auto indexed = render(program, bitmap);
	require(indexed["source"] == 0 && indexed["width"] == 64 && indexed["mask"] != "", "indexed fallback retains supertransparency without an external image");
	trace.push_back({ { "case", "indexed_fallback" }, { "game", content_game }, { "render", indexed } });
	mount(uud, point);
	mount(hires, point);
	write_mission(index.index, content_game == 1 ? "fixture.pg1" : "fixture.POG");
#ifdef DXX_BUILD_DESCENT_II
	if (content_game == 1) {
		const char *error = nullptr;
		auto *assets = d1_in_d2_read_assets("DESCENT.PIG", "palette.256", &error);
		require(assets && d1_custom_read_assets(assets, "fixture.rdl", &error) && d1_in_d2_publish_assets(assets, &error), error ? error : "publish mission D1 assets");
	} else {
		char level[] = "fixture.rl2";
		load_bitmap_replacements(level);
	}
#else
	char level[] = "fixture.rdl";
	load_custom_data(level);
#endif
	const auto custom = render(program, bitmap);
	require(custom["source"] == 0 && custom["mask"] == "" && custom["width"] == 64, "mission indexed artwork overrides both global image packs");
	trace.push_back({ { "case", "mission_override" }, { "game", content_game }, { "render", custom } });
	ogl_freebmtexture(bitmap);
	unmount(uud);
	unmount(hires);
}
int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	require(PHYSFS_init(nullptr) && PHYSFS_setWriteDir(".") && PHYSFS_mount(".", nullptr, 1), "isolated filesystem");
	PHYSFS_permitSymbolicLinks(1);
	require(PHYSFS_mount("DESCENT.HOG", nullptr, 1) && PHYSFS_mount("DESCENT2.HOG", nullptr, 1), "base palettes");
	GameArg.SndNoSound = GameArg.SndNoMusic = GameArg.SysInputDemoNoRender = 1;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SndDigiSampleRate = SAMPLE_RATE_22K;
#endif
	digi_select_system(SDLAUDIO_SYSTEM);
	EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	require(eglInitialize(display, nullptr, nullptr), "initialize EGL");
	const EGLint attributes[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
	EGLConfig config;
	EGLint count;
	require(eglChooseConfig(display, attributes, &config, 1, &count) && count == 1, "pbuffer config");
	const EGLint dimensions[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE }, version[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	auto surface = eglCreatePbufferSurface(display, config, dimensions);
	auto context = eglCreateContext(display, config, EGL_NO_CONTEXT, version);
	require(eglMakeCurrent(display, surface, surface, context), "current EGL context");
	gles3_shim_init();
	ogl_init_prog();
	ogl_init_texture_list_internal();
	ogl_init_pixel_buffers(64, 64);
	grs_screen screen{};
	screen.sc_w = screen.sc_h = 64;
	screen.sc_aspect = F1_0;
	gr_init_canvas(&screen.sc_canvas, nullptr, BM_OGL, 64, 64);
	grd_curscreen = &screen;
	gr_set_current_canvas(&screen.sc_canvas);
	GLuint vertex = shader(GL_VERTEX_SHADER, "#version 300 es\nout vec2 uv; void main(){vec2 p=vec2((gl_VertexID<<1)&2,gl_VertexID&2);uv=p;gl_Position=vec4(p*2.0-1.0,0,1);}");
	GLuint fragment = shader(GL_FRAGMENT_SHADER, "#version 300 es\nprecision highp float; in vec2 uv; uniform sampler2D tex; out vec4 color; void main(){color=textureLod(tex,uv,0.0);}");
	GLuint program = glCreateProgram(), vao;
	glAttachShader(program, vertex);
	glAttachShader(program, fragment);
	glLinkProgram(program);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	json trace = json::array();
#ifdef DXX_BUILD_DESCENT_II
	test_mode(trace, program, 2);
#endif
	test_mode(trace, program, 1);
	gamedata_close();
	ogl_close_pixel_buffers();
	glDeleteProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	glDeleteVertexArrays(1, &vao);
	gles3_shim_shutdown();
	eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(display, context);
	eglDestroySurface(display, surface);
	eglTerminate(display);
	FILE *out = std::fopen(argv[1], "wb");
	const auto bytes = trace.dump(2) + "\n";
	require(out && std::fwrite(bytes.data(), 1, bytes.size(), out) == bytes.size() && std::fclose(out) == 0, "write trace");
	PHYSFS_deinit();
	return 0;
}
