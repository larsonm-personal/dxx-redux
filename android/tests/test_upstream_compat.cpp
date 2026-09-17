#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <SDL.h>
#undef main
#ifdef HAVE_LIBPNG
#include <png.h>
#endif

extern "C" {
#include "args.h"
#include "bm.h"
#include "config.h"
#include "console.h"
#include "dxxerror.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "gauges.h"
#include "iff.h"
#include "mission.h"
#include "multi.h"
#include "newdemo.h"
#include "newmenu.h"
#include "object.h"
#include "palette.h"
#include "physfsx.h"
#include "piggy.h"
#include "polyobj.h"
#include "powerup.h"
#include "pcx.h"
#include "pngfile.h"
#include "robot.h"
#include "text.h"
#include "u_mem.h"
#ifdef OGL
#include "ogl_init.h"
extern grs_bitmap nm_background;
void newmenu_free_background(void);
void ogl_smash_texture_list_internal(void);
#endif
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2.h"
#include "effects.h"
#include "hash.h"
#include "textures.h"
extern hashtable AllBitmapsNames;
void free_bitmap_replacements(void);
void free_d1_tmap_nums(void);
extern int Robot_replacements_loaded;
extern int Gamesave_num_players;
int load_mission_ham(void);
#else
void load_hxm(char *filename);
#endif
}

#pragma push_macro("PATH_MAX")
#undef PATH_MAX
#ifdef DXX_BUILD_DESCENT_II
#include "../../d2/xmodel/xcfile.h"
#else
#include "../../d1/xmodel/xcfile.h"
#endif
#pragma pop_macro("PATH_MAX")

static void require(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		std::exit(1);
	}
}

using bytes = std::vector<unsigned char>;

static void set_int(bytes &data, size_t offset, int value)
{
	for (unsigned i = 0; i < 4; ++i)
		data[offset + i] = static_cast<unsigned char>(static_cast<unsigned>(value) >> (i * 8));
}

static void append_int(bytes &data, int value)
{
	const size_t offset = data.size();
	data.resize(offset + 4);
	set_int(data, offset, value);
}

static void append(bytes &data, const bytes &record)
{
	data.insert(data.end(), record.begin(), record.end());
}

static bytes model_record(fix radius)
{
	// Disk polymodels have a 32-bit pointer placeholder regardless of host ABI
	bytes record(734);
	set_int(record, 0, 1);
	set_int(record, 4, 2); // a two-byte OP_EOF model
	set_int(record, 726, radius);
	return record;
}

static void write_fixture(const char *name, const bytes &data)
{
	PHYSFS_file *file = PHYSFS_openWrite(name);
	require(file != nullptr, "create fixture");
	require(PHYSFS_writeBytes(file, data.data(), data.size()) == static_cast<PHYSFS_sint64>(data.size()), "write fixture");
	require(PHYSFS_close(file) != 0, "close fixture");
}

static bytes hxm_header()
{
	bytes data;
	append_int(data, 0x21584d48);
	append_int(data, 1);
	return data;
}

static void test_seek()
{
	write_fixture("seek.dat", { 1, 2, 3, 4 });
	CFile file;
	require(file.Open("seek.dat", "", "rb", 0) != 0, "open seek fixture");
	require(file.Seek(2, SEEK_SET) == 0, "seek success returns zero at nonzero offset");
	require(file.Seek(-1, SEEK_CUR) == 0, "relative seek");
	require(file.Seek(-1, SEEK_END) == 0, "end-relative seek");
	require(file.Seek(0, -99) == -1, "invalid seek method returns error");
}

#ifdef HAVE_LIBPNG
static const unsigned char png_colors[4][3] = { { 17, 93, 201 }, { 230, 41, 7 }, { 120, 88, 128 }, { 6, 182, 79 } };

static void write_png_fixture(const char *name, int depth, int color_type, bool transparent, bool interlaced)
{
	FILE *file = std::fopen(name, "wb");
	require(file != nullptr, "create PNG fixture");
	png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info = png_create_info_struct(png);
	require(png && info, "allocate PNG encoder");
	if (setjmp(png_jmpbuf(png))) require(false, "encode PNG fixture");
	png_init_io(png, file);
	png_set_IHDR(png, info, 5, 3, depth, color_type, interlaced ? PNG_INTERLACE_ADAM7 : PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
	png_color palette[4];
	std::memcpy(palette, png_colors, sizeof(palette));
	const bool indexed = color_type == PNG_COLOR_TYPE_PALETTE;
	const int colors = depth == 1 ? 2 : 4;
	if (indexed) {
		png_set_PLTE(png, info, palette, colors);
		if (transparent) {
			png_byte alpha[3] = { 255, 0, 128 };
			png_set_tRNS(png, info, alpha, colors == 2 ? 2 : 3, nullptr);
		}
	}
	png_write_info(png, info);
	unsigned char pixels[3][20] = {};
	png_bytep rows[3] = { pixels[0], pixels[1], pixels[2] };
	for (int y = 0; y < 3; ++y) {
		for (int x = 0; x < 5; ++x) {
			const int index = (x + y) % colors;
			if (indexed)
				pixels[y][x * depth / 8] |= index << (8 - depth - (x * depth % 8));
			else {
				const int channels = color_type == PNG_COLOR_TYPE_RGBA ? 4 : 3;
				std::memcpy(&pixels[y][x * channels], png_colors[index], 3);
				if (channels == 4) pixels[y][x * channels + 3] = index == 1 ? 0 : index == 2 ? 128
					                                                                         : 255;
			}
		}
	}
	png_write_image(png, rows);
	png_write_end(png, info);
	png_destroy_write_struct(&png, &info);
	std::fclose(file);
}

static void test_png_decode()
{
	for (int depth : { 1, 2, 4, 8 }) {
		for (bool alpha : { false, true }) {
			for (bool interlaced : { false, true }) {
				write_png_fixture("palette.png", depth, PNG_COLOR_TYPE_PALETTE, alpha, interlaced);
				png_data decoded = {};
				require(read_png("palette.png", &decoded) != 0, "read indexed PNG");
				require(decoded.width == 5 && decoded.height == 3 && decoded.depth == 8, "expanded PNG dimensions and depth");
				require(!decoded.paletted && decoded.color && decoded.alpha == alpha && decoded.channels == (alpha ? 4u : 3u), "expanded PNG channel metadata");
				require(!decoded.palette && decoded.num_palette == 0, "expanded pixels need no palette lookup");
				for (int y = 0; y < 3; ++y) {
					for (int x = 0; x < 5; ++x) {
						const int index = (x + y) % (depth == 1 ? 2 : 4);
						const unsigned char *pixel = decoded.data + (y * 5 + x) * decoded.channels;
						require(std::memcmp(pixel, png_colors[index], 3) == 0, "PNG palette colors including supertransparent marker");
						if (alpha) require(pixel[3] == (index == 1 ? 0 : index == 2 ? 128
							                                                        : 255),
							               "tRNS preserves transparent, partial, and implicit opaque alpha");
					}
				}
				std::free(decoded.data);
			}
		}
	}
	for (int type : { PNG_COLOR_TYPE_RGB, PNG_COLOR_TYPE_RGBA }) {
		write_png_fixture("truecolor.png", 8, type, false, false);
		png_data decoded = {};
		require(read_png("truecolor.png", &decoded) != 0 && !decoded.paletted, "existing truecolor PNG path");
		require(decoded.channels == (type == PNG_COLOR_TYPE_RGBA ? 4u : 3u), "truecolor channels preserved");
		require(std::memcmp(decoded.data, png_colors[0], 3) == 0, "truecolor pixels preserved");
		std::free(decoded.data);
	}
	write_fixture("broken.png", { 137, 80, 78, 71, 13, 10, 26, 10 });
	png_data decoded = {};
	require(!read_png("broken.png", &decoded) && !decoded.data && !decoded.palette, "truncated PNG fails cleanly");
	require(!read_png("no-such-texture.png", &decoded), "missing PNG permits stock fallback");
}

#ifdef OGL
static void check_menu_png()
{
	nm_draw_background(10, 10, 200, 150);
	const ogl_texture *texture = nm_background.gltexture;
	require(texture && texture->handle && texture->is_png && texture->w == 5 && texture->h == 3, "menu uses explicit scores PNG");
	bytes pixels(texture->tw * texture->th * 4);
	glBindTexture(GL_TEXTURE_2D, texture->handle);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	require(std::memcmp(pixels.data(), png_colors[0], 3) == 0, "menu uploads PNG palette colors instead of game palette");
	require(pixels[7] == 0 && pixels[11] == 128 && pixels[15] == 255, "menu upload preserves PNG alpha");
	require(glGetError() == GL_NO_ERROR, "menu rendering has no GL errors");
}

static void test_menu_png()
{
	require(SDL_Init(SDL_INIT_VIDEO) == 0, "initialize SDL video for menu integration");
	Game_mode = 0;
	GameArg.SysWindow = 1;
	GameCfg.WindowMode = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	GameCfg.TexFilt = 0;
	require(gr_init(SM(320, 200)) == 0, "initialize menu renderer");
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	grs_bitmap *stock = gr_create_bitmap(320, 200);
	std::memset(stock->bm_data, 1, 320 * 200);
	unsigned char palette[768] = {};
	palette[3] = 63;
	std::memcpy(gr_palette, palette, sizeof(palette));
	char filename[] = "scores.pcx";
	require(pcx_write_bitmap(filename, stock, palette) == PCX_ERROR_NONE, "write stock menu fallback");
	gr_free_bitmap(stock);
	write_png_fixture("scores.png", 2, PNG_COLOR_TYPE_PALETTE, true, true);
	check_menu_png();
	const GLuint cached = nm_background.gltexture->handle;
	check_menu_png();
	require(nm_background.gltexture->handle == cached, "cached menu draw reuses uploaded texture");
	ogl_smash_texture_list_internal();
	check_menu_png();
	newmenu_free_background();
	GameCfg.TexFilt = 2;
	check_menu_png();
	GLint filter = 0;
	glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &filter);
	require(filter == GL_LINEAR_MIPMAP_LINEAR, "menu keeps requested texture filtering");
	newmenu_free_background();
	Game_mode = GM_MULTI;
	Netgame.AllowCustomModelsTextures = 0;
	nm_draw_background(10, 10, 200, 150);
	require(nm_background.gltexture && !nm_background.gltexture->is_png && nm_background.gltexture->w == 320, "replacement permission retains stock menu");
	newmenu_free_background();
	Game_mode = 0;
	check_menu_png();
	require(PHYSFS_delete("scores.png") != 0, "remove menu replacement fixture");
	ogl_smash_texture_list_internal();
	nm_draw_background(10, 10, 200, 150);
	require(nm_background.gltexture && !nm_background.gltexture->is_png && nm_background.gltexture->w == 320, "removed replacement restores stock dimensions after cache reset");
	newmenu_free_background();
	write_fixture("scores.png", { 137, 80, 78, 71, 13, 10, 26, 10 });
	nm_draw_background(10, 10, 200, 150);
	require(nm_background.gltexture && !nm_background.gltexture->is_png, "invalid replacement retains stock menu");
	newmenu_free_background();
	write_png_fixture("opaque.png", 4, PNG_COLOR_TYPE_PALETTE, false, false);
	grs_bitmap *wall = gr_create_bitmap(5, 3);
	wall->bm_flags = BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT;
	ogl_loadbmtexture_f(wall, 0, "opaque");
	require(wall->gltexture && wall->gltexture->is_png && wall->gltexture->format == GL_RGB, "opaque palette replacement uses RGB despite stock transparency flags");
	bytes pixels(wall->gltexture->tw * wall->gltexture->th * 4);
	glBindTexture(GL_TEXTURE_2D, wall->gltexture->handle);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	require(std::memcmp(pixels.data() + 4, png_colors[1], 3) == 0 && pixels[7] == 255, "opaque replacement upload keeps RGB stride and alpha");
#ifdef OGL_MERGE
	require(wall->gltexture_mask && wall->gltexture_mask->handle, "palette replacement generates supertransparency mask");
	glBindTexture(GL_TEXTURE_2D, wall->gltexture_mask->handle);
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	require(pixels[3] == 255 && pixels[11] == 0, "supertransparency mask follows decoded marker color");
#endif
	require(glGetError() == GL_NO_ERROR, "replacement and mask upload have no GL errors");
	gr_free_bitmap(wall);
	gr_close();
	SDL_Quit();
}
#endif
#endif

static void test_lives()
{
	Player_num = 0;
	Game_mode = 0;
	Newdemo_state = ND_STATE_NORMAL;
	cheats.enabled = 0;
	ConsoleObject = &Objects[0];
	ConsoleObject->type = OBJ_PLAYER;
	Players[0].shields = F1_0;
	Powerup_info[POW_EXTRA_LIFE].hit_sound = -1;
	Text_string[10] = const_cast<char *>("Extra life");
	object pickup = {};
	pickup.type = OBJ_POWERUP;
	pickup.id = POW_EXTRA_LIFE;
	Players[0].lives = 254;
	require(do_powerup(&pickup) != 0 && Players[0].lives == 255, "254 lives can consume one life");
	require(do_powerup(&pickup) == 0 && Players[0].lives == 255, "255 lives cannot wrap on pickup");
	Players[0].score = 49999;
	add_points_to_score(1);
	require(Players[0].lives == 255, "normal scoring cannot wrap lives");
	Players[0].lives = 254;
	Players[0].score = 49999;
	add_bonus_points_to_score(100001);
	require(Players[0].lives == 255, "multiple bonus lives saturate");
	add_bonus_points_to_score(50000);
	require(Players[0].lives == 255, "bonus scoring at cap");
}

#ifdef DXX_BUILD_DESCENT_II
static void set_short(bytes &data, size_t offset, int value)
{
	data[offset] = static_cast<unsigned char>(value);
	data[offset + 1] = static_cast<unsigned char>(static_cast<unsigned>(value) >> 8);
}

static void make_d1_monitor_fixture()
{
	// Registered D1 table layout: textures, tmap info, sound maps, vclips, effects
	const size_t vclips = 8 + 800 * 2 + 800 * 26 + 250 * 2;
	const size_t effects = vclips + 4 + 70 * 82;
	const size_t bitmap_start = effects + 4 + 60 * 130;
	bytes pig(bitmap_start);
	set_int(pig, 0, static_cast<int>(bitmap_start));
	set_int(pig, 4, 9);
	set_short(pig, 8 + 1 * 2, 1); // D1 texture 1 -> D2 texture 0: live monitor
	set_short(pig, 8 + 3 * 2, 2); // D1 texture 3 -> D2 texture 1: destroyed monitor
	set_short(pig, 8 + 8 * 2, 3); // D1 texture 8 -> D2 texture 2: ordinary wall
	set_int(pig, effects, 1);
	const size_t clip = effects + 4;
	set_int(pig, clip, F1_0 / 2);
	set_int(pig, clip + 4, 2);
	set_int(pig, clip + 8, F1_0 / 4);
	set_short(pig, clip + 18, 4);
	set_short(pig, clip + 20, 5);
	set_short(pig, clip + 90, 1); // changing wall texture
	set_short(pig, clip + 92, -1);
	set_int(pig, clip + 102, 3); // destroyed texture
	append_int(pig, 5);
	append_int(pig, 0);
	for (int i = 0; i < 5; ++i) {
		bytes header(17);
		header[0] = 'm';
		header[1] = '0' + i;
		header[9] = header[10] = 2;
		set_int(header, 13, i * 4);
		append(pig, header);
	}
	for (int i = 0; i < 5; ++i) append(pig, bytes(4, 10 + i));
	write_fixture("descent.pig", pig);
	bytes palette(9472);
	for (int i = 0; i < 256; ++i) {
		palette[i * 3] = i % 64;
		palette[i * 3 + 1] = i / 64;
	}
	std::memcpy(gr_palette, palette.data(), sizeof(gr_palette));
	write_fixture("palette.256", palette);
}

static void test_d1_monitors()
{
	const std::string original_write_dir = PHYSFS_getWriteDir();
	const std::string fixture_dir = original_write_dir + "/d1-monitor-fixtures";
	require(PHYSFS_mkdir("d1-monitor-fixtures") && PHYSFS_mount(fixture_dir.c_str(), nullptr, 0) && PHYSFS_setWriteDir(fixture_dir.c_str()), "isolate monitor PIG from exit-model fixtures");
	make_d1_monitor_fixture();
	hashtable_init(&AllBitmapsNames, MAX_BITMAP_FILES);
	Num_bitmap_files = 0;
	unsigned char stock_pixels[6][4];
	for (int i = 0; i < 6; ++i) {
		char name[13];
		std::snprintf(name, sizeof(name), "monitor#%d", i);
		std::memset(stock_pixels[i], 40 + i, sizeof(stock_pixels[i]));
		piggy_register_bitmap(&GameBitmaps[i], name, 1);
	}
	NumTextures = 3;
	Textures[0].index = 1;
	Textures[1].index = 3;
	Textures[2].index = 4;
	Num_effects = 1;
	std::memset(Effects, 0, sizeof(Effects));
	Effects[0].vc.num_frames = 2;
	Effects[0].vc.frame_time = F1_0 / 8;
	Effects[0].vc.frames[0].index = 1;
	Effects[0].vc.frames[1].index = 2;
	Effects[0].changing_wall_texture = 0;
	Effects[0].changing_object_texture = -1;
	Effects[0].dest_bm_num = 1;
	Effects[0].crit_clip = -1;
	const eclip original_effect = Effects[0];
	for (int pass = 0; pass < 2; ++pass) {
		for (int i = 0; i < 6; ++i) {
			gr_init_bitmap(&GameBitmaps[i], BM_LINEAR, 0, 0, 2, 2, 2, stock_pixels[i]);
			piggy_bitmap_set_file_state(i, 100 + i, BM_FLAG_TRANSPARENT);
		}
		load_d1_bitmap_replacements();
		for (int i : { 1, 2, 3 }) {
			require(GameBitmaps[i].bm_data == stock_pixels[i] && GameBitmaps[i].bm_data[0] == 40 + i, "generic D1 replacement preserves monitor frames and destroyed image");
			require(piggy_bitmap_get_offset(i) == 100 + i && piggy_bitmap_get_file_flags(i) == BM_FLAG_TRANSPARENT, "protected monitor paging state preserved");
		}
		require(GameBitmaps[4].bm_data[0] == 12 && GameBitmaps[5].bm_data == GameBitmaps[4].bm_data, "ordinary walls and unprotected animation clones still replaced");
		d1_in_d2_apply_effects(1);
		d1_in_d2_asset_stats stats = {};
		d1_in_d2_get_stats(&stats);
		require(stats.effect_frames_applied == 2 && stats.effect_frames_skipped == 0, "dedicated D1 effect loader still restores both frames");
		require(GameBitmaps[1].bm_data[0] == 13 && GameBitmaps[2].bm_data[0] == 14, "D1 animation frame pixels restored");
		require(GameBitmaps[3].bm_data == stock_pixels[3], "destroyed monitor remains protected after frame restoration");
		init_special_effects();
		Effects[0].frame_count = 0;
		FrameTime = F1_0 / 4 + 1;
		do_special_effects();
		require(Textures[0].index == 2 && GameBitmaps[Textures[0].index].bm_data[0] == 14, "monitor animation selects restored D1 frame");
		Effects[0].flags = EF_ONE_SHOT;
		Effects[0].segnum = 0;
		Effects[0].sidenum = 0;
		Segments[0].sides[0].tmap_num2 = 0x4000 | 2;
		do_special_effects();
		require(Segments[0].sides[0].tmap_num2 == (0x4000 | 1) && GameBitmaps[Textures[1].index].bm_data[0] == 43, "completed monitor effect selects protected destroyed image and retains orientation");
		d1_in_d2_apply_effects(0);
		require(std::memcmp(&Effects[0], &original_effect, sizeof(eclip)) == 0, "leaving D1 emulation restores original D2 effect");
		Textures[0].index = 1;
	}
	free_bitmap_replacements();
	free_d1_tmap_nums();
	hashtable_free(&AllBitmapsNames);
	for (int i = 0; i < 6; ++i) GameBitmaps[i].bm_data = nullptr;
	require(PHYSFS_delete("descent.pig") != 0 && PHYSFS_delete("palette.256") != 0, "remove D1 monitor fixtures");
	require(PHYSFS_setWriteDir(original_write_dir.c_str()) && PHYSFS_unmount(fixture_dir.c_str()), "restore fixture search path");
}

static bytes robot_record(int model, fix mass, fix drag)
{
	bytes record(480);
	set_int(record, 0, model);
	set_int(record, 136, mass);
	set_int(record, 140, drag);
	set_int(record, 476, 0xabcd);
	return record;
}

static void make_robot_fixtures()
{
	bytes ham;
	append_int(ham, 0x214d4148); // HAM!
	append_int(ham, 3);
	for (int i = 0; i < 5; ++i) append_int(ham, 0); // textures, sounds, clips, effects, walls
	append_int(ham, 66);
	for (int i = 0; i < 66; ++i) append(ham, robot_record(0, F1_0, F1_0 / 16));
	for (int i = 0; i < 3; ++i) append_int(ham, 0); // joints, weapons, powerups
	append_int(ham, 166);
	for (int i = 0; i < 166; ++i) append(ham, model_record(F1_0));
	append(ham, bytes(166 * 2));
	for (int i = 0; i < 166 * 2; ++i) append_int(ham, -1);
	append_int(ham, 0);   // gauges
	append_int(ham, 422); // base object bitmaps and their pointers
	append(ham, bytes(422 * 4));
	append(ham, bytes(132));                        // player ship
	for (int i = 0; i < 4; ++i) append_int(ham, 0); // cockpits, multiplayer bitmap, reactors, marker
	append(ham, bytes(2));                          // bitmap translation
	write_fixture("descent2.ham", ham);

	bytes vham;
	append_int(vham, 0x5848414d); // MAKE_SIG('X', 'H', 'A', 'M') stored little-endian
	append_int(vham, 1);
	append_int(vham, 0); // weapons
	append_int(vham, 1);
	append(vham, robot_record(166, 2 * F1_0, F1_0 / 8));
	append_int(vham, 0); // joints
	append_int(vham, 1);
	append(vham, model_record(2 * F1_0));
	append(vham, bytes(2));
	append_int(vham, -1);
	append_int(vham, -1);
	append_int(vham, 0); // bitmaps
	append_int(vham, 0); // bitmap pointers
	write_fixture("testv.ham", vham);

	bytes hxm = hxm_header();
	append_int(hxm, 1);
	append_int(hxm, 66);
	append(hxm, robot_record(166, 7 * F1_0, F1_0 / 4));
	append_int(hxm, 0); // joints
	append_int(hxm, 1);
	append_int(hxm, 166);
	append(hxm, model_record(4 * F1_0));
	append(hxm, bytes(2));
	append_int(hxm, -1);
	append_int(hxm, -1);
	append_int(hxm, 0);
	append_int(hxm, 0);
	write_fixture("custom.HXM", hxm);

	bytes exit;
	append(exit, model_record(80 * F1_0));
	append(exit, model_record(90 * F1_0));
	append(exit, bytes(4));
	write_fixture("exit.ham", exit);
	unsigned char pixels[64 * 64] = {};
	unsigned char palette[768] = {};
	grs_bitmap bitmap = {};
	gr_init_bitmap(&bitmap, BM_LINEAR, 0, 0, 64, 64, 64, pixels);
	for (const char *name : { "steel1.bbm", "rbot061.bbm", "rbot062.bbm", "rbot063.bbm" })
		require(iff_write_bitmap(const_cast<char *>(name), &bitmap, palette) == IFF_NO_ERROR, "write exit bitmap");
}

static void test_robot_reload()
{
	make_robot_fixtures();
	Mission mission = {};
	mission.filename = const_cast<char *>("testv");
	mission.enhanced = 2;
	mission.descent_version = 2;
	Current_mission = &mission;
	Num_bitmap_files = 1;
	require(load_mission_ham() != 0, "load synthetic mission HAM/V-HAM");
	require(N_polygon_models == 167 && N_robot_types == 67, "extra mission tables loaded");
	std::memset(Objects, 0, sizeof(object) * 3);
	Highest_object_index = 2;
	Objects[0].type = OBJ_PLAYER;
	Objects[0].id = 3;
	Objects[1].type = OBJ_POWERUP;
	Objects[1].id = POW_ENERGY;
	Objects[2].type = OBJ_ROBOT;
	Objects[2].id = 66;
	Objects[2].render_type = RT_POLYOBJ;
	Objects[2].movement_type = MT_PHYSICS;
	Objects[2].lifeleft = 123;
	Objects[2].shields = 45 * F1_0;
	Gamesave_num_org_robots = 1;
	Gamesave_num_players = 4;
	Game_mode = GM_MULTI | GM_NETWORK | GM_MULTI_COOP;
	PowerupsInMine[POW_ENERGY] = 5;
	MaxPowerupsAllowed[POW_ENERGY] = 8;
	const object player_before = Objects[0], powerup_before = Objects[1];

	for (int pass = 0; pass < 3; ++pass) {
		load_level_robots_file("plain.rl2");
		require(Objects[2].size == 2 * F1_0 && Objects[2].rtype.pobj_info.model_num == 166, "base mission robot properties restored");
		require(Objects[2].mtype.phys_info.mass == 2 * F1_0, "base robot mass restored");
		load_level_robots_file("custom.rl2");
		require(Objects[2].size == 4 * F1_0 && Objects[2].mtype.phys_info.mass == 7 * F1_0 && Objects[2].mtype.phys_info.drag == F1_0 / 4, "HXM changes applied to existing objects");
		load_level_robots_file("plain.rl2");
		require(Objects[2].size == 2 * F1_0, "next level removes previous HXM");
		require(load_exit_models() == 1, "load actual exit assets");
		require(Polygon_models[166].rad == 80 * F1_0 && Robot_replacements_loaded, "exit replaced robot model and marked tables dirty");
		reset_level_robots_file();
		require(N_polygon_models == 167 && Polygon_models[166].rad == 2 * F1_0, "HAM/V-HAM restored before initial verification");
		require(N_ObjBitmaps == 422, "exit bitmap cleanup preserves mission bitmap count");
	}
	require(PHYSFS_delete("steel1.bbm") != 0, "remove fixture to exercise failed exit load");
	require(load_exit_models() == 0 && Robot_replacements_loaded, "failed exit still marks discarded assets dirty");
	load_level_robots_file("plain.rl2");
	require(Objects[2].size == 2 * F1_0 && Polygon_models[166].model_data, "recover after failed exit load");
	require(Gamesave_num_org_robots == 1 && Gamesave_num_players == 4, "refresh preserves counts");
	require(std::memcmp(&player_before, &Objects[0], sizeof(object)) == 0 && std::memcmp(&powerup_before, &Objects[1], sizeof(object)) == 0, "refresh preserves players and powerups");
	require(PowerupsInMine[POW_ENERGY] == 5 && MaxPowerupsAllowed[POW_ENERGY] == 8, "refresh preserves network accounting");
	require(Objects[2].lifeleft == 123 && Objects[2].shields == 45 * F1_0, "refresh preserves robot runtime state");
	Current_mission = nullptr;
	free_polygon_models();
	bm_free_extra_objbitmaps();
}
#else
static void test_hx1()
{
	bytes data = hxm_header();
	append_int(data, 0); // robots
	append_int(data, 0); // joints
	append_int(data, 1);
	append_int(data, 0);
	append(data, model_record(7 * F1_0));
	append(data, bytes(2));
	append_int(data, 12);
	append_int(data, 13);
	append_int(data, 1); // trailing bitmap proves the reader stayed aligned
	append_int(data, 0);
	append(data, { 17, 0 });
	write_fixture("custom.hx1", data);
	char name[] = "custom.hx1";
	load_hxm(name);
	require(Polygon_models[0].rad == 7 * F1_0 && Polygon_models[0].model_data_size == 2, "HX1 disk model decoded independent of host ABI");
	require(Dying_modelnums[0] == 12 && Dead_modelnums[0] == 13 && ObjBitmaps[0].index == 17, "HX1 trailing records aligned");
	N_polygon_models = 1;
	free_polygon_models();
}
#endif

void test_autoselect();

int main(int argc, char **argv)
{
	(void) argc;
	mem_init();
	require(PHYSFS_init(argv[0]) != 0, "initialize PhysFS");
	require(PHYSFS_setWriteDir(".") != 0 && PHYSFS_mount(".", nullptr, 1) != 0, "mount isolated fixture directory");
	std::fprintf(stderr, "Testing model texture seek\n");
	test_seek();
#ifdef HAVE_LIBPNG
	std::fprintf(stderr, "Testing indexed PNG replacement decoding\n");
	test_png_decode();
#ifdef OGL
	if (argc > 1 && std::strcmp(argv[1], "--graphics") == 0) {
		std::fprintf(stderr, "Testing PNG menu rendering and cache reload\n");
		test_menu_png();
	}
#endif
#endif
	std::fprintf(stderr, "Testing life limits\n");
	test_lives();
	test_autoselect();
#ifdef DXX_BUILD_DESCENT_II
	std::fprintf(stderr, "Testing mission robot reload\n");
	test_robot_reload();
	std::fprintf(stderr, "Testing D1 monitor replacement and animation\n");
	test_d1_monitors();
#else
	std::fprintf(stderr, "Testing HX1 model loading\n");
	test_hx1();
#endif
	PHYSFS_deinit();
	std::puts("Upstream compatibility integration tests passed");
	return 0;
}
