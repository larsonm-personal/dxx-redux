#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>
#include <vector>
#include <zlib.h>
#include "input_demo_object_trace.h"
#include "input_demo_state_trace.h"
#include "render_gameplay_view.h"
#include <nlohmann/json.hpp>
#include <SDL.h>
#undef main
#ifdef HAVE_LIBPNG
#include <png.h>
#endif

extern "C" {
#include "args.h"
#include "ai.h"
#include "bm.h"
#include "boss_health_shared.h"
#include "config.h"
#include "collide.h"
#include "cntrlcen.h"
#include "console.h"
#include "dxxerror.h"
#include "endlevel.h"
#include "effects.h"
#include "fuelcen.h"
#include "fireball.h"
#include "game.h"
#include "gamefont.h"
#include "gameseg.h"
#include "fvi.h"
#include "gameseq.h"
#include "gamemine.h"
#include "gamesave.h"
#include "gauges.h"
#include "hash.h"
#include "iff.h"
#include "laser.h"
#include "maths.h"
#include "mission.h"
#include "morph.h"
#include "menu.h"
#include "multi.h"
#include "newdemo.h"
#include "newmenu.h"
#include "object.h"
#include "palette.h"
#include "physfsx.h"
#include "physics.h"
#include "piggy.h"
#include "polyobj.h"
#include "powerup.h"
#include "pcx.h"
#include "pngfile.h"
#include "robot.h"
#include "rle.h"
#include "render.h"
#include "screens.h"
#include "state.h"
#include "input_demo_hooks.h"
#include "input_demo_start.h"
#include "inferno.h"
#include "key.h"
#include "mouse.h"
#include "switch.h"
#include "text.h"
#include "titles.h"
#include "textures.h"
#include "timer.h"
#include "texmerge.h"
#include "u_mem.h"
#include "wall.h"
extern hashtable AllBitmapsNames, AllDigiSndNames;
void start_endlevel_flythrough(int n, object *obj, fix speed);
void gameseq_init_network_players(void);
void do_endlevel_flythrough(int n);
void apply_force_damage(object *obj, fix force, object *other);
void kill_stuck_objects(int wallnum);
void InitWeaponOrdering(void);
int object_create_egg(object *obj);
int drop_powerup(int type, int id, int num, vms_vector *velocity, vms_vector *position, int segment);
void collide_robot_and_player(object *robot, object *player, vms_vector *point);
extern point_seg Point_segs[];
extern point_seg *Point_segs_free_ptr;
extern ai_cloak_info Ai_cloak_info[];
extern int Overall_agitation;
extern int delayed_primary_autoselect_weapon_index;
extern int delayed_secondary_autoselect_weapon_index;
extern int ai_evaded;
extern int Num_awareness_events;
extern fix Boss_teleport_interval;
extern fix Boss_cloak_interval, Gate_interval;
extern fix64 Last_gate_time;
extern fix64 Boss_dying_start_time;
#ifdef DXX_BUILD_DESCENT_II
extern int Final_boss_is_dead;
#else
extern int Boss_dying_sound_playing;
#endif
extern int Num_boss_gate_segs;
extern short Boss_gate_segs[MAX_BOSS_TELEPORT_SEGS];
int gate_in_robot(int type, int segnum);
void collide_robot_and_weapon(object *robot, object *weapon, vms_vector *point);
extern int Hit_type, Hit_seg;
extern fvi_info Hit_data;
extern vms_vector Hit_pos;
void do_firing_stuff(object *obj, int visibility, vms_vector *direction);
void compute_vis_and_vec(object *obj, vms_vector *pos, ai_local *local, vms_vector *direction, int *visibility, robot_info *info, int *computed);
int find_homing_object_complete(vms_vector *curpos, object *tracker, int type1, int type2);
int find_homing_object(vms_vector *curpos, object *tracker);
int track_track_goal(int goal, object *tracker, fix *dot, unsigned frame, int original);
#ifdef OGL
#include "ogl_init.h"
extern grs_bitmap nm_background;
extern grs_bitmap nm_background1;
void nm_draw_background1(char *filename);
void newmenu_free_background(void);
void ogl_smash_texture_list_internal(void);
#endif
#ifdef DXX_BUILD_DESCENT_II
#include "gamepal.h"
#include "d1_in_d2/d1_in_d2.h"
#include "d1_in_d2/d1_in_d2_ai.h"
#include "d1_in_d2/d1_in_d2_levels.h"
#include "d1_in_d2/d1_in_d2_semantics.h"
#include "d1_in_d2/d1_save_translate.h"
#include "input_demo_start_shared.h"
#include "d1_in_d2/d1_in_d2_assets.h"
#include "d1_in_d2/d1_custom.h"
#include "d1_in_d2/d1_in_d2_bitmaps.h"
#include "d1_in_d2/d1_in_d2_presentation.h"
#include "d1_in_d2/d1_in_d2_cockpit.h"
extern int Num_sound_files;
int digi_unxlat_sound(int soundno);
extern ubyte *SoundBits;
void free_bitmap_replacements(void);
void free_d1_tmap_nums(void);
extern int Robot_replacements_loaded;
extern int Gamesave_num_players;
void maybe_delete_object(object *del_obj);
void verify_object(object *obj);
void blast_nearby_glass(object *obj, fix damage);
void do_physics_sim_rot(object *obj);
void set_next_fire_time(object *obj, ai_local *local, robot_info *info, int gun);
void ai_fire_laser_at_player(object *obj, vms_vector *fire_point, int gun, vms_vector *believed_player_pos);
const char *gamefont_curfontname(int gf);
void update_cockpits(void);
#else
// Native D1 keeps this shared queue layout private to ai.c; inspect the real
// queue here without adding a production API solely for this fixture
struct awareness_event {
	short segnum, type;
	vms_vector pos;
};
extern awareness_event Awareness_events[64];
void load_hxm(char *filename);
void load_custom_data(char *filename);
void custom_remove(void);
extern int GameBitmapOffset[];
void ai_move_relative_to_player(object *obj, ai_local *local, fix distance, vms_vector *direction, fix circle_distance, int evade_only);
void ai_fire_laser_at_player(object *obj, vms_vector *fire_point);
void ai_do_actual_firing_stuff(object *obj, ai_static *state, ai_local *local, robot_info *info,
                               vms_vector *direction, fix distance, vms_vector *gun, int visibility, int animates);
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

static void set_short(bytes &data, size_t offset, int value)
{
	data[offset] = static_cast<unsigned char>(value);
	data[offset + 1] = static_cast<unsigned char>(static_cast<unsigned>(value) >> 8);
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

static bytes d1_custom_definition_fixture()
{
	bytes data = hxm_header();
	append_int(data, 1);
	append_int(data, 0);
	bytes robot(480);
	set_int(robot, 136, 7 * F1_0);
	set_int(robot, 140, F1_0 / 4);
	robot[116] = 100; // native D1 falls back to weapon zero
	robot[117] = 9;
	robot[118] = 1;
	robot[123] = robot[126] = robot[127] = 8;
	set_int(robot, 184, F1_0); // D2-only second-weapon firing wait
	robot[279] = 8;
	for (int i = 281; i < 296; ++i) robot[i] = 8;
	robot[296] = 1; // body/gun animation uses replaced joint zero
	set_int(robot, 476, 0xabcd);
	append(data, robot);
	append_int(data, 1);
	append_int(data, 0);
	append(data, { 0, 0, 100, 0, 200, 0, 44, 1 });
	append_int(data, 1);
	append_int(data, 0);
	bytes model = model_record(7 * F1_0);
	set_int(model, 8, -1); // disk pointer must never become an owned allocation
	append(data, model);
	append(data, bytes(2));
	append_int(data, 1);
	append_int(data, 2);
	append_int(data, 1);
	append_int(data, 0);
	append(data, { 1, 0 });
	return data;
}

static bytes d1_custom_dpog_fixture()
{
	bytes data;
	append_int(data, 0x474f5044);
	append_int(data, 1);
	append_int(data, 1);
	append(data, { 1, 0 });
	bytes header(18);
	std::memcpy(header.data(), "ignored", 7);
	header[9] = header[10] = 2;
	append(data, header);
	append(data, { 9, 10, 11, 12 });
	return data;
}

static bytes d1_custom_dtx_fixture()
{
	bytes data;
	append_int(data, 1);
	append_int(data, 1);
	bytes bitmap(17), sound(20);
	std::memcpy(bitmap.data(), "SOURCE", 6);
	bitmap[9] = bitmap[10] = 2;
	bitmap[12] = 7;
	std::memcpy(sound.data(), "SOUND", 5);
	set_int(sound, 8, 3);
	set_int(sound, 12, 3);
	set_int(sound, 16, 4);
	append(data, bitmap);
	append(data, sound);
	append(data, { 21, 22, 23, 24, 31, 32, 33 });
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

static void test_pcx_short_header()
{
	for (int size : { 0, 3, 64, 127 }) {
		write_fixture("short.pcx", bytes(size, 0));
		grs_bitmap bitmap = {};
		ubyte palette[768] = {};
		char name[] = "short.pcx";
		require(pcx_read_bitmap(name, &bitmap, BM_LINEAR, palette) == PCX_ERROR_NO_HEADER && !bitmap.bm_data,
		        "truncated PCX header returns an error without terminating or allocating pixels");
	}
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
	// A 3D subview must preserve its surrounding frame and the caller's clip state
	gr_set_current_canvas(nullptr);
	glDisable(GL_SCISSOR_TEST);
	glClearColor(1, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	grs_canvas subview;
	gr_init_sub_canvas(&subview, &grd_curscreen->sc_canvas, 20, 30, 64, 40);
	gr_set_current_canvas(&subview);
	glEnable(GL_SCISSOR_TEST);
	glScissor(1, 2, 3, 4);
	g3_start_frame();
	g3_end_frame();
	GLint restored_scissor[4];
	glGetIntegerv(GL_SCISSOR_BOX, restored_scissor);
	require(glIsEnabled(GL_SCISSOR_TEST) && restored_scissor[0] == 1 && restored_scissor[1] == 2 && restored_scissor[2] == 3 && restored_scissor[3] == 4, "subview clear restores the caller's scissor state");
	ubyte inside[3], outside[3];
	glReadPixels(30, SHEIGHT - 40, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, inside);
	glReadPixels(120, SHEIGHT - 40, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, outside);
	require(inside[0] == 0 && inside[1] == 0 && inside[2] == 0 && outside[0] >= 250, "3D clear affects only the current subview in both engines");
	glDisable(GL_SCISSOR_TEST);
	gr_set_current_canvas(nullptr);
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

static void init_test_corridor(int cells = 2)
{
	Highest_segment_index = cells - 1;
	Num_segments = cells;
	Num_vertices = (cells + 1) * 4;
	Highest_vertex_index = Num_vertices - 1;
	const int xy[4][2] = { { -10, 10 }, { 10, 10 }, { 10, -10 }, { -10, -10 } };
	for (int plane = 0; plane <= cells; ++plane)
		for (int corner = 0; corner < 4; ++corner)
			Vertices[plane * 4 + corner] = { xy[corner][0] * F1_0, xy[corner][1] * F1_0, (plane * 20 - 10) * F1_0 };
	for (int cell = 0; cell < cells; ++cell) {
		segment &seg = Segments[cell];
		std::memset(&seg, 0, sizeof(seg));
		for (int vertex = 0; vertex < 8; ++vertex) seg.verts[vertex] = cell * 4 + vertex;
		for (int face = 0; face < 6; ++face) {
			seg.children[face] = -1;
			seg.sides[face].wall_num = -1;
			seg.sides[face].tmap_num = 1;
			for (auto &uv : seg.sides[face].uvls) {
				uv.u = F1_0 / 4;
				uv.v = 5 * F1_0 / 8;
			}
		}
	}
	for (int cell = 0; cell < cells; ++cell) {
		Segments[cell].children[4] = cell + 1 < cells ? cell + 1 : -1;
		Segments[cell].children[5] = cell > 0 ? cell - 1 : -1;
	}
	validate_segment_all();
	// D2 init_objects retains unused fields; each authored fixture starts fresh
	std::memset(Objects, 0, MAX_OBJECTS * sizeof(*Objects));
	init_objects();
	reset_objects(1);
	Num_walls = 0;
}

// Shared native/imported activation scenarios, through the real crossing entry
static void install_native_trigger(int index, short flags)
{
#ifdef DXX_BUILD_DESCENT_II
	v29_trigger source = {};
	source.flags = flags;
	source.num_links = 1;
	source.seg[0] = 0;
	source.side[0] = 4;
	source.value = 3 * F1_0;
	source.time = 9 * F1_0;
	require(d1_in_d2_decode_trigger(&Triggers[index], &source, 1), "decode native trigger");
#else
	trigger &source = Triggers[index];
	source = {};
	source.flags = flags;
	source.num_links = 1;
	source.seg[0] = 0;
	source.side[0] = 4;
	source.value = 3 * F1_0;
	source.time = 9 * F1_0;
#endif
}

static void test_object_state_trace()
{
	for (const char *path : { "object-state-test.jsonl", "object-state-test.jsonl.gz" }) {
		init_test_corridor();
		vms_vector point = {};
		const int slot = obj_create(OBJ_WEAPON, VULCAN_ID, 0, &point, &vmd_identity_matrix, F1_0, CT_WEAPON, MT_PHYSICS, RT_NONE);
		require(slot > 0, "create observed weapon");
		const int signature = Objects[slot].signature;
		char error[256] = {};
		require(input_demo_state_trace_start(path, "replay", "d1", "d1", 1, 0, "save_checkpoint", 5, error, sizeof(error)), "open object trace");
		const auto sim_calls = d_rand_get_call_count();
		const auto fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
		require(input_demo_object_trace_write(0, error, sizeof(error)), "initial object snapshot");
		require(input_demo_object_trace_write(1, error, sizeof(error)), "unchanged object delta");
		Objects[slot].orient.fvec.z = 123;
		Objects[slot].mtype.phys_info.rotthrust.y = 456;
		Objects[slot].ctype.laser_info.hitobj_list[MAX_OBJECTS - 1] = 1;
		require(input_demo_object_trace_write(2, error, sizeof(error)), "object field delta");
		obj_delete(slot);
		require(input_demo_object_trace_write(3, error, sizeof(error)), "object deletion delta");
		require(obj_create(OBJ_WEAPON, VULCAN_ID, 0, &point, &vmd_identity_matrix, F1_0, CT_WEAPON, MT_PHYSICS, RT_NONE) == slot, "reuse observed slot");
		require(input_demo_object_trace_write(4, error, sizeof(error)), "object reuse delta");
		input_demo_state_trace_stop();
		require(sim_calls == d_rand_get_call_count() && fx_calls == d_rand_get_stream_call_count(D_RNG_FX), "object observation consumes no RNG");
		gzFile file = gzopen(path, "rb");
		require(file != nullptr, "read plain or compressed object trace");
		std::string text;
		char block[4096];
		int size;
		while ((size = gzread(file, block, sizeof(block))) > 0) text.append(block, size);
		require(size == 0 && gzclose(file) == Z_OK, "object trace stream is complete");
		std::remove(path);
		std::istringstream lines(text);
		std::string line;
		std::vector<nlohmann::json> rows;
		while (std::getline(lines, line)) rows.push_back(nlohmann::json::parse(line));
		const auto key = std::to_string(slot);
		require(rows.size() == 6 && rows[1]["reset"].is_boolean() && rows[1]["reset"].get<bool>(), "initial object trace is complete");
		require(rows[1]["slots"][key]["signature"] == signature && rows[1]["slots"][key]["orient"][2][2] == F1_0, "trace retains source identity and orientation");
		require(rows[2]["slots"].empty(), "unchanged slots are losslessly elided");
		require(rows[3]["slots"][key]["orient"][2][2] == 123 && rows[3]["slots"][key]["physics"]["rotthrust"][1] == 456 &&
			rows[3]["slots"][key]["weapon"]["hitobj_list"][MAX_OBJECTS - 1] == 1, "observe fields absent from old hashes");
		require(rows[4]["slots"].contains(key) && rows[4]["slots"][key].is_null(), "deleted slots emit tombstones");
		require(rows[5]["slots"][key]["signature"] != signature && rows[5]["slots"][key]["weapon"]["hitobj_list"][MAX_OBJECTS - 1] == 0, "reused slot replaces its complete state");
		require(rows[5]["allocator"]["free_obj_list"].size() == MAX_OBJECTS && rows[5]["rng"].size() == 2, "allocator and both actual RNG streams are observed");
	}
}

static short native_trigger_flags(int index)
{
#ifdef DXX_BUILD_DESCENT_II
	short flags;
	require(d1_in_d2_trigger_source_flags(&Triggers[index], &flags), "retain native trigger identity");
	return flags;
#else
	return Triggers[index].flags;
#endif
}

static nlohmann::json snapshot_native_triggers()
{
	auto result = nlohmann::json::array();
	for (int i = 0; i < Num_triggers; ++i) {
		auto links = nlohmann::json::array();
		for (int j = 0; j < Triggers[i].num_links; ++j) links.push_back({ Triggers[i].seg[j], Triggers[i].side[j] });
		result.push_back({ { "flags", native_trigger_flags(i) }, { "value", Triggers[i].value }, { "time", Triggers[i].time }, { "links", links } });
	}
	return result;
}

static void test_native_triggers()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	mission.descent_version = 1;
	Current_mission = &mission;
#endif
	Game_mode = 0;
	Newdemo_state = ND_STATE_NORMAL;
	GameArg.SndNoSound = 1;
	Player_num = 0;
	Players[0].objnum = 0;
	const short choices[] = { TRIGGER_SHIELD_DAMAGE, TRIGGER_ENERGY_DRAIN, TRIGGER_ILLUSION_ON,
		                      TRIGGER_ILLUSION_OFF, TRIGGER_ON, TRIGGER_ONE_SHOT };
	for (int mask = 0; mask < 64; ++mask) {
		init_test_corridor();
		Num_walls = 2;
		Num_triggers = 2;
		for (int w = 0; w < 2; ++w) {
			Walls[w] = {};
			Walls[w].segnum = w;
			Walls[w].sidenum = w ? 5 : 4;
			Walls[w].type = WALL_ILLUSION;
			Walls[w].trigger = w;
			Segments[w].sides[w ? 5 : 4].wall_num = w;
		}
		short flags = 0;
		for (int bit = 0; bit < 6; ++bit)
			if (mask & (1 << bit)) flags |= choices[bit];
		install_native_trigger(0, flags);
		install_native_trigger(1, TRIGGER_ON);
		Players[0].shields = Players[0].energy = 100 * F1_0;
		for (int repeat = 1; repeat <= 3; ++repeat) {
			check_trigger(&Segments[0], 4, 0, 0);
			require(Players[0].shields == (100 - ((flags & TRIGGER_SHIELD_DAMAGE) ? 3 * repeat : 0)) * F1_0,
			        "native crossing applies shield action repeatedly even when source ON is clear");
			require(Players[0].energy == (100 - ((flags & TRIGGER_ENERGY_DRAIN) ? 3 * repeat : 0)) * F1_0,
			        "compound native crossing also applies energy action");
			require((Walls[0].flags & WALL_ILLUSION_OFF) == ((flags & TRIGGER_ILLUSION_OFF) ? WALL_ILLUSION_OFF : 0) &&
			            Walls[0].flags == Walls[1].flags,
			        "native illusion actions affect both sides in source order");
			require(native_trigger_flags(0) == (flags & ((flags & TRIGGER_ONE_SHOT) ? ~TRIGGER_ON : ~0)),
			        "one-shot crossing retains actions and clears only native ON");
			require(native_trigger_flags(1) == ((flags & TRIGGER_ONE_SHOT) ? 0 : TRIGGER_ON),
			        "one-shot crossing clears paired trigger state");
		}
		const fix shields = Players[0].shields, energy = Players[0].energy;
		require(check_trigger_sub(0, 1, 0) == 0, "remote activation handles linked actions");
		require(Players[0].shields == shields && Players[0].energy == energy, "remote activation does not damage local ship");
		Newdemo_state = ND_STATE_PLAYBACK;
		check_trigger(&Segments[0], 4, 0, 0);
		require(Players[0].shields == shields && Players[0].energy == energy, "playback crossings do not replay native damage");
		Newdemo_state = ND_STATE_NORMAL;
	}
#ifdef DXX_BUILD_DESCENT_II
	install_native_trigger(0, TRIGGER_SHIELD_DAMAGE | TRIGGER_ILLUSION_OFF);
	Segments[0].children[4] = -1;
	const fix shields = Players[0].shields;
	require(check_trigger_sub(0, 0, 0) == 1 && Players[0].shields == shields, "malformed illusion pair rejects before mutation");
	Current_mission = nullptr;
#endif
	Num_triggers = Num_walls = 0;
}

#ifdef DXX_BUILD_DESCENT_II
static void test_native_trigger_serialization()
{
	for (const int version : { 29, 30, 31 }) {
		PHYSFS_file *file = PHYSFS_openWrite("triggers.bin");
		require(file != nullptr, "open trigger round-trip output");
		for (int flags = 0; flags < 1024; ++flags) {
			install_native_trigger(0, flags);
			trigger_write(&Triggers[0], version, file);
		}
		PHYSFS_close(file);
		file = PHYSFS_openRead("triggers.bin");
		for (int flags = 0; flags < 1024; ++flags) {
			if (version == 31)
				trigger_read(&Triggers[0], file);
			else {
				v29_trigger source = {};
				if (version == 29) v29_trigger_read(&source, file);
				else {
					v30_trigger old;
					v30_trigger_read(&old, file);
					source.flags = old.flags;
					source.num_links = old.num_links;
					source.value = old.value;
					source.time = old.time;
					std::memcpy(source.seg, old.seg, sizeof(source.seg));
					std::memcpy(source.side, old.side, sizeof(source.side));
				}
				require(d1_in_d2_decode_trigger(&Triggers[0], &source, 1), "decode written native flags");
			}
			require(native_trigger_flags(0) == flags && Triggers[0].value == 3 * F1_0 &&
			            Triggers[0].time == 9 * F1_0 && Triggers[0].num_links == 1 && Triggers[0].side[0] == 4,
			        "all native flag combinations and linked state survive trigger serialization");
		}
		PHYSFS_close(file);
	}
	PHYSFS_delete("triggers.bin");
	v29_trigger source = {};
	source.flags = TRIGGER_CONTROL_DOORS;
	const trigger before = Triggers[0];
	for (const int count : { -1, 11, 256, 257, 32767 }) {
		source.num_links = count;
		require(!d1_in_d2_decode_trigger(&Triggers[0], &source, 1) &&
		            std::memcmp(&before, &Triggers[0], sizeof(before)) == 0,
		        "reject wide invalid link counts without truncation or publication");
	}
	source.num_links = 0;
	source.flags = TRIGGER_UNLOCK_DOORS;
	require(!d1_in_d2_decode_trigger(&Triggers[0], &source, 1), "D2 extension is not a native D1 action");
	require(d1_in_d2_decode_trigger(&Triggers[0], &source, 0) && Triggers[0].type == TT_UNLOCK_DOOR,
	        "explicit old D2 adapter retains D2 extension action");
	short flags;
	require(!d1_in_d2_trigger_source_flags(&Triggers[0], &flags), "old D2 conversion is not marked native");
	Triggers[0].flags |= TF_DISABLED;
	Num_triggers = 1;
	require(check_trigger_sub(0, 0, 0) == 1, "ordinary D2 disabled gate is unchanged");
	Num_triggers = 0;
}
#endif

// The same movement must reach the exit when a slow frame crosses its whole cell
static void test_endlevel_flythrough()
{
	const fix saved_frame_time = FrameTime;
	for (const bool bend : { false, true })
		for (const int actor : { 0, 1 })
			for (const fix frame_time : { F1_0 / 60, F1_0 / 2, F1_0 }) {
				init_test_corridor();
				const int exit = bend ? 2 : 1;
				if (bend) {
					// Third cell turns down Y through the second cell's side 0
					Highest_segment_index = 2;
					Num_segments = 3;
					Highest_vertex_index = 15;
					Num_vertices = 16;
					Vertices[12] = { 10 * F1_0, -30 * F1_0, 10 * F1_0 };
					Vertices[13] = { -10 * F1_0, -30 * F1_0, 10 * F1_0 };
					Vertices[14] = { 10 * F1_0, -30 * F1_0, 30 * F1_0 };
					Vertices[15] = { -10 * F1_0, -30 * F1_0, 30 * F1_0 };
					Segments[2] = Segments[1];
					const int vertices[] = { 7, 6, 12, 13, 11, 10, 14, 15 };
					for (int v = 0; v < 8; ++v) Segments[2].verts[v] = vertices[v];
					for (auto &child : Segments[2].children) child = -1;
					Segments[1].children[0] = 2;
					Segments[2].children[2] = 1;
					Segments[2].children[0] = -2;
					validate_segment_all();
					init_objects();
				} else
					Segments[1].children[4] = -2;
				object &ship = Objects[0];
				ship.pos = { 0, 0, 0 };
				ship.last_pos = { 0, 0, -F1_0 };
				ship.orient = vmd_identity_matrix;
				start_endlevel_flythrough(actor, &ship, actor ? 125 * F1_0 / 2 : 50 * F1_0);
				FrameTime = frame_time;
				do_endlevel_flythrough(actor);
				for (int frame = 0; frame < 120 && ship.segnum != exit; ++frame)
					do_endlevel_flythrough(actor);
				require(ship.segnum == exit, "player/camera flyout follows bends and reaches the exit even when a frame ends outside the mine");
			}
	FrameTime = saved_frame_time;
}

// Compare actual target acquisition and retention against the native D1 engine
static void test_robot_initialization()
{
	const auto old_time = GameTime64;
	GameTime64 = 7 * F1_0;
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		for (int behavior : { 0, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, -1 }) {
			for (const int attack : { 0, 1 }) {
				object &robot = Objects[0];
				std::memset(&robot, 0, sizeof(robot));
				robot.type = OBJ_ROBOT;
				robot.ctype.ai_info.behavior = 0x86; // valid D2 behavior, outside native D1's range
				robot.ctype.ai_info.hide_segment = 4;
				robot.ctype.ai_info.hide_index = 19;
				robot.mtype.phys_info.velocity.x = F1_0;
				robot_info &info = Robot_info[0];
				std::memset(&info, 0, sizeof(info));
				info.attack_type = attack;
				info.cloak_type = RI_CLOAKED_ALWAYS;
				const auto rng_before = d_rand_get_call_count();
				init_ai_object(0, behavior, 9);
				const ai_static &state = robot.ctype.ai_info;
				const ai_local &local = Ai_local_info[0];
				const int effective = behavior == 0 ? 0x81 : behavior;
				int expected_mode = AIM_STILL;
				if (effective == 0x81) expected_mode = AIM_CHASE_OBJECT;
				if (effective == 0x82) expected_mode = 5; // native D1 hide / D2 behind
				if (effective == 0x83) expected_mode = AIM_RUN_FROM_OBJECT;
				if (effective == 0x84 && profile == 1) expected_mode = AIM_FOLLOW_PATH;
				int expected_behavior = effective == -1 ? (profile == 1 ? 0x81 : 0x86) : effective;
				if (attack && profile == 2) {
					expected_behavior = AIB_NORMAL;
					expected_mode = AIM_CHASE_OBJECT;
				}
				require(state.behavior == expected_behavior && local.mode == expected_mode,
				        "robot initialization interprets source behavior codes and attack policy in the active game");
				const bool path = effective == 0x83 || effective == 0x84 || effective == 0x85 || (profile == 1 && effective == 0x82);
				require(state.hide_segment == (path ? 9 : 4) && state.hide_index == (path ? -1 : 19) && local.goal_segment == (path ? 9 : 0),
				        "robot initialization installs native path goals for hide, follow, station and run-from behavior");
				require(state.GOAL_STATE == AIS_SRCH && state.CURRENT_STATE == AIS_REST && state.CLOAKED && state.REMOTE_OWNER == -1 && state.danger_laser_num == -1,
				        "owned behavior initialization retains shared cloak, state and ownership setup");
				require(local.time_player_seen == GameTime64 && local.next_misc_sound_time == GameTime64 && !robot.mtype.phys_info.velocity.x &&
				            (robot.mtype.phys_info.flags & (PF_BOUNCE | PF_TURNROLL)) == (PF_BOUNCE | PF_TURNROLL) && rng_before == d_rand_get_call_count(),
				        "initialization preserves shared clocks and physics without consuming RNG");
			}
		}
#ifdef DXX_BUILD_DESCENT_II
		Robot_info[0].attack_type = 0;
		Robot_info[0].companion = 1;
		init_ai_object(0, AIB_NORMAL, 9);
		require(Ai_local_info[0].mode == AIM_GOTO_PLAYER, "optional companion retains D2 initialization in either content profile");
		Robot_info[0].companion = 0;
#endif
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
	GameTime64 = old_time;
}

static void test_d1_follow_path_frame()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 1, 2, 1 };
#else
	const int profiles[] = { 1 };
#endif
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		const fix old_frame = FrameTime;
		FrameTime = F1_0 / 64;
		Game_mode = 0;
		GameArg.SndNoSound = 1;
		init_test_corridor();
		init_objects();
		Player_num = 0;
		Players[0].objnum = 0;
		Players[0].flags = 0;
		ConsoleObject = &Objects[0];
		ConsoleObject->orient = vmd_identity_matrix;
		ConsoleObject->pos = { 0, 0, 0 };
		robot_info &info = Robot_info[0];
		std::memset(&info, 0, sizeof(info));
		info.always_0xabcd = 0xabcd;
#ifdef DXX_BUILD_DESCENT_II
		info.weapon_type2 = -1;
#endif
		for (int i = 0; i < NDL; ++i) {
			info.turn_time[i] = F1_0;
			info.max_speed[i] = 10 * F1_0;
			info.field_of_view[i] = -F1_0;
		}
		N_robot_types = 1;
		vms_vector origin = { 0, 0, 20 * F1_0 };
		const int objnum = obj_create(OBJ_ROBOT, 0, 1, &origin, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
		require(objnum > 0, "create a real robot for the native follow-path frame");
		object &robot = Objects[objnum];
		init_ai_object(objnum, 0x84, 0);
		robot.ctype.ai_info.hide_index = 0;
		robot.ctype.ai_info.path_length = 2;
		robot.ctype.ai_info.cur_path_index = 0;
		robot.ctype.ai_info.PATH_DIR = 1;
		Point_segs[0].segnum = 1;
		Point_segs[0].point = origin;
		Point_segs[1].segnum = 0;
		Point_segs[1].point = ConsoleObject->pos;
		Point_segs_free_ptr = Point_segs + 2;
		Ai_local_info[objnum].previous_visibility = 1;
		Ai_local_info[objnum].next_fire = 10 * F1_0;
		do_ai_frame(&robot);
		if (profile == 1) {
			require(robot.ctype.ai_info.behavior == 0x84 && Ai_local_info[objnum].mode == AIM_FOLLOW_PATH && robot.ctype.ai_info.cur_path_index == 1,
			        "native follow-path behavior reaches the path follower without entering D2 sniper logic");
			require(robot.mtype.phys_info.velocity.z < 0, "native follow-path frame moves toward the next segment");
		}
#ifdef DXX_BUILD_DESCENT_II
		else
			require(robot.ctype.ai_info.behavior == AIB_SNIPE && Ai_local_info[objnum].mode >= AIM_SNIPE_ATTACK && Ai_local_info[objnum].mode <= AIM_SNIPE_WAIT,
			        "D2 content retains its sniper phase for the same serialized behavior code");
#endif
		// Exercise the factory's public creation path, including its final mode assignment
		init_test_corridor(8);
		Point_segs_free_ptr = Point_segs;
		N_robot_types = 11;
		Polygon_models[0].rad = F1_0;
		Robot_info[10] = info;
		for (const int id : { 0, 10 }) {
#ifdef DXX_BUILD_DESCENT_II
			Robot_info[id].behavior = id == 10 ? AIB_RUN_FROM : AIB_NORMAL;
#endif
			object *spawned = create_morph_robot(&Segments[1], &origin, id);
			require(spawned && spawned->ctype.ai_info.path_length > 1, "factory spawn constructs a usable exit path");
			const int expected_mode = id == 10 ? AIM_RUN_FROM_OBJECT : profile == 1 ? AIM_FOLLOW_PATH : AIM_CHASE_OBJECT;
			require(Ai_local_info[spawned - Objects].mode == expected_mode,
			        "native factory robots retain their exit path mode while D2 retains behavior-based startup");
		}
		FrameTime = old_frame;
		Point_segs_free_ptr = Point_segs;
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_robot_perception()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	// Imported D1 is covered through the complete frame comparison runner
	// These direct engine-helper checks retain native D1 and ordinary D2 baselines
	const int profiles[] = { 2 };
#else
	const int profiles[] = { 1 };
#endif
	const auto old_time = GameTime64;
	const int old_agitation = Overall_agitation;
	GameTime64 = 20 * F1_0;
	Overall_agitation = 0;
	Game_mode = 0;
	GameArg.SndNoSound = 1;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		init_test_corridor();
		Player_num = 0;
		Players[0].objnum = 0;
		Players[0].flags = 0;
		ConsoleObject = &Objects[0];
		ConsoleObject->pos = { 0, 0, 20 * F1_0 };
		ConsoleObject->size = F1_0;
		obj_relink(0, 1);
		Believed_player_pos = ConsoleObject->pos;
		vms_vector origin = {}, direction = { 0, 0, F1_0 };
		const int objnum = obj_create(OBJ_ROBOT, 0, 0, &origin, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
		require(objnum > 0, "create perception scene robot");
		object &robot = Objects[objnum];
		robot_info &info = Robot_info[0];
		std::memset(&info, 0, sizeof(info));
		info.turn_time[Difficulty_level] = F1_0;
		info.field_of_view[Difficulty_level] = F1_0 / 2;
		ai_local &local = Ai_local_info[objnum];
		init_ai_object(objnum, AIB_STILL, 0);
		const auto sim_before = d_rand_get_call_count();
		require(player_is_visible_from_object(&robot, &origin, F1_0 / 2, &direction) == 2,
		        "perception sees the player across real connected segments");
		require(Hit_type == (profile == 1 ? HIT_OBJECT : HIT_NONE) && (profile != 1 || Hit_data.hit_object == 0),
		        "D1 sight rays hit the player while D2 rays ignore objects");
		vms_vector blocker_pos = { 0, 0, 5 * F1_0 };
		const int blocker = obj_create(OBJ_PLAYER, 1, 0, &blocker_pos, &vmd_identity_matrix, 2 * F1_0, CT_NONE, MT_NONE, RT_NONE);
		require(blocker > 0, "create another ship occluding the local player");
		require(player_is_visible_from_object(&robot, &origin, F1_0 / 2, &direction) == (profile == 1 ? 0 : 2),
		        "D1 perception respects intervening objects while ordinary D2 retains wall-only sight");
		obj_delete(blocker);
		Segments[0].children[4] = -1;
		require(player_is_visible_from_object(&robot, &origin, F1_0 / 2, &direction) == 0 && Hit_type == HIT_WALL,
		        "both profiles reject sight through an actual solid wall");
		Segments[0].children[4] = 1;
		robot.ctype.ai_info.flags[4] = 1; // D1 hiding submode / D2 gun-segment flag
		player_is_visible_from_object(&robot, &origin, F1_0 / 2, &direction);
		require(robot.ctype.ai_info.flags[4] == (profile == 1 ? 1 : 0),
		        "D1 sight preserves hiding submode rather than applying D2 gun-segment flags");
		robot.ctype.ai_info.flags[4] = 0;
		vms_vector gun = { 0, 0, 12 * F1_0 };
		player_is_visible_from_object(&robot, &gun, F1_0 / 2, &direction);
		require(robot.ctype.ai_info.flags[4] == (profile == 1 ? 0 : 1),
		        "a gun in another segment only sets D2's gun-segment flag");
		gun = { 100 * F1_0, 0, 0 };
		player_is_visible_from_object(&robot, &gun, F1_0 / 2, &direction);
		require(!std::memcmp(&gun, &robot.pos, sizeof(gun)), "an invalid sight origin recovers to the robot position");
		require(sim_before == d_rand_get_call_count(), "ray visibility and origin recovery consume no simulation RNG");

		robot.orient.fvec.z = -F1_0;
		local = {};
		local.player_awareness_type = PA_NEARBY_ROBOT_FIRED;
		int computed = 0, visibility = -1;
		compute_vis_and_vec(&robot, &origin, &local, &direction, &visibility, &info, &computed);
		require(visibility == (profile == 1 ? 1 : 2) && local.previous_visibility == 1 && computed == 1,
		        "D1 preserves outside-field-of-view visibility while D2 awareness promotes the result");
#ifdef DXX_BUILD_DESCENT_II
		info.companion = 1;
		computed = 0;
		compute_vis_and_vec(&robot, &origin, &local, &direction, &visibility, &info, &computed);
		require(visibility == 2, "companion perception retains D2 awareness promotion inside D1 content");
		info.companion = 0;
#endif
		robot.orient = vmd_identity_matrix;
		robot.ctype.ai_info.GOAL_STATE = robot.ctype.ai_info.CURRENT_STATE = AIS_REST;
		local = {};
		local.time_player_seen = GameTime64;
		local.next_misc_sound_time = GameTime64 + 10 * F1_0;
		computed = 0;
		compute_vis_and_vec(&robot, &origin, &local, &direction, &visibility, &info, &computed);
		require(visibility == 2 && local.previous_visibility == 2 && local.time_player_seen == GameTime64 &&
		            robot.ctype.ai_info.GOAL_STATE == AIS_FIRE && robot.ctype.ai_info.CURRENT_STATE == AIS_FIRE,
		        "first unobstructed sight wakes a resting robot and updates its perception clocks");
		const auto cached_local = local;
		const auto cached_direction = direction;
		const auto cached_rng = d_rand_get_call_count();
		const auto cached_fx = d_rand_get_stream_call_count(D_RNG_FX);
		Believed_player_pos.x += 3 * F1_0;
		compute_vis_and_vec(&robot, &origin, &local, &direction, &visibility, &info, &computed);
		require(!std::memcmp(&local, &cached_local, sizeof(local)) && !std::memcmp(&direction, &cached_direction, sizeof(direction)) &&
		            cached_rng == d_rand_get_call_count() && cached_fx == d_rand_get_stream_call_count(D_RNG_FX),
		        "a cached perception result performs no second state update or RNG draw");
		Believed_player_pos = ConsoleObject->pos;

		Players[0].flags = PLAYER_FLAGS_CLOAKED;
		auto &cloak = Ai_cloak_info[objnum]; // fixture indices are below either engine's cloak-slot count
		cloak.last_time = GameTime64;
		cloak.last_position = { 0, 0, 5 * F1_0 };
		local = {};
		local.previous_visibility = 1;
		local.next_fire = 2 * F1_0;
		computed = 0;
		const auto cloak_fx = d_rand_get_stream_call_count(D_RNG_FX);
		compute_vis_and_vec(&robot, &origin, &local, &direction, &visibility, &info, &computed);
		require(local.previous_visibility == 1 && cloak.last_time == GameTime64,
		        "cloaked perception retains previous visibility and recent belief position");
		require(d_rand_get_stream_call_count(D_RNG_FX) == cloak_fx + (profile == 2 ? 1 : 0),
		        "D1 cloaked chatter uses only its primary firing clock");
		cloak.last_time = GameTime64 - 3 * F1_0;
		const auto drift_before = cloak.last_position;
		local.next_misc_sound_time = GameTime64 + 10 * F1_0;
		computed = 0;
		const auto drift_rng = d_rand_get_call_count();
		compute_vis_and_vec(&robot, &origin, &local, &direction, &visibility, &info, &computed);
		require(cloak.last_time == GameTime64 && std::memcmp(&cloak.last_position, &drift_before, sizeof(drift_before)) && d_rand_get_call_count() == drift_rng + 3,
		        "stale cloak belief drifts once using the original three simulation draws");
		Players[0].flags = 0;
		Player_exploded = 1;
		local = {};
		local.next_misc_sound_time = GameTime64 + 10 * F1_0;
		computed = 0;
		const auto exploded_fx = d_rand_get_stream_call_count(D_RNG_FX);
		compute_vis_and_vec(&robot, &origin, &local, &direction, &visibility, &info, &computed);
		require(d_rand_get_stream_call_count(D_RNG_FX) == exploded_fx + (profile == 2 ? 1 : 0),
		        "native D1 suppresses new sight chatter after the player has exploded");
		Player_exploded = 0;
	}
	GameTime64 = old_time;
	Overall_agitation = old_agitation;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_robot_scheduling()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	struct schedule_case {
		int distance;
		fix elapsed;
		bool alerted, station, d1_skip, d2_skip;
	};
	const schedule_case cases[] = {
		{ 100, 0, false, false, false, false },
		{ 101, F1_0 / 2, false, false, true, false },
		{ 101, F1_0 / 2 + 1, false, false, false, false },
		{ 151, F1_0, false, false, true, false },
		{ 151, F1_0 + 1, false, false, false, false },
		{ 251, 2 * F1_0, false, false, true, false },
		{ 251, 2 * F1_0 + 1, false, false, false, false },
		{ 251, 0, true, false, false, false },
		{ 200, 0, false, true, false, false },
		{ 251, 2 * F1_0, false, true, true, true },
		{ 251, 2 * F1_0 + 1, false, true, false, true }
	};
	const fix old_frame = FrameTime;
	FrameTime = F1_0 / 64;
	Game_mode = 0;
	GameArg.SndNoSound = 1;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		for (const auto &sample : cases) {
			init_test_corridor();
			for (int i = 0; i < Num_vertices; ++i) vm_vec_scale(&Vertices[i], 20 * F1_0);
			validate_segment_all();
			Player_num = 0;
			Players[0].objnum = 0;
			Players[0].flags = 0;
			ConsoleObject = &Objects[0];
			ConsoleObject->pos = { 0, 0, sample.distance * F1_0 };
			if (sample.distance > 200) obj_relink(0, 1);
			robot_info &info = Robot_info[0];
			std::memset(&info, 0, sizeof(info));
			info.always_0xabcd = 0xabcd;
			info.turn_time[Difficulty_level] = F1_0;
			info.max_speed[Difficulty_level] = 10 * F1_0;
#ifdef DXX_BUILD_DESCENT_II
			info.weapon_type2 = -1;
#endif
			N_robot_types = 1;
			vms_vector origin = {};
			const int objnum = obj_create(OBJ_ROBOT, 0, 0, &origin, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
			require(objnum > 0, "create a scheduling scene robot");
			object &robot = Objects[objnum];
			init_ai_object(objnum, sample.station ? AIB_STATION : AIB_STILL, 1);
			ai_local &local = Ai_local_info[objnum];
			local.previous_visibility = 1;
			local.time_since_processed = sample.elapsed - FrameTime;
			local.next_fire = 10 * F1_0;
			local.next_misc_sound_time = GameTime64 + 100 * F1_0;
			local.player_awareness_type = sample.alerted ? PA_WEAPON_ROBOT_COLLISION - 1 : 0;
			local.player_awareness_time = 100 * F1_0;
			if (sample.station) {
				local.mode = AIM_FOLLOW_PATH;
				robot.ctype.ai_info.hide_index = 0;
				robot.ctype.ai_info.path_length = 2;
				robot.ctype.ai_info.PATH_DIR = 1;
				Point_segs[0].point = origin;
				Point_segs[0].segnum = 0;
				Point_segs[1].point = { 0, 0, 400 * F1_0 };
				Point_segs[1].segnum = 1;
				Point_segs_free_ptr = Point_segs + 2;
			}
#ifdef DXX_BUILD_DESCENT_II
			const ai_local before_schedule = local;
			const auto schedule_rng = d_rand_get_call_count();
			const auto schedule_fx = d_rand_get_stream_call_count(D_RNG_FX);
			info.companion = 1;
			require(!d1_in_d2_ai_run_frame(&robot) &&
			            !std::memcmp(&local, &before_schedule, sizeof(local)) && schedule_rng == d_rand_get_call_count() && schedule_fx == d_rand_get_stream_call_count(D_RNG_FX),
			        "native D1 frame declines companion actors without changing clocks or RNG");
			info.companion = 0;
#endif
			do_ai_frame(&robot);
			const bool skip = profile == 1 ? sample.d1_skip : sample.d2_skip;
			require(local.time_since_processed == (skip ? sample.elapsed : -((objnum & 3) * FrameTime) / 2),
			        "actual robot frames preserve native distance/time thresholds, alert bypass and station-return scheduling");
		}
	}
	Point_segs_free_ptr = Point_segs;
	FrameTime = old_frame;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static object &create_frame_robot(int behavior, fix distance, int scale = 20)
{
	init_test_corridor();
	for (int i = 0; i < Num_vertices; ++i) vm_vec_scale(&Vertices[i], scale * F1_0);
	validate_segment_all();
	Point_segs_free_ptr = Point_segs;
	Game_mode = 0;
	GameArg.SndNoSound = 1;
	Player_num = 0;
	Player_is_dead = Player_exploded = 0;
	Players[0].objnum = 0;
	Players[0].flags = 0;
	Overall_agitation = 0;
	std::memset(&cheats, 0, sizeof(cheats));
	ConsoleObject = &Objects[0];
	ConsoleObject->pos = { 0, 0, distance };
	ConsoleObject->orient = vmd_identity_matrix;
	if (distance > 10 * scale * F1_0) obj_relink(0, 1);
	robot_info &info = Robot_info[0];
	std::memset(&info, 0, sizeof(info));
	info.always_0xabcd = 0xabcd;
	info.turn_time[Difficulty_level] = F1_0;
	info.max_speed[Difficulty_level] = 10 * F1_0;
#ifdef DXX_BUILD_DESCENT_II
	info.weapon_type2 = -1;
#endif
	N_robot_types = 1;
	vms_vector origin = {};
	const int objnum = obj_create(OBJ_ROBOT, 0, 0, &origin, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
	require(objnum > 0, "create a robot frame scene");
	init_ai_object(objnum, behavior, 1);
	Ai_local_info[objnum].next_fire = 10 * F1_0;
	Ai_local_info[objnum].next_misc_sound_time = GameTime64 + 100 * F1_0;
	Ai_cloak_info[objnum].last_position = ConsoleObject->pos;
	Ai_cloak_info[objnum].last_time = GameTime64;
	return Objects[objnum];
}

static void test_robot_path_creation()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		object &robot = create_frame_robot(AIB_NORMAL, 20 * F1_0, 1);
		ai_static &ai = robot.ctype.ai_info;
		ai_local &local = Ai_local_info[&robot - Objects];
		Segments[0].sides[4].wall_num = 0;
		Walls[0] = {};
		Walls[0].type = WALL_DOOR;
		Walls[0].keys = KEY_NONE;
		Walls[0].state = WALL_DOOR_CLOSED;
		Num_walls = 1;
		for (const int behavior : { AIB_NORMAL, AIB_RUN_FROM, 0x84 }) {
			ai.behavior = behavior;
			require(ai_door_is_openable(&robot, &Segments[0], 4) ==
			            (behavior == AIB_RUN_FROM || (profile == 2 && behavior == 0x84)),
			        "path door permissions interpret native follow versus D2 sniper");
		}
		ai.behavior = AIB_RUN_FROM;
		Walls[0].keys = KEY_BLUE;
		Players[0].flags = PLAYER_FLAGS_BLUE_KEY;
		require(ai_door_is_openable(&robot, &Segments[0], 4) == (profile == 2),
		        "native fleeing robots do not borrow player keys");
		require(ai_door_is_openable(ConsoleObject, &Segments[0], 4) == 1,
		        "ordinary player path query can plan through a door");
		Walls[0].keys = KEY_NONE;
		Walls[0].flags = WALL_DOOR_LOCKED;
		require(!ai_door_is_openable(&robot, &Segments[0], 4), "locked doors block fleeing robots");
		Walls[0].flags = 0;
		const robot_info old_brain = Robot_info[ROBOT_BRAIN];
		Robot_info[ROBOT_BRAIN] = Robot_info[0];
		robot.id = ROBOT_BRAIN;
		ai.behavior = AIB_NORMAL;
		require(ai_door_is_openable(&robot, &Segments[0], 4), "native brain can open an unlocked keyless door");
		robot.id = 0;
		Robot_info[ROBOT_BRAIN] = old_brain;
		Segments[0].sides[4].wall_num = -1;
		Num_walls = 0;
		ai.behavior = AIB_NORMAL;
		short count = 0;
		d_srand(17);
		const unsigned before = d_rand_get_call_count();
		require(create_path_points(&robot, 0, 1, Point_segs, &count, 10, 1, 1, -1) == 0,
		        "construct actual corridor path with safety points");
		require(count == (profile == 1 ? 3 : 2), "native safety points retain the doorway point");
		require(Point_segs[0].segnum == 0 && Point_segs[count - 1].segnum == 1,
		        "path reconstruction preserves endpoints");
		if (profile == 1) {
			require(d_rand_get_call_count() - before == 6, "native path shuffles once without per-node RNG refresh");
			require(Point_segs[1].segnum == 1 && Point_segs[1].point.z == 10 * F1_0 + 10 * F1_0 / 16,
			        "native safety point keeps its original coordinate and destination segment");
		} else
			require(d_rand_get_call_count() - before > 6, "D2 path keeps per-node randomization");
		Point_segs_free_ptr = Point_segs;
		ai.flags[4] = 17;
#ifdef DXX_BUILD_DESCENT_II
		Believed_player_seg = 0;
#endif
		create_path_to_player(&robot, 10, 0);
		require(local.goal_segment == (profile == 1 ? 1 : 0), "native pursuit uses actual player segment");
		require(ai.flags[4] == (profile == 1 ? 0 : 17), "native pursuit initializes hide submode only for native actor");
		require(local.mode == AIM_FOLLOW_PATH && local.player_awareness_type == 0 && ai.PATH_DIR == 1,
		        "path request commits robot following state");
		Point_segs_free_ptr = Point_segs;
		ai.flags[4] = 17;
		ai.hide_segment = 1;
		create_path_to_station(&robot, 10);
		require(ai.flags[4] == 17 && ai.path_length == (profile == 1 ? 3 : 2),
		        "station path retains submode and native safety points");
		Point_segs_free_ptr = Point_segs;
		local.previous_visibility = 0;
		create_n_segment_path(&robot, 5, -1);
		require(ai.flags[4] == (profile == 1 ? -1 : 17) && ai.path_length == 2,
		        "random request clears native hide submode and retains reachable partial path");
		ai.hide_segment = -1;
		const short old_length = ai.path_length;
		local.mode = AIM_STILL;
		create_path_to_station(&robot, 10);
		require(ai.path_length == old_length && local.mode == AIM_STILL, "absent station destination does not publish a new path");
		// Extend the actual corridor to test depth, avoidance and disconnected routes
		for (int plane = 3; plane < 5; ++plane)
			for (int corner = 0; corner < 4; ++corner) {
				Vertices[plane * 4 + corner] = Vertices[corner];
				Vertices[plane * 4 + corner].z = (plane * 20 - 10) * F1_0;
			}
		for (int cell = 2; cell < 4; ++cell) {
			Segments[cell] = Segments[1];
			for (int vertex = 0; vertex < 8; ++vertex) Segments[cell].verts[vertex] = cell * 4 + vertex;
			Segments[cell].children[5] = cell - 1;
			Segments[cell].children[4] = cell == 3 ? -1 : cell + 1;
		}
		Segments[1].children[4] = 2;
		Num_segments = 4;
		Highest_segment_index = 3;
		Num_vertices = 20;
		Highest_vertex_index = 19;
		validate_segment_all();
		for (const int depth : { 1, 2, 3, 10 }) {
			const unsigned calls = d_rand_get_call_count();
			require(create_path_points(&robot, 0, 3, Point_segs, &count, depth, 0, 0, -1) == 0 &&
			            count == (depth < 3 ? depth + 1 : 4),
			        "path depth returns the native reachable endpoint");
			require(Point_segs[count - 1].segnum == count - 1 && d_rand_get_call_count() == calls,
			        "unrandomized path has ordered source segments and no RNG draws");
		}
		require(create_path_points(&robot, 0, 3, Point_segs, &count, 10, 0, 0, 1) == 0 && count == 1,
		        "avoided connecting segment returns a start-only partial path");
		Segments[1].children[4] = -1;
		Segments[2].children[5] = -1;
		require(create_path_points(&robot, 0, 3, Point_segs, &count, 10, 0, 0, -1) == 0 && count == 2,
		        "disconnected target returns the last reachable segment");
		Point_segs_free_ptr = Point_segs + MAX_POINT_SEGS - 10;
		ai.flags[4] = 17;
		local.mode = AIM_STILL;
		create_path_to_player(&robot, 10, 0);
		require(Point_segs_free_ptr == Point_segs && ai.path_length == 0 && ai.flags[4] == 17 && local.mode == AIM_STILL,
		        "near-full pool resets paths before destination request changes mode or submode");
#ifdef DXX_BUILD_DESCENT_II
		Robot_info[0].companion = 1;
		Point_segs_free_ptr = Point_segs;
		ai.flags[4] = 17;
		const object unchanged_robot = robot;
		const ai_local unchanged_local = local;
		const unsigned untouched_rng = d_rand_get_call_count();
		count = 123;
		Point_segs[0].segnum = 42;
		require(d1_in_d2_ai_create_path_points(&robot, 0, 1, Point_segs, &count, 10, 1, 1, -1) == D1_AI_PATH_NOT_APPLICABLE &&
		            !d1_in_d2_ai_create_path_to_player(&robot, 10, 0) &&
		            !d1_in_d2_ai_create_path_to_station(&robot, 10) &&
		            !d1_in_d2_ai_create_random_path(&robot, 5, -1) &&
		            d1_in_d2_ai_door_is_openable(&robot, &Segments[0], 4) == D1_AI_NOT_APPLICABLE &&
		            d1_in_d2_ai_door_is_openable(nullptr, &Segments[0], 4) == D1_AI_NOT_APPLICABLE,
		        "native path operations decline companion and null door queries");
		require(count == 123 && Point_segs[0].segnum == 42 && Point_segs_free_ptr == Point_segs &&
		            !std::memcmp(&robot, &unchanged_robot, sizeof(robot)) && !std::memcmp(&local, &unchanged_local, sizeof(local)) &&
		            d_rand_get_call_count() == untouched_rng,
		        "inactive path operations leave robot, path output and RNG untouched");
		create_path_to_player(&robot, 10, 0);
		require(local.goal_segment == 0 && ai.flags[4] == 17, "companion keeps D2 destination and flags in either profile");
		d_srand(17);
		const unsigned companion_before = d_rand_get_call_count();
		require(create_path_points(&robot, 0, 1, Point_segs, &count, 10, 1, 1, -1) == 0 && count == 2,
		        "companion keeps engine path point processing");
		require(d_rand_get_call_count() - companion_before > 6, "companion keeps engine path RNG in D1 world");
		Robot_info[0].companion = 0;
		if (profile == 1) {
			Point_segs_free_ptr = Point_segs + MAX_POINT_SEGS;
			require(create_path_points(&robot, 0, 1, Point_segs_free_ptr, &count, 10, 0, 0, -1) == -1 &&
			            count == 0 && Point_segs_free_ptr == Point_segs,
			        "native pool exhaustion resets safely without falling through to a D2 route");
		}
#endif
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void follow_test_robot(object &robot, int visibility)
{
#ifdef DXX_BUILD_DESCENT_II
	ai_follow_path(&robot, visibility, visibility, nullptr);
#else
	ai_follow_path(&robot, visibility);
#endif
}

static void install_test_robot_path(object &robot)
{
	ai_static &ai = robot.ctype.ai_info;
	ai.hide_index = 0;
	ai.path_length = 2;
	ai.cur_path_index = 1;
	ai.PATH_DIR = 1;
	Point_segs_free_ptr = Point_segs + 2;
	for (int i = 0; i < 2; ++i) {
		Point_segs[i].segnum = i;
		compute_segment_center(&Point_segs[i].point, &Segments[i]);
	}
}

static void test_robot_path_following()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const fix old_frame = FrameTime;
	const auto old_tick = d_tick_count;
	FrameTime = F1_0 / 64;
	d_tick_count = 100;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		{
			object &hider = create_frame_robot(0x82, 20 * F1_0, 1);
			install_test_robot_path(hider);
			hider.ctype.ai_info.flags[4] = 1;
			hider.mtype.phys_info.velocity = { F1_0, 2 * F1_0, 3 * F1_0 };
			const vms_vector velocity = hider.mtype.phys_info.velocity;
			follow_test_robot(hider, 2);
			require((std::memcmp(&velocity, &hider.mtype.phys_info.velocity, sizeof(velocity)) == 0) == (profile == 1),
			        "native hiding submode leaves path motion untouched");
		}
		{
			object &follower = create_frame_robot(0x84, 20 * F1_0, 1);
			install_test_robot_path(follower);
			follow_test_robot(follower, 0);
			require(follower.mtype.phys_info.velocity.z == (profile == 1 ? 10 * F1_0 : 15 * F1_0),
			        "native patrol speed does not inherit D2 sniper acceleration");
		}
		{
			object &follower = create_frame_robot(0x84, 20 * F1_0, 1);
			install_test_robot_path(follower);
			follower.pos = Point_segs[1].point;
			obj_relink(&follower - Objects, 1);
			Ai_local_info[&follower - Objects].mode = AIM_FOLLOW_PATH;
#ifdef DXX_BUILD_DESCENT_II
			Believed_player_seg = 1;
#endif
			const unsigned rng = d_rand_get_call_count();
			follow_test_robot(follower, 2);
			if (profile == 1)
				require(follower.ctype.ai_info.cur_path_index == 0 && follower.ctype.ai_info.path_length == 2 &&
				            Point_segs_free_ptr == Point_segs + 2 && d_rand_get_call_count() == rng,
				        "native patrol wraps to reachable opposite endpoint without replanning");
			else
				require(follower.ctype.ai_info.path_length == 1 && d_rand_get_call_count() > rng,
				        "D2 follow mode retains its player-path replan");
		}
		{
			object &hider = create_frame_robot(0x82, 20 * F1_0, 1);
			install_test_robot_path(hider);
			hider.ctype.ai_info.flags[4] = 0;
			hider.pos = Point_segs[1].point;
			obj_relink(&hider - Objects, 1);
			follow_test_robot(hider, 2);
			require(Ai_local_info[&hider - Objects].mode == (profile == 1 ? AIM_STILL : 5),
			        "native hide arrival stops instead of running D2 behind behavior");
		}
		{
			object &fleeing = create_frame_robot(AIB_RUN_FROM, 20 * F1_0, 1);
			install_test_robot_path(fleeing);
			fleeing.pos.z = -5 * F1_0;
			fleeing.ctype.ai_info.cur_path_index = 0;
			ai_local &local = Ai_local_info[&fleeing - Objects];
			local.player_awareness_type = 0;
			local.player_awareness_time = 0;
			const unsigned rng = d_rand_get_call_count();
			follow_test_robot(fleeing, 2);
			require(d_rand_get_call_count() - rng == (profile == 1 ? 6 : 0),
			        "native fleeing checks player obstruction on every follow call");
			require(local.player_awareness_type == (profile == 1 ? 1 : 0) &&
			            local.player_awareness_time == (profile == 1 ? F1_0 : 0),
			        "native fleeing visibility refreshes awareness in the follow phase");
		}
		{
			object &fleeing = create_frame_robot(AIB_RUN_FROM, 20 * F1_0, 1);
			install_test_robot_path(fleeing);
			Ai_local_info[&fleeing - Objects].player_awareness_type = 0;
			fleeing.mtype.phys_info.velocity = { 0, 0, 8 * F1_0 };
			follow_test_robot(fleeing, 0);
			require(fleeing.mtype.phys_info.velocity.z == fixmul(8 * F1_0, F1_0 - FrameTime / 2),
			        "unseen unaware fleeing robot slows without replanning");
		}
		if (profile == 1) {
			object &hider = create_frame_robot(0x82, 20 * F1_0, 1);
			hider.ctype.ai_info.hide_index = -1;
			hider.ctype.ai_info.path_length = 0;
			Ai_local_info[&hider - Objects].goal_segment = 1;
			const unsigned rng = d_rand_get_call_count();
			follow_test_robot(hider, 2);
			require(hider.ctype.ai_info.path_length == 2 && hider.ctype.ai_info.flags[4] == 1 &&
			            d_rand_get_call_count() == rng && hider.mtype.phys_info.velocity.z == 0,
			        "missing native hide path uses its assigned goal and hiding submode without randomization");
		}
		{
			object &patrol = create_frame_robot(0x84, 20 * F1_0, 1);
			install_test_robot_path(patrol);
			Ai_local_info[&patrol - Objects].mode = AIM_STILL;
			patrol.pos = Point_segs[1].point;
			obj_relink(&patrol - Objects, 1);
			Segments[0].sides[4].wall_num = 0;
			Segments[1].sides[5].wall_num = 1;
			Num_walls = 2;
			Walls[0] = {};
			Walls[1] = {};
			Walls[0].type = Walls[1].type = WALL_CLOSED;
			follow_test_robot(patrol, 0);
			require(patrol.ctype.ai_info.PATH_DIR == -1 && patrol.ctype.ai_info.cur_path_index == (profile == 1 ? 2 : 1),
			        "blocked opposite endpoint preserves native reversal cursor timing");
			patrol.ctype.ai_info.behavior = AIB_STATION;
			patrol.ctype.ai_info.hide_segment = 0;
			patrol.ctype.ai_info.cur_path_index = 1;
			patrol.ctype.ai_info.PATH_DIR = 1;
			follow_test_robot(patrol, 0);
			require(Ai_local_info[&patrol - Objects].mode == AIM_STILL && patrol.ctype.ai_info.path_length == 1,
			        "unreachable station replan stops at the reachable segment");
		}
#ifdef DXX_BUILD_DESCENT_II
		{
			object &companion = create_frame_robot(AIB_NORMAL, 20 * F1_0, 1);
			Robot_info[0].companion = 1;
			install_test_robot_path(companion);
			const object saved = companion;
			const ai_local saved_local = Ai_local_info[&companion - Objects];
			const unsigned rng = d_rand_get_call_count();
			require(!d1_in_d2_ai_follow_path(&companion, 2) && !d1_in_d2_ai_run_frame(&companion) &&
			            !std::memcmp(&saved, &companion, sizeof(saved)) &&
			            !std::memcmp(&saved_local, &Ai_local_info[&companion - Objects], sizeof(saved_local)) &&
			            rng == d_rand_get_call_count() &&
			            Point_segs_free_ptr == Point_segs + 2,
			        "native follow and frame operations leave companion state and RNG untouched");
			Robot_info[0].companion = 0;
		}
		if (profile == 1) {
			object &robot = create_frame_robot(AIB_NORMAL, 20 * F1_0, 1);
			install_test_robot_path(robot);
			robot.ctype.ai_info.hide_index = MAX_POINT_SEGS - 1;
			follow_test_robot(robot, 0);
			require(robot.ctype.ai_info.path_length == 0 && Point_segs_free_ptr == Point_segs &&
			            Ai_local_info[&robot - Objects].mode == AIM_STILL,
			        "invalid native shared path is reset before dereference");
			install_test_robot_path(robot);
			robot.ctype.ai_info.behavior = AIB_STATION;
			robot.ctype.ai_info.hide_segment = 1;
			robot.pos = Point_segs[1].point;
			obj_relink(&robot - Objects, 1);
			Point_segs_free_ptr = Point_segs + MAX_POINT_SEGS;
			follow_test_robot(robot, 0);
			require(robot.ctype.ai_info.path_length == 0 && Point_segs_free_ptr == Point_segs &&
			            Ai_local_info[&robot - Objects].mode == AIM_STILL,
			        "native station follow handles allocation reset before checking its destination");
		}
#endif
	}
	FrameTime = old_frame;
	d_tick_count = old_tick;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_hide_and_brain_frame()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const fix old_frame = FrameTime;
	FrameTime = F1_0 / 64;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		{
			object &hider = create_frame_robot(0x82, 100 * F1_0);
			install_test_robot_path(hider);
			hider.ctype.ai_info.flags[4] = 1;
			Robot_info[0].field_of_view[Difficulty_level] = -F1_0;
			ai_local &local = Ai_local_info[&hider - Objects];
			local.player_awareness_type = PA_NEARBY_ROBOT_FIRED;
			local.player_awareness_time = 10 * F1_0;
			local.previous_visibility = 2;
			local.time_since_processed = 10 * F1_0;
			Believed_player_pos = ConsoleObject->pos;
			do_ai_frame(&hider);
			const vms_vector zero = {};
			require((std::memcmp(&zero, &hider.mtype.phys_info.velocity, sizeof(zero)) == 0) == (profile == 1),
			        "actual native hide frame stays hidden instead of moving behind the player");
		}
		{
			object &brain = create_frame_robot(AIB_NORMAL, 20 * F1_0, 1);
			const robot_info saved_brain = Robot_info[ROBOT_BRAIN];
			Robot_info[ROBOT_BRAIN] = Robot_info[0];
			N_robot_types = ROBOT_BRAIN + 1;
			brain.id = ROBOT_BRAIN;
			ai_local &local = Ai_local_info[&brain - Objects];
			local.mode = AIM_CHASE_OBJECT;
			local.player_awareness_type = PA_WEAPON_ROBOT_COLLISION;
			local.player_awareness_time = 10 * F1_0;
			local.time_since_processed = 10 * F1_0;
#ifdef DXX_BUILD_DESCENT_II
			local.next_action_time = 10 * F1_0;
			Believed_player_seg = 1;
#endif
			Believed_player_pos = ConsoleObject->pos;
			Segments[0].sides[4].wall_num = 0;
			Walls[0] = {};
			Walls[0].type = WALL_DOOR;
			Walls[0].state = WALL_DOOR_CLOSED;
			Walls[0].keys = KEY_NONE;
			Walls[0].clip_num = 0;
			Num_walls = 1;
			const auto saved_flags = WallAnims[0].flags;
			WallAnims[0].flags = WCF_HIDDEN;
			do_ai_frame(&brain);
			require((local.mode == AIM_OPEN_DOOR && brain.ctype.ai_info.GOALSIDE == 4) == (profile == 1),
			        "native brain chooses the hidden door through the actual frame");
			WallAnims[0].flags = saved_flags;
			Robot_info[ROBOT_BRAIN] = saved_brain;
		}
	}
	FrameTime = old_frame;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

// Compare these normalized full-frame results between the two executables with
// helpers/test_d1_ai_frames.ps1; no assets or private AI entry points are used
static void write_robot_frame_trace(const char *filename)
{
	using nlohmann::json;
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	mission.descent_version = 1;
	Current_mission = &mission;
#endif
	const auto vector = [](const vms_vector &v) { return json::array({ v.x, v.y, v.z }); };
	json cases = json::array();
	Difficulty_level = 2;
	FrameTime = F1_0 / 64;
	for (int behavior = 0x80; behavior <= 0x85; ++behavior)
		for (int state = AIS_NONE; state <= AIS_RECO; ++state)
			for (int sight = 0; sight <= 2; ++sight)
				for (int variant = 0; variant < 10; ++variant) {
					GameTime64 = 20 * F1_0;
					d_tick_count = 100;
					const fix distance = (variant == 4 ? 10 : variant == 8 ? 300
					                                                       : 100) *
					                     F1_0;
					object &robot = create_frame_robot(behavior, distance, variant == 8 ? 20 : 5);
					ai_static &aip = robot.ctype.ai_info;
					ai_local &local = Ai_local_info[&robot - Objects];
					robot_info &info = Robot_info[0];
					info.n_guns = 2;
					info.gun_points[0] = { F1_0 / 2, 0, F1_0 };
					info.gun_points[1] = { -F1_0 / 2, 0, F1_0 };
					info.field_of_view[Difficulty_level] = F1_0 / 2;
					info.circle_distance[Difficulty_level] = 20 * F1_0;
					info.firing_wait[Difficulty_level] = F1_0;
					info.rapidfire_count[Difficulty_level] = 3;
					info.attack_type = variant == 4;
					info.evade_speed[Difficulty_level] = 2;
					info.strength = robot.shields = F1_0;
					robot.render_type = RT_POLYOBJ;
					Polygon_models[0] = {};
					Polygon_models[0].n_models = 1;
					ConsoleObject->size = F1_0;
					if (sight == 1) {
						robot.orient.fvec = { F1_0, 0, 0 };
						robot.orient.rvec = { 0, 0, -F1_0 };
					}
					if (variant == 6) Weapon_info[0].homing_flag = 1;
					if (variant == 5 || variant == 9) {
						vms_vector position = { 0, 0, 10 * F1_0 };
						const int laser = obj_create(OBJ_WEAPON, 0, 0, &position, &vmd_identity_matrix, F1_0 / 8, CT_WEAPON, MT_PHYSICS, RT_NONE);
						require(laser > 0, "create a danger laser in the full-frame scene");
						Objects[laser].mtype.phys_info.velocity = { 0, 0, -F1_0 };
						aip.danger_laser_num = laser;
						aip.danger_laser_signature = Objects[laser].signature + (variant == 9);
					}
					if (sight == 0) {
						Num_walls = 2;
						Walls[0] = Walls[1] = {};
						Walls[0].type = Walls[1].type = WALL_CLOSED;
						Segments[0].sides[4].wall_num = 0;
						Segments[1].sides[5].wall_num = 1;
					}
					N_weapon_types = PROXIMITY_ID + 1;
					for (int weapon = 0; weapon < N_weapon_types; ++weapon) {
						Weapon_info[weapon] = {};
						Weapon_info[weapon].render_type = WEAPON_RENDER_NONE;
						Weapon_info[weapon].flash_sound = Weapon_info[weapon].flash_vclip = -1;
						Weapon_info[weapon].mass = F1_0;
						Weapon_info[weapon].lifetime = 10 * F1_0;
						Weapon_info[weapon].speed[Difficulty_level] = 40 * F1_0;
#ifdef DXX_BUILD_DESCENT_II
						Weapon_info[weapon].speedvar = 128;
						Weapon_info[weapon].children = -1;
#endif
					}
					if (behavior == 0x82 || behavior == 0x84 || behavior == AIB_RUN_FROM)
						install_test_robot_path(robot);
					aip.CURRENT_STATE = aip.GOAL_STATE = state;
					aip.CURRENT_GUN = 1;
					aip.SKIP_AI_COUNT = variant == 3;
					local.player_awareness_type = PA_NEARBY_ROBOT_FIRED;
					local.player_awareness_time = 10 * F1_0;
					local.time_since_processed = (variant == 8 ? 0 : 10 * F1_0);
					local.previous_visibility = sight;
					local.time_player_seen = GameTime64 - 10 * F1_0;
					local.next_fire = variant == 1 || variant == 4 ? F1_0 : 0;
					if (variant == 2 || variant == 7) Players[0].flags = PLAYER_FLAGS_CLOAKED;
					if (variant == 7) Ai_cloak_info[&robot - Objects].last_time = GameTime64 - 3 * F1_0;
					Num_awareness_events = 0;
					ai_evaded = 0;
					d_srand(0x1234);
					d_srand_fx(0x5678);
					const auto sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
					json frames = json::array();
					for (int frame = 0; frame < 4; ++frame) {
						do_ai_frame(&robot);
						json path = json::array(), shots = json::array();
						for (int i = 0; i < aip.path_length; ++i) {
							require(aip.hide_index >= 0 && aip.hide_index + i < Point_segs_free_ptr - Point_segs, "frame path stays in shared storage");
							const point_seg &p = Point_segs[aip.hide_index + i];
							path.push_back({ { "segment", p.segnum }, { "point", vector(p.point) } });
						}
						for (int i = 0; i <= Highest_object_index; ++i)
							if (Objects[i].type == OBJ_WEAPON) {
								const object &shot = Objects[i];
								shots.push_back({ { "id", shot.id }, { "parent", shot.ctype.laser_info.parent_num }, { "position", vector(shot.pos) }, { "velocity", vector(shot.mtype.phys_info.velocity) }, { "life", shot.lifeleft } });
							}
						frames.push_back({ { "mode", local.mode }, { "behavior", aip.behavior }, { "state", aip.CURRENT_STATE }, { "goal", aip.GOAL_STATE }, { "gun", aip.CURRENT_GUN }, { "skip", aip.SKIP_AI_COUNT }, { "submode", aip.flags[4] }, { "position", vector(robot.pos) }, { "segment", robot.segnum }, { "velocity", vector(robot.mtype.phys_info.velocity) }, { "forward", vector(robot.orient.fvec) }, { "right", vector(robot.orient.rvec) }, { "up", vector(robot.orient.uvec) }, { "rotvel", vector(robot.mtype.phys_info.rotvel) }, { "next_fire", local.next_fire }, { "burst", local.rapidfire_count }, { "danger_laser", aip.danger_laser_num }, { "cloak_belief", vector(Ai_cloak_info[&robot - Objects].last_position) }, { "cloak_time", Ai_cloak_info[&robot - Objects].last_time }, { "previous_visibility", local.previous_visibility }, { "last_seen", local.time_player_seen }, { "awareness", local.player_awareness_type }, { "awareness_time", local.player_awareness_time }, { "processed_time", local.time_since_processed }, { "path_index", aip.cur_path_index }, { "path_direction", aip.PATH_DIR }, { "path", path }, { "shots", shots }, { "events", Num_awareness_events }, { "agitation", Overall_agitation }, { "sim_draws", d_rand_get_call_count() - sim }, { "fx_draws", d_rand_get_stream_call_count(D_RNG_FX) - fx } });
						GameTime64 += FrameTime;
						++d_tick_count;
					}
					cases.push_back({ { "behavior", behavior }, { "state", state }, { "sight", sight }, { "variant", variant }, { "frames", frames } });
				}
	FILE *out = std::fopen(filename, "wb");
	require(out != nullptr, "open native frame trace");
	const std::string text = json({ { "cases", cases } }).dump(2) + "\n";
	require(std::fwrite(text.data(), 1, text.size(), out) == text.size(), "write native frame trace");
	require(std::fclose(out) == 0, "close native frame trace");
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_robot_chase_timeout()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const fix saved_frame = FrameTime;
	FrameTime = 0;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		object &robot = create_frame_robot(AIB_NORMAL, 20 * F1_0, 1);
		ai_local &local = Ai_local_info[&robot - Objects];
		local.mode = AIM_CHASE_OBJECT;
		local.player_awareness_type = PA_PLAYER_COLLISION;
		local.player_awareness_time = 10 * F1_0;
		local.time_since_processed = 10 * F1_0;
		local.time_player_seen = GameTime64 - 8 * F1_0 - 1;
		local.previous_visibility = 0;
		robot.ctype.ai_info.CURRENT_STATE = robot.ctype.ai_info.GOAL_STATE = AIS_LOCK;
		Segments[0].sides[4].wall_num = 0;
		Segments[1].sides[5].wall_num = 1;
		Num_walls = 2;
		std::memset(Walls, 0, sizeof(Walls[0]) * 2);
		Walls[0].type = Walls[1].type = WALL_CLOSED;
		do_ai_frame(&robot);
		require(local.mode == (profile == 1 ? AIM_FOLLOW_PATH : AIM_CHASE_OBJECT),
		        "native chase timeout requests a pursuit path even near an occluded player");
		require(profile != 1 || (robot.ctype.ai_info.path_length == 1 && local.time_player_seen == GameTime64),
		        "native timeout commits the reachable partial path and pursuit clock");
	}
	FrameTime = saved_frame;
	Point_segs_free_ptr = Point_segs;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_fleeing_robot_path_state()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const fix old_frame = FrameTime;
	const int old_difficulty = Difficulty_level;
	FrameTime = 0;
	Difficulty_level = 2;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		object &robot = create_frame_robot(AIB_RUN_FROM, 100 * F1_0);
		Robot_info[0].field_of_view[Difficulty_level] = -F1_0;
		weapon_info &mine = Weapon_info[PROXIMITY_ID];
		mine = {};
		mine.render_type = WEAPON_RENDER_NONE;
		mine.flash_sound = mine.flash_vclip = -1;
		mine.mass = F1_0;
		mine.lifetime = 20 * F1_0;
		mine.strength[Difficulty_level] = 10 * F1_0;
		N_weapon_types = PROXIMITY_ID + 1;
#ifdef DXX_BUILD_DESCENT_II
		mine.children = -1;
		mine.speedvar = 128;
#endif
		create_n_segment_path(&robot, 5, -1);
		ai_local &local = Ai_local_info[&robot - Objects];
		local.mode = AIM_RUN_FROM_OBJECT;
		local.next_fire = 0;
		local.player_awareness_type = PA_WEAPON_ROBOT_COLLISION;
		local.player_awareness_time = 3 * F1_0;
		local.previous_visibility = 2;
		local.time_since_processed = F1_0;
		robot.ctype.ai_info.CURRENT_STATE = AIS_LOCK;
		robot.ctype.ai_info.GOAL_STATE = AIS_LOCK;
		Believed_player_pos = ConsoleObject->pos;
		do_ai_frame(&robot);
		int mines = 0;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON) {
				require(Objects[i].id == PROXIMITY_ID, "native random-path submode cannot select a D2 super proximity mine");
				++mines;
			}
		if (mines != 1 || local.next_fire != (profile == 1 ? 5 * F1_0 : 4 * F1_0))
			std::fprintf(stderr, "Fleeing profile=%d mines=%d next_fire=%d mode=%d flags=%d\n", profile, mines, local.next_fire, local.mode, robot.ctype.ai_info.flags[4]);
		require(mines == 1 && local.next_fire == (profile == 1 ? 5 * F1_0 : 4 * F1_0),
		        "actual fleeing frame drops one original mine with native five-second cooldown");
		if (profile == 1)
			require(robot.ctype.ai_info.flags[4] == -1, "frame preserves native path submode");
	}
	FrameTime = old_frame;
	Difficulty_level = old_difficulty;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_robot_frame_entry()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const fix old_frame = FrameTime;
	FrameTime = F1_0 / 64;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		object &skipped = create_frame_robot(AIB_NORMAL, 400 * F1_0);
		ai_local &local = Ai_local_info[&skipped - Objects];
		local.mode = AIM_STILL;
		local.previous_visibility = 1;
		local.player_awareness_type = PA_NEARBY_ROBOT_FIRED;
		skipped.ctype.ai_info.SKIP_AI_COUNT = 2;
		skipped.mtype.phys_info.flags |= PF_USES_THRUST;
		skipped.mtype.phys_info.rotthrust = { 16 * F1_0, 8 * F1_0, 4 * F1_0 };
#ifdef DXX_BUILD_DESCENT_II
		local.next_action_time = 5 * F1_0;
#endif
		const auto skip_rng = d_rand_get_call_count();
		for (int remaining = 1; remaining >= 0; --remaining) {
			do_ai_frame(&skipped);
			require(skipped.ctype.ai_info.SKIP_AI_COUNT == remaining && local.mode == AIM_STILL && local.next_fire == 10 * F1_0 && local.time_since_processed == 0,
			        "skipped native D1 frames do not wake robots or advance AI firing/processing clocks");
			const fix thrust = profile == 1 ? 16 * F1_0 : (remaining ? 15 * F1_0 : 225 * F1_0 / 16);
			require(skipped.mtype.phys_info.rotthrust.x == thrust && ((skipped.mtype.phys_info.flags & PF_USES_THRUST) != 0) == (profile == 1 || remaining != 0),
			        "D1 skip handling preserves thrust while D2 retains its rotational-thrust decay");
#ifdef DXX_BUILD_DESCENT_II
			require(local.next_action_time == 5 * F1_0 - (profile == 2 ? (2 - remaining) * FrameTime : 0),
			        "only D2 actors advance the D2 action clock during skipped frames");
#endif
		}
		require(skip_rng == d_rand_get_call_count(), "skipped AI frames consume no simulation RNG");
		Game_mode = GM_OBSERVER;
		local.next_fire = -F1_0;
		skipped.ctype.ai_info.GOAL_STATE = AIS_FLIN;
		do_ai_frame(&skipped);
		require(local.next_fire == -F1_0 && skipped.ctype.ai_info.GOAL_STATE == AIS_FLIN,
		        "observer frames do not unflinch or tick ordinary robots");
#ifdef DXX_BUILD_DESCENT_II
		object &companion = create_frame_robot(AIB_NORMAL, 400 * F1_0);
		Robot_info[0].companion = 1;
		companion.ctype.ai_info.SKIP_AI_COUNT = 1;
		Ai_local_info[&companion - Objects].next_action_time = F1_0;
		do_ai_frame(&companion);
		require(!companion.ctype.ai_info.SKIP_AI_COUNT && Ai_local_info[&companion - Objects].next_action_time == F1_0 - FrameTime,
		        "optional companion keeps the D2 entry clock in either content profile");
#endif
		for (const fix firing_clock : { F1_0 / 2, -8 * F1_0, -8 * F1_0 + 1 }) {
			object &robot = create_frame_robot(AIB_NORMAL, 400 * F1_0);
			ai_local &clock = Ai_local_info[&robot - Objects];
			Robot_info[0].cloak_type = RI_CLOAKED_EXCEPT_FIRING;
			robot.ctype.ai_info.behavior = 0x86;
			clock.mode = AIM_STILL;
			clock.next_fire = firing_clock;
#ifdef DXX_BUILD_DESCENT_II
			clock.next_fire2 = 3 * F1_0;
#endif
			do_ai_frame(&robot);
			require(clock.next_fire == firing_clock - (firing_clock > -8 * F1_0 ? FrameTime : 0) && clock.time_since_processed == FrameTime,
			        "frame entry preserves the native firing-clock floor and processing-clock order");
			require(robot.ctype.ai_info.behavior == (profile == 1 ? AIB_NORMAL : 0x86) && robot.ctype.ai_info.CLOAKED == (profile == 1),
			        "native D1 frame entry validates source behavior and updates firing-dependent cloaking");
#ifdef DXX_BUILD_DESCENT_II
			require(clock.next_fire2 == (profile == 1 ? 3 * F1_0 : 8 * F1_0), "D1 frame preparation leaves D2-only secondary clocks untouched");
#endif
		}
	}
	Game_mode = 0;
	FrameTime = old_frame;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_robot_awareness_frame()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const fix old_frame = FrameTime;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		FrameTime = F1_0 / 64;
		object &robot = create_frame_robot(AIB_STILL, 400 * F1_0);
		ai_local &local = Ai_local_info[&robot - Objects];
		local.player_awareness_type = PA_NEARBY_ROBOT_FIRED;
		local.player_awareness_time = FrameTime;
		robot.ctype.ai_info.GOAL_STATE = AIS_LOCK;
		do_ai_frame(&robot);
		require(!local.player_awareness_type && local.player_awareness_time == 2 * F1_0 && robot.ctype.ai_info.GOAL_STATE == AIS_LOCK,
		        "awareness expiry reduces the level before the next frame returns the robot to rest");
		do_ai_frame(&robot);
		require(robot.ctype.ai_info.GOAL_STATE == AIS_REST, "the next unaware frame restores the rest goal");
		FrameTime = 8 * F1_0; // every valid first RNG roll enters the dead-player path branch
		object &follower = create_frame_robot(0x84, 100 * F1_0, 5);
		if (profile == 1) {
			// A valid unfinished path prevents the path follower from independently
			// generating a route and masking the dead-player awareness decision
			follower.ctype.ai_info.hide_index = 0;
			follower.ctype.ai_info.path_length = 2;
			follower.ctype.ai_info.cur_path_index = 1;
			follower.ctype.ai_info.PATH_DIR = 1;
			Point_segs[0].point = follower.pos;
			Point_segs[0].segnum = 0;
			Point_segs[1].point = { 30 * F1_0, 0, 0 };
			Point_segs[1].segnum = 0;
			Point_segs_free_ptr = Point_segs + 2;
		}
		Player_is_dead = 1;
		do_ai_frame(&follower);
		require(profile == 1 ? Point_segs_free_ptr > Point_segs + 2 : Point_segs_free_ptr == Point_segs,
		        "native follow-path robots can re-path toward a dead player while D2 sniper behavior keeps its own policy");
		Player_is_dead = 0;
	}
	Point_segs_free_ptr = Point_segs;
	FrameTime = old_frame;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_world_awareness()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	// Chain 0-1-2-3-4-5, triangle 1-2-6, and disconnected segment 7
	const int from_zero[] = { 0, 1, 2, 3, 4, 5, 2, 99 };
	const int from_five[] = { 5, 4, 3, 2, 1, 0, 4, 99 };
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		for (int strength : { 1, 2, 3, 4 })
			for (int initial : { 0, 2, 4 })
				for (bool overlapping : { false, true })
					for (int mode : { 0, GM_MULTI, GM_MULTI | GM_MULTI_ROBOTS }) {
						create_frame_robot(AIB_STILL, 10 * F1_0);
						Highest_segment_index = 7;
						for (int seg = 0; seg < 8; ++seg)
							for (auto &child : Segments[seg].children) child = -1;
						for (int seg = 0; seg < 5; ++seg) {
							Segments[seg].children[0] = seg + 1;
							Segments[seg + 1].children[1] = seg;
						}
						Segments[1].children[2] = 6;
						Segments[6].children[0] = 1;
						Segments[2].children[2] = 6;
						Segments[6].children[1] = 2;
						int roles = 1;
#ifdef DXX_BUILD_DESCENT_II
						roles = 2;
						Robot_info[1] = {};
						Robot_info[1].companion = 1;
						N_robot_types = 2;
#endif
						Highest_object_index = 8 * roles;
						for (int i = 1; i <= Highest_object_index; ++i) {
							Objects[i] = {};
							Objects[i].type = OBJ_ROBOT;
							Objects[i].id = (i - 1) / 8;
							Objects[i].control_type = CT_AI;
							Objects[i].segnum = (i - 1) % 8;
							Objects[i].ctype.ai_info.flags[4] = -1;
							Ai_local_info[i] = {};
							Ai_local_info[i].player_awareness_type = initial;
							Ai_local_info[i].player_awareness_time = F1_0 / 2;
						}
						Boss_dying = 0;
						Num_awareness_events = 0;
						create_awareness_event(&Objects[1], strength);
						if (overlapping) create_awareness_event(&Objects[6], 3);
						require(Num_awareness_events == (overlapping ? 2 : 1), "publish real events before the world update");
						// Populate in single-player, then exercise the delivery gate alone
						Game_mode = mode;
						const auto rng = d_rand_get_call_count();
						const auto fx = d_rand_get_stream_call_count(D_RNG_FX);
#ifdef DXX_BUILD_DESCENT_II
						if (profile == 2) {
							const ai_local before = Ai_local_info[1];
							require(!d1_in_d2_ai_deliver_awareness() && Num_awareness_events == (overlapping ? 2 : 1) &&
							            !std::memcmp(&before, &Ai_local_info[1], sizeof(before)),
							        "inactive native world operation leaves queued events and actor state untouched");
						}
#endif
						do_ai_frame_all();
						for (int i = 1; i <= Highest_object_index; ++i) {
							const int seg = Objects[i].segnum;
							const int depth = profile == 1 && Objects[i].id == 0 ? 4 : 3;
							int propagated = from_zero[seg] <= depth ? (strength == 4 && seg != 0 ? 3 : strength) : 0;
							if (overlapping && from_five[seg] <= depth) propagated = (std::max) (propagated, 3);
							if (mode == GM_MULTI && !(profile == 1 && Objects[i].id == 0)) propagated = 0;
							require(Ai_local_info[i].player_awareness_type == (std::max) (initial, propagated),
							        "world delivery uses native four-edge or companion/D2 three-edge propagation and maximum event strength");
							require(Ai_local_info[i].player_awareness_time == (propagated > initial ? PLAYER_AWARENESS_INITIAL_TIME : F1_0 / 2),
							        "only stronger world awareness refreshes its timer");
							require(Objects[i].ctype.ai_info.flags[4] == -1, "awareness delivery preserves native submode and existing engine flags");
							Ai_local_info[i].player_awareness_type = 0;
							Ai_local_info[i].player_awareness_time = F1_0 / 4;
						}
						require(!Num_awareness_events, "mixed-role world consumes the shared queue once");
						do_ai_frame_all();
						for (int i = 1; i <= Highest_object_index; ++i)
							require(!Ai_local_info[i].player_awareness_type && Ai_local_info[i].player_awareness_time == F1_0 / 4,
							        "an empty next world update cannot redeliver the previous events");
						require(rng == d_rand_get_call_count() && fx == d_rand_get_stream_call_count(D_RNG_FX),
						        "world propagation and delivery consume neither SIM nor FX RNG");
					}
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
	Game_mode = 0;
	init_test_corridor();
}

static void test_awareness_event_creation()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const auto old_time = GameTime64;
	const int queued_counts[] = {
		0, 63,
#ifdef NDEBUG
		64 // Native D1 debug builds assert on a full queue
#endif
	};
	for (int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		for (int mode : { 0, GM_MULTI, GM_MULTI | GM_MULTI_ROBOTS, GM_MULTI | GM_MULTI_ROBOTS | GM_OBSERVER })
			for (int type : { 1, 2, 3, 4 })
				for (int source_kind : { 0, 1, 2 })
					for (int queued : queued_counts)
						for (int agitation : { 0, 99, 100 })
							for (unsigned seed : { 0u, 1u, 12345u }) {
								create_frame_robot(AIB_STILL, 10 * F1_0);
								GameTime64 = 50 * F1_0;
								const int weapon = obj_create(OBJ_WEAPON, source_kind == 1 ? VULCAN_ID : 0, 1, &ConsoleObject->pos,
								                              &vmd_identity_matrix, F1_0, CT_WEAPON, MT_NONE, RT_NONE);
								require(weapon > 0, "create a real awareness-event source");
								object *source = source_kind == 2 ? ConsoleObject : &Objects[weapon];
								Num_awareness_events = 0;
								awareness_event expected_queue[64];
								std::memset(Awareness_events, 0x5a, sizeof(expected_queue));
								for (int i = 0; i < queued; ++i) create_awareness_event(ConsoleObject, 1);
								require(Num_awareness_events == queued, "populate the shared queue through actual event creation");
								std::memcpy(expected_queue, Awareness_events, sizeof(expected_queue));
								for (int i = 0; i < 8; ++i) {
									Ai_cloak_info[i].last_position = { i * F1_0, -F1_0, 3 * F1_0 };
									Ai_cloak_info[i].last_time = i * F1_0;
#ifdef DXX_BUILD_DESCENT_II
									Ai_cloak_info[i].last_segment = -1;
#endif
								}
								Believed_player_pos = { -F1_0, 0, 0 };
								Overall_agitation = agitation;
								Game_mode = mode;
								d_srand(seed);
								const int first = d_rand(), second = d_rand();
								d_srand(seed);
								const bool enabled = (profile == 1 || !(mode & GM_MULTI) || (mode & GM_MULTI_ROBOTS)) &&
								                     !((mode & GM_OBSERVER) && source == ConsoleObject);
								const bool filtered = source_kind == 1 && (type == 2 || type == 4) && queued < 64;
								const bool accepted = enabled && (!filtered || first <= 3276);
								const bool appended = accepted && queued < 64;
								const bool refreshed = enabled && type != 1;
								const int expected_draws = enabled ? (filtered ? 1 : 0) + (accepted ? 1 : 0) : 0;
								const int agitation_roll = filtered ? second : first;
								const int expected_agitation = accepted ? (std::min) (100, agitation + (((agitation_roll * (type + 4)) >> 15) > 4)) : agitation;
								if (appended) {
									expected_queue[queued].segnum = source->segnum;
									expected_queue[queued].type = type;
									expected_queue[queued].pos = source->pos;
								}
#ifdef DXX_BUILD_DESCENT_II
								input_demo_set_awareness_source("event_fixture", source - Objects, type);
#endif
								const auto sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
#ifdef DXX_BUILD_DESCENT_II
								if (profile == 2) {
									const char *pending_tag;
									int pending_source, pending_auxiliary;
									require(!d1_in_d2_ai_create_awareness_event(source, type), "native event operation declines ordinary D2");
									input_demo_consume_awareness_source(&pending_tag, &pending_source, &pending_auxiliary);
									require(!std::strcmp(pending_tag, "event_fixture") && pending_source == source - Objects && pending_auxiliary == type &&
									            sim == d_rand_get_call_count() && fx == d_rand_get_stream_call_count(D_RNG_FX),
									        "inactive event operation preserves pending diagnostic correlation and RNG");
									input_demo_set_awareness_source(pending_tag, pending_source, pending_auxiliary);
								}
#endif
								create_awareness_event(source, type);
								if (Num_awareness_events != queued + appended || std::memcmp(Awareness_events, expected_queue, sizeof(expected_queue)))
									std::fprintf(stderr, "Event case: profile=%d mode=%d type=%d source=%d queued=%d seed=%u expected_count=%d actual_count=%d\n",
									             profile, mode, type, source_kind, queued, seed, queued + appended, Num_awareness_events);
								require(Num_awareness_events == queued + appended && !std::memcmp(Awareness_events, expected_queue, sizeof(expected_queue)),
								        "event creation preserves source admission, payload, Vulcan filtering and queue capacity");
								require(Overall_agitation == expected_agitation && sim + expected_draws == d_rand_get_call_count() &&
								            fx == d_rand_get_stream_call_count(D_RNG_FX),
								        "event creation preserves exact SIM draw order and agitation saturation, including a full queue");
								for (int i = 0; i < 8; ++i) {
									const vms_vector position = refreshed ? ConsoleObject->pos : vms_vector{ i * F1_0, -F1_0, 3 * F1_0 };
									require(!std::memcmp(&Ai_cloak_info[i].last_position, &position, sizeof(position)) &&
									            Ai_cloak_info[i].last_time == (refreshed ? GameTime64 : i * F1_0),
									        "cloak belief refresh precedes impact filtering and queue capacity checks");
#ifdef DXX_BUILD_DESCENT_II
									require(Ai_cloak_info[i].last_segment == (refreshed ? ConsoleObject->segnum : -1), "common refresh also maintains the engine actor segment cache");
#endif
								}
								const vms_vector belief = refreshed ? ConsoleObject->pos : vms_vector{ -F1_0, 0, 0 };
								require(!std::memcmp(&Believed_player_pos, &belief, sizeof(belief)), "event refresh updates the control-center belief at the native boundary");
#ifdef DXX_BUILD_DESCENT_II
								const char *tag;
								int source_index, auxiliary;
								input_demo_consume_awareness_source(&tag, &source_index, &auxiliary);
								require(!std::strcmp(tag, "unset") && source_index == -1 && auxiliary == -1,
								        "event diagnostics consume their source correlation even when the event is rejected");
#endif
							}
	}
	Num_awareness_events = Overall_agitation = Game_mode = 0;
	GameTime64 = old_time;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_robot_hit_response()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	for (int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		for (int behavior : { 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0, 5 })
			for (int type : { 1, 2, 3, 4 })
				for (int control : { CT_AI, CT_NONE }) {
					int roles = 1;
#ifdef DXX_BUILD_DESCENT_II
					roles = 2;
#endif
					for (int companion = 0; companion < roles; ++companion) {
						object &robot = create_frame_robot(AIB_STILL, 10 * F1_0);
#ifdef DXX_BUILD_DESCENT_II
						Robot_info[0].companion = companion;
#endif
						robot.control_type = control;
						robot.ctype.ai_info.behavior = behavior;
						robot.ctype.ai_info.flags[4] = 1;
						ai_local &local = Ai_local_info[&robot - Objects];
						local.mode = AIM_FOLLOW_PATH;
						object expected = robot;
						ai_local expected_local = local;
						const bool native = profile == 1 && !companion;
						const bool reacts = control == CT_AI && (type == PA_WEAPON_ROBOT_COLLISION || type == PA_PLAYER_COLLISION);
						if (native && reacts && behavior == 0) expected_local.mode = AIM_CHASE_OBJECT;
						if (native && reacts && behavior == 5) expected.ctype.ai_info.flags[4] = 0;
						d_srand(12345);
						require(d_rand() >= 12288, "hit fixture selects the ordinary D2 no-path branch");
						d_srand(12345);
						const auto sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
#ifdef DXX_BUILD_DESCENT_II
						if (!native || control != CT_AI)
							require(!d1_in_d2_ai_robot_hit(&robot, type), "native hit operation declines inactive actors");
#endif
						do_ai_robot_hit(&robot, type);
						require(!std::memcmp(&robot, &expected, sizeof(robot)) && !std::memcmp(&local, &expected_local, sizeof(local)),
						        "actual hit response preserves the native mode-code quirk and engine actor behavior");
						require(d_rand_get_call_count() == sim + (!native && reacts && behavior == AIB_STILL) &&
						            d_rand_get_stream_call_count(D_RNG_FX) == fx && Point_segs_free_ptr == Point_segs,
						        "native hit response consumes no RNG or paths while companions retain the D2 decision draw");
					}
				}
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_world_camera_completion()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int old_tick = d_tick_count;
	for (int profile : { 2, 1, 2 }) {
		mission.descent_version = profile;
		for (int camera_state : { 0, 1, 2, 3 }) {
			object &native = create_frame_robot(0x82, 10 * F1_0);
			Robot_info[1] = {};
			Robot_info[1].companion = 1;
			N_robot_types = 2;
			const int companion = obj_create(OBJ_ROBOT, 1, 0, &native.pos, &native.orient, F1_0, CT_AI, MT_NONE, RT_NONE);
			const int camera = obj_create(OBJ_WEAPON, 0, 0, &native.pos, &native.orient, F1_0, CT_NONE, MT_NONE, RT_NONE);
			require(companion > 0 && camera > 0, "create actors for the real world completion phase");
			native.ctype.ai_info.flags[4] = -1;
			Objects[companion].ctype.ai_info.SUB_FLAGS = -1;
			if (camera_state == 2) Objects[camera].type = OBJ_NONE;
			Ai_last_missile_camera = camera_state ? camera : -1;
			d_tick_count = camera_state == 3 ? 16 : 1;
			Boss_dying = 0;
			Num_awareness_events = 0;
			const auto sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
			if (profile == 2)
				require(!d1_in_d2_ai_finish_world_frame() && Ai_last_missile_camera == (camera_state ? camera : -1) &&
				            native.ctype.ai_info.flags[4] == -1 && Objects[companion].ctype.ai_info.SUB_FLAGS == -1,
				        "inactive world completion leaves camera and actor state to D2");
			do_ai_frame_all();
			const bool retired = camera_state >= 2;
			require(Ai_last_missile_camera == (camera_state && !retired ? camera : -1), "camera reference retires at the existing validity/tick boundary");
			require(native.ctype.ai_info.flags[4] == (profile == 1 || !retired ? -1 : ~SUB_FLAGS_CAMERA_AWAKE),
			        "world completion preserves native hide submode while ordinary D2 clears its camera bit");
			require(Objects[companion].ctype.ai_info.SUB_FLAGS == (retired ? ~SUB_FLAGS_CAMERA_AWAKE : -1),
			        "companion camera retirement is identical in both content profiles");
			require(sim == d_rand_get_call_count() && fx == d_rand_get_stream_call_count(D_RNG_FX), "camera completion consumes no simulation or effect RNG");
		}
	}
	Ai_last_missile_camera = -1;
	d_tick_count = old_tick;
	Current_mission = nullptr;
#endif
}

static void test_missile_camera_awareness()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int old_tick = d_tick_count;
	for (const int profile : { 2, 1, 2 }) {
		mission.descent_version = profile;
		for (const int blocked : { 0, 1 })
			for (const int guided : { 0, 1 })
				for (const int stale : { 0, 1 }) {
					object &native = create_frame_robot(0x82, -5 * F1_0, 1);
					ConsoleObject->pos = { 0, 0, -5 * F1_0 };
					obj_relink(0, 0);
					native.pos = { -2 * F1_0, 0, 20 * F1_0 };
					obj_relink(&native - Objects, 1);
					Robot_info[1] = {};
					Robot_info[1].companion = 1;
					N_robot_types = 2;
					vms_vector point = { 2 * F1_0, 0, 20 * F1_0 };
					const int companion = obj_create(OBJ_ROBOT, 1, 1, &point, &vmd_identity_matrix, F1_0, CT_AI, MT_NONE, RT_NONE);
					point = {};
					const int camera = obj_create(OBJ_WEAPON, guided ? GUIDEDMISS_ID : CONCUSSION_ID, 0, &point, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_NONE, RT_NONE);
					require(companion > 0 && camera > 0, "create real camera and companion actors");
					Objects[camera].ctype.laser_info.parent_type = OBJ_PLAYER;
					Objects[camera].ctype.laser_info.parent_num = 0;
					Objects[camera].ctype.laser_info.parent_signature = ConsoleObject->signature;
					if (stale) Objects[camera].ctype.laser_info.parent_signature++;
					Missile_viewer = guided ? nullptr : &Objects[camera];
					Missile_viewer_sig = Objects[camera].signature;
					Guided_missile[0] = guided ? &Objects[camera] : nullptr;
					Guided_missile_sig[0] = Objects[camera].signature + stale;
					Ai_last_missile_camera = -1;
					if (blocked) {
						Segments[0].children[4] = -1;
						Segments[1].children[5] = -1;
					}
					Ai_local_info[&native - Objects] = {};
					Ai_local_info[companion] = {};
					native.ctype.ai_info.flags[4] = 1; // Native hide submode, not D2 camera flags
					Objects[companion].ctype.ai_info.SUB_FLAGS = 0;
					const object before = native;
					const ai_local local_before = Ai_local_info[&native - Objects];
					const auto sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
					for (int tick = 0; tick < 4; ++tick) {
						d_tick_count = tick;
						wake_up_missile_camera_robots();
					}
					const bool awake = !blocked && !stale;
					require(Ai_local_info[companion].player_awareness_type == (awake ? PA_WEAPON_ROBOT_COLLISION : 0) &&
					            Objects[companion].ctype.ai_info.SUB_FLAGS == (awake ? SUB_FLAGS_CAMERA_AWAKE : 0),
					        "optional companion retains D2 camera wake behavior and visibility/signature rejection");
					if (profile == 1)
						require(!std::memcmp(&native, &before, sizeof(native)) && !std::memcmp(&Ai_local_info[&native - Objects], &local_before, sizeof(local_before)),
						        "camera update leaves every native enemy object/AI byte intact, including hide submode");
					else
						require(Ai_local_info[&native - Objects].player_awareness_type == (awake ? PA_WEAPON_ROBOT_COLLISION : 0), "ordinary D2 enemy camera wake remains active");
					require(sim == d_rand_get_call_count() && fx == d_rand_get_stream_call_count(D_RNG_FX), "camera wake consumes neither RNG stream");
				}
	}
	Missile_viewer = Guided_missile[0] = nullptr;
	Missile_viewer_sig = Ai_last_missile_camera = -1;
	d_tick_count = old_tick;
	Current_mission = nullptr;
#endif
}

static void test_companion_physics()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	nlohmann::json expected;
	const fix old_frame = FrameTime;
	FrameTime = F1_0 / 16;
	for (const int profile : { 2, 1, 2 }) {
		mission.descent_version = profile;
		auto trace = nlohmann::json::array();
		for (const int free_spin : { 0, 1 })
			for (const int thrust : { 0, 1 })
				for (const int bounce : { 0, 1 }) {
					object &robot = create_frame_robot(AIB_NORMAL, 0, 1);
					Robot_info[0].companion = 1;
					ConsoleObject->pos = { -8 * F1_0, -8 * F1_0, -8 * F1_0 };
					ConsoleObject->size = F1_0 / 4;
					robot.pos = { 8 * F1_0, 0, 0 };
					robot.mtype.phys_info = {};
					robot.mtype.phys_info.mass = F1_0;
					robot.mtype.phys_info.drag = F1_0 / 8;
					robot.mtype.phys_info.rotvel = { F1_0 / 2, F1_0 / 4, 0 };
					robot.mtype.phys_info.rotthrust.x = thrust * F1_0 / 8;
					robot.mtype.phys_info.velocity.x = 32 * F1_0;
					robot.mtype.phys_info.flags = (free_spin ? PF_FREE_SPINNING : 0) | (thrust ? PF_USES_THRUST : 0) | (bounce ? PF_BOUNCE : 0);
					d_srand(177);
					d_rand_reset_call_count();
					vms_vector force = { 16 * F1_0, 0, 0 };
					phys_apply_rot(&robot, &force);
					for (int frame = 0; frame < 8; ++frame) {
						robot.last_pos = robot.pos;
						do_physics_sim(&robot);
						const auto &p = robot.mtype.phys_info;
						const auto &o = robot.orient;
						trace.push_back({ free_spin, thrust, bounce, frame, robot.pos.x, robot.pos.y, robot.pos.z,
						                  p.velocity.x, p.velocity.y, p.velocity.z, p.rotvel.x, p.rotvel.y, p.rotvel.z,
						                  o.rvec.x, o.rvec.y, o.rvec.z, o.uvec.x, o.uvec.y, o.uvec.z, o.fvec.x, o.fvec.y, o.fvec.z,
						                  robot.ctype.ai_info.SKIP_AI_COUNT, d_rand_get_call_count() });
					}
				}
		if (expected.is_null()) expected = trace;
		else require(trace == expected, "optional companion hit, rotation, bounce and motion remain D2 across content profiles");
	}
	FrameTime = old_frame;
	Current_mission = nullptr;
#endif
}

static void test_robot_navigation_preparation()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const fix old_frame = FrameTime;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		for (const int agitation : { 0, 80 }) {
			FrameTime = F1_0 / 64;
			object &hider = create_frame_robot(0x82, 400 * F1_0);
			ai_local &local = Ai_local_info[&hider - Objects];
			local.retry_count = 4;
			hider.mtype.phys_info.velocity = { F1_0, F1_0, F1_0 };
			Overall_agitation = agitation;
			d_srand(1);
			do_ai_frame(&hider);
			require(!local.retry_count && !local.consecutive_retries && vm_vec_mag(&hider.mtype.phys_info.velocity) == 0,
			        "stuck hide/behind robots complete recovery and clear their velocity");
			require((Point_segs_free_ptr > Point_segs) == (profile == 1),
			        "native D1 hide recovery creates a route while D2 behind recovery only recenters");
			if (profile == 1 && agitation == 80)
				require(local.mode == AIM_FOLLOW_PATH && local.goal_segment == ConsoleObject->segnum,
				        "an agitated stuck D1 hider chooses a route toward the player");
		}
		object &remote = create_frame_robot(AIB_NORMAL, 400 * F1_0);
		ai_local &remote_local = Ai_local_info[&remote - Objects];
		remote_local.retry_count = 4;
		remote_local.consecutive_retries = 6;
		Game_mode = GM_MULTI | GM_MULTI_ROBOTS;
		const auto remote_rng = d_rand_get_call_count();
		do_ai_frame(&remote);
		require(remote_local.retry_count == 4 && remote_local.consecutive_retries == 3 && Point_segs_free_ptr == Point_segs && d_rand_get_call_count() == remote_rng,
		        "ordinary multiplayer robots skip local recovery and decay consecutive retries");

		object &follower = create_frame_robot(0x84, 100 * F1_0, 5);
		FrameTime = 2 * F1_0;
		Overall_agitation = 100;
		d_srand(1);
		do_ai_frame(&follower);
		require((Point_segs_free_ptr > Point_segs) == (profile == 1),
		        "D1 follow-path robots participate in agitation path selection while D2 snipers do not");

		FrameTime = F1_0 / 64;
		object &factory_robot = create_frame_robot(AIB_NORMAL, 400 * F1_0);
		ai_static &factory_state = factory_robot.ctype.ai_info;
		ai_local &factory_local = Ai_local_info[&factory_robot - Objects];
		factory_local.mode = AIM_FOLLOW_PATH;
		factory_local.player_awareness_type = PA_NEARBY_ROBOT_FIRED;
		factory_local.player_awareness_time = F1_0;
		factory_state.hide_index = 0;
		factory_state.path_length = 2;
		factory_state.cur_path_index = 1;
		factory_state.PATH_DIR = 1;
		Point_segs[0].point = factory_robot.pos;
		Point_segs[0].segnum = 0;
		Point_segs[1].point = ConsoleObject->pos;
		Point_segs[1].segnum = 1;
		Point_segs_free_ptr = Point_segs + 2;
		Station[0].Enabled = 0;
#ifdef DXX_BUILD_DESCENT_II
		Segment2s[0].special = SEGMENT_IS_ROBOTMAKER;
		Segment2s[0].value = 0;
#else
		Segments[0].special = SEGMENT_IS_ROBOTMAKER;
		Segments[0].value = 0;
#endif
		do_ai_frame(&factory_robot);
		require(factory_local.player_awareness_time == F1_0 - (profile == 1 ? 0 : FrameTime),
		        "D1 exits even a disabled robot factory before awareness decay while D2 checks factory activation");
#ifdef DXX_BUILD_DESCENT_II
		Segment2s[0].special = SEGMENT_IS_NOTHING;
		object &companion = create_frame_robot(AIB_NORMAL, 100 * F1_0);
		Robot_info[0].companion = 1;
		Overall_agitation = 100;
		Ai_local_info[&companion - Objects].retry_count = 4;
		const object untouched = companion;
		const ai_local untouched_local = Ai_local_info[&companion - Objects];
		const auto inactive_rng = d_rand_get_call_count();
		const auto inactive_fx = d_rand_get_stream_call_count(D_RNG_FX);
		require(!d1_in_d2_ai_run_frame(&companion) &&
		            !std::memcmp(&companion, &untouched, sizeof(companion)) &&
		            !std::memcmp(&Ai_local_info[&companion - Objects], &untouched_local, sizeof(untouched_local)) &&
		            Point_segs_free_ptr == Point_segs && inactive_rng == d_rand_get_call_count() && inactive_fx == d_rand_get_stream_call_count(D_RNG_FX),
		        "optional companions fall through the D1 frame without state, path or RNG mutation");
#else
		Segments[0].special = SEGMENT_IS_NOTHING;
#endif
	}
	Game_mode = 0;
	Overall_agitation = 0;
	Point_segs_free_ptr = Point_segs;
	FrameTime = old_frame;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static object &create_firing_robot()
{
	object &robot = create_frame_robot(AIB_NORMAL, 100 * F1_0);
	Robot_info[0].n_guns = 2;
	Robot_info[0].weapon_type = 0;
	Robot_info[0].firing_wait[Difficulty_level] = F1_0;
	Robot_info[0].rapidfire_count[Difficulty_level] = 3;
	Ai_local_info[&robot - Objects].next_fire = 0;
	N_weapon_types = 2;
	for (int i = 0; i < N_weapon_types; ++i) {
		weapon_info &info = Weapon_info[i];
		std::memset(&info, 0, sizeof(info));
		info.render_type = WEAPON_RENDER_NONE;
		info.flash_sound = info.flash_vclip = -1;
		info.mass = F1_0;
		info.lifetime = 10 * F1_0;
		info.strength[Difficulty_level] = 10 * F1_0;
		info.speed[Difficulty_level] = 40 * F1_0;
#ifdef DXX_BUILD_DESCENT_II
		info.speedvar = 128;
		info.children = -1;
#endif
	}
	Believed_player_pos = ConsoleObject->pos;
	Num_awareness_events = 0;
	return robot;
}

static void fire_test_robot(object &robot, vms_vector &gun)
{
#ifdef DXX_BUILD_DESCENT_II
	ai_fire_laser_at_player(&robot, &gun, robot.ctype.ai_info.CURRENT_GUN, &Believed_player_pos);
#else
	ai_fire_laser_at_player(&robot, &gun);
#endif
}

static void decide_test_robot_fire(object &robot, vms_vector &gun, vms_vector direction, int visibility, int animates)
{
	ai_do_actual_firing_stuff(&robot, &robot.ctype.ai_info, &Ai_local_info[&robot - Objects], &Robot_info[robot.id],
	                          &direction, 100 * F1_0, &gun, visibility, animates
#ifdef DXX_BUILD_DESCENT_II
	                          ,
	                          robot.ctype.ai_info.CURRENT_GUN
#endif
	);
}

static int count_test_projectiles()
{
	int count = 0;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_WEAPON) ++count;
	return count;
}

static void test_robot_firing()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	// Imported D1 is covered through the complete frame comparison runner
	// These direct engine-helper checks retain native D1 and ordinary D2 baselines
	const int profiles[] = { 2 };
#else
	const int profiles[] = { 1 };
#endif
	const int old_difficulty = Difficulty_level;
	const fix64 old_teleport = Last_teleport_time;
	Difficulty_level = 2;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		for (const int flags : { 0, 1, 4 }) {
			object &robot = create_firing_robot();
			robot.ctype.ai_info.flags[4] = flags;
			vms_vector gun = { F1_0, 0, F1_0 };
			d_srand(1);
			const auto before_rng = d_rand_get_call_count();
			fire_test_robot(robot, gun);
			const bool fired = profile == 1 || flags != 4;
			require(count_test_projectiles() == (fired ? 1 : 0) && Num_awareness_events == (fired ? 1 : 0),
			        "D1 firing does not interpret hide-state storage as D2 camera flags; each shot publishes one awareness event");
			require(Ai_local_info[&robot - Objects].next_fire == (fired ? F1_0 / 8 : 0), "robot shots advance the burst clock once");
			if (profile == 1)
				require(d_rand_get_call_count() == before_rng + 5, "D1 firing consumes four aim draws and one awareness draw");
			if (fired) {
				const object &shot = Objects[Highest_object_index];
				require(shot.type == OBJ_WEAPON && shot.id == 0 && shot.ctype.laser_info.parent_num == &robot - Objects &&
				            shot.pos.x == gun.x && shot.pos.z == gun.z && shot.mtype.phys_info.velocity.z > 0,
				        "actual robot projectile retains its source weapon, parent, gun origin and forward velocity");
			}
		}
		for (int suppression = 0; suppression < 4; ++suppression) {
			object &robot = create_firing_robot();
			if (suppression == 0) cheats.robotfiringsuspended = 1;
			if (suppression == 1) robot.control_type = CT_MORPH;
			if (suppression == 2) Player_exploded = 1;
			if (suppression == 3) {
				Players[Player_num].flags |= PLAYER_FLAGS_CLOAKED;
				Ai_cloak_info[&robot - Objects].last_time = GameTime64 - CLOAK_TIME_MAX / 2;
			}
			vms_vector gun = { F1_0, 0, F1_0 };
			d_srand(1);
			const auto before_rng = d_rand_get_call_count();
			fire_test_robot(robot, gun);
			require(count_test_projectiles() == 0 && Num_awareness_events == 0 && d_rand_get_call_count() == before_rng + (suppression == 3 ? 1 : 0),
			        "suppressed robot shots preserve native gate and RNG order");
			require(Ai_local_info[&robot - Objects].next_fire == (suppression == 3 ? F1_0 / 8 : 0),
			        "only cloak suppression advances the native firing clock");
		}
		object &boss = create_firing_robot();
		Robot_info[0].boss_flag = 1;
		Last_teleport_time = 10 * F1_0;
		vms_vector gun = { F1_0, 0, F1_0 };
		fire_test_robot(boss, gun);
		require(Last_teleport_time == 10 * F1_0 - (profile == 1 ? Boss_teleport_interval / 2 : 0),
		        "a native D1 boss shot advances eligibility for its next teleport");

		for (const int animates : { 0, 1 }) {
			object &robot = create_firing_robot();
			Ai_local_info[&robot - Objects].next_fire = F1_0;
			decide_test_robot_fire(robot, gun, vmd_identity_matrix.fvec, 2, animates);
			require(count_test_projectiles() == (profile == 1 && !animates ? 1 : 0),
			        "native firing readiness distinguishes animation eligibility from D2 inner cooldown checks");
		}
		for (const fix forward_dot : { 7 * F1_0 / 8 - 1, 7 * F1_0 / 8 }) {
			object &robot = create_firing_robot();
			Robot_info[0].boss_flag = 1;
			vms_vector direction = { 0, 0, forward_dot };
			decide_test_robot_fire(robot, gun, direction, 2, 0);
			require(count_test_projectiles() == (profile == 2 || forward_dot == 7 * F1_0 / 8 ? 1 : 0),
			        "D1 bosses require the native direct-fire cone; D2 retains its wider boss cone");
		}
		object &no_gun = create_firing_robot();
		vms_vector empty_gun = {};
		const auto null_rng = d_rand_get_call_count();
		decide_test_robot_fire(no_gun, empty_gun, vmd_identity_matrix.fvec, 2, 0);
		require(count_test_projectiles() == 0 && no_gun.ctype.ai_info.CURRENT_GUN == 1 && no_gun.ctype.ai_info.GOAL_STATE == AIS_RECO && d_rand_get_call_count() == null_rng,
		        "a null direct gun origin preserves recoil and gun cycling without firing or RNG");
		for (const int visible : { 0, 1 }) {
			object &robot = create_firing_robot();
			Weapon_info[0].homing_flag = 1;
			Hit_pos = { 0, 0, 100 * F1_0 };
#ifdef DXX_BUILD_DESCENT_II
			Dist_to_last_fired_upon_player_pos = 1000 * F1_0;
			Ai_local_info[&robot - Objects].next_fire2 = F1_0;
#endif
			decide_test_robot_fire(robot, gun, vmd_identity_matrix.fvec, visible, 0);
			require(count_test_projectiles() == (profile == 1 ? 1 : 0) && robot.ctype.ai_info.CURRENT_GUN == 1,
			        "native homing fire uses the primary clock even at gun zero and advances to the next gun");
		}
		for (const int visible : { 0, 1 })
			for (const fix forward_dot : { F1_0 / 2 - 1, F1_0 / 2, 7 * F1_0 / 8 }) {
				object &robot = create_firing_robot();
				robot.ctype.ai_info.GOAL_STATE = AIS_REST;
				vms_vector direction = { 0, 0, forward_dot };
#ifdef DXX_BUILD_DESCENT_II
				Dist_to_last_fired_upon_player_pos = 0;
#endif
				const auto prepare_rng = d_rand_get_call_count();
				do_firing_stuff(&robot, visible, &direction);
				const int expected = profile == 1 && !visible ? AIS_REST : forward_dot >= 7 * F1_0 / 8 ? AIS_FIRE
				                                                       : forward_dot >= F1_0 / 2       ? AIS_LOCK
				                                                                                       : AIS_REST;
				require(robot.ctype.ai_info.GOAL_STATE == expected && count_test_projectiles() == 0 && prepare_rng == d_rand_get_call_count(),
				        "D1 lock/fire preparation requires sight while D2 retains the recent-target shortcut");
				require(Ai_local_info[&robot - Objects].player_awareness_type == (expected == AIS_FIRE ? PA_NEARBY_ROBOT_FIRED : 0),
				        "fire preparation raises awareness only when selecting the fire goal");
			}
#ifdef DXX_BUILD_DESCENT_II
		object &companion = create_firing_robot();
		Robot_info[0].companion = 1;
		Robot_info[0].weapon_type2 = 1;
		Robot_info[0].firing_wait2[Difficulty_level] = 2 * F1_0;
		ai_local &local = Ai_local_info[&companion - Objects];
		local.next_fire = F1_0;
		local.next_fire2 = 0;
		require(ready_to_fire(&Robot_info[0], &local), "companion secondary readiness is independent of the world's content profile");
		const object before_robot = companion;
		const ai_local before_local = local;
		const auto inactive_rng = d_rand_get_call_count();
		vms_vector direction = vmd_identity_matrix.fvec;
		require(!d1_in_d2_ai_run_frame(&companion) &&
		            !std::memcmp(&before_robot, &companion, sizeof(companion)) && !std::memcmp(&before_local, &local, sizeof(local)) &&
		            count_test_projectiles() == 0 && Num_awareness_events == 0 && inactive_rng == d_rand_get_call_count(),
		        "inactive D1 firing leaves companion state, projectile and awareness registries and RNG untouched");
		fire_test_robot(companion, gun);
		require(count_test_projectiles() == 1 && Objects[Highest_object_index].id == 1 && local.next_fire == F1_0 && local.next_fire2 == 2 * F1_0,
		        "optional companion retains its D2 secondary projectile and firing clock in both profiles");
		companion.ctype.ai_info.GOAL_STATE = AIS_REST;
		Dist_to_last_fired_upon_player_pos = 0;
		do_firing_stuff(&companion, 0, &direction);
		require(companion.ctype.ai_info.GOAL_STATE == AIS_FIRE, "optional companion retains D2 recent-target fire preparation in D1 content");
#endif
	}
	Last_teleport_time = old_teleport;
	Difficulty_level = old_difficulty;
	Player_exploded = 0;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void move_test_robot(object &robot, fix distance, int evade_only = 0)
{
	vms_vector direction = vmd_identity_matrix.fvec;
	ai_move_relative_to_player(&robot, &Ai_local_info[&robot - Objects], distance, &direction, 20 * F1_0, evade_only
#ifdef DXX_BUILD_DESCENT_II
	                           ,
	                           2
#endif
	);
}

static void test_robot_relative_movement()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	// Imported D1 is covered through the complete frame comparison runner
	// These direct engine-helper checks retain native D1 and ordinary D2 baselines
	const int profiles[] = { 2 };
#else
	const int profiles[] = { 1 };
#endif
	const fix old_frame = FrameTime;
	const int old_tick = d_tick_count, old_difficulty = Difficulty_level;
	FrameTime = F1_0 / 64;
	d_tick_count = 0;
	Difficulty_level = 2;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
#endif
		for (const fix distance : { 20 * F1_0 - 1, 20 * F1_0, 40 * F1_0 - 1, 40 * F1_0 }) {
			object &robot = create_frame_robot(AIB_NORMAL, 100 * F1_0);
			require(&robot - Objects == 1, "movement boundary scene has a stable robot slot");
			const auto before_rng = d_rand_get_call_count();
			move_test_robot(robot, distance);
			const vms_vector &velocity = robot.mtype.phys_info.velocity;
			const bool retreat = distance < 20 * F1_0;
			const bool approach = profile == 1 && distance >= 40 * F1_0;
			require(velocity.x == (!retreat && !approach ? -F1_0 / 2 : 0) && velocity.y == 0 &&
			            velocity.z == (retreat ? -F1_0 / 4 : approach ? F1_0 / 2
			                                                          : 0) &&
			            before_rng == d_rand_get_call_count(),
			        "native D1 retreats, circles and approaches at its original distance boundaries with no RNG");
		}
		for (const int filler : { 0, 1 }) {
			object &first = create_frame_robot(AIB_NORMAL, 100 * F1_0);
			object *robot = &first;
			if (filler) {
				vms_vector origin = {};
				const int objnum = obj_create(OBJ_ROBOT, 0, 0, &origin, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
				require(objnum == 2, "create a second robot for slot-independent D1 speed limiting");
				init_ai_object(objnum, AIB_NORMAL, 1);
				Ai_local_info[objnum].next_fire = F1_0;
				robot = &Objects[objnum];
			}
			Robot_info[0].max_speed[Difficulty_level] = F1_0 / 4;
			move_test_robot(*robot, 25 * F1_0);
			const fix expected_speed = profile == 2 && !filler ? F1_0 / 2 : 3 * F1_0 / 8;
			require(vm_vec_mag(&robot->mtype.phys_info.velocity) == expected_speed,
			        "D1 circling applies speed reduction to every robot; D2 keeps its existing slot-one exception");
		}
		for (const bool aligned : { false, true }) {
			object &robot = create_frame_robot(AIB_NORMAL, 100 * F1_0);
			robot.mtype.phys_info.velocity = aligned ? vms_vector{ 0, 0, F1_0 } : vms_vector{ 2 * F1_0, 0, 0 };
			move_test_robot(robot, 200 * F1_0);
			require(robot.mtype.phys_info.velocity.x == (aligned ? 0 : F1_0) &&
			            robot.mtype.phys_info.velocity.z == (aligned ? 11 * F1_0 / 4 : F1_0 / 2),
			        "shared forward acceleration preserves native aligned and turning fixed-point arithmetic");
		}
		for (const unsigned seed : { 0u, 1u }) {
			object &robot = create_frame_robot(AIB_NORMAL, 100 * F1_0);
			Robot_info[0].attack_type = 1;
			Robot_info[0].firing_wait[Difficulty_level] = F1_0;
			d_srand(seed);
			const auto before_rng = d_rand_get_call_count();
			move_test_robot(robot, 10 * F1_0, 1);
			const vms_vector &velocity = robot.mtype.phys_info.velocity;
			require(d_rand_get_call_count() == before_rng + 1 && velocity.x == (seed ? 0 : -F1_0 / 2) &&
			            velocity.y == (seed ? -F1_0 / 2 : 0) && velocity.z == (seed ? -F1_0 / 4 : 0),
			        "melee robots retain native circling/retreat RNG and continue moving in evade-only mode");
		}
		for (const int stale : { 0, 1 })
			for (const fix shields : { 0, F1_0 / 2, F1_0 }) {
				object &robot = create_frame_robot(AIB_NORMAL, 100 * F1_0);
				Robot_info[0].strength = F1_0;
				Robot_info[0].field_of_view[Difficulty_level] = F1_0 / 2;
				Robot_info[0].evade_speed[Difficulty_level] = 2;
				robot.shields = shields;
				vms_vector laser_pos = { 0, 0, 10 * F1_0 };
				const int laser = obj_create(OBJ_WEAPON, 0, 0, &laser_pos, &vmd_identity_matrix, F1_0 / 8, CT_WEAPON, MT_PHYSICS, RT_NONE);
				require(laser >= 0, "create an approaching danger laser");
				Objects[laser].mtype.phys_info.velocity = { 0, 0, -F1_0 };
				robot.ctype.ai_info.danger_laser_num = laser;
				robot.ctype.ai_info.danger_laser_signature = Objects[laser].signature + stale;
				ai_evaded = 0;
				const auto before_rng = d_rand_get_call_count();
				move_test_robot(robot, 100 * F1_0, 1);
				require(robot.mtype.phys_info.velocity.x == (stale ? 0 : -F1_0 - shields / 2) && ai_evaded == !stale &&
				            robot.ctype.ai_info.danger_laser_num == laser && before_rng == d_rand_get_call_count(),
				        "valid danger lasers trigger scaled native evasion; stale evade-only references do not move or consume RNG");
			}
#ifdef DXX_BUILD_DESCENT_II
		object &companion = create_frame_robot(AIB_NORMAL, 100 * F1_0);
		Robot_info[0].companion = 1;
		const object before_companion = companion;
		const auto before_rng = d_rand_get_call_count();
		const auto before_fx = d_rand_get_stream_call_count(D_RNG_FX);
		ai_evaded = 0;
		require(!d1_in_d2_ai_run_frame(&companion) &&
		            !std::memcmp(&companion, &before_companion, sizeof(companion)) && !ai_evaded &&
		            before_rng == d_rand_get_call_count() && before_fx == d_rand_get_stream_call_count(D_RNG_FX),
		        "inactive D1 movement leaves companion state and both RNG streams untouched");
		move_test_robot(companion, 40 * F1_0);
		require(companion.mtype.phys_info.velocity.x == -F1_0 / 2 && companion.mtype.phys_info.velocity.z == 0,
		        "optional companion keeps D2 circling decisions when hosted in D1 content");
#endif
	}
	FrameTime = old_frame;
	d_tick_count = old_tick;
	Difficulty_level = old_difficulty;
	ai_evaded = 0;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_boss_preparation_and_gating()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int profiles[] = { 2, 1, 2 };
#else
	const int profiles[] = { 1 };
#endif
	const int old_difficulty = Difficulty_level, old_level = Current_level_num;
	const fix64 old_time = GameTime64;
	const polymodel old_model = Polygon_models[0];
	const vclip old_clip = Vclip[VCLIP_MORPHING_ROBOT];
	struct {
		short opcode, count;
		vms_vector point;
		short end;
	} model = { 1, 1, { F1_0, F1_0, F1_0 }, 0 };
	Polygon_models[0] = {};
	Polygon_models[0].n_models = 1;
	Polygon_models[0].model_data = reinterpret_cast<ubyte *>(&model);
	Polygon_models[0].model_data_size = sizeof(model);
	Polygon_models[0].rad = F1_0 / 2;
	Vclip[VCLIP_MORPHING_ROBOT] = {};
	Vclip[VCLIP_MORPHING_ROBOT].num_frames = 1;
	Vclip[VCLIP_MORPHING_ROBOT].play_time = F1_0;
	Vclip[VCLIP_MORPHING_ROBOT].sound_num = -1;
	GameTime64 = 100 * F1_0;
	for (const int profile : profiles) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = profile;
		mission.last_level = 27;
#endif
		for (const int level : { 1, 27 }) {
			for (const int geometry : { 0, 1, 2 }) {
				object &boss = create_frame_robot(AIB_NORMAL, 100 * F1_0, 1);
				Robot_info[0].boss_flag = 1;
				boss.size = geometry == 2 ? 12 * F1_0 : F1_0;
				if (geometry == 1) {
					Num_walls = 2;
					for (int i = 0; i < 2; ++i) {
						const int side = i ? 5 : 4;
						Segments[i].sides[side].wall_num = i;
						Walls[i] = {};
						Walls[i].segnum = i;
						Walls[i].sidenum = side;
						Walls[i].type = WALL_CLOSED;
					}
				}
				Current_level_num = level;
				const object before = boss;
				const auto rng_before = d_rand_get_call_count();
				init_ai_objects();
				require(boss.size == before.size && boss.segnum == before.segnum &&
				            !std::memcmp(&boss.pos, &before.pos, sizeof(boss.pos)),
				        "boss destination preparation restores live radius, position and segment");
				require(Num_boss_gate_segs == (geometry == 1 ? 1 : 3),
				        "gate destination order preserves the native connected walk, including its start revisit");
				const int expected_teleports = geometry == 0 || (geometry == 2 && profile == 1) ? 3 : geometry == 1 && profile == 2 ? 2
				                                                                                                                    : 1;
				require(Num_boss_teleport_segs == expected_teleports,
				        "D1 boss destinations use the native reduced fit radius and never the D2 through-wall fallback");
				require(Boss_teleport_interval == (profile == 1 ? 8 : level == 27 ? 10
				                                                                  : 7) *
				                                      F1_0 &&
				            Boss_cloak_interval == (profile == 2 && level == 27 ? 15 : 10) * F1_0 &&
				            rng_before == d_rand_get_call_count(),
				        "boss preparation keeps native intervals on ordinary and final levels without RNG");
			}
		}
		for (int difficulty = 0; difficulty < NDL; ++difficulty) {
			Difficulty_level = difficulty;
			difficulty_refresh_runtime_parameters();
			require(Gate_interval == (profile == 1 ? 5 * F1_0 - difficulty * F1_0 / 2 : 4 * F1_0 - difficulty * i2f(2) / 3),
			        "live difficulty refresh uses the active content's boss gate interval");
		}
		Difficulty_level = 0;
		difficulty_refresh_runtime_parameters();
		for (const int type : { 8, 10 })
			for (const int situation : { 0, 1, 2, 3, 4 }) {
				create_frame_robot(AIB_NORMAL, 100 * F1_0);
				init_morphs();
				N_robot_types = 11;
				Robot_info[type] = Robot_info[0];
				Robot_info[type].mass = 2 * F1_0;
				Robot_info[type].drag = F1_0 / 4;
				Robot_info[type].strength = 30 * F1_0;
#ifdef DXX_BUILD_DESCENT_II
				Robot_info[type].behavior = AIB_STILL;
#endif
				Num_boss_gate_segs = 1;
				Boss_gate_segs[0] = 1;
				Last_gate_time = GameTime64 - (situation == 1 ? Gate_interval / 2 : 2 * Gate_interval);
				const int cap_count = situation == 2 ? 4 : situation == 3 ? 7
				                                                          : 0;
				for (int i = 0; i < cap_count; ++i) {
					vms_vector origin = {};
					const int objnum = obj_create(OBJ_ROBOT, 0, 0, &origin, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
					require(objnum > 0, "create pre-existing boss-gated robots");
					Objects[objnum].matcen_creator = BOSS_GATE_MATCEN_NUM;
				}
				if (situation == 4) {
					obj_relink(0, 1);
					ConsoleObject->size = 10000 * F1_0;
				}
				const int previous_highest = Highest_object_index;
				const int previous_total = Players[0].num_robots_total;
				d_srand(17);
				const auto rng_before = d_rand_get_call_count();
				const int result = gate_in_robot(type, -1);
				const bool success = situation == 0 || (situation == 1 && profile == 1) || (situation == 2 && profile == 2);
#ifdef DXX_BUILD_DESCENT_II
				require((result >= 0) == success, "D2 gate API reports object index or failure for either content profile");
#else
				require((result != 0) == success, "native D1 gate success matches the scenario");
#endif
				int spawned = -1;
				for (int i = previous_highest + 1; i <= Highest_object_index; ++i)
					if (Objects[i].type == OBJ_ROBOT) spawned = i;
				require((spawned >= 0) == success && Players[0].num_robots_total == previous_total + success,
				        "gating creates and counts exactly one robot only on success");
				const bool rejected_before_placement = situation == 3 || (situation == 2 && profile == 1) || (situation == 1 && profile == 2);
				require(d_rand_get_call_count() == rng_before + (rejected_before_placement ? 1 : 3),
				        "gating consumes the native segment and placement draws only at their original phases");
				if (success) {
					const object &robot = Objects[spawned];
					require(robot.id == type && robot.segnum == 1 && robot.control_type == CT_MORPH && robot.render_type == RT_MORPH &&
					            robot.shields == 30 * F1_0 && robot.mtype.phys_info.mass == 2 * F1_0 && robot.mtype.phys_info.drag == F1_0 / 4 &&
					            robot.matcen_creator == BOSS_GATE_MATCEN_NUM && robot.lifeleft == (profile == 1 ? IMMORTAL_TIME : 30 * F1_0) &&
					            robot.ctype.ai_info.behavior == (profile == 1 ? type == 10 ? AIB_RUN_FROM : AIB_NORMAL : AIB_STILL),
					        "gated robot retains native definition, morph, permanent lifetime and toaster behavior");
				}
				const fix64 expected_time = success ? GameTime64 : situation == 1 && profile == 2 ? GameTime64 - Gate_interval / 2
				                                                                                  : GameTime64 - 3 * Gate_interval / 4;
				require(Last_gate_time == expected_time, "gate success and failed placement/cap preserve native retry timing");
			}
		if (profile == 1) {
			const fix old_frame = FrameTime;
			const eclip old_effect = Effects[ECLIP_NUM_BOSS];
			FrameTime = 0;
			for (const fix distance : { 600 * F1_0 - 1, 600 * F1_0 })
				for (const fix elapsed : { Gate_interval / 2, Gate_interval / 2 + 1, Gate_interval, Gate_interval + 1 }) {
					object &boss = create_frame_robot(AIB_STILL, distance, 100);
					init_morphs();
					for (int i = 1; i < 24; ++i) Robot_info[i] = Robot_info[0];
					N_robot_types = 24;
					boss.id = 17;
					Robot_info[17].boss_flag = 2;
					init_ai_objects();
					Players[0].flags = PLAYER_FLAGS_CLOAKED;
					Ai_cloak_info[&boss - Objects].last_time = GameTime64;
					Ai_cloak_info[&boss - Objects].last_position = ConsoleObject->pos;
					Boss_cloak_end_time = GameTime64;
					Last_gate_time = GameTime64 - elapsed;
					Num_boss_gate_segs = 1;
					Boss_gate_segs[0] = 1;
					Effects[ECLIP_NUM_BOSS] = {};
					Effects[ECLIP_NUM_BOSS].flags = EF_STOPPED;
					Effects[ECLIP_NUM_BOSS].changing_wall_texture = -1;
					Effects[ECLIP_NUM_BOSS].changing_object_texture = -1;
					const int before_highest = Highest_object_index;
					d_srand(17);
					const auto before_rng = d_rand_get_call_count();
					do_ai_frame(&boss);
					const bool player_near = distance < 600 * F1_0;
					const bool spawn = player_near && elapsed > Gate_interval;
					require(!!(Effects[ECLIP_NUM_BOSS].flags & EF_STOPPED) == !(player_near && elapsed > Gate_interval / 2),
					        "actual super-boss frame honors native effect clock and cloaked-player distance boundaries");
					int spawned = -1;
					for (int i = before_highest + 1; i <= Highest_object_index; ++i)
						if (Objects[i].type == OBJ_ROBOT) spawned = i;
					require((spawned >= 0) == spawn && d_rand_get_call_count() == before_rng + (spawn ? 4 : 0),
					        "actual super-boss frame gates one native robot with exactly four SIM draws at the interval boundary");
					if (spawn) require(Objects[spawned].id == 11 && Objects[spawned].lifeleft == IMMORTAL_TIME,
						               "seeded super-boss selection uses the original D1 type list and permanent lifetime");
				}
			Effects[ECLIP_NUM_BOSS] = old_effect;
			FrameTime = old_frame;
			Players[0].flags = 0;
		}
	}
	init_morphs();
	Polygon_models[0] = old_model;
	Vclip[VCLIP_MORPHING_ROBOT] = old_clip;
	Current_level_num = old_level;
	Difficulty_level = old_difficulty;
	GameTime64 = old_time;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
	difficulty_refresh_runtime_parameters();
}

static object &create_boss_frame_scene()
{
	object &boss = create_frame_robot(AIB_STILL, 300 * F1_0, 40);
	Robot_info[0].boss_flag = 1;
	init_ai_objects();
	Ai_local_info[&boss - Objects].next_fire = 10 * F1_0;
	Ai_local_info[&boss - Objects].next_misc_sound_time = GameTime64 + 100 * F1_0;
	Boss_cloak_start_time = 0;
	Boss_cloak_end_time = 20 * F1_0;
	Last_teleport_time = 0;
	Boss_dying_start_time = 0;
	Control_center_destroyed = 0;
	ControlCenterTriggers.num_links = 0;
#ifndef DXX_BUILD_DESCENT_II
	Boss_hit_this_frame = 0;
#endif
	return boss;
}

static void hit_boss_with_repeat_weapon(object &boss)
{
	const int laser = obj_create(OBJ_WEAPON, 0, boss.segnum, &boss.pos, &vmd_identity_matrix, F1_0 / 8, CT_WEAPON, MT_PHYSICS, RT_NONE);
	require(laser >= 0, "create a boss weapon-hit event");
	Objects[laser].mtype.phys_info.flags |= PF_PERSISTENT;
	Objects[laser].ctype.laser_info.hitobj_list[&boss - Objects] = 1;
	collide_robot_and_weapon(&boss, &Objects[laser], &boss.pos);
}

static void test_boss_frame_updates()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	mission.descent_version = 1;
	mission.last_level = 27;
	mission.builtin_hogsize = 6856701;
	Current_mission = &mission;
#endif
	const fix old_frame = FrameTime;
	const fix64 old_time = GameTime64;
	const int old_level = Current_level_num;
	FrameTime = 0;
	GameTime64 = 30 * F1_0;
	object &boss = create_boss_frame_scene();
	do_ai_frame(&boss);
	require(!boss.ctype.ai_info.CLOAKED, "boss waits through the exact native cloak interval");
	++GameTime64;
	do_ai_frame(&boss);
	require(boss.ctype.ai_info.CLOAKED && Boss_cloak_start_time == GameTime64 && Boss_cloak_end_time == GameTime64 + 7 * F1_0,
	        "actual native boss frame starts a seven-second cloak after its interval");
	GameTime64 = Boss_cloak_start_time + 3 * F1_0;
	Num_boss_teleport_segs = 1;
	Boss_teleport_segs[0] = 1;
	const auto before_teleport = d_rand_get_call_count();
	do_ai_frame(&boss);
	vms_vector center;
	compute_segment_center(&center, &Segments[1]);
	require(boss.segnum == 1 && !std::memcmp(&boss.pos, &center, sizeof(center)) && Last_teleport_time == GameTime64 &&
	            Ai_local_info[&boss - Objects].next_fire == 0 && d_rand_get_call_count() == before_teleport + 1,
	        "native boss teleport selects one destination, relinks, resets primary fire and consumes one SIM draw");
	GameTime64 = Boss_cloak_end_time;
	do_ai_frame(&boss);
	require(boss.ctype.ai_info.CLOAKED, "boss remains cloaked at the exact end time");
	++GameTime64;
	do_ai_frame(&boss);
	require(!boss.ctype.ai_info.CLOAKED, "boss uncloaks after its native end time");

	GameTime64 = 21 * F1_0;
	object &hit = create_boss_frame_scene();
	hit_boss_with_repeat_weapon(hit);
	do_ai_frame(&hit);
	require(hit.ctype.ai_info.CLOAKED && Boss_cloak_start_time == GameTime64, "weapon contact forces an early native boss cloak");
	Last_teleport_time = GameTime64;
	hit_boss_with_repeat_weapon(hit);
	do_ai_frame(&hit);
	require(Last_teleport_time == GameTime64 - Boss_teleport_interval / 4, "cloaked boss consumes a hit to accelerate its teleport clock");
	do_ai_frame(&hit);
	require(Last_teleport_time == GameTime64 - Boss_teleport_interval / 4, "consumed boss hit does not accelerate twice");

	for (const bool pending : { false, true }) {
		object &saved = create_boss_frame_scene();
		if (pending) hit_boss_with_repeat_weapon(saved);
		Num_awareness_events = 0;
#ifdef DXX_BUILD_DESCENT_II
		Believed_player_seg = 0;
#endif
		PHYSFS_file *write_handle = PHYSFS_openWrite("boss-ai-state.bin");
		REWIND_PHYSFS_FILE(writer, write_handle);
		require(write_handle && ai_save_state(writer) && PHYSFS_close(write_handle), "write actual AI state with pending/clear boss hit");
		/* Consume an existing hit, or create one absent from the saved state */
		if (!pending) hit_boss_with_repeat_weapon(saved);
		do_ai_frame(&saved);
		saved.ctype.ai_info.CLOAKED = 0;
		PHYSFS_file *read_handle = PHYSFS_openRead("boss-ai-state.bin");
		REWIND_PHYSFS_FILE(reader, read_handle);
#ifdef DXX_BUILD_DESCENT_II
		require(read_handle && ai_restore_state(reader, 31, 0), "restore actual D2 AI record for D1 content");
#else
		require(read_handle && ai_restore_state(reader, 17, 0, 0), "restore native D1 AI record");
#endif
		require(PHYSFS_close(read_handle), "close restored boss AI record");
		do_ai_frame(&saved);
		require(!!saved.ctype.ai_info.CLOAKED == pending, "restored pending boss hit affects exactly the next native frame");
	}

	GameTime64 = 100 * F1_0;
	object &dying = create_boss_frame_scene();
	Current_level_num = 27;
	Players[0].shields = 20 * F1_0;
	const int before_kills = Players[0].num_kills_total;
	dying.shields = F1_0;
	require(apply_damage_to_robot(&dying, 2 * F1_0, 0) && Boss_dying && Boss_dying_start_time == GameTime64 &&
	            Players[0].num_kills_total == before_kills + 1 && !(Players[0].flags & PLAYER_FLAGS_INVULNERABLE),
	        "actual lethal native boss hit begins death once without D2 final-level invulnerability");
#ifdef DXX_BUILD_DESCENT_II
	require(!Final_boss_is_dead, "native boss death does not enable D2's automatic ending");
#endif
	GameTime64 += 6 * F1_0 - 0x2ae14;
	do_ai_frame(&dying);
	require(!Boss_dying_sound_playing, "native death sound waits through its exact fixed-duration boundary");
	const auto before_world = d_rand_get_stream_call_count(D_RNG_FX);
	do_ai_frame_all();
	require(!Boss_dying_sound_playing && before_world == d_rand_get_stream_call_count(D_RNG_FX), "world AI does not repeat a native boss death update");
	++GameTime64;
	do_ai_frame(&dying);
	require(Boss_dying_sound_playing && !Control_center_destroyed, "native fixed death sound starts before destruction");
	GameTime64 = Boss_dying_start_time + 6 * F1_0 + 1;
	do_ai_frame(&dying);
	require(Control_center_destroyed && (dying.flags & OF_EXPLODING) && Boss_dying_start_time == GameTime64,
	        "native six-second death completes with reactor destruction and boss explosion");
	require(Countdown_timer == (50 - 5 * Difficulty_level) * F1_0, "boss destruction uses the original D1 reactor countdown");
#ifdef DXX_BUILD_DESCENT_II
	mission.descent_version = 2;
	object &d2_boss = create_boss_frame_scene();
	Robot_info[0].boss_flag = BOSS_D2;
	const object before_d2 = d2_boss;
	const auto sim_before = d_rand_get_call_count(), fx_before = d_rand_get_stream_call_count(D_RNG_FX);
	require(!d1_in_d2_ai_run_frame(&d2_boss) &&
	            d1_in_d2_ai_finish_boss_damage(&d2_boss, 0) == -1 && !d1_in_d2_ai_boss_weapon_hit(&d2_boss) &&
	            !std::memcmp(&d2_boss, &before_d2, sizeof(d2_boss)) &&
	            d_rand_get_call_count() == sim_before && d_rand_get_stream_call_count(D_RNG_FX) == fx_before,
	        "inactive native frame and boss operations leave ordinary D2 actors and both RNG streams untouched");
	Boss_hit_time = GameTime64 - F1_0;
	do_ai_frame(&d2_boss);
	require(d2_boss.ctype.ai_info.CLOAKED && Boss_cloak_start_time == GameTime64, "ordinary D2 boss still runs its original update after a D1 session");
	require(d1_in_d2_ai_save_boss_hit(-F1_0) == -F1_0 && d1_in_d2_ai_restore_boss_hit_time(-F1_0) == GameTime64 - F1_0,
	        "ordinary D2 hit-time serialization values remain unchanged");
	for (const int profile : { 2, 1, 2 }) {
		mission.descent_version = profile;
		const int old_difficulty = Difficulty_level;
		const int old_base_time = Base_control_center_explosion_time;
		const int native_d2_times[] = { 90, 60, 45, 35, 30 };
		for (int difficulty = 0; difficulty < NDL; ++difficulty) {
			Difficulty_level = difficulty;
			Base_control_center_explosion_time = DEFAULT_CONTROL_CENTER_EXPLOSION_TIME;
			do_controlcen_destroyed_stuff(nullptr);
			require(Countdown_timer == (profile == 1 ? 50 - 5 * difficulty : native_d2_times[difficulty]) * F1_0,
			        "reactor countdown keeps each game's original duration at every difficulty");
		}
		Base_control_center_explosion_time = old_base_time;
		Difficulty_level = old_difficulty;
	}
#endif
	Boss_dying = Boss_dying_sound_playing = 0;
	Control_center_destroyed = 0;
	Current_level_num = old_level;
	GameTime64 = old_time;
	FrameTime = old_frame;
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_projectile_collision_relationships()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int versions[] = { 2, 1, 2 };
#else
	const int versions[] = { 1 };
#endif
	for (const int version : versions) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = version;
#endif
		init_test_corridor();
		collide_init();
		Game_mode = 0;
		GameTime64 = 10 * F1_0;
		vms_vector origin = { 0, 0, 0 }, crossing = { 0, 0, 4 * F1_0 }, end = { 0, 0, 8 * F1_0 };
		const int first = obj_create(OBJ_WEAPON, 0, 0, &origin, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_PHYSICS, RT_NONE);
		const int second = obj_create(OBJ_WEAPON, 0, 0, &crossing, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_PHYSICS, RT_NONE);
		require(first >= 0 && second >= 0, "create crossing projectile scene");
		object &a = Objects[first], &b = Objects[second];
		a.ctype.laser_info.parent_num = 0;
		a.ctype.laser_info.parent_signature = Objects[0].signature;
		b.ctype.laser_info.parent_num = -1;
		b.ctype.laser_info.parent_signature = Objects[0].signature + 100;
		require(laser_are_related(first, second) == (version == 2), "foreign ordinary projectiles collide only in D1");
		fvi_query query = {};
		fvi_info hit = {};
		query.p0 = &origin;
		query.p1 = &end;
		query.startseg = 0;
		query.rad = a.size;
		query.thisobjnum = first;
		query.flags = FQ_CHECK_OBJS;
		const int fate = find_vector_intersection(&query, &hit);
		require(fate == (version == 1 ? HIT_OBJECT : HIT_NONE), "real collision traversal retains D1 foreign projectile contacts");
		if (version == 1) require(hit.hit_object == second && hit.hit_pnt.z < end.z, "D1 movement must split at the crossing projectile");
		b.ctype.laser_info.parent_signature = a.ctype.laser_info.parent_signature;
		require(laser_are_related(first, second), "ordinary siblings ignore one another in both games");
		a.id = PROXIMITY_ID;
		a.ctype.laser_info.creation_time = b.ctype.laser_info.creation_time = GameTime64;
		require(laser_are_related(first, second) == (version == 2), "D1 mines can collide with siblings immediately");
		a.ctype.laser_info.creation_time = GameTime64 - 2 * F1_0;
		require(laser_are_related(first, 0), "mine still ignores its owner at the grace boundary");
		--a.ctype.laser_info.creation_time;
		require(laser_are_related(first, 0) == (version == 2), "D1 mine owner grace expires just after two seconds");
		a.ctype.laser_info.creation_time = GameTime64 - 5 * F1_0;
		require(!laser_are_related(first, 0) && laser_are_related(0, first) == (version == 1), "native D1 preserves the directional parent check for old mines");
		require(!laser_are_related(-1, first), "invalid object indices are unrelated");
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_stuck_projectiles()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int versions[] = { 2, 1, 2 };
#else
	const int versions[] = { 1 };
#endif
	for (const int version : versions) {
#ifdef DXX_BUILD_DESCENT_II
		mission.descent_version = version;
#endif
		init_test_corridor();
		Num_walls = 2;
		Walls[1] = {};
		Walls[1].state = WALL_DOOR_CLOSED;
		Segments[0].sides[4].wall_num = 1;
		vms_vector origin = {};
		const int flare = obj_create(OBJ_WEAPON, FLARE_ID, 0, &origin, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_PHYSICS, RT_NONE);
		require(flare >= 0, "create flare stuck in a doorway");
		for (auto &entry : Stuck_objects) entry.wallnum = -1;
		Num_stuck_objects = 0;
		add_stuck_object(&Objects[flare], 0, 4);
		kill_stuck_objects(1);
		require(!Num_stuck_objects && Objects[flare].lifeleft == (version == 1 ? F1_0 / 4 : F1_0 / 8), "opening a door preserves native flare retirement timing");
		add_stuck_object(&Objects[flare], 0, 4);
		++Objects[flare].signature;
		Objects[flare].lifeleft = 10 * F1_0;
		d_tick_count = 0;
		remove_obsolete_stuck_objects();
		require(!Num_stuck_objects && Stuck_objects[0].wallnum == -1, "retire stale stuck-object signature");
		require(Objects[flare].lifeleft == (version == 1 ? 10 * F1_0 : F1_0 / 8), "native stale-entry cleanup leaves the replacement object's lifetime alone");
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
}

static void test_homing_targets()
{
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	const int versions[] = { 2, 1, 2 };
#else
	const int versions[] = { 1 };
#endif
	for (const int version : versions)
		for (const int original : { 0, 1 }) {
#ifdef DXX_BUILD_DESCENT_II
			mission.descent_version = version;
#endif
			init_test_corridor();
			Game_mode = 0;
			Player_num = 0;
			Players[0].objnum = 0;
			Players[0].flags = 0;
			ConsoleObject = Viewer = &Objects[0];
			ConsoleObject->type = OBJ_PLAYER;
			ConsoleObject->id = 0;
			std::memset(&cheats, 0, sizeof(cheats));
			std::memset(&Robot_info[0], 0, sizeof(Robot_info[0]));
			Weapon_info[HOMING_ID].homing_flag = 1;
			set_homing_update_rate(25, original);
			vms_vector origin = { 0, 0, 0 }, wide = { 6 * F1_0, 0, 8 * F1_0 };
			const int missile = obj_create(OBJ_WEAPON, HOMING_ID, 0, &origin, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_PHYSICS, RT_NONE);
			const int target = obj_create(OBJ_ROBOT, 0, 0, &wide, &vmd_identity_matrix, F1_0 / 2, CT_AI, MT_NONE, RT_NONE);
			require(missile >= 0 && target >= 0, "create homing target scene");
			object &homer = Objects[missile];
			homer.ctype.laser_info.parent_num = 0;
			homer.ctype.laser_info.parent_type = OBJ_PLAYER;
			homer.ctype.laser_info.parent_signature = ConsoleObject->signature;
			homer.ctype.laser_info.creation_framecount = 0;
			const unsigned rng_before = d_rand_get_call_count();
			require(find_homing_object_complete(&origin, &homer, OBJ_ROBOT, -1) == (version == 1 ? target : -1), "D1 keeps its original acquisition cone under both homing settings; D2 keeps its narrower cone");
			Objects[target].pos = { 0, 0, 8 * F1_0 };
			vms_vector mine_pos = { 0, 0, 5 * F1_0 };
			const int mine = obj_create(OBJ_WEAPON, PROXIMITY_ID, 0, &mine_pos, &vmd_identity_matrix, F1_0 / 2, CT_WEAPON, MT_NONE, RT_NONE);
			require(mine >= 0, "create foreign proximity bomb candidate");
			Objects[mine].ctype.laser_info.parent_signature = ConsoleObject->signature + 1;
			require(find_homing_object_complete(&origin, &homer, OBJ_ROBOT, -1) == (version == 1 ? target : mine), "D1 excludes proximity bombs from reacquisition while D2 retains its mine preference");
			obj_delete(mine);
			Objects[target].pos.x = F1_0;
			vms_vector ahead = { 0, 0, 9 * F1_0 };
			const int better = obj_create(OBJ_ROBOT, 0, 0, &ahead, &vmd_identity_matrix, F1_0 / 2, CT_AI, MT_NONE, RT_NONE);
			require(better >= 0, "create a better aligned replacement target");
			homer.ctype.laser_info.track_goal = target;
			fix dot = -1;
			const unsigned scan_frame = original ? missile : 0;
			require(track_track_goal(target, &homer, &dot, scan_frame, original) == (version == 2 && original ? better : target), "D1 retains a valid lock while original D2 periodically scans for a better one");
			require(dot > F1_0 * 9 / 10, "retention computes the target alignment");
			Objects[target].ctype.ai_info.CLOAKED = 1;
			dot = 123;
			require(track_track_goal(target, &homer, &dot, scan_frame ^ 1, original) == -1 && dot == 123, "cloaked lock is lost between scans without changing the unused dot");
			require(track_track_goal(target, &homer, &dot, scan_frame, original) == better, "the next scheduled scan reacquires an eligible target");
			Objects[better].ctype.ai_info.CLOAKED = 1;
			require(find_homing_object_complete(&origin, &homer, OBJ_ROBOT, -1) == -1, "cloaked robots cannot be acquired");
			Objects[better].ctype.ai_info.CLOAKED = 0;
			Objects[better].pos = { 0, 0, 20 * F1_0 };
			obj_relink(better, 1);
			Segments[0].children[4] = -1;
			require(find_homing_object_complete(&origin, &homer, OBJ_ROBOT, -1) == -1, "target acquisition uses real wall visibility");
			// A rendered target beyond the full-scan range remains eligible in D1
			for (int i = 0; i < Num_vertices; ++i) {
				Vertices[i].x *= 100;
				Vertices[i].y *= 100;
				Vertices[i].z *= 100;
			}
			validate_segment_all();
			Objects[target].ctype.ai_info.CLOAKED = 0;
			Objects[target].pos = { 0, 0, 300 * F1_0 };
#ifdef DXX_BUILD_DESCENT_II
			std::memset(Window_rendered_data, 0, sizeof(Window_rendered_data));
			Window_rendered_data[0].time = timer_query();
			Window_rendered_data[0].viewer = ConsoleObject;
			Window_rendered_data[0].num_objects = 1;
			Window_rendered_data[0].rendered_objects[0] = target;
#else
			Num_rendered_objects = 1;
			Ordered_rendered_object_list[0] = target;
#endif
			require(find_homing_object(&origin, &homer) == (version == 1 ? target : -1), "D1 rendered acquisition retains its original range while D2 keeps its distance cap");
			if (version == 1) {
				// Native D1 consumes the last main-view list, even after a pause or rear view
				Objects[target].pos = { F1_0, 0, 8 * F1_0 };
				Objects[better].pos = { 0, 0, 9 * F1_0 };
				obj_relink(better, 0);
				auto main_candidates = [&](std::initializer_list<int> candidates) {
#ifdef DXX_BUILD_DESCENT_II
					Window_rendered_data[0].num_objects = static_cast<int>(candidates.size());
					std::copy(candidates.begin(), candidates.end(), Window_rendered_data[0].rendered_objects);
#else
					Num_rendered_objects = static_cast<int>(candidates.size());
					std::copy(candidates.begin(), candidates.end(), Ordered_rendered_object_list);
#endif
				};
				main_candidates({ target });
#ifdef DXX_BUILD_DESCENT_II
				Window_rendered_data[0].time = timer_query() - 2 * F1_0;
#endif
				require(find_homing_object(&origin, &homer) == target, "native main-view candidates do not expire with wall-clock time");
				for (int view_case = 0; view_case < 3; ++view_case) {
#ifdef DXX_BUILD_DESCENT_II
					// Keep the HUD camera eligible under D2 rules; it must never replace the D1 main list
					Window_rendered_data[1].time = timer_query() + F1_0;
					Window_rendered_data[1].viewer = ConsoleObject;
					Window_rendered_data[1].rear_view = 0;
					Window_rendered_data[1].num_objects = 1;
					Window_rendered_data[1].rendered_objects[0] = better;
					Window_rendered_data[0].time = timer_query() + F1_0;
					Window_rendered_data[0].rear_view = view_case == 1;
					Window_rendered_data[0].viewer = view_case == 2 ? &Objects[better] : ConsoleObject;
#endif
					require(find_homing_object(&origin, &homer) == target, "native main-view candidates survive rear/external views and competing HUD cameras");
					main_candidates({});
					require(find_homing_object(&origin, &homer) == -1, "an empty native main list does not trigger a complete scan or a HUD-camera search");
					main_candidates({ target });
				}
				Objects[better].pos = { -F1_0, 0, 8 * F1_0 };
				main_candidates({ target, better });
				require(find_homing_object(&origin, &homer) == better, "equal native alignments prefer the last candidate in render order");
				main_candidates({ better, target });
				require(find_homing_object(&origin, &homer) == target, "reversing equal candidates reverses the native winner");
				main_candidates({ target });
#ifdef DXX_BUILD_DESCENT_II
				std::memset(Window_rendered_data, 0, sizeof(Window_rendered_data));
#endif
				// Restore the surrounding fixture's target and blocked second-segment candidate
				Objects[target].pos = { 0, 0, 300 * F1_0 };
				Objects[better].pos = { 0, 0, 20 * F1_0 };
				obj_relink(better, 1);
			}
			homer.ctype.laser_info.parent_num = target;
			homer.ctype.laser_info.parent_type = OBJ_ROBOT;
			homer.ctype.laser_info.track_goal = -1;
			ConsoleObject->pos = { 0, 0, 8 * F1_0 };
			require(find_homing_object(&origin, &homer) == 0 && track_track_goal(-1, &homer, &dot, scan_frame, original) == 0, "robot-fired missiles acquire and reacquire the player");
			Players[0].flags = PLAYER_FLAGS_CLOAKED;
			require(find_homing_object(&origin, &homer) == -1 && track_track_goal(0, &homer, &dot, scan_frame, original) == -1, "robot-fired missiles lose the cloaked player");
			Players[0].flags = 0;
			homer.ctype.laser_info.parent_num = 0;
			homer.ctype.laser_info.parent_type = OBJ_PLAYER;
			homer.ctype.laser_info.track_goal = target;
			Objects[target].pos = { 0, 0, 8 * F1_0 };
			Game_mode = GM_NETWORK | GM_MULTI_COOP | GM_MULTI_ROBOTS;
			require(find_homing_object(&origin, &homer) == target, "cooperative player missiles acquire robots");
			require(track_track_goal(target, &homer, &dot, scan_frame ^ 1, original) == -1 && track_track_goal(target, &homer, &dot, scan_frame, original) == target, "cooperative tracking preserves the native rescan cadence");
			Game_mode = 0;
#ifdef DXX_BUILD_DESCENT_II
			Robot_info[0].companion = 1;
			require(find_homing_object_complete(&origin, &homer, OBJ_ROBOT, -1) == -1 && track_track_goal(target, &homer, &dot, scan_frame, original) == -1, "optional companions remain excluded from player homing targets in either profile");
			Robot_info[0].companion = 0;
#endif
			homer.id = CONCUSSION_ID;
			homer.lifeleft = 10 * F1_0;
			Weapon_info[CONCUSSION_ID].homing_flag = 0;
			Weapon_info[CONCUSSION_ID].damage_radius = 0;
			Weapon_info[CONCUSSION_ID].speed[Difficulty_level] = 20 * F1_0;
			for (const int thrust : { 0, 1 }) {
				Weapon_info[CONCUSSION_ID].thrust = thrust;
				homer.mtype.phys_info.velocity = { 0, 0, 40 * F1_0 };
				Laser_do_weapon_sequence(&homer, 0, F1_0 / 25, 0, original);
				require(homer.mtype.phys_info.velocity.z == (version == 1 && !thrust ? 40 : 20) * F1_0, "D1 caps only thrust-driven weapons while D2 retains its general speed cap");
			}
			require(d_rand_get_call_count() == rng_before, "target selection and retention do not consume simulation RNG");
		}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
	std::memset(Window_rendered_data, 0, sizeof(Window_rendered_data));
#else
	Num_rendered_objects = 0;
#endif
}

// Exercise actual collision traversal and texture sampling in both executables
static void test_wall_crossing()
{
	init_test_corridor();
	Segments[0].sides[4].wall_num = 0;
	Segments[1].sides[5].wall_num = 1;
	Game_mode = 0;
	std::memset(&cheats, 0, sizeof(cheats));
	Num_walls = 2;
	std::memset(Walls, 0, 2 * sizeof(Walls[0]));
	Walls[0].segnum = 0;
	Walls[0].sidenum = 4;
	Walls[1].segnum = 1;
	Walls[1].sidenum = 5;
	Walls[0].type = Walls[1].type = WALL_CLOSED;
	const grs_bitmap old_base = GameBitmaps[1], old_overlay = GameBitmaps[2];
	const bitmap_index old_texture1 = Textures[1], old_texture2 = Textures[2];
	bytes base(64 * 64), overlay(64 * 64);
	Textures[1].index = 1;
	Textures[2].index = 2;
	texmerge_init(4);
	auto query = [](int flags) {
		vms_vector start = { 0, 0, 0 }, end = { 0, 0, 20 * F1_0 };
		fvi_query request = {};
		request.p0 = &start;
		request.p1 = &end;
		request.startseg = 0;
		request.thisobjnum = -1;
		request.flags = flags;
		fvi_info hit = {};
		const int result = find_vector_intersection(&request, &hit);
		require(result == HIT_WALL || (result == HIT_NONE && hit.hit_seg == 1), "collision reaches the wall or the adjoining fixture cell");
		return result;
	};
#ifdef DXX_BUILD_DESCENT_II
	Mission mission = {};
	Current_mission = &mission;
	for (int version : { 2, 1, 2 }) {
		mission.descent_version = version;
#else
	{
#endif
		for (int encoded = 0; encoded < 2; ++encoded) {
			Segments[0].sides[4].tmap_num2 = 0;
			for (int pixel = 0; pixel < 64 * 64; ++pixel) base[pixel] = pixel % 64 < 32 ? TRANSPARENCY_COLOR : 7;
			gr_init_bitmap(&GameBitmaps[1], BM_LINEAR, 0, 0, 64, 64, 64, base.data());
			GameBitmaps[1].bm_flags = BM_FLAG_TRANSPARENT;
			if (encoded) require(gr_bitmap_rle_compress(&GameBitmaps[1]) != 0, "encode the actual collision bitmap as RLE");
			rle_cache_flush();
			Walls[0].type = WALL_CLOSED;
			for (auto &uv : Segments[0].sides[4].uvls) uv.u = F1_0 / 4;
			require(query(FQ_TRANSPOINT) == HIT_NONE, "transparent pixels admit a point query through a solid wall");
			require(query(0) == HIT_WALL, "ordinary collision still blocks at a transparent wall");
			for (auto &uv : Segments[0].sides[4].uvls) uv.u = -3 * F1_0 / 4;
			require(query(FQ_TRANSPOINT) == HIT_NONE, "negative UVs wrap to the same transparent pixel");
			for (auto &uv : Segments[0].sides[4].uvls) uv.u = 3 * F1_0 / 4;
			require(query(FQ_TRANSPOINT) == HIT_WALL, "opaque pixels stop a point query");
			require(query(FQ_TRANSWALL) == HIT_NONE, "whole-wall visibility queries ignore the sampled opaque pixel");
			Walls[0].type = WALL_DOOR;
			Walls[0].state = WALL_DOOR_CLOSED;
			require(query(FQ_TRANSPOINT) == HIT_WALL, "closed door opaque pixels block projectiles");
			for (auto &uv : Segments[0].sides[4].uvls) uv.u = F1_0 / 4;
			require(query(FQ_TRANSPOINT) == HIT_NONE, "closed door transparent pixels retain native D1 point-query semantics");
			Walls[0].state = WALL_DOOR_OPENING;
			require(query(FQ_TRANSPOINT) == HIT_NONE, "opening door transparent pixels admit projectiles");
			Walls[0].type = WALL_OPEN;
			require(query(0) == HIT_NONE, "open doorway admits ordinary collision traversal");
			Walls[0].type = WALL_CLOSED;
			std::fill(base.begin(), base.end(), 7);
			for (int y = 0; y < 64; ++y)
				for (int x = 0; x < 64; ++x) overlay[y * 64 + x] = x >= 32 && y >= 32 ? 254 : 7;
			gr_init_bitmap(&GameBitmaps[1], BM_LINEAR, 0, 0, 64, 64, 64, base.data());
			gr_init_bitmap(&GameBitmaps[2], BM_LINEAR, 0, 0, 64, 64, 64, overlay.data());
			GameBitmaps[2].bm_flags = BM_FLAG_SUPER_TRANSPARENT;
			if (encoded) {
				require(gr_bitmap_rle_compress(&GameBitmaps[1]) != 0 && gr_bitmap_rle_compress(&GameBitmaps[2]) != 0, "encode both layers of the merged collision texture");
			}
			rle_cache_flush();
			texmerge_flush();
			for (int rotation = 0; rotation < 4; ++rotation) {
				Segments[0].sides[4].tmap_num2 = static_cast<short>(2 | (rotation << 14));
				require(query(FQ_TRANSPOINT) == (rotation == 3 ? HIT_NONE : HIT_WALL), "rotated overlay supertransparency controls crossing through an opaque base");
			}
		}
	}
#ifdef DXX_BUILD_DESCENT_II
	Current_mission = nullptr;
#endif
	texmerge_close();
	rle_cache_flush();
	GameBitmaps[1] = old_base;
	GameBitmaps[2] = old_overlay;
	Textures[1] = old_texture1;
	Textures[2] = old_texture2;
}

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
	set_int(pig, effects, 2);
	const size_t clip = effects + 4;
	set_int(pig, clip, F1_0 / 2);
	set_int(pig, clip + 4, 2);
	set_int(pig, clip + 8, F1_0 / 4);
	set_short(pig, clip + 18, 4);
	set_short(pig, clip + 20, 5);
	set_short(pig, clip + 90, 1); // changing wall texture
	set_short(pig, clip + 92, -1);
	set_int(pig, clip + 102, 3); // destroyed texture
	const size_t lava = clip + 130;
	set_int(pig, lava, F1_0 / 2);
	set_int(pig, lava + 4, 2);
	set_int(pig, lava + 8, F1_0 / 4);
	set_short(pig, lava + 18, 4);
	set_short(pig, lava + 20, 5);
	set_short(pig, lava + 90, 333); // D1 misc11 -> D2 texture 409, in a different effect slot
	set_short(pig, lava + 92, -1);
	set_int(pig, lava + 98, -1);
	set_int(pig, lava + 102, -1);
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
	// Remap D2's border index to an escaped RLE value to exercise size growth
	std::memcpy(palette.data() + 154 * 3, gr_palette + 240 * 3, 3);
	write_fixture("groupa.256", palette);
}

static void test_d1_monitors()
{
	const std::string original_write_dir = PHYSFS_getWriteDir();
	const std::string fixture_dir = original_write_dir + "/d1-monitor-fixtures";
	require(PHYSFS_mkdir("d1-monitor-fixtures") && PHYSFS_mount(fixture_dir.c_str(), nullptr, 0) && PHYSFS_setWriteDir(fixture_dir.c_str()), "isolate monitor PIG from exit-model fixtures");
	make_d1_monitor_fixture();
	hashtable_init(&AllBitmapsNames, MAX_BITMAP_FILES);
	Num_bitmap_files = 0;
	unsigned char stock_pixels[10][4];
	for (int i = 0; i < 10; ++i) {
		char name[13];
		std::snprintf(name, sizeof(name), i < 6 ? "monitor#%d" : "light%d", i);
		std::memset(stock_pixels[i], 40 + i, sizeof(stock_pixels[i]));
		piggy_register_bitmap(&GameBitmaps[i], name, 1);
	}
	NumTextures = 410;
	Textures[0].index = 1;
	Textures[1].index = 3;
	Textures[2].index = 4;
	Textures[3].index = 6;
	Textures[4].index = 7;
	Textures[409].index = 8;
	for (int i = 0; i < NumTextures; ++i) TmapInfo[i].destroyed = -1;
	TmapInfo[2].destroyed = 3;
	TmapInfo[3].destroyed = 4;
	Num_effects = 3;
	std::memset(Effects, 0, sizeof(Effects));
	Effects[0].vc.num_frames = 2;
	Effects[0].vc.frame_time = F1_0 / 8;
	Effects[0].vc.frames[0].index = 1;
	Effects[0].vc.frames[1].index = 2;
	Effects[0].changing_wall_texture = 0;
	Effects[0].changing_object_texture = -1;
	Effects[0].dest_bm_num = 1;
	Effects[0].crit_clip = -1;
	Effects[1].vc.num_frames = -1;
	Effects[1].changing_wall_texture = -1;
	Effects[1].changing_object_texture = -1;
	Effects[2] = Effects[0];
	Effects[2].changing_wall_texture = 409;
	Effects[2].dest_bm_num = -1;
	Effects[2].vc.frames[0].index = 8;
	Effects[2].vc.frames[1].index = 9;
	const eclip original_effect = Effects[0];
	const eclip original_lava = Effects[2];
	for (int pass = 0; pass < 2; ++pass) {
		for (int i = 0; i < 10; ++i) {
			gr_init_bitmap(&GameBitmaps[i], BM_LINEAR, 0, 0, 2, 2, 2, stock_pixels[i]);
			piggy_bitmap_set_file_state(i, 100 + i, BM_FLAG_TRANSPARENT);
		}
		unsigned char light_rle[] = { 14, 0, 0, 0, 4, 4, 154, 0xe1, 255, 0xe0, 154, 0xe1, 254, 0xe0 };
		unsigned char light_raw[] = { 154, 255, 154, 254 };
		GameBitmaps[6].bm_data = light_rle;
		GameBitmaps[6].bm_flags = BM_FLAG_RLE | BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT;
		GameBitmaps[7].bm_data = light_raw;
		GameBitmaps[7].bm_flags = BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT;
		for (int i : { 6, 7 }) piggy_bitmap_set_file_state(i, 100 + i, GameBitmaps[i].bm_flags);
		load_d1_bitmap_replacements();
		for (int i : { 6, 7 }) {
			grs_bitmap *light = &GameBitmaps[i];
			if (light->bm_flags & BM_FLAG_RLE) light = rle_expand_texture(light);
			require(light->bm_data[0] == 240 && light->bm_data[1] == 255 && light->bm_data[2] == 240 && light->bm_data[3] == 254, "destroyed D2 light uses D1 colors and preserves both transparency indices");
			require(piggy_bitmap_get_offset(i) == 0 && (GameBitmaps[i].bm_flags & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT)) == (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT), "remapped light keeps transparency flags and cannot page in unconverted D2 pixels");
		}
		require(GameBitmaps[6].bm_data[0] == 16, "converted light RLE can grow without overlapping the next bitmap");
		require(light_rle[6] == 154 && light_raw[0] == 154, "retained light conversion leaves source pixels unchanged");
		for (int i : { 1, 2, 3 }) {
			require(GameBitmaps[i].bm_data == stock_pixels[i] && GameBitmaps[i].bm_data[0] == 40 + i, "generic D1 replacement preserves monitor frames and destroyed image");
			require(piggy_bitmap_get_offset(i) == 100 + i && piggy_bitmap_get_file_flags(i) == BM_FLAG_TRANSPARENT, "protected monitor paging state preserved");
		}
		require(GameBitmaps[4].bm_data[0] == 12 && GameBitmaps[5].bm_data == GameBitmaps[4].bm_data, "ordinary walls and unprotected animation clones still replaced");
		d1_in_d2_apply_effects(1);
		d1_in_d2_asset_stats stats = {};
		d1_in_d2_get_stats(&stats);
		require(GameBitmaps[8].bm_data[0] == 13 && GameBitmaps[9].bm_data[0] == 14, "relocated lava effect loads D1 pixels into the mapped D2 animation slots");
		require(stats.effect_frames_applied == 4 && stats.effect_frames_skipped == 0, "dedicated D1 effect loader restores monitor and relocated lava frames");
		require(GameBitmaps[1].bm_data[0] == 13 && GameBitmaps[2].bm_data[0] == 14, "D1 animation frame pixels restored");
		require(GameBitmaps[3].bm_data == stock_pixels[3], "destroyed monitor remains protected after frame restoration");
		init_special_effects();
		Effects[0].frame_count = 0;
		FrameTime = F1_0 / 4 + 1;
		do_special_effects();
		require(Textures[0].index == 2 && GameBitmaps[Textures[0].index].bm_data[0] == 14, "monitor animation selects restored D1 frame");
		require(Textures[409].index == 9 && GameBitmaps[Textures[409].index].bm_data[0] == 14, "lava animation continues using restored D1 colors");
		Effects[0].flags = EF_ONE_SHOT;
		Effects[0].segnum = 0;
		Effects[0].sidenum = 0;
		Segments[0].sides[0].tmap_num2 = 0x4000 | 2;
		do_special_effects();
		require(Segments[0].sides[0].tmap_num2 == (0x4000 | 1) && GameBitmaps[Textures[1].index].bm_data[0] == 43, "completed monitor effect selects protected destroyed image and retains orientation");
		d1_in_d2_apply_effects(0);
		require(std::memcmp(&Effects[0], &original_effect, sizeof(eclip)) == 0, "leaving D1 emulation restores original D2 effect");
		require(std::memcmp(&Effects[2], &original_lava, sizeof(eclip)) == 0, "leaving D1 restores relocated effects beyond the D1 effect count");
		Textures[0].index = 1;
		if (pass == 1) {
			// A new PIG or custom image can replace the slot before arena cleanup
			GameBitmaps[7].bm_data = light_raw;
			piggy_bitmap_set_file_state(7, 207, BM_FLAG_TRANSPARENT);
		}
		free_bitmap_replacements();
		require(piggy_bitmap_get_offset(6) == 106 && GameBitmaps[6].bm_flags == BM_FLAG_PAGED_OUT && piggy_bitmap_get_file_flags(6) == (BM_FLAG_RLE | BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT), "light cleanup restores original D2 paging for the next level");
		if (pass == 0)
			require(piggy_bitmap_get_offset(7) == 107 && GameBitmaps[7].bm_flags == BM_FLAG_PAGED_OUT, "raw light cleanup restores D2 paging");
		else
			require(piggy_bitmap_get_offset(7) == 207 && GameBitmaps[7].bm_data == light_raw, "light cleanup preserves a newer replacement");
	}
	free_bitmap_replacements();
	free_d1_tmap_nums();
	hashtable_free(&AllBitmapsNames);
	for (int i = 0; i < 10; ++i) GameBitmaps[i].bm_data = nullptr;
	require(PHYSFS_delete("descent.pig") != 0 && PHYSFS_delete("palette.256") != 0, "remove D1 monitor fixtures");
	require(PHYSFS_delete("groupa.256") != 0, "remove retained light palette fixture");
	require(PHYSFS_setWriteDir(original_write_dir.c_str()) && PHYSFS_unmount(fixture_dir.c_str()), "restore fixture search path");
}

static void test_d1_native_bitmaps()
{
	bytes pig(12 + 3 * 17);
	set_int(pig, 0, 4);
	set_int(pig, 4, 3);
	const size_t raw_header = 12;
	const size_t rle_header = raw_header + 17;
	const size_t anim_header = rle_header + 17;
	std::memcpy(pig.data() + raw_header, "cockpit", 7);
	pig[raw_header + 8] = 128;
	pig[raw_header + 9] = 64;
	pig[raw_header + 10] = 1;
	pig[raw_header + 11] = BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT;
	pig[raw_header + 12] = 154;
	std::memcpy(pig.data() + rle_header, "sprdblob", 8);
	pig[rle_header + 9] = 3;
	pig[rle_header + 10] = 1;
	pig[rle_header + 11] = BM_FLAG_RLE | BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT;
	set_int(pig, rle_header + 13, 320);
	std::memcpy(pig.data() + anim_header, "misc11", 6);
	pig[anim_header + 8] = 64 | 3;
	pig[anim_header + 9] = 2;
	pig[anim_header + 10] = 1;
	set_int(pig, anim_header + 13, 331);
	bytes cockpit(320, 154);
	cockpit[318] = 254;
	cockpit[319] = 255;
	append(pig, cockpit);
	const bytes encoded = { 11, 0, 0, 0, 6, 154, 225, 254, 225, 255, 224 };
	append(pig, encoded);
	append(pig, { 2, 3 });
	bytes palette(9472);
	for (int i = 0; i < 768; ++i) palette[i] = static_cast<unsigned char>(i % 64);
	for (size_t i = 768; i < palette.size(); ++i) palette[i] = static_cast<unsigned char>(i % 251);
	write_fixture("native-d1.pig", pig);
	write_fixture("native-d1.256", palette);
	unsigned char previous_palette[768];
	std::memcpy(previous_palette, gr_palette, sizeof(previous_palette));
	std::memset(gr_palette, 0, sizeof(previous_palette));
	const int previous_count = Num_bitmap_files;
	Num_bitmap_files = 0;
	grs_bitmap previous_live_bitmap;
	std::memcpy(&previous_live_bitmap, &GameBitmaps[1], sizeof(previous_live_bitmap));
	d1_bitmap_generation *generation = d1_in_d2_read_bitmaps("native-d1.pig", "native-d1.256");
	require(generation && generation->bitmap_count == 3, "prepare every D1 bitmap without a D2 bitmap registry");
	require(generation->bitmaps[1].bm_w == 320 && generation->bitmaps[1].bm_rowsize == 320, "native D1 cockpit preserves its large-width flag");
	require(std::memcmp(generation->bitmaps[1].bm_data, cockpit.data(), cockpit.size()) == 0 && generation->bitmaps[1].avg_color == 154, "native D1 bitmap colors and transparency survive an unrelated live palette");
	require(std::memcmp(generation->bitmaps[2].bm_data, encoded.data(), encoded.size()) == 0, "native D1 RLE data retains original palette indices");
	require(std::strcmp(generation->names[1], "cockpit") == 0 && std::strcmp(generation->names[3], "misc11#3") == 0 && generation->bitmaps[3].bm_data[0] == 2, "native D1 bitmap source indices and animation names are preserved");
	require(std::memcmp(generation->palette, palette.data(), sizeof(generation->palette)) == 0 && gr_palette[1] == 0, "preparation owns the original D1 palette without publishing it");
	require(std::memcmp(generation->fade_table, palette.data() + 768, sizeof(generation->fade_table)) == 0, "original D1 light-level lookup colors are retained");
	require(Num_bitmap_files == 0 && std::memcmp(&GameBitmaps[1], &previous_live_bitmap, sizeof(previous_live_bitmap)) == 0, "preparation leaves active engine bitmap tables untouched");
	set_int(pig, rle_header + 13, static_cast<int>(pig.size()));
	write_fixture("native-d1.pig", pig);
	require(!d1_in_d2_read_bitmaps("native-d1.pig", "native-d1.256"), "invalid bitmap after a valid frame rejects the whole staged generation");
	require(generation->bitmaps[1].bm_data[0] == 154, "failed preparation leaves the earlier generation intact");
	d1_in_d2_free_bitmaps(generation);
	Num_bitmap_files = previous_count;
	std::memcpy(gr_palette, previous_palette, sizeof(previous_palette));
	require(PHYSFS_delete("native-d1.pig") && PHYSFS_delete("native-d1.256"), "remove native D1 bitmap fixtures");
}

static void test_d1_text_resources()
{
	bytes text;
	for (int i = 0; i < 622; ++i) {
		const std::string line = i == 116 ? "SPREADFIRE\r\n" : "original-" + std::to_string(i) + "\\nline\\tend\r\n";
		text.insert(text.end(), line.begin(), line.end());
	}
	write_fixture("d1-text.tex", text);
	require(d1_in_d2_read_text("d1-text.tex", 0), "read original text without a D2 text bank");
	require(std::strcmp(TXT_NEW_GAME, "original-0\nline\tend") == 0, "D1 text preserves escapes and handles CRLF");
	require(std::strcmp(TXT_SECRET_EXIT, "original-547\nline\tend") == 0, "D2 symbolic text resolves to the original D1 ordinal");
	require(std::strcmp(TXT_W_SPREADFIRE_S, "SPREAD") == 0, "original Spreadfire label fits the D1 weapon box");
	require(std::strcmp(TXT_HELP, "Type '%s -help' for a list of command-line options.") == 0, "format-incompatible text uses its call-site contract");
	require(std::strcmp(TXT_CONTROL_WINJOY, "Windows 95 Joystick") == 0, "D2-only controls use built-in text without D2 assets");
	write_fixture("d1-text.tex", { 'b', 'a', 'd', '\n' });
	require(!d1_in_d2_read_text("d1-text.tex", 0) && std::strcmp(TXT_SECRET_EXIT, "original-547\nline\tend") == 0, "failed text preparation leaves the active text intact");
	free_text();
	require(!d1_in_d2_free_text() && Text_string[350] == nullptr, "D1 text retirement avoids the D2 separately allocated string cleanup");
	require(PHYSFS_delete("d1-text.tex"), "remove text fixture");
}

static void test_loaded_d1_weapon_firing(bool d1)
{
	Player_num = 0;
	Player_is_dead = 0;
	Difficulty_level = 2;
	Game_mode = 0;
	FrameTime = F1_0 / 64;
	player &player = Players[Player_num];
	ConsoleObject = Viewer = &Objects[player.objnum];
	player.primary_weapon = SPREADFIRE_INDEX;
	player.primary_weapon_flags |= HAS_SPREADFIRE_FLAG;
	player.energy = 100 * F1_0;
	Next_laser_fire_time = GameTime64;
	const int accounting = Primary_weapon_to_weapon_info[SPREADFIRE_INDEX];
	require(accounting == 12, "Spreadfire accounting remains record 12");
	Num_awareness_events = 0;
	do_laser_firing_player();
	require(Num_awareness_events == (d1 ? 0 : 3), "D1 leaves awareness to impacts while each D2 Spreadfire shot retains its firing event");
	int shots = 0;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const object &shot = Objects[i];
		if (shot.type == OBJ_WEAPON && shot.ctype.laser_info.parent_num == player.objnum) {
			++shots;
			require(shot.id == (d1 ? 20 : 12), "normal player firing selects the profile's Spreadfire projectile");
			require(shot.shields == Weapon_info[shot.id].strength[Difficulty_level], "Spreadfire damage comes from its projectile record");
		}
	}
	require(shots == 3, "normal Spreadfire firing creates all three bolts");
	require(player.energy == 100 * F1_0 - Weapon_info[accounting].energy_usage / Weapon_info[accounting].fire_count, "Spreadfire charges the accounting record rather than the projectile");
	require(Next_laser_fire_time == GameTime64 + Weapon_info[accounting].fire_wait, "Spreadfire uses the accounting record's cooldown");

	// A target straight ahead of the missile's orientation but perpendicular to its velocity
	vms_vector position;
	vm_vec_scale_add(&position, &ConsoleObject->pos, &ConsoleObject->orient.fvec, -2 * F1_0);
	const int missile = Laser_create_new(&ConsoleObject->orient.fvec, &position, ConsoleObject->segnum, player.objnum, HOMING_ID, 0);
	require(missile >= 0, "create a homing missile in the loaded level");
	object &homer = Objects[missile];
	homer.pos = position;
	homer.orient = ConsoleObject->orient;
	vm_vec_copy_scale(&homer.mtype.phys_info.velocity, &ConsoleObject->orient.rvec, 40 * F1_0);
	homer.ctype.laser_info.creation_time = GameTime64 - F1_0;
	homer.ctype.laser_info.track_goal = player.objnum;
	homer.lifeleft = 10 * F1_0;
	player.flags &= ~PLAYER_FLAGS_CLOAKED;
	Laser_do_weapon_sequence(&homer, 1, FrameTime, missile ^ 1, 1);
	require(homer.lifeleft == 10 * F1_0 - (d1 ? 4 * FrameTime : 0), "D1 steering measures the velocity turn with its capped lifetime cost; D2 keeps its existing rule");
}

static void check_profile_fonts(bool d1)
{
	gamefont_choose_game_font(640, 480);
	const char *prefix = d1 ? "d1-original/" : "d2-original/";
	for (int i = 0; i < MAX_FONTS; ++i) {
		const char *name = gamefont_curfontname(i);
		require(std::strncmp(name, prefix, std::strlen(prefix)) == 0, "font resolves from the selected game's own archive");
		require((std::strstr(name, "h.fnt") == nullptr) == d1, "original D1 does not inherit D2 high-resolution fonts");
		PHYSFS_file *file = PHYSFSX_openReadBuffered(name);
		require(file != nullptr, "open selected source font");
		require(PHYSFS_seek(file, 8) != 0, "seek original font geometry");
		const int width = PHYSFSX_readShort(file), height = PHYSFSX_readShort(file);
		PHYSFS_close(file);
		require(Gamefonts[i] && Gamefonts[i]->ft_w == width && Gamefonts[i]->ft_h == height, "font geometry matches the original archive definition");
	}
	// Remapping reopens saved font paths and must retain the full archive prefix
	gr_remap_color_fonts();
	gr_remap_mono_fonts();
	gr_set_curfont(GAME_FONT);
	gr_set_fontcolor(1, -1);
	gr_string(0, 0, "Original profile font");
	load_palette(MENU_PALETTE, 0, 1);
	if (d1) {
		ubyte original_palette[768];
		PHYSFS_file *file = PHYSFSX_openReadBuffered("d1-original/palette.256");
		require(file && PHYSFS_readBytes(file, original_palette, sizeof(original_palette)) == sizeof(original_palette), "read original D1 menu palette");
		PHYSFS_close(file);
		require(std::memcmp(gr_palette, original_palette, sizeof(original_palette)) == 0, "interactive menu palette uses original D1 colors");
	}
	nm_draw_background1(Menu_pcx_name);
	require(nm_background1.bm_data && (!d1 || (nm_background1.bm_w == 320 && nm_background1.bm_h == 200)), "draw the original fullscreen menu without substituting D2 art");
	nm_draw_background1(STARS_BACKGROUND);
	load_palette(MENU_PALETTE, 0, 1);
	nm_draw_background(0, 0, 320, 200);
	require(nm_background.bm_data && (!d1 || (nm_background.bm_w == 320 && nm_background.bm_h == 200)), "draw the original framed menu background");
}

static void check_profile_resources(bool d1)
{
	char path[PATH_MAX];
	const char *prefix = d1 ? "d1-original/" : "d2-original/";
	require(d1_in_d2_presentation_resource("font1-1.fnt", path, sizeof(path)) != 0, "resolve the selected game's base font");
	require(std::strncmp(path, prefix, std::strlen(prefix)) == 0, "base resources use the selected profile regardless of archive order");
	if (d1) {
		require(!d1_in_d2_presentation_resource("font1-1h.fnt", path, sizeof(path)), "missing D1 high-resolution art must not fall back to the D2 archive");
		require(d1_in_d2_presentation_resource("brief03.pcx", path, sizeof(path)) != 0, "resolve the original D1 robot briefing background");
		grs_bitmap screen = {};
		ubyte colors[768];
		require(pcx_read_bitmap(path, &screen, BM_LINEAR, colors) == PCX_ERROR_NONE, "decode the original D1 briefing image and its palette");
		require(screen.bm_w == 320 && screen.bm_h == 200, "retain original registered D1 briefing geometry");
		gr_free_bitmap_data(&screen);
	}
	bytes custom = { 'D', 'H', 'F' };
	const char name[13] = "font1-1.fnt";
	custom.insert(custom.end(), name, name + sizeof(name));
	append_int(custom, 4);
	append(custom, { 19, 20, 21, 22 });
	write_fixture("presentation-override.hog", custom);
	require(PHYSFS_mount("presentation-override.hog", nullptr, 0) != 0, "mount a mission's same-named presentation replacement");
	require(d1_in_d2_presentation_resource("FONT1-1.FNT", path, sizeof(path)) && std::strcmp(path, "FONT1-1.FNT") == 0, "custom resource precedence remains case insensitive");
	PHYSFS_file *file = PHYSFSX_openReadBuffered(path);
	ubyte replacement[4] = {};
	require(file && PHYSFS_readBytes(file, replacement, sizeof(replacement)) == 4 && replacement[0] == 19, "resolved custom resource supplies the mission's bytes");
	PHYSFS_close(file);
	require(PHYSFS_unmount("presentation-override.hog") != 0, "retire mission presentation replacement");
	require(d1_in_d2_presentation_resource("font1-1.fnt", path, sizeof(path)) && std::strncmp(path, prefix, std::strlen(prefix)) == 0, "custom-to-stock transition restores the selected base resource");
}

static const char *Cockpit_dump_prefix;

static void check_profile_cockpit(bool d1)
{
	const auto saved_player = Players[Player_num];
	const int saved_mode = PlayerCfg.CurrentCockpitMode, saved_screen = Screen_mode;
	const int saved_hires = GameArg.GfxHiresGFXAvailable, saved_hud = PlayerCfg.HudMode;
	const int saved_rear = Rear_view;
	object *saved_viewer = Viewer;
	Viewer = &Objects[Players[Player_num].objnum];
	GameArg.GfxHiresGFXAvailable = 1; // D2 startup capability must not choose D1 source layout
	PlayerCfg.HudMode = 0;
	Screen_mode = SCREEN_GAME;
	Game_screen_mode = SM(640, 480);
	GameCfg.TexFilt = 0;
	Players[Player_num].energy = 70 * F1_0;
	Players[Player_num].shields = 80 * F1_0;
	Players[Player_num].flags = PLAYER_FLAGS_BLUE_KEY | PLAYER_FLAGS_GOLD_KEY | PLAYER_FLAGS_RED_KEY | PLAYER_FLAGS_QUAD_LASERS;
	Players[Player_num].laser_level = 2;
	Players[Player_num].primary_weapon = LASER_INDEX;
	Players[Player_num].secondary_weapon = HOMING_INDEX;
	Players[Player_num].secondary_ammo[HOMING_INDEX] = 7;
	Players[Player_num].secondary_ammo[PROXIMITY_INDEX] = 3;
	Players[Player_num].homing_object_dist = -1;
	for (int mode : { CM_FULL_COCKPIT, CM_STATUS_BAR, CM_REAR_VIEW }) {
		std::fprintf(stderr, "Drawing %s cockpit mode %d: viewport\n", d1 ? "D1" : "D2", mode);
		PlayerCfg.CurrentCockpitMode = mode;
		Rear_view = mode == CM_REAR_VIEW;
		init_cockpit();
		init_gauges();
		std::fprintf(stderr, "Cockpit mode %d: background\n", mode);
		if (d1 && mode == CM_STATUS_BAR)
			require(Screen_3d_window.cv_bitmap.bm_h == int(480 * 2 / 2.72), "D1 status-bar viewport scales from the source artwork, not D2 high-resolution capability");
		gr_set_current_canvas(nullptr);
		gr_clear_canvas(BM_XRGB(0, 0, 20));
		gr_set_current_canvas(&Screen_3d_window);
		render_frame(0, 0);
		gr_set_current_canvas(nullptr);
		update_cockpits();
		std::fprintf(stderr, "Cockpit mode %d: gauges\n", mode);
		if (mode != CM_REAR_VIEW) render_gauges();
		require(glGetError() == GL_NO_ERROR, "cockpit and gauge draw uses valid graphics resources");
		std::fprintf(stderr, "Cockpit mode %d: pixels\n", mode);
		if (d1 && mode == CM_FULL_COCKPIT) {
			grs_bitmap *key = &GameBitmaps[Gauges[24].index];
			if (key->bm_flags & BM_FLAG_RLE) key = rle_expand_texture(key);
			int checked = 0;
			for (int y = 0; y < key->bm_h; ++y)
				for (int x = 0; x < key->bm_w; ++x) {
					const int color = key->bm_data[y * key->bm_rowsize + x];
					if (color == TRANSPARENCY_COLOR) continue;
					ubyte pixel[3];
					glReadPixels(int((45 + x + .5) * 2), 479 - int((152 + y + .5) * 2.4), 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
					for (int c = 0; c < 3; ++c)
						require(std::abs(int(pixel[c]) - int(gr_palette[color * 3 + c]) * 4) <= 3, "D1 key gauge renders original pixels at its original left-side position");
					++checked;
				}
			require(checked > 5, "key placement verification covers opaque source pixels");
		}
		if (mode != CM_REAR_VIEW) {
			std::fprintf(stderr, "Cockpit mode %d: camera\n", mode);
			ubyte surround_before[3], surround_after[3];
			glReadPixels(320, 240, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, surround_before);
			object *viewer = &Objects[Players[Player_num].objnum];
			object *saved_viewer = Viewer;
			Viewer = viewer;
			do_cockpit_window_view(0, viewer, 1, WBU_REAR, "REAR");
			do_cockpit_window_view(1, viewer, 0, WBU_COOP, "FRONT");
			glReadPixels(320, 240, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, surround_after);
			require(std::memcmp(surround_before, surround_after, sizeof(surround_before)) == 0, "cockpit cameras preserve pixels outside their viewports");
			require(Viewer == viewer && glGetError() == GL_NO_ERROR, "shared camera rendering restores the viewer and uses valid cockpit resources");
			Viewer = saved_viewer;
		}
#ifdef HAVE_LIBPNG
		if (Cockpit_dump_prefix) {
			bytes pixels(640 * 480 * 3);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glReadPixels(0, 0, 640, 480, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
			std::string path = std::string(Cockpit_dump_prefix) + (d1 ? "-d1-" : "-d2-") + std::to_string(mode) + ".png";
			png_image image = {};
			image.version = PNG_IMAGE_VERSION;
			image.width = 640;
			image.height = 480;
			image.format = PNG_FORMAT_RGB;
			require(png_image_write_to_file(&image, path.c_str(), 0, pixels.data(), -640 * 3, nullptr) != 0, "write cockpit visual review artifact");
		}
#endif
	}
	close_gauges();
	Players[Player_num] = saved_player;
	PlayerCfg.CurrentCockpitMode = saved_mode;
	PlayerCfg.HudMode = saved_hud;
	Screen_mode = saved_screen;
	GameArg.GfxHiresGFXAvailable = saved_hires;
	Rear_view = saved_rear;
	Viewer = saved_viewer;
	std::fprintf(stderr, "Original %s cockpit/full/status/rear drawing passed\n", d1 ? "D1" : "D2");
}

static bytes snapshot_feature_consumers()
{
	bytes result;
	auto capture = [&](const void *data, size_t size) {
		const auto *first = static_cast<const unsigned char *>(data);
		result.insert(result.end(), first, first + size);
	};
	// Preparation must not modify any live definitions, palette, sample rate,
	// registry counters or model pointers, regardless of the active profile
	for (const auto &entry : { std::make_pair(static_cast<const void *>(Robot_info), sizeof(Robot_info)),
	                           { Robot_joints, sizeof(Robot_joints) },
	                           { Polygon_models, sizeof(Polygon_models) },
	                           { Weapon_info, sizeof(Weapon_info) },
	                           { Powerup_info, sizeof(Powerup_info) },
	                           { Vclip, sizeof(Vclip) },
	                           { Effects, sizeof(Effects) },
	                           { Textures, sizeof(Textures) },
	                           { TmapInfo, sizeof(TmapInfo) },
	                           { ObjBitmaps, sizeof(ObjBitmaps) },
	                           { ObjBitmapPtrs, sizeof(ObjBitmapPtrs) },
	                           { Sounds, sizeof(Sounds) },
	                           { AltSounds, sizeof(AltSounds) },
	                           { GameSounds, sizeof(GameSounds) },
	                           { GameBitmaps, sizeof(GameBitmaps) },
	                           { gr_palette, sizeof(gr_palette) },
	                           { gr_fade_table, sizeof(gr_fade_table) } })
		capture(entry.first, entry.second);
	const int counts[] = { N_robot_types, N_robot_joints, N_polygon_models, N_weapon_types, N_powerup_types,
		                   Num_vclips, Num_effects, NumTextures, N_ObjBitmaps, Num_sound_files, Num_bitmap_files,
		                   GameArg.SndDigiSampleRate, Piggy_hamfile_version, d1_in_d2_use_d1_gameplay() };
	capture(counts, sizeof(counts));
	return result;
}

static void test_guidebot_source()
{
	d1_sound_generation bank = {};
	bank.count = 2;
	bank.bytes = 8;
	bank.data = static_cast<ubyte *>(d_malloc(bank.bytes));
	const ubyte input[] = { 1, 2, 3, 4, 21, 22, 23, 24 };
	std::memcpy(bank.data, input, sizeof(input));
	for (int i = 0; i < 2; ++i) {
		bank.samples[i].data = bank.data + i * 4;
		bank.samples[i].length = 4;
		bank.samples[i].bits = 8;
		bank.samples[i].freq = i ? SAMPLE_RATE_22K : SAMPLE_RATE_11K;
	}
	ubyte *arena = bank.data;
	require(d1_in_d2_prepare_sound_output(&bank, 0) && bank.data == arena && bank.samples[0].freq == SAMPLE_RATE_11K,
	        "mixer preparation retains original per-sample rates and data");
	require(d1_in_d2_prepare_sound_output(&bank, SAMPLE_RATE_22K) && bank.bytes == 12 &&
	            bank.samples[0].length == 8 && bank.samples[0].data[1] == 1 && bank.samples[0].data[2] == 2 &&
	            bank.samples[1].length == 4 && std::memcmp(bank.samples[1].data, input + 4, 4) == 0,
	        "mixed-rate preparation resamples only according to each source rate");
	arena = bank.data;
	require(d1_in_d2_prepare_sound_output(&bank, SAMPLE_RATE_22K) && bank.data == arena, "output preparation is idempotent");
	require(d1_in_d2_prepare_sound_output(&bank, SAMPLE_RATE_11K) && bank.bytes == 6 &&
	            bank.samples[0].length == 4 && std::memcmp(bank.samples[0].data, input, 4) == 0 &&
	            bank.samples[1].length == 2 && bank.samples[1].data[0] == 21 && bank.samples[1].data[1] == 23,
	        "22 kHz sources also retain their duration at an 11 kHz output rate");
	arena = bank.data;
	bank.samples[1].freq = 0;
	require(!d1_in_d2_prepare_sound_output(&bank, SAMPLE_RATE_22K) && bank.data == arena && bank.samples[0].length == 4,
	        "invalid later source rate rejects the whole conversion before mutating earlier samples");
	bank.samples[1].freq = SAMPLE_RATE_11K;
	bank.samples[0].length = INT_MAX;
	require(!d1_in_d2_prepare_sound_output(&bank, SAMPLE_RATE_22K) && bank.data == arena,
	        "oversized output is rejected before sample reads or allocation");
	d_free(bank.data);
	bytes disk_robot(480);
	for (size_t i = 0; i < disk_robot.size(); ++i) disk_robot[i] = static_cast<unsigned char>(i * 37 + 19);
	write_fixture("robot-codec.bin", disk_robot);
	PHYSFS_file *robot_file = PHYSFS_openRead("robot-codec.bin");
	robot_info decoded = {};
	require(robot_file && robot_info_read_n(&decoded, 1, robot_file) == 1 && PHYSFS_tell(robot_file) == 480,
	        "runtime robot sound widths do not change disk decoding");
	PHYSFS_close(robot_file);
	robot_file = PHYSFS_openWrite("robot-roundtrip.bin");
	require(robot_file && robot_info_write_n(&decoded, 1, robot_file) && PHYSFS_tell(robot_file) == 480,
	        "robot writer emits the original fixed disk record");
	PHYSFS_close(robot_file);
	robot_file = PHYSFS_openRead("robot-roundtrip.bin");
	bytes roundtrip(480);
	require(robot_file && PHYSFS_readBytes(robot_file, roundtrip.data(), roundtrip.size()) == 480 && roundtrip == disk_robot,
	        "every original robot field, signed byte, pad and joint round-trips byte for byte");
	PHYSFS_close(robot_file);
	robot_info records[2] = { decoded, decoded };
	records[1].see_sound = 256;
	robot_file = PHYSFS_openWrite("robot-extended.bin");
	require(robot_file && !robot_info_write_n(records, 2, robot_file) && PHYSFS_tell(robot_file) == 0,
	        "original robot export rejects extended references before writing any record");
	PHYSFS_close(robot_file);
	bytes ham = { 'H', 'A', 'M', '!', 3, 0, 0, 0 };
	std::vector<size_t> boundaries;
	auto count = [&](int value) {
		boundaries.push_back(ham.size());
		append_int(ham, value);
		return ham.size();
	};
	count(2);
	append(ham, { 1, 0, 2, 0 });
	bytes texture(20);
	set_short(texture, 12, -1);
	set_short(texture, 14, -1);
	append(ham, texture);
	set_short(texture, 14, 0);
	append(ham, texture);
	const size_t sound_map = count(52);
	ham.resize(ham.size() + 52, 0);
	ham[sound_map + 51] = 1;
	ham.resize(ham.size() + 52, 255);
	ham[sound_map + 52] = 0;
	ham[sound_map + 52 + 51] = 0;
	bytes clip(82);
	set_int(clip, 0, F1_0);
	set_int(clip, 4, 2);
	set_int(clip, 8, F1_0 / 2);
	set_short(clip, 18, 1);
	set_short(clip, 20, 2);
	const size_t clips = count(11);
	for (int i = 0; i < 11; ++i) append(ham, clip);
	const size_t effects = count(1);
	bytes effect = clip;
	effect.resize(130);
	set_short(effect, 18, 3);
	set_short(effect, 90, -1);
	set_short(effect, 92, 0);
	set_int(effect, 98, -1);
	set_int(effect, 102, 1);
	set_int(effect, 106, 0);
	set_int(effect, 110, -1);
	set_int(effect, 122, -1);
	append(ham, effect);
	count(0); // wall animations are not part of the feature
	const size_t robots = count(1);
	bytes robot(480);
	robot[117] = 255;
	robot[281] = 1;
	set_short(robot, 296, 1);
	append(ham, robot);
	const size_t joints = count(1);
	append(ham, { 1, 0, 0, 0, 0, 0, 0, 0 });
	const size_t weapons = count(FLARE_ID + 1);
	bytes weapon(125);
	weapon[0] = WEAPON_RENDER_BLOB;
	weapon[26] = 255;
	set_short(weapon, 39, 1);
	for (int i = 0; i <= FLARE_ID; ++i) {
		if (i == FLARE_ID) weapon[26] = 0; // a shared child dependency
		append(ham, weapon);
	}
	count(0); // powerups
	const size_t models = count(2);
	bytes model = model_record(F1_0);
	set_int(model, 0, 2);
	set_int(model, 4, 4);
	set_int(model, 8, 0x12345678); // serialized pointer is never freed
	set_int(model, 16, 2);
	model[730] = 1;
	model[733] = 2;
	append(ham, model);
	model = model_record(F1_0);
	model[730] = 1;
	append(ham, model);
	const size_t model_data = ham.size();
	ham.resize(ham.size() + 6);
	const size_t destroyed = ham.size();
	append_int(ham, 1);
	append_int(ham, -1);
	append_int(ham, 1);
	append_int(ham, -1);
	count(0); // gauges
	const size_t object_bitmaps = count(1);
	append(ham, { 1, 0, 0, 0 });

	bytes pig = { 'P', 'P', 'I', 'G', 2, 0, 0, 0, 3, 0, 0, 0 };
	for (int i = 0; i < 3; ++i) {
		bytes header(18);
		header[0] = 'a' + i;
		header[9] = header[10] = 2;
		header[12] = i == 2 ? BM_FLAG_RLE : 0;
		set_int(header, 14, i * 4);
		append(pig, header);
	}
	append(pig, { 5, 6, 254, 255, 7, 8, 9, 10 });
	append(pig, { 12, 0, 0, 0, 3, 3, 11, 12, 0xe0, 13, 14, 0xe0 });
	bytes palette(9472);
	for (int i = 0; i < 768; ++i) palette[i] = i % 64;
	bytes sounds = { 'D', 'S', 'N', 'D', 1, 0, 0, 0, 2, 0, 0, 0 };
	for (int i = 0; i < 2; ++i) {
		bytes header(20);
		header[0] = 's' + i;
		set_int(header, 8, 4);
		set_int(header, 12, 4);
		set_int(header, 16, i * 4);
		append(sounds, header);
	}
	append(sounds, { 21, 22, 23, 24, 31, 32, 33, 34 });
	const d1_guidebot_source source = { "feature.ham", "feature.pig", "feature.256", "feature.s22", SAMPLE_RATE_22K };
	write_fixture(source.ham_path, ham);
	write_fixture(source.pig_path, pig);
	write_fixture(source.palette_path, palette);
	write_fixture(source.sound_path, sounds);
	const bytes before = snapshot_feature_consumers();
	const char *error = nullptr;
	auto prepare = [&]() {
		d1_guidebot_assets *assets = d1_in_d2_read_guidebot_source(&source, &error);
		require(assets && !error, error ? error : "prepare explicit companion source");
		d1_guidebot_asset_stats stats = {};
		d1_in_d2_guidebot_source_stats(assets, &stats);
		require(stats.source_robot == 0 && stats.robots == 1 && stats.models == 2 && stats.joints == 1 && stats.weapons == 2 &&
		            stats.vclips == 2 && stats.effects == 1 && stats.textures == 2 && stats.bitmaps == 3 &&
		            stats.sounds == 2 && stats.logical_sounds == 2 && stats.model_bytes == 6 && stats.sound_bytes == 8,
		        "dependency closure includes model indirection, effects, destroyed models, child weapons and alternate sounds exactly once");
		d1_in_d2_free_guidebot_source(assets);
		require(snapshot_feature_consumers() == before, "optional preparation and retirement leave all live engine consumers untouched");
	};
	prepare();
	ubyte selected[MAX_BITMAP_FILES] = {};
	selected[1] = selected[3] = 1;
	d1_bitmap_generation *images = d1_in_d2_read_feature_bitmaps(source.pig_path, source.palette_path, selected, MAX_BITMAP_FILES);
	require(images && !images->bitmaps[2].bm_data && std::memcmp(images->palette, palette.data(), 768) == 0 &&
	            std::memcmp(images->bitmaps[1].bm_data, pig.data() + 66, 4) == 0 &&
	            std::memcmp(images->bitmaps[3].bm_data, pig.data() + 74, 12) == 0,
	        "selective bitmap preparation retains source pixels, transparency, RLE and palette without consulting the live palette");
	d1_in_d2_free_bitmaps(images);

	int rejected = 0;
	auto reject = [&](const char *path, const bytes &bad) {
		write_fixture(path, bad);
		require(!d1_in_d2_read_guidebot_source(&source, &error) && error, "invalid optional source is rejected before publication");
		require(snapshot_feature_consumers() == before, "rejected optional source leaves the active game intact");
		++rejected;
	};
	for (size_t offset : boundaries) {
		reject(source.ham_path, bytes(ham.begin(), ham.begin() + offset + 3));
		bytes bad = ham;
		set_int(bad, offset, INT_MAX);
		reject(source.ham_path, bad);
	}
	for (const auto &mutation : std::vector<std::pair<size_t, int>>{
	         { 4, 2 }, { robots, MAX_POLYGON_MODELS }, { models + 4, INT_MAX }, { destroyed, MAX_POLYGON_MODELS }, { clips + 4, 31 }, { effects + 110, MAX_EFFECTS } }) {
		bytes bad = ham;
		set_int(bad, mutation.first, mutation.second);
		reject(source.ham_path, bad);
	}
	for (const auto &mutation : std::vector<std::pair<size_t, int>>{
	         { robots + 296, 2 }, { joints, 2 }, { object_bitmaps + 2, MAX_OBJ_BITMAPS }, { models + 731, MAX_OBJ_BITMAPS }, { clips + 18, MAX_BITMAP_FILES }, { model_data, 999 } }) {
		bytes bad = ham;
		set_short(bad, mutation.first, mutation.second);
		reject(source.ham_path, bad);
	}
	for (const auto &mutation : std::vector<std::pair<size_t, int>>{
	         { robots + 281, 0 }, { models + 733, 1 }, { models + 453, 1 }, { weapons + 26, 0 }, { sound_map + 51, 253 }, { sound_map + 52 + 51, 254 } }) {
		bytes bad = ham;
		bad[mutation.first] = static_cast<unsigned char>(mutation.second);
		reject(source.ham_path, bad);
	}
	write_fixture(source.ham_path, ham);
	for (size_t length : { size_t(7), size_t(12), pig.size() - 1 })
		reject(source.pig_path, bytes(pig.begin(), pig.begin() + length));
	bytes bad = pig;
	set_int(bad, 26, -1);
	reject(source.pig_path, bad);
	bad = pig;
	bad[78] = 255; // malformed RLE row length
	reject(source.pig_path, bad);
	write_fixture(source.pig_path, pig);
	for (size_t length : { size_t(7), size_t(12), sounds.size() - 1 })
		reject(source.sound_path, bytes(sounds.begin(), sounds.begin() + length));
	bad = sounds;
	set_int(bad, 28, -1);
	reject(source.sound_path, bad);
	write_fixture(source.sound_path, sounds);
	reject(source.palette_path, bytes(palette.begin(), palette.end() - 1));
	bad = palette;
	bad[0] = 64;
	reject(source.palette_path, bad);
	write_fixture(source.palette_path, palette);
	prepare();
	d1_guidebot_source missing = source;
	missing.ham_path = "unavailable-feature.ham";
	require(!d1_in_d2_read_guidebot_source(&missing, &error) && error && snapshot_feature_consumers() == before,
	        "missing optional content cannot fall back to the active D2 tables");
	std::fprintf(stderr, "Independent companion source: dependency closure and %d malformed inputs passed\n", rejected);
}

static void test_guidebot_publication(const char *pig_path, const char *palette_path, const d1_guidebot_source &source)
{
	const int saved_rate = GameArg.SndDigiSampleRate, saved_lowmem = GameArg.SysLowMem;
#ifdef USE_SDLMIXER
	const int saved_mixer = GameArg.SndDisableSdlMixer;
	GameArg.SndDisableSdlMixer = 1;
#endif
	GameArg.SndDigiSampleRate = SAMPLE_RATE_22K;
	const char *error = nullptr;
	int last_sound = -1;
	for (int pass = 0; pass < 3; ++pass) {
		d1_asset_generation *base = d1_in_d2_read_assets(pig_path, palette_path, &error);
		require(base != nullptr, error ? error : "prepare native baseline for optional publication");
		const int native_robots = base->num_robot_types, native_models = base->num_polygon_models;
		const int native_samples = base->sound_bank.count, native_bitmaps = base->bitmap_data->bitmap_count + 1;
		const int native_weapons = base->num_weapon_types, native_effects = base->num_effects;
		const bytes before = snapshot_feature_consumers();
		if (pass != 1) {
			const int previous_count = base->sound_bank.count;
			base->sound_bank.count = MAX_SOUND_FILES;
			require(!d1_in_d2_prepare_guidebot_extension(base, &source, &error) && !base->guidebot && error,
			        "extension capacity failure discards optional storage without attaching partial data");
			base->sound_bank.count = previous_count;
			require(snapshot_feature_consumers() == before, "capacity rejection cannot alter the current game");
			require(d1_in_d2_prepare_guidebot_extension(base, &source, &error) && base->guidebot &&
			            d1_in_d2_validate_guidebot_extension(base) && snapshot_feature_consumers() == before,
			        error ? error : "prepare all extension slots and palette conversions before live publication");
			++base->sound_bank.count;
			require(!d1_in_d2_validate_guidebot_extension(base) && !d1_in_d2_publish_assets(base, &error) &&
			            snapshot_feature_consumers() == before,
			        "changed base namespace rejects publication before replacing the active generation");
			--base->sound_bank.count;
		}
		d1_guidebot_asset_stats stats = {};
		d1_in_d2_guidebot_source_stats(base->guidebot, &stats);
		require(d1_in_d2_publish_assets(base, &error), error ? error : "publish native definitions and optional extension together");
		require(std::memcmp(Textures, base->textures, base->num_textures * sizeof(*Textures)) == 0 &&
		            std::memcmp(TmapInfo, base->texture_info, base->num_textures * sizeof(*TmapInfo)) == 0 &&
		            std::memcmp(Robot_info, base->robots, native_robots * sizeof(*Robot_info)) == 0 &&
		            std::memcmp(Weapon_info, base->weapons, native_weapons * sizeof(*Weapon_info)) == 0 &&
		            std::memcmp(Effects, base->effects, native_effects * sizeof(*Effects)) == 0 &&
		            std::memcmp(Sounds, base->sound_maps[0], D1_MAX_PIG_SOUNDS) == 0 &&
		            std::memcmp(AltSounds, base->sound_maps[1], D1_MAX_PIG_SOUNDS) == 0,
		        "extension publication preserves every native definition and even undefined logical sound slots");
		require(N_robot_types == native_robots + stats.robots && N_polygon_models == native_models + stats.models &&
		            N_weapon_types == native_weapons + stats.weapons && Num_effects == native_effects + stats.effects &&
		            Num_bitmap_files == native_bitmaps + stats.bitmaps && Num_sound_files == native_samples + stats.sounds,
		        "extension resources append deterministic distinct slots without accumulating on reload");
		for (int i = 1; i < native_bitmaps; ++i)
			require(GameBitmaps[i].bm_data == base->bitmap_data->bitmaps[i].bm_data, "native source pixels remain the published images");
		if (pass == 1) {
			require(digi_xlat_sound(last_sound) == -1 && !d1_in_d2_ensure_spawnable_guidebot(), "removing an extension retires its logical references and availability");
			continue;
		}
		const robot_info &companion = Robot_info[native_robots];
		require(d1_in_d2_ensure_spawnable_guidebot(), "native generation publication makes the prepared companion available without legacy flags");
		require(companion.companion && companion.model_num >= native_models && companion.weapon_type >= native_weapons &&
		            companion.exp1_vclip_num >= base->num_vclips && companion.see_sound >= 256,
		        "published companion references stay in their mapped namespaces");
		for (int lowmem : { 0, 1 }) {
			GameArg.SysLowMem = lowmem;
			const int sound = digi_xlat_sound(companion.see_sound);
			require(sound >= native_samples && sound < Num_sound_files && GameSounds[sound].bits == 8 &&
			            GameSounds[sound].freq == SAMPLE_RATE_22K && GameSounds[sound].data,
			        "ordinary audio translation resolves extended references at the correct sample rate");
			require(digi_xlat_sound(digi_unxlat_sound(sound)) == sound, "linked-sound recording reverses extended references without entering the native map");
		}
		last_sound = companion.see_sound;
		for (int i = native_models; i < N_polygon_models; ++i) {
			const polymodel &model = Polygon_models[i];
			require(d1_in_d2_is_spawnable_guidebot_model(i), "all imported model slots retain their own artwork instead of unrelated D2 model replacements");
			require(model.model_data && (!model.n_textures || model.first_texture >= D1_MAX_OBJ_BITMAPS), "optional model stream and indirection are published");
			for (int j = 0; j < model.n_textures; ++j) {
				const int bitmap = ObjBitmaps[ObjBitmapPtrs[model.first_texture + j]].index;
				require(bitmap >= native_bitmaps && bitmap < Num_bitmap_files && GameBitmaps[bitmap].bm_data,
				        "every mapped model texture reaches its own resident image");
			}
		}
		for (int i = native_effects; i < Num_effects; ++i) {
			const eclip &effect = Effects[i];
			require(effect.changing_object_texture == -1 || effect.changing_object_texture >= D1_MAX_OBJ_BITMAPS,
			        "optional animations cannot replace native object textures");
			for (int frame = 0; frame < effect.vc.num_frames; ++frame)
				require(effect.vc.frames[frame].index >= native_bitmaps && effect.vc.frames[frame].index < Num_bitmap_files,
				        "every effect frame maps into its own bitmap namespace");
		}
		if (source.ham_path == std::string("feature.ham")) {
			require(GameBitmaps[native_bitmaps].bm_data[0] == 0 && GameBitmaps[native_bitmaps].bm_data[2] == 254 &&
			            GameBitmaps[native_bitmaps].bm_data[3] == 255,
			        "optional palette conversion uses the prepared D1 palette and retains both transparency markers");
			const int goal = companion.see_sound + 1;
			GameArg.SysLowMem = 1;
			require(digi_xlat_sound(goal) == native_samples, "extended low-memory sound mapping follows the source alternate logical ID");
			GameArg.SysLowMem = 0;
			require(digi_xlat_sound(goal) == native_samples + 1, "extended normal sound mapping selects the source sample");
			const int sample = digi_xlat_sound(companion.see_sound);
			require(GameSounds[sample].length == 4 && GameSounds[sample].data[0] == 21 && GameSounds[sample].data[3] == 24,
			        "22 kHz optional samples are not erroneously doubled by native 11 kHz conversion");
		}
	}
	free_polygon_models();
	piggy_reset_asset_registry();
	require(digi_xlat_sound(last_sound) == -1, "generation retirement clears extended sound translation");
	d1_asset_generation *stock = d1_in_d2_read_assets(pig_path, palette_path, &error);
	require(stock && d1_in_d2_publish_assets(stock, &error) && digi_xlat_sound(last_sound) == -1,
	        "stock restoration cannot retain retired feature sound references");
	GameArg.SndDigiSampleRate = saved_rate;
	GameArg.SysLowMem = saved_lowmem;
#ifdef USE_SDLMIXER
	GameArg.SndDisableSdlMixer = saved_mixer;
#endif
	std::fprintf(stderr, "Companion extension publication, native invariance, sound translation and retirement passed\n");
}

static void test_registered_guidebot_source(const char *directory)
{
	const std::string hog = std::string(directory) + "/DESCENT2.HOG";
	require(PHYSFS_mount(directory, "optional-d2", 0) && PHYSFS_mount(hog.c_str(), "optional-d2", 0), "mount explicit optional package independently of base content");
	d1_guidebot_source source = { "optional-d2/descent2.ham", "optional-d2/groupa.pig", "optional-d2/groupa.256", "optional-d2/descent2.s22", SAMPLE_RATE_22K };
	const bytes before = snapshot_feature_consumers();
	d1_guidebot_asset_stats previous = {};
	for (int rate : { SAMPLE_RATE_22K, SAMPLE_RATE_11K, SAMPLE_RATE_22K }) {
		source.sound_path = rate == SAMPLE_RATE_22K ? "optional-d2/descent2.s22" : "optional-d2/descent2.s11";
		source.sample_rate = rate;
		const char *error = nullptr;
		d1_guidebot_assets *assets = d1_in_d2_read_guidebot_source(&source, &error);
		require(assets && !error, error ? error : "prepare registered Guide-Bot directly from its source package");
		d1_guidebot_asset_stats stats = {};
		d1_in_d2_guidebot_source_stats(assets, &stats);
		require(stats.source_robot == 33 && stats.robots == 1 && stats.models >= 1 && stats.weapons >= 2 &&
		            stats.vclips >= 3 && stats.bitmaps > 2 && stats.sounds > 1 && stats.model_bytes >= 6392 && stats.sound_bytes > 0,
		        "registered dependency set includes companion art and its sound/weapon/clip dependencies");
		if (rate == SAMPLE_RATE_22K) {
			if (previous.model_bytes) require(std::memcmp(&previous, &stats, sizeof(stats)) == 0, "repeated preparation is independent of intermediate sound sources");
			previous = stats;
		}
		std::fprintf(stderr, "Registered companion source at %d Hz: %d models, %d weapons, %d clips, %d effects, %d bitmaps, %d samples (%zu bytes)\n",
		             rate, stats.models, stats.weapons, stats.vclips, stats.effects, stats.bitmaps, stats.sounds, stats.sound_bytes);
		d1_in_d2_free_guidebot_source(assets);
		require(snapshot_feature_consumers() == before, "registered optional source neither installs D2 tables nor modifies the active D1 generation");
	}
	test_guidebot_publication("native-d1/DESCENT.PIG", "native-d1/palette.256", source);
	source.sound_path = "optional-d2/descent2.s11";
	source.sample_rate = SAMPLE_RATE_11K;
	test_guidebot_publication("native-d1/DESCENT.PIG", "native-d1/palette.256", source);
	require(PHYSFS_unmount(hog.c_str()) && PHYSFS_unmount(directory), "release isolated optional package mounts");
}

static void test_d1_registered_bitmaps(const char *directory, const char *d2_directory, bool graphics)
{
	const std::string hog = std::string(directory) + "/DESCENT.HOG";
	const std::string d2_hog = d2_directory ? std::string(d2_directory) + "/DESCENT2.HOG" : "";
	require(PHYSFS_mount(directory, "native-d1", 0) && PHYSFS_mount(hog.c_str(), "native-d1", 0), "mount original D1 sources for the independent bitmap reader");
	d1_bitmap_generation *generation = d1_in_d2_read_bitmaps("native-d1/DESCENT.PIG", "native-d1/palette.256");
	require(generation && generation->bitmap_count > 1000, "prepare a complete registered D1 bitmap collection");
	int large_images = 0;
	for (int i = 1; i <= generation->bitmap_count; ++i) {
		require(generation->bitmaps[i].bm_data && generation->names[i][0], "every registered D1 image has source pixels and a name");
		if (generation->bitmaps[i].bm_w > 255) ++large_images;
	}
	require(large_images > 0, "registered D1 includes original wide cockpit images");
	std::fprintf(stderr, "Prepared %d original D1 bitmaps, including %d wide images\n", generation->bitmap_count, large_images);
	d1_in_d2_free_bitmaps(generation);
	const char *error = nullptr;
	d1_asset_generation *assets = d1_in_d2_read_assets("native-d1/DESCENT.PIG", "native-d1/palette.256", &error);
	require(assets != nullptr, error ? error : "prepare registered D1 definitions independently");
	require(assets->num_textures == 584 && assets->num_effects == 55 && assets->num_polygon_models == 78, "registered D1 retains original table counts");
	require(assets->effects[10].changing_wall_texture == 333 && assets->effects[10].vc.num_frames == 4, "original D1 lava uses its source texture and all frames");
	require(assets->effects[24].vc.num_frames == 9 && assets->effects[27].vc.num_frames == 10 && assets->effects[29].vc.num_frames == 8, "D1 animations are not clamped to D2 target clips");
	require(assets->control_center.model_num == 39 && assets->dead_models[39] == 40, "original D1 live reactor and wreck references remain intact");
	require(assets->num_cockpits == 3 && assets->num_gauges == 80 && assets->exit_model == 41 && assets->destroyed_exit_model == 42, "original cockpit, gauge and exit definitions are prepared");
	require(assets->sound_bank.count > 0 && assets->sound_bank.bytes > 0, "registered D1 sound samples are owned by the generation");
	std::fprintf(stderr, "Prepared original D1 definitions: %d textures, %d models, %d sounds\n", assets->num_textures, assets->num_polygon_models, assets->sound_bank.count);
	if (d2_directory) test_registered_guidebot_source(d2_directory);
	Mission mission = {};
	mission.descent_version = 1;
	Current_mission = &mission;
	Game_mode = 0;
	GameArg.SndNoSound = 1;
	GameArg.SysInputDemoNoRender = 1;
	require(d1_in_d2_publish_assets(assets, &error), error ? error : "publish registered D1 generation");
	if (d2_directory) test_registered_guidebot_source(d2_directory);
	for (int texture = 0; texture < NumTextures; ++texture)
		for (int orientation = 0; orientation < 4; ++orientation) {
			short primary = static_cast<short>(texture);
			short overlay = static_cast<short>(texture | (orientation << 14));
			const short saved_overlay = overlay;
			require(d1_in_d2_decode_level_textures(&primary, &overlay, 1) && primary == texture && overlay == saved_overlay,
			        "every original D1 texture and packed overlay orientation survives the level adapter");
		}
	for (int invalid : { -1 }) {
		short primary = static_cast<short>(invalid), overlay = 0x4001;
		require(!d1_in_d2_decode_level_textures(&primary, &overlay, 1) && primary == invalid && overlay == 0x4001,
		        "invalid source primary leaves both references unchanged");
	}
	for (int orientation = 0; orientation < 4; ++orientation) {
		short primary = 333, overlay = static_cast<short>(NumTextures | (orientation << 14));
		require(d1_in_d2_decode_level_textures(&primary, &overlay, 1) && primary == 333 &&
		            overlay == static_cast<short>(orientation << 14),
		        "native D1 wraps excess overlay indices while preserving orientation");
	}
	for (int excess : { NumTextures, 1056, MAX_TEXTURES }) {
		short primary = static_cast<short>(excess), overlay = 0;
		require(d1_in_d2_decode_level_textures(&primary, &overlay, 1) && primary == excess % NumTextures,
		        "native D1 convert_tmap wraps authored excess primary indices");
	}
	// The retained legacy adapter always returns D2 slots, even while D1 is
	// published. Its source layout and PIG availability are explicit inputs
	for (int orientation = 0; orientation < 4; ++orientation)
		for (int have_pig = 0; have_pig < 2; ++have_pig)
			for (int registered = 0; registered < 2; ++registered) {
				const int bits = orientation << 14;
				require(static_cast<ushort>(d1_in_d2_legacy_texture(static_cast<short>(bits | 333), have_pig, registered)) == (bits | 409),
				        "legacy lava conversion is explicit and retains overlay rotation");
				require(static_cast<ushort>(d1_in_d2_legacy_texture(static_cast<short>(bits | 583), have_pig, registered)) == (bits | (registered ? 647 : 730)),
				        "legacy door conversion uses the supplied registered/shareware layout");
				require(static_cast<ushort>(d1_in_d2_legacy_texture(static_cast<short>(bits | 2), have_pig, registered)) == (bits | (have_pig ? 137 : 43)),
				        "legacy duplicate rock slots use the supplied PIG availability");
			}
	init_objects();
	require(load_level("native-d1/level01.rdl") == 0, "load First Strike against original D1 definitions");
	int reactors = 0, robots = 0;
	for (int i = 0; i <= Highest_object_index; ++i) {
		const object &obj = Objects[i];
		if (obj.type == OBJ_CNTRLCEN) {
			++reactors;
			require(obj.rtype.pobj_info.model_num == 39, "First Strike reactor is verified against its original model");
		}
		if (obj.type == OBJ_ROBOT) {
			++robots;
			require(obj.id < N_robot_types && obj.rtype.pobj_info.model_num == Robot_info[obj.id].model_num, "First Strike robots reference published D1 models");
		}
	}
	require(reactors == 1 && robots > 0, "First Strike loads real reactor and robot objects");
	std::fprintf(stderr, "Loaded First Strike with %d segments, %d robots and its original reactor\n", Num_segments, robots);
	require(PHYSFS_unmount(hog.c_str()) && PHYSFS_unmount(directory) && PHYSFS_unmount("."), "remove fixture and prefixed search paths for the D1-only level-load test");
	require(PHYSFS_mount(directory, nullptr, 0) && PHYSFS_mount(hog.c_str(), nullptr, 0), "mount only original D1 resources at the normal engine search path");
	require(!PHYSFS_exists("descent2.ham") && !PHYSFS_exists("groupa.pig") && !PHYSFS_exists("descent2.hog"), "normal level-load test cannot borrow D2 base assets");
	Current_mission = nullptr;
	require(d1_in_d2_init_base_resources(0) && d1_in_d2_use_d1_gameplay(), "startup automatically selects D1 when only D1 resources are available");
	load_text();
	require(std::strcmp(TXT_NEW_GAME, "New game") == 0 && std::strstr(TXT_SECRET_EXIT, "Alternate exit"), "startup decodes the original encrypted D1 text with correct symbolic mapping");
	char *d1_secret_text = TXT_SECRET_EXIT;
	if (graphics) {
		require(SDL_Init(SDL_INIT_VIDEO) == 0, "initialize SDL for original profile fonts");
		GameArg.SysWindow = GameCfg.WindowMode = 1;
		GameCfg.AspectX = 4;
		GameCfg.AspectY = 3;
		GameArg.GfxHiresFNTAvailable = 1;
		require(gr_init(SM(640, 480)) == 0, "initialize profile presentation renderer");
		gamefont_init();
		check_profile_fonts(true);
	}
	gamedata_init();
	d1_in_d2_init_startup_bitmaps();
	require(d1_in_d2_has_native_assets() && NumTextures == 584, "normal game-data initialization publishes D1 without a HAM or D2 PIG");
	check_profile_resources(true);
	require(!d1_in_d2_init_base_resources(2) && d1_in_d2_use_d1_gameplay(), "unavailable D2 startup request leaves committed D1 content and its startup preference intact");
	for (int reset = 0; reset < 2; ++reset) {
		free_polygon_models();
		piggy_reset_asset_registry();
		d1_in_d2_restore_base_resources();
		require(Current_mission == nullptr && d1_in_d2_use_d1_gameplay(), "base reset retains startup content without a mission descriptor");
		require(d1_in_d2_has_native_assets() && NumTextures == 584 && N_polygon_models == 78 && Num_sound_files == 98,
		        "repeated baseline restoration rebuilds original D1 definitions and samples with D2 absent");
		require(!std::strcmp(last_palette_loaded, D1_DEFAULT_PALETTE) && !std::strcmp(last_palette_loaded_pig, D1_DEFAULT_PALETTE),
		        "baseline palette restoration selects D1 content rather than the D2 executable's PIG");
		check_profile_resources(true);
		if (graphics) check_profile_fonts(true);
	}
	char base_mission[] = "descent";
	require(load_mission_by_name(base_mission) && Current_mission->descent_version == 1, "D1-only mission enumeration does not require the Counterstrike mission");
	free_mission();
	Current_mission = &mission;
	d_fname levels[2] = { "level01.rdl", "level02.rdl" };
	mission.path = mission.filename = const_cast<char *>("descent");
	mission.level_names = levels;
	mission.last_level = 2;
	GameArg.SndNoMusic = 1;
	for (int level : { 1, 2, 1 }) {
		LoadLevel(level, 0);
		require(d1_in_d2_has_native_assets() && NumTextures == 584 && N_polygon_models == 78 && Reactors[0].model_num == 39, "normal level lifecycle retains the complete original D1 tables");
		require(load_exit_models() && N_polygon_models == 78, "D1 exit models remain part of the base generation");
		require(GameSounds[0].data != nullptr && GameSounds[0].freq == 11025 && Effects[27].vc.num_frames == 10, "normal level finalization does not overwrite D1 sounds or animations");
	}
	std::fprintf(stderr, "Normal D1 level loading passed 1 -> 2 -> 1 with D2 archives absent\n");
	if (graphics) check_profile_cockpit(true);
	test_loaded_d1_weapon_firing(true);
	if (d2_directory) {
		require(PHYSFS_mount(d2_directory, nullptr, 0) && PHYSFS_mount(d2_hog.c_str(), nullptr, 0), "mount D2 sources for profile switching");
		for (int entry = 0; entry < 2; ++entry) {
			mission.descent_version = 2;
			require(d1_in_d2_use_d1_gameplay(), "requesting D2 does not change policy while D1 definitions remain active");
			check_profile_resources(true);
			mission.path = mission.filename = const_cast<char *>("d2");
			std::strcpy(levels[0], "d2leva-1.rl2");
			if (!entry) {
				Current_mission = nullptr; // the synthetic mission is stack-owned
				char d2_mission[] = "d2";
				require(load_mission_by_name(d2_mission) != 0, "normal mission selection transitions from D1 to D2 definitions");
				require(!d1_in_d2_has_native_assets() && N_robot_types == 66, "mission selection publishes D2 definitions before level loading");
			}
			LoadLevel(1, 0);
			require(!d1_in_d2_has_native_assets() && NumTextures > 584 && N_robot_types == 66 && N_polygon_models >= 166, "D2 level loading restores D2 tables and retires D1 ownership");
			require(std::strstr(TXT_SECRET_EXIT, "Secret Teleporter") && std::strstr(d1_secret_text, "Alternate exit"), "D2 restores its text while existing D1 menu text references remain valid");
			char *d2_secret_text = TXT_SECRET_EXIT;
			check_profile_resources(false);
			if (graphics) check_profile_fonts(false);
			if (graphics) check_profile_cockpit(false);
			test_loaded_d1_weapon_firing(false);
			if (!entry) {
				free_mission();
				require(!d1_in_d2_use_d1_gameplay(), "freeing a D2 descriptor does not select startup D1 while D2 tables remain installed");
				check_profile_resources(false);
			}
			Current_mission = &mission;
			mission.descent_version = 1;
			require(!d1_in_d2_use_d1_gameplay(), "requesting D1 leaves D2 policy active until D1 publication");
			d1_in_d2_load_mission_assets();
			require(!d1_in_d2_use_d1_gameplay() && !d1_in_d2_has_native_assets(), "deferred D1 mission preparation does not relabel D2 tables");
			check_profile_resources(false);
			if (!entry) {
				d1_in_d2_prepare_intro_assets();
				require(d1_in_d2_use_d1_gameplay() && d1_in_d2_has_native_assets() && N_robot_types == 24,
				        "actual intro entry commits D1 definitions for briefing robot rendering before level decode");
				check_profile_resources(true);
				if (graphics) check_profile_fonts(true);
				const auto intro_model = Polygon_models[0].model_data;
				d1_in_d2_prepare_intro_assets();
				require(Polygon_models[0].model_data == intro_model, "same-profile briefing entry does not reload its base generation");
			}
			mission.path = mission.filename = const_cast<char *>("descent");
			std::strcpy(levels[0], "level01.rdl");
			LoadLevel(1, 0);
			require(d1_in_d2_has_native_assets() && N_polygon_models == 78 && NumTextures == 584 && Effects[27].vc.num_frames == 10, "returning to D1 restores original definitions after D2 loading");
			require(TXT_SECRET_EXIT == d1_secret_text && std::strstr(d2_secret_text, "Secret Teleporter"), "returning to D1 restores its text bank without invalidating D2 references");
			check_profile_resources(true);
			if (graphics) check_profile_fonts(true);
			if (graphics) check_profile_cockpit(true);
			test_loaded_d1_weapon_firing(true);
			std::fprintf(stderr, "D1 -> D2 -> D1 profile switching passed through %s preparation\n", entry ? "level" : "mission");
		}
		Current_mission = nullptr;
		for (int profile : { 2, 1 }) {
			std::fprintf(stderr, "Testing explicit D%d baseline restoration\n", profile);
			free_polygon_models();
			piggy_reset_asset_registry();
			require(d1_in_d2_init_base_resources(profile), "select an explicit baseline with both installations present");
			d1_in_d2_restore_base_resources();
			require(d1_in_d2_has_native_assets() == (profile == 1) && N_robot_types == (profile == 1 ? 24 : 66),
			        "baseline restoration follows explicit content selection with both installations present");
			check_profile_resources(profile == 1);
			if (graphics) check_profile_fonts(profile == 1);
		}
	}
	free_polygon_models();
	piggy_reset_asset_registry();
	Current_mission = nullptr;
	if (graphics) {
		gamefont_close();
		newmenu_free_background();
		gr_close();
		SDL_Quit();
	}
	free_text();
	if (d2_directory)
		require(PHYSFS_unmount(d2_hog.c_str()) && PHYSFS_unmount(d2_directory), "presentation shutdown releases its D2 archive handle before source unmount");
	require(PHYSFS_unmount(hog.c_str()) && PHYSFS_unmount(directory), "unmount original D1 bitmap sources");
	require(PHYSFS_mount(".", nullptr, 1) != 0, "restore the fixture search path");
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

static void test_d1_indestructible_lights()
{
	Mission mission = {};
	Current_mission = &mission;
	Game_mode = 0;
	GameArg.SndNoSound = 1;
	Highest_segment_index = 0;
	Num_static_lights = 0;
	std::memset(&Segments[0], 0, sizeof(Segments[0]));
	for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side) {
		Segments[0].children[side] = -1;
		Segments[0].sides[side].wall_num = -1;
		Segments[0].sides[side].type = SIDE_IS_QUAD;
	}
	init_objects();
	unsigned char pixels[4] = { 1, 1, 1, 1 };
	gr_init_bitmap(&GameBitmaps[1], BM_LINEAR, 0, 0, 2, 2, 2, pixels);
	Textures[1].index = 1;
	std::memset(&TmapInfo[0], 0, 3 * sizeof(TmapInfo[0]));
	Vclip[3].play_time = F1_0;
	Vclip[3].sound_num = -1;
	object weapon = {};
	weapon.ctype.laser_info.parent_type = OBJ_PLAYER;
	vms_vector hit = {};
	const int overlay = 0x4000 | 1;
	for (int version : { 1, 2, 1 }) {
		mission.descent_version = version;
		TmapInfo[1].eclip_num = -1;
		TmapInfo[1].destroyed = version == 1 ? -1 : 2;
		Segments[0].sides[0].tmap_num2 = overlay;
		Light_subtracted[0] = 0;
		require(check_effect_blowup(&Segments[0], 0, &hit, &weapon, 1) == (version == 2), "original D1 light metadata stays untagged while tagged D2 lights remain destructible");
		require(Segments[0].sides[0].tmap_num2 == (version == 2 ? (0x4000 | 2) : overlay) && Light_subtracted[0] == (version == 2 ? 1 : 0), "D1 light hits preserve both the texture and illumination");
		if (version == 1) {
			require(!check_effect_blowup(&Segments[0], 0, &hit, &weapon, 0), "ordinary D1 weapon hits cannot destroy lights");
			blast_nearby_glass(&weapon, F1_0);
			require(Segments[0].sides[0].tmap_num2 == overlay && Light_subtracted[0] == 0, "nearby explosions preserve untagged D1 lights through the normal wall path");
		}
		TmapInfo[1].destroyed = -1;
		Segments[0].sides[0].tmap_num2 = overlay;
		require(!check_effect_blowup(&Segments[0], 0, &hit, &weapon, 1), "untagged lights stay indestructible in both games");
		TmapInfo[1].eclip_num = 0;
		Effects[0].flags = 0;
		Effects[0].dest_bm_num = 2;
		Effects[0].dest_eclip = -1;
		Effects[0].dest_vclip = 3;
		Effects[0].sound_num = -1;
		require(check_effect_blowup(&Segments[0], 0, &hit, &weapon, 1) && Segments[0].sides[0].tmap_num2 == (0x4000 | 2), "shootable monitors remain destructible in D1 and D2");
	}
	GameBitmaps[1].bm_data = nullptr;
	Current_mission = nullptr;
}

static void test_d1_simulation_dispatch()
{
	Mission mission = {};
	const fix old_frame_time = FrameTime;
	const int old_difficulty = Difficulty_level;
	Current_mission = &mission;
	FrameTime = F1_0 / 64;
	Difficulty_level = 2;
	Game_mode = 0;
	for (int version : { 1, 2, 1 }) {
		mission.descent_version = version;
		object &robot = Objects[0];
		std::memset(&robot, 0, sizeof(robot));
		robot.type = OBJ_ROBOT;
		robot.movement_type = MT_PHYSICS;
		robot.orient = vmd_identity_matrix;
		robot.mtype.phys_info.mass = F1_0;
		robot.mtype.phys_info.drag = F1_0 / 8;
		robot.mtype.phys_info.rotvel.x = F1_0 / 2;
		robot.mtype.phys_info.flags = PF_FREE_SPINNING;
		std::memset(&Robot_info[0], 0, sizeof(Robot_info[0]));
		do_physics_sim_rot(&robot);
		require(robot.mtype.phys_info.rotvel.x == (version == 1 ? 22528 : F1_0 / 2), "rotation dispatch preserves D1 damping and D2 free spinning across mission switches");

		robot_info &info = Robot_info[0];
		std::memset(&info, 0, sizeof(info));
		info.thief = 1;
		robot.ctype.ai_info.SKIP_AI_COUNT = 7;
		vms_vector force = { 16 * F1_0, 0, 0 };
		phys_apply_rot(&robot, &force);
		require(robot.ctype.ai_info.SKIP_AI_COUNT == (version == 1 ? 2 : 7), "rotational hits preserve D1 scheduling and D2 thief exemption");

		info.weapon_type2 = 1;
		info.rapidfire_count[Difficulty_level] = 3;
		info.firing_wait[Difficulty_level] = F1_0;
		info.firing_wait2[Difficulty_level] = 2 * F1_0;
		ai_local local = {};
		set_next_fire_time(&robot, &local, &info, 0);
		require(local.next_fire == (version == 1 ? F1_0 / 8 : 0) && local.next_fire2 == (version == 1 ? 0 : 2 * F1_0), "D1 firing uses its burst clock while D2 retains its secondary gun clock");
		if (version == 1) {
			set_next_fire_time(&robot, &local, &info, 0);
			set_next_fire_time(&robot, &local, &info, 0);
			require(local.rapidfire_count == 0 && local.next_fire == F1_0, "D1 burst completion restores the source firing interval");
		}
		info.companion = 1;
		local = {};
		const auto companion_rng = d_rand_get_call_count();
		set_next_fire_time(&robot, &local, &info, 0);
		require(local.next_fire == 0 && local.next_fire2 == 2 * F1_0 && !local.rapidfire_count && companion_rng == d_rand_get_call_count(),
		        "companion firing keeps its D2 secondary clock without extra RNG in both content profiles");
	}
	Current_mission = nullptr;
	FrameTime = old_frame_time;
	Difficulty_level = old_difficulty;
}

static void test_d1_weapon_creation()
{
	Mission mission = {};
	Current_mission = &mission;
	GameArg.SndNoSound = 1;
	init_test_corridor();
	Player_num = 0;
	Difficulty_level = 2;
	N_weapon_types = 30;
	std::memset(Weapon_info, 0, sizeof(Weapon_info));
	for (int i = 0; i < N_weapon_types; ++i) {
		weapon_info &info = Weapon_info[i];
		info.render_type = WEAPON_RENDER_NONE;
		info.flash_sound = info.flash_vclip = -1;
		info.mass = F1_0;
		info.speedvar = 128;
		info.lifetime = 10 * F1_0;
		info.strength[Difficulty_level] = 10 * F1_0;
		info.speed[Difficulty_level] = 40 * F1_0;
		info.children = -1;
	}
	Weapon_info[SMART_ID].children = PLAYER_SMART_HOMING_ID;
	for (int version : { 1, 2, 1 }) {
		mission.descent_version = version;
		Game_mode = 0;
		init_objects();
		ConsoleObject = Viewer = &Objects[0];
		ConsoleObject->orient = vmd_identity_matrix;
		Players[0].objnum = 0;
		Players[0].flags = PLAYER_FLAGS_QUAD_LASERS;
		for (int laser = LASER_ID_L1; laser <= LASER_ID_L4; ++laser) {
			const int shot = Laser_create_new(&ConsoleObject->orient.fvec, &ConsoleObject->pos, 0, 0, laser, 0);
			require(shot >= 0, "create quad laser bolt");
			require(Objects[shot].ctype.laser_info.multiplier == (version == 1 ? F1_0 : 3 * F1_0 / 4), "D1 quad lasers keep full damage and D2 retains its multiplier");
			obj_delete(shot);
		}
		for (int mode : { 0, GM_MULTI }) {
			Game_mode = mode;
			for (fix charge : { 0, 2 * F1_0, 3 * F1_0 }) {
				Fusion_charge = charge;
				const int shot = Laser_create_new(&ConsoleObject->orient.fvec, &ConsoleObject->pos, 0, 0, FUSION_ID, 0);
				require(shot >= 0, "create charged fusion bolt");
				const fix expected = version == 1 && mode ? (charge == 0 ? F1_0 / 2 : F1_0) : F1_0 + charge / 2;
				require(Objects[shot].ctype.laser_info.multiplier == expected, "fusion initialization preserves original D1 single/multiplayer charge rules and D2 defaults");
				obj_delete(shot);
			}
		}
		Game_mode = 0;
		Fusion_charge = 0;
		const fix old_radius = Polygon_models[0].rad;
		Polygon_models[0].rad = F1_0;
		Weapon_info[CONCUSSION_ID].render_type = WEAPON_RENDER_POLYMODEL;
		Weapon_info[CONCUSSION_ID].model_num = 0;
		Weapon_info[CONCUSSION_ID].po_len_to_width_ratio = F1_0;
		for (int owner : { OBJ_PLAYER, OBJ_ROBOT }) {
			ConsoleObject->type = owner;
			vms_vector origin = { 0, 0, 19 * F1_0 / 2 };
			const int shot = Laser_create_new(&ConsoleObject->orient.fvec, &origin, 0, 0, CONCUSSION_ID, 0);
			require(shot >= 0, "create model projectile at a segment boundary");
			const bool offset = version == 1 || owner == OBJ_PLAYER;
			require(Objects[shot].segnum == (offset ? 1 : 0) && Objects[shot].pos.z == origin.z + (offset ? F1_0 : 0), "D1 offsets player and robot shots, with shared segment relinking; D2 offsets only player shots");
			obj_delete(shot);
			origin.z = 59 * F1_0 / 2;
			const int edge_shot = Laser_create_new(&ConsoleObject->orient.fvec, &origin, 1, 0, CONCUSSION_ID, 0);
			require(edge_shot >= 0 && Objects[edge_shot].segnum == 1 && Objects[edge_shot].pos.z == origin.z, "gun-tip offset cannot place a projectile outside the mine");
			obj_delete(edge_shot);
		}
		Polygon_models[0].rad = old_radius;
		Weapon_info[CONCUSSION_ID].render_type = WEAPON_RENDER_NONE;
		for (int owner : { OBJ_PLAYER, OBJ_ROBOT }) {
			ConsoleObject->type = owner;
			const int smart = Laser_create_new(&ConsoleObject->orient.fvec, &ConsoleObject->pos, 0, 0, SMART_ID, 0);
			require(smart >= 0, "create smart missile");
			create_smart_children(&Objects[smart], 3);
			int children = 0;
			for (int i = Highest_object_index; i > 0; --i) {
				if (Objects[i].type != OBJ_WEAPON || i == smart) continue;
				++children;
				require(Objects[i].id == (version == 1 && owner == OBJ_ROBOT ? ROBOT_SMART_HOMING_ID : PLAYER_SMART_HOMING_ID), "D1 smart missiles select robot children from ownership while D2 uses its declared child");
				require(Objects[i].ctype.laser_info.parent_type == owner, "smart children retain the original shooter type");
				obj_delete(i);
			}
			require(children == 3, "smart missile creates the requested children");
			obj_delete(smart);
		}
	}
	Current_mission = nullptr;
	Game_mode = 0;
}

static void test_d1_custom_definitions()
{
	const bytes custom = d1_custom_definition_fixture();
	const bytes pg1 = d1_custom_dpog_fixture(), dtx = d1_custom_dtx_fixture();
	const char *error = nullptr;
	const int previous_rate = GameArg.SndDigiSampleRate;
	GameArg.SndDigiSampleRate = SAMPLE_RATE_22K;
#ifdef USE_SDLMIXER
	const int previous_mixer = GameArg.SndDisableSdlMixer;
#endif
	write_fixture("palette.256", bytes(9472, 17));
	Mission *saved_mission = Current_mission;
	Mission mission = {};
	mission.descent_version = 1;
	Current_mission = &mission;
	char level[] = "native.rdl", stock[] = "stock.rdl";
	for (int pass = 0; pass < 3; ++pass) {
#ifdef USE_SDLMIXER
		GameArg.SndDisableSdlMixer = pass != 2;
#endif
		write_fixture("native.pg1", pg1);
		write_fixture("native.dtx", dtx);
		write_fixture("native.hx1", custom);
		require(d1_in_d2_prepare_level_assets(level), "prepare custom D1 definitions through the level lifecycle");
		require(GameBitmaps[1].bm_data[0] == 21 && GameBitmaps[1].bm_data[3] == 24 && GameBitmaps[1].avg_color == 7,
		        "DTX takes precedence over PG1 while preserving source palette indices");
		int output_rate = SAMPLE_RATE_22K, output_length = 6;
#ifdef USE_SDLMIXER
		if (!GameArg.SndDisableSdlMixer) {
			output_rate = SAMPLE_RATE_11K;
			output_length = 3;
		}
#endif
		require(GameSounds[0].length == output_length && GameSounds[0].freq == output_rate && GameSounds[0].data[0] == 31 &&
		            GameSounds[0].data[output_length / 3] == 32 && GameSounds[0].data[output_length - 1] == 33,
		        "custom samples preserve pitch and duration with exactly one output conversion");
		d1_custom_texture_stats stats = {};
		d1_custom_get_stats(&stats);
		require(stats.files_found == 2 && stats.bitmap_entries == 2 && stats.bitmap_applied == 2 && stats.sound_entries == 1 && stats.sound_applied == 1,
		        "custom diagnostics describe the published generation");
		const robot_info &robot = Robot_info[0];
		require(robot.mass == 7 * F1_0 && robot.drag == F1_0 / 4 && robot.weapon_type == 0 && robot.n_guns == 1 && robot.anim_states[0][0].n_joints == 1,
		        "HX1 preserves native robot fields and invalid-weapon fallback");
		require(robot.weapon_type2 == -1 && !robot.kamikaze && !robot.badass && !robot.energy_drain && !robot.firing_wait2[0] &&
		            !robot.taunt_sound && !robot.companion && !robot.smart_blobs && !robot.energy_blobs && !robot.thief && !robot.pursuit &&
		            !robot.death_roll && !robot.flags && !robot.deathroll_sound && !robot.glow && robot.lightcast == 1 && robot.behavior == AIB_NORMAL && robot.aim == 255,
		        "HX1 does not enable D2-only robot abilities from fields ignored by native D1");
		require(Polygon_models[0].rad == 7 * F1_0 && Dying_modelnums[0] == 1 && Dead_modelnums[0] == 2 && ObjBitmaps[0].index == 1 &&
		            Robot_joints[0].angles.p == 100 && Robot_joints[0].angles.b == 200 && Robot_joints[0].angles.h == 300,
		        "HX1 models, wrecks, joints and bitmap references publish together");
		object instance = {};
		instance.type = OBJ_ROBOT;
		instance.render_type = RT_POLYOBJ;
		instance.movement_type = MT_PHYSICS;
		verify_robot_object(&instance);
		require(instance.size == 7 * F1_0 && instance.mtype.phys_info.mass == 7 * F1_0 && instance.mtype.phys_info.drag == F1_0 / 4,
		        "initial object verification consumes HX1 definitions without a post-load repair");
		if (!pass) {
			const auto active_model = Polygon_models[0].model_data;
			const auto active_pixels = GameBitmaps[1].bm_data;
			const auto active_samples = GameSounds[0].data;
			const robot_info active_robot = robot;
			const d1_custom_texture_stats active_stats = stats;
			std::vector<bytes> invalid;
			for (size_t end : { 0u, 7u, 11u, 15u, 495u, 499u, 511u, 515u, 519u, 1253u, 1255u, 1259u, 1263u, 1267u, 1271u, 1273u })
				invalid.emplace_back(custom.begin(), custom.begin() + end);
			for (auto mutation : { std::pair<size_t, int>{ 4, 2 }, { 8, -1 }, { 8, INT_MAX }, { 12, 1 }, { 16, 3 }, { 500, 1 }, { 516, 3 }, { 520, 11 }, { 524, INT_MAX }, { 532, 2 }, { 1256, 3 }, { 1268, 210 } }) {
				invalid.push_back(custom);
				set_int(invalid.back(), mutation.first, mutation.second);
			}
			for (auto mutation : { std::pair<size_t, short>{ 16 + 298, 2 }, { 504, 1 }, { 1254, 99 }, { 1272, 2 } }) {
				invalid.push_back(custom);
				set_short(invalid.back(), mutation.first, mutation.second);
			}
			for (const bytes &damaged : invalid) {
				write_fixture("native.hx1", damaged);
				d1_asset_generation *staged = d1_in_d2_read_assets("descent.pig", "palette.256", &error);
				require(staged != nullptr, "prepare stock generation for invalid HX1");
				require(!d1_custom_read_assets(staged, level, &error) && error, "reject incomplete or inconsistent HX1 before publication");
				d1_in_d2_free_assets(staged);
				require(Polygon_models[0].model_data == active_model && GameBitmaps[1].bm_data == active_pixels && GameSounds[0].data == active_samples &&
				            std::memcmp(&Robot_info[0], &active_robot, sizeof(active_robot)) == 0 && d1_in_d2_has_native_assets(),
				        "failed custom staging and cleanup leave all active assets intact");
			}
			write_fixture("native.hx1", custom);
			for (const char *file : { "native.pg1", "native.dtx" }) {
				const bytes &source = std::strcmp(file, "native.pg1") == 0 ? pg1 : dtx;
				std::vector<bytes> broken;
				for (size_t end = 0; end < source.size(); ++end)
					broken.emplace_back(source.begin(), source.begin() + end);
				broken.push_back(source);
				if (source == pg1) {
					set_short(broken.back(), 12, 2); // explicit index outside source bank
					broken.push_back(source);
					set_int(broken.back(), 28, INT_MAX);
					broken.push_back(source);
					broken.back()[26] = BM_FLAG_RLE;
					set_int(broken.back(), 32, 4); // no row table or pixel stream
				} else {
					set_int(broken.back(), 33, INT_MAX); // sample extends past file
					broken.push_back(source);
					set_int(broken.back(), 21, -1); // image points into headers
				}
				for (const bytes &damaged : broken) {
					write_fixture(file, damaged);
					d1_asset_generation *staged = d1_in_d2_read_assets("descent.pig", "palette.256", &error);
					require(staged && !d1_custom_read_assets(staged, level, &error) && error, "reject incomplete custom images/samples before publication");
					d1_in_d2_free_assets(staged);
					d1_custom_get_stats(&stats);
					require(Polygon_models[0].model_data == active_model && GameBitmaps[1].bm_data == active_pixels && GameSounds[0].data == active_samples &&
					            std::memcmp(&stats, &active_stats, sizeof(stats)) == 0 && GameBitmaps[1].bm_data[0] == 21 && GameSounds[0].data[0] == 31,
					        "failed PG1/DTX preparation cannot alter active buffers or published diagnostics");
				}
				write_fixture(file, source);
			}
			// Names exist only in the unpublished bank, never in live hash tables
			bytes named = dtx;
			std::memcpy(named.data() + 8, "ONLYNEW", 8);
			std::memcpy(named.data() + 25, "ONLYSND", 8);
			write_fixture("lookup.dtx", named);
			d1_asset_generation *staged = d1_in_d2_read_assets("descent.pig", "palette.256", &error);
			require(staged != nullptr, "prepare independent custom namespace");
			std::strcpy(staged->bitmap_data->names[1], "onlynew");
			std::strcpy(staged->sound_bank.names[0], "onlysnd");
			require(d1_custom_read_assets(staged, "lookup.rdl", &error) && staged->bitmap_data->bitmaps[1].bm_data[0] == 21 &&
			            staged->sound_bank.samples[0].length == 3 && staged->sound_bank.samples[0].freq == SAMPLE_RATE_11K && staged->sound_bank.samples[0].data[2] == 33,
			        "custom resolution and preparation use source names and original samples without live-registry or device-rate dependencies");
			d1_in_d2_free_assets(staged);
			require(PHYSFS_delete("lookup.dtx"), "remove independent namespace fixture");
			for (int format = 0; format < 6; ++format) {
				bytes variant = format < 2 ? pg1 : dtx;
				if (format == 0) {
					set_int(variant, 0, 0x47495050); // PPIG resolves names, not explicit IDs
					set_int(variant, 4, 2);
					variant.erase(variant.begin() + 12, variant.begin() + 14);
					std::memcpy(variant.data() + 12, "source\0\0", 8);
				} else if (format == 1) {
					variant[26] = BM_FLAG_RLE;
					variant.resize(32);
					append(variant, { 12, 0, 0, 0, 3, 3, 21, 22, 0xe0, 23, 24, 0xe0 });
				} else if (format == 2) {
					variant = bytes(5000);
					set_int(variant, 0, 5000); // registered-style PIG directory offset
					append(variant, dtx);
				} else if (format == 3) {
					std::memcpy(variant.data() + 8, "unknown", 8);
					std::memcpy(variant.data() + 25, "unknown", 8);
				} else if (format == 4) {
					variant[16] = 128; // source large-width flag
					variant[17] = 44;
					set_int(variant, 41, 600);
					variant.resize(45);
					append(variant, bytes(600, 21));
					append(variant, { 31, 32, 33 });
				} else {
					set_int(variant, 4, 2);
					bytes second(dtx.begin() + 25, dtx.begin() + 45);
					set_int(second, 16, 7);
					variant.insert(variant.begin() + 45, second.begin(), second.end());
					append(variant, { 41, 42, 43 });
				}
				write_fixture("formats.pg1", variant);
				staged = d1_in_d2_read_assets("descent.pig", "palette.256", &error);
				require(staged && d1_custom_read_assets(staged, "formats.rdl", &error), "prepare supported custom image/sample layouts");
				const grs_bitmap &image = staged->bitmap_data->bitmaps[1];
				if (format == 1)
					require((image.bm_flags & BM_FLAG_RLE) && image.bm_data[0] == 12 && image.bm_data[6] == 21, "custom RLE retains validated source bytes");
				else
					require(image.bm_data[0] == (format == 0 ? 9 : format == 3 ? 1
					                                                           : 21) &&
					            image.bm_w == (format == 4 ? 300 : 2),
					        "custom layout retains source pixels and wide-image geometry");
				require(staged->sound_bank.samples[0].data[0] == (format < 2 || format == 3 ? 5 : format == 5 ? 41
				                                                                                              : 31),
				        "named custom samples and duplicate-entry precedence are preserved");
				if (format == 3)
					require(staged->custom_stats.bitmap_unresolved == 1 && staged->custom_stats.sound_unresolved == 1, "unresolved source names are counted without inventing destination assets");
				d1_in_d2_free_assets(staged);
			}
			require(PHYSFS_delete("formats.pg1"), "remove alternate custom format fixtures");
		}
		require(d1_in_d2_prepare_level_assets(stock), "prepare a stock level after custom definitions");
		require(Robot_info[0].mass == 0 && !Robot_info[0].n_guns && Polygon_models[0].rad == F1_0 && Dying_modelnums[0] == -1 && Dead_modelnums[0] == -1 &&
		            ObjBitmaps[0].index == 0 && Robot_joints[0].angles.p == 0,
		        "custom-to-stock transition removes all prior HX1 replacements");
		d1_custom_get_stats(&stats);
		require(!stats.files_found && !stats.bitmap_applied && !stats.sound_applied && GameBitmaps[1].bm_data[0] == 1 && GameSounds[0].data[0] == 5,
		        "stock publication removes custom images, samples and diagnostics together");
	}
	require(PHYSFS_delete("native.hx1") && PHYSFS_delete("native.pg1") && PHYSFS_delete("native.dtx") && PHYSFS_delete("palette.256"), "remove staged custom-definition fixtures");
	GameArg.SndDigiSampleRate = previous_rate;
#ifdef USE_SDLMIXER
	GameArg.SndDisableSdlMixer = previous_mixer;
#endif
	Current_mission = saved_mission;
}

static void test_d1_reactor()
{
	const std::string original_write_dir = PHYSFS_getWriteDir();
	const std::string fixture_dir = original_write_dir + "/d1-reactor-fixtures";
	require(PHYSFS_mkdir("d1-reactor-fixtures") && PHYSFS_mount(fixture_dir.c_str(), nullptr, 0) && PHYSFS_setWriteDir(fixture_dir.c_str()), "isolate D1 reactor assets from exit fixtures");
	// A failed prior fixture must not supply overlays to the next base-only case
	for (const char *name : { "native.pg1", "native.dtx", "native.hx1", "lookup.dtx", "formats.pg1", "palette.256" })
		if (PHYSFS_exists(name)) require(PHYSFS_delete(name), "retire previous custom fixture files");
	// Minimal registered D1 PIG with a live reactor and a separate wreck model
	bytes pig(8 + 800 * 28 + 500);
	set_int(pig, 4, 1); // one source texture
	set_short(pig, 8, 1);
	const size_t texture_info = 8 + 800 * 2;
	pig[texture_info + 13] = TMI_VOLATILE | TMI_WATER | TMI_FORCE_FIELD;
	set_int(pig, texture_info + 14, F1_0 / 2);
	set_int(pig, texture_info + 18, 4 * F1_0);
	set_int(pig, texture_info + 22, -1);
	append_int(pig, 1);
	const size_t clip = pig.size();
	append(pig, bytes(70 * 82));
	set_int(pig, clip, F1_0);
	set_int(pig, clip + 4, 1);
	set_int(pig, clip + 8, F1_0);
	set_short(pig, clip + 18, 1);
	append_int(pig, 0);
	append(pig, bytes(60 * 130));
	append_int(pig, 1);
	const size_t wall_clip = pig.size();
	append(pig, bytes(30 * 66));
	set_int(pig, wall_clip, F1_0);
	set_short(pig, wall_clip + 4, 1);
	append_int(pig, 1);
	append(pig, bytes(30 * 486));
	append_int(pig, 1);
	append(pig, bytes(600 * 8));
	append_int(pig, 1);
	append(pig, bytes(30 * 115));
	append_int(pig, 0);
	append(pig, bytes(29 * 16));
	append_int(pig, 3);
	for (int i = 0; i < 3; ++i) append(pig, model_record((i + 1) * F1_0));
	append(pig, bytes(3 * 2));
	const size_t gauges = pig.size();
	append(pig, bytes(80 * 2));
	set_short(pig, gauges, 1);
	for (int i = 0; i < 85; ++i) append_int(pig, -1);
	for (int i = 0; i < 85; ++i) append_int(pig, i == 1 ? 2 : -1);
	append(pig, bytes(210 * 4 + 132)); // object bitmaps, pointers, player ship
	append_int(pig, 1);
	const size_t cockpit = pig.size();
	append(pig, bytes(4 * 2 + 500));
	set_short(pig, cockpit, 1);
	append_int(pig, 1);
	const size_t object_types = pig.size();
	append(pig, bytes(100 * 6));
	pig[object_types] = 4; // OL_CONTROL_CENTER
	pig[object_types + 100] = 1;
	append_int(pig, 0); // first multiplayer bitmap
	append_int(pig, 1); // reactor gun count
	const size_t guns = pig.size();
	append(pig, bytes(4 * 12 * 2));
	set_int(pig, guns, 3 * F1_0);
	set_int(pig, guns + 4 * 12 + 8, F1_0);
	append_int(pig, 1); // exit model
	append_int(pig, 2); // destroyed exit model
	set_int(pig, 0, static_cast<int>(pig.size()));
	append_int(pig, 1); // bitmap count
	append_int(pig, 1); // sound count
	bytes bitmap_header(17);
	std::memcpy(bitmap_header.data(), "source", 6);
	bitmap_header[9] = bitmap_header[10] = 2;
	append(pig, bitmap_header);
	bytes sound_header(20);
	std::memcpy(sound_header.data(), "sound", 5);
	set_int(sound_header, 8, 4);
	set_int(sound_header, 12, 4);
	set_int(sound_header, 16, 4);
	append(pig, sound_header);
	append(pig, bytes{ 1, 2, 3, 4, 5, 6, 7, 8 });
	write_fixture("descent.pig", pig);
	write_fixture("source.256", bytes(9472, 17));
	const int previous_bitmap_count = Num_bitmap_files, previous_model_count = N_polygon_models;
	const int previous_sound_count = Num_sound_files;
	Num_bitmap_files = N_polygon_models = Num_sound_files = 0;
	const tmap_info previous_texture = TmapInfo[0];
	const ubyte previous_palette_color = gr_palette[0];
	const char *error = nullptr;
	d1_asset_generation *prepared = d1_in_d2_read_assets("descent.pig", "source.256", &error);
	require(prepared != nullptr, error ? error : "prepare full synthetic D1 assets");
	require(prepared->textures[0].index == 1 && prepared->texture_info[0].destroyed == -1 && prepared->texture_info[0].eclip_num == -1, "D1 texture identity and non-blowable static light metadata are explicit");
	require(prepared->texture_info[0].flags == TMI_VOLATILE && prepared->texture_info[0].damage == 4 * F1_0 && prepared->texture_info[0].lighting == F1_0 / 2, "D1 texture properties survive preparation without interpreting reserved source bits as D2 water/force fields");
	require(prepared->wall_anims[0].frames[0] == 0 && prepared->wall_anims[0].num_frames == 1 && prepared->wall_anims[0].play_time == F1_0, "D1 wall animations retain source texture references and timing");
	require(prepared->gauges[0].index == 1 && prepared->cockpits[0].index == 1 && prepared->exit_model == 1 && prepared->destroyed_exit_model == 2, "presentation and exit references are owned");
	require(prepared->sound_bank.count == 1 && prepared->sound_bank.samples[0].length == 4 && prepared->sound_bank.samples[0].data[0] == 5, "sound preparation preserves original samples");
	require(prepared->bitmap_data->bitmaps[1].bm_data[0] == 1 && prepared->bitmap_data->palette[0] == 17, "source pixels and palette stay together");
	set_short(pig, 8, 2);
	write_fixture("descent.pig", pig);
	require(!d1_in_d2_read_assets("descent.pig", "source.256", &error) && std::strcmp(error, "texture bitmap references") == 0, "reject references outside the prepared bitmap collection");
	set_short(pig, 8, 1);
	const int directory = static_cast<int>(pig.size() - 8 - 17 - 20 - 8);
	set_int(pig, 0, directory - 4);
	write_fixture("descent.pig", pig);
	require(!d1_in_d2_read_assets("descent.pig", "source.256", &error) && std::strcmp(error, "exit model references") == 0, "property reads cannot spill into the bitmap directory");
	set_int(pig, 0, directory);
	set_int(pig, directory + 8 + 17 + 16, static_cast<int>(pig.size()));
	write_fixture("descent.pig", pig);
	require(!d1_in_d2_read_assets("descent.pig", "source.256", &error) && std::strcmp(error, "sample spans") == 0, "failed sample validation releases the staged bitmap and model collection");
	set_int(pig, directory + 8 + 17 + 16, 4);
	require(prepared->textures[0].index == 1 && prepared->sound_bank.samples[0].data[0] == 5, "failed preparation leaves an earlier generation intact");
	require(Num_bitmap_files == 0 && N_polygon_models == 0 && Num_sound_files == 0 && std::memcmp(&TmapInfo[0], &previous_texture, sizeof(previous_texture)) == 0 && gr_palette[0] == previous_palette_color, "preparation works with empty live registries and never publishes data");
	Num_bitmap_files = previous_bitmap_count;
	N_polygon_models = previous_model_count;
	Num_sound_files = previous_sound_count;
	write_fixture("descent.pig", pig);
	const int previous_sample_rate = GameArg.SndDigiSampleRate;
	GameArg.SndDigiSampleRate = SAMPLE_RATE_22K;
#ifdef USE_SDLMIXER
	const int previous_mixer_disabled = GameArg.SndDisableSdlMixer;
	GameArg.SndDisableSdlMixer = 1;
#endif
	for (int pass = 0; pass < 2; ++pass) {
		if (pass)
			prepared = d1_in_d2_read_assets("descent.pig", "source.256", &error);
		require(prepared && d1_in_d2_publish_assets(prepared, &error), error ? error : "publish original D1 generation");
		for (const auto &texture : TmapInfo)
			require(!(texture.flags & (TMI_WATER | TMI_FORCE_FIELD)), "native publication clears D2 surface flags across the complete table");
		require(d1_in_d2_use_d1_gameplay(), "publication commits D1 gameplay independently of the mission descriptor");
		require(d1_in_d2_has_native_assets() && NumTextures == 1 && Num_bitmap_files == 2 && N_polygon_models == 3 && Num_sound_files == 1, "publication replaces live table counts instead of overlaying D2 capacities");
		require(TmapInfo[0].destroyed == -1 && TmapInfo[0].damage == 4 * F1_0 && TmapInfo[0].lighting == F1_0 / 2 && WallAnims[0].frames[0] == 0, "engine tables consume original D1 texture and door definitions");
		char bitmap_name[] = "source", sound_name[] = "sound";
		require(piggy_find_bitmap(bitmap_name).index == 1 && piggy_find_sound(sound_name) == 0, "published resources resolve by their original names");
		require(GameBitmaps[1].bm_data[0] == 1 && GameSounds[0].data[0] == 5 && gr_palette[0] == 17 && gr_fade_table[255] == 255, "engine owns usable source pixels, samples and palette with transparent light levels");
		require(GameSounds[0].freq == SAMPLE_RATE_22K && GameSounds[0].length == 8 && GameSounds[0].data[1] == 5 && GameSounds[0].data[2] == 6, "plain SDL output retains the duration of D1 11 kHz samples at a 22 kHz device rate");
		piggy_bitmap_page_out_all();
		require(GameBitmaps[1].bm_data[0] == 1 && !(GameBitmaps[1].bm_flags & BM_FLAG_PAGED_OUT), "resident D1 images survive the normal engine paging flush");
		short primary = 0, overlay = static_cast<short>(0x4000);
		require(d1_in_d2_decode_level_textures(&primary, &overlay, 1) && primary == 0 && overlay == 0x4000,
		        "native D1 level decoding preserves source texture identity and orientation");
		primary = 1;
		require(d1_in_d2_decode_level_textures(&primary, &overlay, 1) && primary == 0 && overlay == 0x4000,
		        "native excess indices wrap in the original D1 table, not the larger D2 capacity");
		const int previous_version = Gamesave_current_version;
		Gamesave_current_version = 1;
		object reactor_object = {};
		reactor_object.type = OBJ_CNTRLCEN;
		verify_object(&reactor_object);
		require(reactor_object.rtype.pobj_info.model_num == 1, "initial object verification already sees the D1 reactor definition");
		Gamesave_current_version = previous_version;
		if (!pass) {
			bytes custom;
			append_int(custom, 0x474f5044); // DPOG
			append_int(custom, 1);
			append_int(custom, 1);
			append(custom, bytes{ 1, 0 });
			bytes header(18);
			std::memcpy(header.data(), "ignored", 7);
			header[9] = header[10] = 2;
			append(custom, header);
			append(custom, bytes{ 9, 10, 11, 12 });
			write_fixture("native.pg1", custom);
			char level_name[] = "native.rdl";
			d1_asset_generation *custom_assets = d1_in_d2_read_assets("descent.pig", "source.256", &error);
			require(custom_assets && d1_custom_read_assets(custom_assets, level_name, &error) && d1_in_d2_publish_assets(custom_assets, &error),
			        "publish original D1 data and custom pixels together");
			require(GameBitmaps[1].bm_data[0] == 9, "native DPOG uses its explicit source bitmap index even when its name differs");
			const auto active_pixels = GameBitmaps[1].bm_data;
			const auto active_model = Polygon_models[1].model_data;
			const auto active_samples = GameSounds[0].data;
			const char descriptor[] = "name = Keep D1\nnum_levels = 1\nnative.rdl\n";
			require(PHYSFS_mkdir("missions") != 0, "create D1 mission discovery fixture directory");
			write_fixture("missions/keepd1.msn", bytes(descriptor, descriptor + sizeof(descriptor) - 1));
			Mission *saved_mission = Current_mission;
			Current_mission = nullptr;
			char mission_name[] = "keepd1";
			require(load_mission_by_name(mission_name) != 0, "select a custom D1 mission while original assets are active");
			require(d1_in_d2_has_native_assets() && Polygon_models[1].model_data == active_model && active_model != nullptr && GameBitmaps[1].bm_data == active_pixels && GameSounds[0].data == active_samples, "deferred D1 mission preparation preserves the active model, custom pixels and samples");
			const char rejected_descriptor[] = "name = Invalid D2\nnum_levels = 0\n";
			write_fixture("missions/rejectd2.mn2", bytes(rejected_descriptor, rejected_descriptor + sizeof(rejected_descriptor) - 1));
			char rejected_name[] = "rejectd2";
			require(!load_mission_by_name(rejected_name) && !Current_mission,
			        "failed D2 selection frees the old and rejected descriptors through the normal mission loader");
			require(d1_in_d2_use_d1_gameplay() && Polygon_models[1].model_data == active_model && GameBitmaps[1].bm_data == active_pixels && GameSounds[0].data == active_samples,
			        "failed cross-profile selection retains committed D1 identity, models, custom images and sound storage");
			Current_mission = saved_mission;
			require(PHYSFS_delete("missions/rejectd2.mn2") != 0, "remove rejected D2 mission fixture");
			require(PHYSFS_delete("missions/keepd1.msn") != 0, "remove D1 mission selection fixture");

			d1_asset_generation *invalid = d1_in_d2_read_assets("descent.pig", "source.256", &error);
			require(invalid != nullptr, "prepare a replacement for rejected-publication coverage");
			invalid->textures[0].index = MAX_BITMAP_FILES;
			require(!d1_in_d2_publish_assets(invalid, &error), "invalid destination references reject publication");
			require(d1_in_d2_use_d1_gameplay(), "rejected publication leaves committed content identity unchanged");
			d1_in_d2_free_assets(invalid);
			require(d1_in_d2_has_native_assets() && NumTextures == 1 && N_polygon_models == 3 && Num_sound_files == 1 && Polygon_models[1].model_data == active_model && GameBitmaps[1].bm_data == active_pixels && GameBitmaps[1].bm_data[0] == 9 && GameSounds[0].data == active_samples, "rejected publication preserves live tables, models, custom replacements and sounds");
			require(PHYSFS_delete("native.pg1") != 0, "remove native custom bitmap fixture");
		}
	}
	GameArg.SndDigiSampleRate = previous_sample_rate;
#ifdef USE_SDLMIXER
	GameArg.SndDisableSdlMixer = previous_mixer_disabled;
#endif
	test_guidebot_source();
	const d1_guidebot_source guidebot_source = { "feature.ham", "feature.pig", "feature.256", "feature.s22", SAMPLE_RATE_22K };
	test_guidebot_publication("descent.pig", "source.256", guidebot_source);
	test_d1_custom_definitions();
	free_polygon_models();
	piggy_reset_asset_registry();
	require(!d1_in_d2_has_native_assets() && Num_bitmap_files == 1 && Num_sound_files == 0 && GameBitmaps[1].bm_data == nullptr, "registry cleanup retires the D1 generation without reloading D2 files");
	Highest_object_index = 0;
	std::memset(&Objects[0], 0, sizeof(Objects[0]));
	Objects[0].type = OBJ_CNTRLCEN;
	Objects[0].render_type = RT_POLYOBJ;
	Objects[0].rtype.pobj_info.model_num = 90;
	Reactors[0].model_num = 90;
	N_robot_types = 0;
	require(d1_in_d2_validate_assets() != 0, d1_in_d2_asset_validation_error());
	d1_in_d2_apply_robot_assets(1);
	require(Reactors[0].model_num == 1 && Objects[0].rtype.pobj_info.model_num == 1, "D1 reactor uses the live model from its own object table");
	require(Reactors[0].n_guns == 1 && Reactors[0].gun_points[0].x == 3 * F1_0 && Reactors[0].gun_dirs[0].z == F1_0, "D1 reactor gun geometry matches its model");
	maybe_delete_object(&Objects[0]);
	require(Objects[0].rtype.pobj_info.model_num == 2 && (Objects[0].flags & OF_DESTROYED) && !(Objects[0].flags & OF_SHOULD_BE_DEAD), "destroyed D1 reactor keeps its wreck instead of being deleted");
	d1_in_d2_apply_robot_assets(1);
	require(Objects[0].rtype.pobj_info.model_num == 2, "reloading D1 assets preserves an existing reactor wreck");
	pig[object_types + 100] = 3;
	write_fixture("descent.pig", pig);
	require(!d1_in_d2_validate_assets(), "out-of-range D1 reactor model is rejected");
	pig[object_types + 100] = 1;
	set_int(pig, guns - 4, 5);
	write_fixture("descent.pig", pig);
	require(!d1_in_d2_validate_assets(), "D1 reactor gun count cannot exceed the source table");
	pig.resize(guns + 1);
	write_fixture("descent.pig", pig);
	require(!d1_in_d2_validate_assets(), "truncated D1 reactor gun geometry is rejected");
	d1_in_d2_apply_robot_assets(0);
	require(Reactors[0].model_num == 90, "leaving D1 restores the D2 reactor definition");
	free_polygon_models();
	require(PHYSFS_delete("descent.pig") != 0, "remove D1 reactor fixture");
	require(PHYSFS_delete("source.256") != 0, "remove D1 generation palette fixture");
	require(PHYSFS_setWriteDir(original_write_dir.c_str()) && PHYSFS_unmount(fixture_dir.c_str()), "restore reactor fixture search path");
}

static void test_d2_embedded_sound_reload()
{
	const std::string original_write_dir = PHYSFS_getWriteDir();
	const std::string fixture_dir = original_write_dir + "/embedded-sound-fixtures";
	require(PHYSFS_mkdir("embedded-sound-fixtures") && PHYSFS_mount(fixture_dir.c_str(), nullptr, 0) && PHYSFS_setWriteDir(fixture_dir.c_str()), "isolate embedded HAM sound fixtures");
	bytes ham;
	append_int(ham, 0x214d4148);
	append_int(ham, 2);
	append_int(ham, 0);                              // sound directory offset, filled after the definitions
	for (int i = 0; i < 12; ++i) append_int(ham, 0); // tables through gauges and object bitmaps
	append(ham, bytes(132));                         // player ship
	for (int i = 0; i < 4; ++i) append_int(ham, 0);  // cockpits, multiplayer bitmap, reactors, marker
	append_int(ham, -1);                             // exit models in the older HAM layout
	append_int(ham, -1);
	append(ham, bytes(MAX_BITMAP_FILES * 2));
	set_int(ham, 8, static_cast<int>(ham.size()));
	append_int(ham, 1);
	append(ham, { 'd', 'e', 'm', 'o', 0, 0, 0, 0 });
	append_int(ham, 4);
	append_int(ham, 4);
	append_int(ham, 0);
	append(ham, { 11, 22, 33, 44 });
	// The registered filename keeps this fixture independent of device reopen;
	// the original embedded-sample reader opens the demo HAM for its payload
	write_fixture("descent2.ham", ham);
	write_fixture("d2demo.ham", ham);
	Mission *saved_mission = Current_mission;
	Mission mission = {};
	mission.descent_version = 2;
	Current_mission = &mission;
	free_polygon_models();
	piggy_reset_asset_registry();
	for (int pass = 0; pass < 2; ++pass) {
		d1_in_d2_load_mission_assets();
		char name[] = "demo";
		require(Num_sound_files == 1 && piggy_find_sound(name) == 0, "fresh registries reload embedded HAM sound headers");
		require(GameSounds[0].data == SoundBits && SoundBits != nullptr, "mission loading resolves embedded sound offsets to owned sample memory");
		require(GameSounds[0].length == 4 && GameSounds[0].data[0] == 11 && GameSounds[0].data[3] == 44, "embedded HAM sound payload survives registry replacement");
		free_polygon_models();
		piggy_reset_asset_registry();
	}
	Current_mission = saved_mission;
	require(PHYSFS_delete("descent2.ham") && PHYSFS_delete("d2demo.ham"), "remove embedded sound fixture sources");
	require(PHYSFS_setWriteDir(original_write_dir.c_str()) && PHYSFS_unmount(fixture_dir.c_str()), "restore embedded sound fixture search path");
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
	for (int i = 0; i < 2; ++i) append_int(ham, 0); // cockpits, multiplayer bitmap
	append_int(ham, 1);
	bytes reactor_record(200);
	set_int(reactor_record, 0, 90);
	append(ham, reactor_record);
	append_int(ham, 0);    // marker
	append(ham, bytes(2)); // bitmap translation
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
	require(d1_in_d2_load_mission_assets() != 0, "load synthetic mission HAM/V-HAM through the lifecycle facade");
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
	write_fixture("custom.hx1", d1_custom_definition_fixture());
	load_hxm(name);
	require(Robot_info[0].mass == 7 * F1_0 && Robot_info[0].drag == F1_0 / 4 && Robot_info[0].weapon_type == 0 && Robot_info[0].n_guns == 1 &&
	            Robot_info[0].anim_states[0][0].n_joints == 1 && Robot_joints[0].angles.p == 100 && Robot_joints[0].angles.b == 200 && Robot_joints[0].angles.h == 300,
	        "native D1 agrees with the imported HX1 robot/joint fixture");
	require(Polygon_models[0].rad == 7 * F1_0 && Dying_modelnums[0] == 1 && Dead_modelnums[0] == 2 && ObjBitmaps[0].index == 1,
	        "native D1 agrees with imported HX1 model/wreck/bitmap definitions");
	const hashtable previous_bitmaps = AllBitmapsNames, previous_sounds = AllDigiSndNames;
	const grs_bitmap previous_bitmap = GameBitmaps[1];
	const digi_sound previous_sound = GameSounds[0];
	const int previous_offset = GameBitmapOffset[1];
	hashtable_init(&AllBitmapsNames, 8);
	hashtable_init(&AllDigiSndNames, 8);
	char bitmap_name[] = "source", sound_name[] = "sound";
	hashtable_insert(&AllBitmapsNames, bitmap_name, 1);
	hashtable_insert(&AllDigiSndNames, sound_name, 0);
	ubyte pixels[] = { 1, 2, 3, 4 }, samples[] = { 5, 6, 7, 8 };
	gr_init_bitmap(&GameBitmaps[1], BM_LINEAR, 0, 0, 2, 2, 2, pixels);
	GameBitmapOffset[1] = 0;
	GameSounds[0].data = samples;
	GameSounds[0].length = 4;
	write_fixture("custom.pg1", d1_custom_dpog_fixture());
	write_fixture("custom.dtx", d1_custom_dtx_fixture());
	char level[] = "custom.rdl";
	load_custom_data(level);
	require(GameBitmaps[1].bm_data[0] == 21 && GameBitmaps[1].bm_data[3] == 24 && GameSounds[0].length == 3 &&
	            GameSounds[0].data[0] == 31 && GameSounds[0].data[2] == 33,
	        "native D1 agrees on DTX-over-PG1 precedence, original pixels and source samples");
	custom_remove();
	require(GameBitmaps[1].bm_data == pixels && GameSounds[0].data == samples, "native custom cleanup restores original resources");
	GameBitmaps[1] = previous_bitmap;
	GameSounds[0] = previous_sound;
	GameBitmapOffset[1] = previous_offset;
	hashtable_free(&AllBitmapsNames);
	hashtable_free(&AllDigiSndNames);
	AllBitmapsNames = previous_bitmaps;
	AllDigiSndNames = previous_sounds;
	require(PHYSFS_delete("custom.pg1") && PHYSFS_delete("custom.dtx"), "remove native custom pixel/sample fixtures");
	N_polygon_models = 1;
	free_polygon_models();
}
#endif

#ifndef DXX_BUILD_DESCENT_II
// Serialize a real source robot/model into the supported HX1 disk layout
// Only the native process writes these files; the imported process uses them
static void write_checkpoint_custom_assets(int robot_id, int bitmap_id, const std::string &stem)
{
	const robot_info &robot = Robot_info[robot_id];
	const polymodel &model = Polygon_models[robot.model_num];
	const auto vector = [](bytes &data, size_t at, const vms_vector &v) {
		set_int(data, at, v.x);
		set_int(data, at + 4, v.y);
		set_int(data, at + 8, v.z);
	};
	bytes record(480);
	set_int(record, 0, robot.model_num);
	for (int gun = 0; gun < MAX_GUNS; ++gun) {
		vector(record, 4 + gun * 12, robot.gun_points[gun]);
		record[100 + gun] = robot.gun_submodels[gun];
	}
	set_short(record, 108, robot.exp1_vclip_num);
	set_short(record, 110, robot.exp1_sound_num);
	set_short(record, 112, robot.exp2_vclip_num);
	set_short(record, 114, robot.exp2_sound_num);
	record[116] = static_cast<ubyte>(robot.weapon_type);
	record[118] = static_cast<ubyte>(robot.n_guns);
	record[119] = robot.contains_id;
	record[120] = robot.contains_count;
	record[121] = robot.contains_prob;
	record[122] = robot.contains_type;
	set_short(record, 124, robot.score_value);
	set_int(record, 128, robot.lighting);
	set_int(record, 132, 83 * F1_0);
	set_int(record, 136, 7 * F1_0);
	set_int(record, 140, F1_0 / 4);
	for (int difficulty = 0; difficulty < NDL; ++difficulty) {
		set_int(record, 144 + difficulty * 4, robot.field_of_view[difficulty]);
		set_int(record, 164 + difficulty * 4, robot.firing_wait[difficulty]);
		set_int(record, 204 + difficulty * 4, robot.turn_time[difficulty]);
		set_int(record, 224 + difficulty * 4, robot.max_speed[difficulty]);
		set_int(record, 244 + difficulty * 4, robot.circle_distance[difficulty]);
		record[264 + difficulty] = robot.rapidfire_count[difficulty];
		record[269 + difficulty] = robot.evade_speed[difficulty];
	}
	record[274] = robot.cloak_type;
	record[275] = robot.attack_type;
	record[276] = robot.see_sound;
	record[277] = robot.attack_sound;
	record[278] = robot.claw_sound;
	record[280] = robot.boss_flag;
	for (int gun = 0; gun <= MAX_GUNS; ++gun)
		for (int state = 0; state < N_ANIM_STATES; ++state) {
			const size_t at = 296 + (gun * N_ANIM_STATES + state) * 4;
			set_short(record, at, robot.anim_states[gun][state].n_joints);
			set_short(record, at + 2, robot.anim_states[gun][state].offset);
		}
	set_int(record, 476, 0xabcd);
	bytes data = hxm_header();
	append_int(data, 1);
	append_int(data, robot_id);
	append(data, record);
	append_int(data, 1);
	append_int(data, 0);
	bytes joint(8);
	set_short(joint, 0, Robot_joints[0].jointnum);
	set_short(joint, 2, 123);
	set_short(joint, 4, 234);
	set_short(joint, 6, 345);
	append(data, joint);
	append_int(data, 1);
	append_int(data, robot.model_num);
	record = bytes(734);
	set_int(record, 0, model.n_models);
	set_int(record, 4, model.model_data_size);
	for (int sub = 0; sub < MAX_SUBMODELS; ++sub) {
		set_int(record, 12 + sub * 4, model.submodel_ptrs[sub]);
		vector(record, 52 + sub * 12, model.submodel_offsets[sub]);
		vector(record, 172 + sub * 12, model.submodel_norms[sub]);
		vector(record, 292 + sub * 12, model.submodel_pnts[sub]);
		set_int(record, 412 + sub * 4, model.submodel_rads[sub]);
		record[452 + sub] = model.submodel_parents[sub];
		vector(record, 462 + sub * 12, model.submodel_mins[sub]);
		vector(record, 582 + sub * 12, model.submodel_maxs[sub]);
	}
	vector(record, 702, model.mins);
	vector(record, 714, model.maxs);
	set_int(record, 726, model.rad + F1_0);
	record[730] = model.n_textures;
	set_short(record, 731, model.first_texture);
	record[733] = model.simpler_model;
	append(data, record);
	append(data, bytes(model.model_data, model.model_data + model.model_data_size));
	append_int(data, Dying_modelnums[robot.model_num]);
	append_int(data, Dead_modelnums[robot.model_num]);
	append_int(data, 1);
	append_int(data, 0);
	append(data, { static_cast<ubyte>(bitmap_id), static_cast<ubyte>(bitmap_id >> 8) });
	write_fixture((stem + ".hx1").c_str(), data);
	data = d1_custom_dpog_fixture();
	set_short(data, 12, bitmap_id);
	const int width = GameBitmaps[bitmap_id].bm_w, height = GameBitmaps[bitmap_id].bm_h;
	data[23] = static_cast<ubyte>(width);
	data[24] = static_cast<ubyte>(height);
	data[25] = static_cast<ubyte>((width >> 8) | ((height >> 8) << 4));
	data.resize(32 + width * height);
	for (size_t i = 32; i < data.size(); ++i) data[i] = static_cast<ubyte>(9 + (i - 32) % 4);
	write_fixture((stem + ".pg1").c_str(), data);
	data.clear();
	append_int(data, 0);
	append_int(data, 1);
	record = bytes(20);
	bool found = false;
	for (int i = 0; i < AllDigiSndNames.size; ++i)
		if (AllDigiSndNames.key[i] && AllDigiSndNames.value[i] == 0) {
			std::memcpy(record.data(), AllDigiSndNames.key[i], std::min<size_t>(8, std::strlen(AllDigiSndNames.key[i])));
			found = true;
			break;
		}
	require(found, "resolve the original checkpoint sample name");
	set_int(record, 8, 3);
	set_int(record, 12, 3);
	append(data, record);
	append(data, { 31, 32, 33 });
	write_fixture((stem + ".dtx").c_str(), data);
}
#endif

// Use real native save/restore and the public import adapter, including a second
// checkpoint taken during the route. The runner provides an isolated write dir
static void write_checkpoint_frame_trace(const char *directory, const char *checkpoints, bool custom, int level)
{
	using nlohmann::json;
	require((level == 1 || level == 7 || level == 27) && (!custom || level == 1), "select a supported checkpoint fixture level");
	const std::string hog = std::string(directory) + "/DESCENT.HOG";
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount original resources for restored robot frames");
	Game_mode = 0;
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SndDigiSampleRate = SAMPLE_RATE_11K;
#endif
	GameArg.SysInputDemoNoRender = 1;
	GameArg.SysWindow = GameCfg.WindowMode = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	GameCfg.TexFilt = 0;
	Game_screen_mode = SM(640, 480);
	digi_select_system(SDLAUDIO_SYSTEM);
#ifdef DXX_BUILD_DESCENT_II
	require(PHYSFS_mount(checkpoints, "checkpoints", 1), "mount native checkpoints for import");
	require(d1_in_d2_init_base_resources(1), "select original D1 resources for checkpoint import");
#else
	(void) checkpoints;
#endif
	load_text();
	require(SDL_Init(SDL_INIT_VIDEO) == 0 && gr_init(Game_screen_mode) == 0, "initialize native save thumbnail renderer");
	gr_use_palette_table("palette.256");
	gamefont_init();
	gamedata_init();
#ifdef DXX_BUILD_DESCENT_II
	d1_in_d2_init_startup_bitmaps();
	char mission[] = "descent";
#else
	char mission[] = "";
#endif
	texmerge_init(10);
	init_game();
	require(load_mission_by_name(mission), "select First Strike for checkpoint frames");
	const std::string level_name = Level_names[level - 1];
	const std::string source_stem = level_name.substr(0, level_name.find_last_of('.'));
	const std::string stem = custom ? "chklevel" : source_stem;
	// These overlay names belong only to the runner's isolated write directory
	for (const std::string &prefix : { source_stem, stem })
		for (const char *extension : { ".pg1", ".dtx", ".hx1" }) PHYSFS_delete((prefix + extension).c_str());
	Player_num = 0;
	N_players = 1;
	std::strcpy(Players[0].callsign, "aistate");
	init_player_stats_game(0);
	Difficulty_level = 2;
#ifdef DXX_BUILD_DESCENT_II
	input_demo_set_skip_level_intro(1);
	StartNewGame(level);
#else
	StartNewLevelSub(level, 0, 0);
	char baseline[] = "baseline.sav";
	char description[21] = "AI checkpoint"; // native DESC_LENGTH is 20
#endif
	int robot_index = -1;
	for (int i = 1; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_ROBOT && !Robot_info[Objects[i].id].boss_flag) {
			robot_index = i;
			break;
		}
	require(robot_index >= 0, "First Strike contains a native robot for checkpoint scenarios");
	const int custom_bitmap = Textures[0].index;
	const int original_bitmap_width = GameBitmaps[custom_bitmap].bm_w, original_bitmap_height = GameBitmaps[custom_bitmap].bm_h;
#ifdef DXX_BUILD_DESCENT_II
	const int robot_id = Objects[robot_index].id;
	const fix original_mass = Robot_info[robot_id].mass;
#endif
	const fix original_radius = Polygon_models[Robot_info[Objects[robot_index].id].model_num].rad;
	if (custom) {
#ifdef DXX_BUILD_DESCENT_II
		for (const char *extension : { ".pg1", ".dtx", ".hx1" }) {
			const std::string name = stem + extension;
			PHYSFS_file *file = PHYSFS_openRead(("checkpoints/" + name).c_str());
			require(file != nullptr, "open the exact custom assets used by native checkpoint recording");
			const auto size = PHYSFS_fileLength(file);
			require(size > 0, "native checkpoint custom asset is nonempty");
			bytes payload(static_cast<size_t>(size));
			require(PHYSFS_readBytes(file, payload.data(), payload.size()) == size && PHYSFS_close(file), "read complete native custom asset");
			write_fixture(name.c_str(), payload);
		}
#else
		write_checkpoint_custom_assets(Objects[robot_index].id, custom_bitmap, stem);
#endif
		require(PHYSFS_mkdir("missions"), "create isolated custom checkpoint mission directory");
		PHYSFS_file *source_level = PHYSFS_openRead(level_name.c_str());
		require(source_level != nullptr && PHYSFS_fileLength(source_level) > 0, "open source mine for isolated custom mission");
		bytes mine(static_cast<size_t>(PHYSFS_fileLength(source_level)));
		require(PHYSFS_readBytes(source_level, mine.data(), mine.size()) == static_cast<PHYSFS_sint64>(mine.size()) && PHYSFS_close(source_level), "read the complete source mine");
		write_fixture((stem + ".rdl").c_str(), mine);
		const std::string descriptor = "name = Checkpoint assets\nnum_levels = 1\n" + stem + ".rdl\n";
		write_fixture("missions/chkasset.msn", bytes(descriptor.begin(), descriptor.end()));
		char custom_mission[] = "chkasset";
		require(load_mission_by_name(custom_mission), "select the custom D1 mission before checkpoint recording/import");
#ifdef DXX_BUILD_DESCENT_II
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
#else
		StartNewLevelSub(1, 0, 0);
#endif
	}
	const auto asset_snapshot = [&]() -> json {
		if (!custom) return nullptr;
		const robot_info &info = Robot_info[Objects[robot_index].id];
		const polymodel &model = Polygon_models[info.model_num];
		const grs_bitmap &bitmap = GameBitmaps[custom_bitmap];
		const digi_sound &sample = GameSounds[0];
		require(info.mass == 7 * F1_0 && info.drag == F1_0 / 4 && info.strength == 83 * F1_0 && model.rad == original_radius + F1_0,
		        "checkpoint level preparation installs the custom robot and model definitions");
		require(bitmap.bm_w == original_bitmap_width && bitmap.bm_h == original_bitmap_height && bitmap.bm_data[0] == 9 && bitmap.bm_data[3] == 12 &&
		            sample.length == 3 && sample.data[0] == 31 && sample.data[2] == 33,
		        "checkpoint level preparation installs the original custom pixels and samples");
		require(ObjBitmaps[0].index == custom_bitmap && Robot_joints[0].angles.p == 123 && Robot_joints[0].angles.b == 234 && Robot_joints[0].angles.h == 345,
		        "checkpoint level preparation installs custom joint and object texture references");
		return { { "robot", Objects[robot_index].id }, { "model", info.model_num }, { "mass", info.mass }, { "drag", info.drag }, { "strength", info.strength }, { "model_radius", model.rad }, { "submodels", model.n_models }, { "model_bytes", model.model_data_size }, { "bitmap", custom_bitmap }, { "pixels", bytes(bitmap.bm_data, bitmap.bm_data + bitmap.bm_w * bitmap.bm_h) }, { "samples", bytes(sample.data, sample.data + sample.length) }, { "joint", { Robot_joints[0].jointnum, Robot_joints[0].angles.p, Robot_joints[0].angles.b, Robot_joints[0].angles.h } } };
	};
	const json fresh_assets = asset_snapshot();
#ifndef DXX_BUILD_DESCENT_II
	require(state_save_all_sub(baseline, description), "write actual native baseline checkpoint");
#endif
	const auto texture_references = [] {
		json result = json::array();
		for (int segment = 0; segment <= Highest_segment_index; ++segment)
			for (int side = 0; side < MAX_SIDES_PER_SEGMENT; ++side)
				result.push_back({ Segments[segment].sides[side].tmap_num, Segments[segment].sides[side].tmap_num2 });
		return result;
	};
	const json fresh_textures = texture_references();
	const json fresh_triggers = snapshot_native_triggers();
	const auto reactor_guns = [] {
		json guns = json::array();
		for (int slot = 0; slot <= Highest_object_index; ++slot) {
			const auto &obj = Objects[slot];
			if (obj.type != OBJ_CNTRLCEN || obj.control_type != CT_CNTRLCEN || obj.render_type != RT_POLYOBJ)
				continue;
			const reactor *definition = get_reactor_definition(obj.id);
			for (int gun = 0; gun < definition->n_guns; ++gun) {
				const auto &position = obj.ctype.reactor_info.gun_pos[gun];
				const auto &direction = obj.ctype.reactor_info.gun_dir[gun];
				guns.push_back({ slot, gun, position.x, position.y, position.z, direction.x, direction.y, direction.z });
			}
		}
		return guns;
	};
	const json fresh_reactor_guns = reactor_guns();
	const auto hidden_reactors = [] {
		json result = json::array();
		for (int slot = 0; slot <= Highest_object_index; ++slot) {
			const auto &obj = Objects[slot];
			if (obj.type == OBJ_GHOST && obj.control_type == CT_CNTRLCEN) {
				require(obj.render_type == RT_NONE && !Control_center_present, "boss reactor placeholder is hidden and inactive");
				result.push_back({ slot, obj.type, obj.control_type, obj.render_type, obj.movement_type, obj.segnum,
				                   obj.pos.x, obj.pos.y, obj.pos.z, obj.shields, obj.rtype.pobj_info.model_num });
			}
		}
		return result;
	};
	const json fresh_hidden_reactors = hidden_reactors();
#ifdef DXX_BUILD_DESCENT_II
	for (int segment = 0; segment <= Highest_segment_index; ++segment)
		require(!(Segment2s[segment].s2_flags & (S2F_AMBIENT_LAVA | S2F_AMBIENT_WATER)),
		        "loading an original D1 mine does not add D2's random water/lava sounds");
#endif
	for (int slot = 0; slot <= Highest_object_index; ++slot) {
		const object &boss = Objects[slot];
		if (boss.type != OBJ_ROBOT || !Robot_info[boss.id].boss_flag)
			continue;
		const fix strength = Robot_info[boss.id].strength;
		require(boss.shields == strength, "fresh D1 bosses use their original full strength");
		for (int difficulty = 0; difficulty < NDL; ++difficulty)
			require(boss_health_maximum_for_difficulty(strength, difficulty) == strength,
			        "native boss health maximum is independent of difficulty");
		for (int difficulty = 0; difficulty < NDL; ++difficulty) {
			difficulty_health_rescale_live_robots(Difficulty_level, difficulty);
			require(Objects[slot].shields == strength, "live difficulty changes retain native boss health");
		}
	}
	require(level == 1 ? !fresh_reactor_guns.empty() && fresh_hidden_reactors.empty()
	                   : fresh_reactor_guns.empty() && fresh_hidden_reactors.size() == 1,
	        "checkpoint mine contains the expected live or hidden reactor");
	const auto restore = [&](char *filename) {
#ifdef DXX_BUILD_DESCENT_II
		if (custom && std::strcmp(filename, "route-0.sav") == 0) {
			char stock_mission[] = "descent";
			require(load_mission_by_name(stock_mission), "leave the custom mission before checkpoint import");
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			d1_custom_texture_stats stats = {};
			d1_custom_get_stats(&stats);
			require(!stats.files_found && Robot_info[robot_id].mass == original_mass && Polygon_models[Robot_info[robot_id].model_num].rad == original_radius,
			        "checkpoint import starts from stock assets, with no retained custom definitions");
		}
		const std::string path = std::string("checkpoints/") + filename;
		PHYSFS_file *file = PHYSFS_openRead(path.c_str());
		require(file != nullptr, "open the actual native checkpoint");
		std::vector<uint8_t> data(static_cast<size_t>(PHYSFS_fileLength(file)));
		require(PHYSFS_readBytes(file, data.data(), data.size()) == static_cast<PHYSFS_sint64>(data.size()), "read complete native checkpoint");
		PHYSFS_close(file);
		d1_save_translate_checkpoint_start start = {};
		require(d1_save_translate_read_checkpoint_start(data.data(), data.size(), &start), "decode actual native checkpoint start");
		char base_mission[] = "descent";
		require(load_mission_by_name(start.mission_name[0] ? start.mission_name : base_mission),
		        "reselect the actual checkpoint mission through the production mission loader");
		// Match input_demo_start_loaded_replay_common: the normal level lifecycle
		// prepares all assets before the adapter applies checkpoint world state
		input_demo_set_skip_level_intro(1);
		StartNewGame(start.current_level);
		GameTime64 = start.game_time;
		Difficulty_level = start.difficulty;
		if (std::strcmp(filename, "route-0.sav") == 0) {
			const std::vector<object> previous_objects(Objects, Objects + Highest_object_index + 1);
			const int previous_primary = delayed_primary_autoselect_weapon_index;
			for (int invalid = 0; invalid < 7; ++invalid) {
				auto damaged = data;
				if (!invalid)
					damaged.pop_back();
				else if (invalid < 4) {
					// Version 17 ends in the four autoselect_runtime.h integers
					const size_t offset = damaged.size() - (invalid == 1 ? 16 : invalid == 2 ? 8
					                                                                         : 4);
					set_int(damaged, offset, invalid == 1 ? 2 : 5);
				} else {
					// Native object_rw starts with signature, type/id, links, then
					// control/movement/render bytes; change the first player to an
					// invalid ghost without changing the serialized record size
					const size_t offset = start.object_stream_offset;
					require(damaged.at(offset + 4) == OBJ_PLAYER, "invalid ghost fixture starts from the actual saved player");
					damaged.at(offset + 4) = OBJ_GHOST;
					damaged.at(offset + 5) = MAX_PLAYERS;
					damaged.at(offset + 10) = invalid == 4 ? CT_NONE : CT_CNTRLCEN;
					damaged.at(offset + 11) = invalid == 6 ? MT_PHYSICS : MT_NONE;
					damaged.at(offset + 12) = invalid == 5 ? RT_POLYOBJ : RT_NONE;
				}
				require(!d1_save_translate_apply_checkpoint_objects(damaged.data(), damaged.size(), &start), "reject truncated checkpoints, invalid pending selection and invalid player/reactor ghosts");
				require(std::memcmp(Objects, previous_objects.data(), previous_objects.size() * sizeof(object)) == 0 &&
				            delayed_primary_autoselect_weapon_index == previous_primary,
				        "rejected checkpoint leaves the current objects and pending selection intact");
			}
		}
		require(d1_save_translate_apply_checkpoint_objects(data.data(), data.size(), &start), "restore actual native checkpoint objects, world and AI");
		d1_save_translate_apply_checkpoint_player(&start, "aistate");
#else
		require(state_restore_all_sub(filename), "restore actual checkpoint through the native D1 loader");
#endif
		ConsoleObject = Viewer = &Objects[Players[Player_num].objnum];
		FrameTime = F1_0 / 64;
		const auto restored_reactor_guns = reactor_guns();
		if (restored_reactor_guns != fresh_reactor_guns)
			std::fprintf(stderr, "Reactor gun restore: fresh=%s restored=%s\n", fresh_reactor_guns.dump().c_str(), restored_reactor_guns.dump().c_str());
		require(restored_reactor_guns == fresh_reactor_guns, "checkpoint rebuilds reactor gun positions and directions before simulation");
		require(hidden_reactors() == fresh_hidden_reactors, "checkpoint preserves the hidden reactor instead of treating it as a player");
	};
	const auto vector = [](const vms_vector &v) { return json::array({ v.x, v.y, v.z }); };
	json cases = json::array();
	for (int scenario = 0; scenario < 7; ++scenario) {
		char filename[32], resume[32];
		std::snprintf(filename, sizeof(filename), "route-%d.sav", scenario);
		std::snprintf(resume, sizeof(resume), "resume-%d.sav", scenario);
#ifndef DXX_BUILD_DESCENT_II
		restore(baseline);
		GameTime64 = 20 * F1_0;
		object &source = Objects[robot_index];
		init_ai_object(robot_index, scenario < 2 ? 0x82 : 0x84, source.segnum);
		d_srand(12345);
		create_n_segment_path(&source, scenario == 3 || scenario == 4 ? 12 : 4, -1);
		ai_static &source_ai = source.ctype.ai_info;
		require(source_ai.path_length >= 2, "create a real multi-segment checkpoint route");
		source_ai.flags[4] = scenario == 0 || scenario == 5;
		source_ai.PATH_DIR = scenario == 4 ? -1 : 1;
		source_ai.cur_path_index = scenario == 3 ? source_ai.path_length - 1 : 0;
		source_ai.SKIP_AI_COUNT = scenario == 5 ? 1 : 0;
		source_ai.CURRENT_STATE = source_ai.GOAL_STATE = AIS_REST;
		const point_seg &point = Point_segs[source_ai.hide_index + source_ai.cur_path_index];
		source.pos = point.point;
		obj_relink(robot_index, point.segnum);
		source.mtype.phys_info.velocity = {};
		ai_local &local = Ai_local_info[robot_index];
		local.mode = scenario < 2 ? 5 : AIM_FOLLOW_PATH;
		local.next_fire = F1_0 / 2;
		local.time_since_processed = 10 * F1_0;
		local.player_awareness_type = PA_NEARBY_ROBOT_FIRED;
		local.player_awareness_time = 10 * F1_0;
		local.time_player_seen = GameTime64 - F1_0;
		if (scenario == 6) {
			// A visible target and ready gun exercise firing clocks and RNG after
			// restore, using the original robot and weapon definitions
			ConsoleObject->pos = source.pos;
			vm_vec_scale_add2(&ConsoleObject->pos, &source.orient.fvec, 8 * F1_0);
			const int player_segment = find_point_seg(&ConsoleObject->pos, source.segnum);
			require(player_segment >= 0, "place the checkpoint target inside the real mine");
			obj_relink(Players[0].objnum, player_segment);
			source_ai.CURRENT_STATE = source_ai.GOAL_STATE = AIS_FIRE;
			local.next_fire = 0;
			local.previous_visibility = 2;
			for (int gun = 0; gun < MAX_GUNS; ++gun)
				local.goal_state[gun] = local.achieved_state[gun] = AIS_FIRE;
		}
		PrimaryWeaponPickedUp = scenario % 2;
		SecondaryWeaponPickedUp = (scenario / 2) % 2;
		delayed_primary_autoselect_weapon_index = scenario == 6 ? 16 : scenario - 1;
		delayed_secondary_autoselect_weapon_index = scenario % 6 - 1;
		Believed_player_pos = ConsoleObject->pos;
		// Native checkpoints carry compound actions and source ON independently
		// of one-shot state; importing must not reduce these to one D2 type
		require(Num_triggers > 0, "checkpoint mine has a trigger record");
		Triggers[0].flags = TRIGGER_SHIELD_DAMAGE | TRIGGER_ENERGY_DRAIN |
		                    ((scenario & 1) ? TRIGGER_ON : 0) |
		                    ((scenario & 2) ? TRIGGER_ONE_SHOT : 0);
		require(state_save_all_sub(filename, description), "write native hide/follow checkpoint");
#endif
		restore(filename);
		const json restored_assets = asset_snapshot();
		require(restored_assets == fresh_assets, "checkpoint restore retains the complete level asset generation");
		const json restored_textures = texture_references();
		const json restored_triggers = snapshot_native_triggers();
		require(native_trigger_flags(0) == (TRIGGER_SHIELD_DAMAGE | TRIGGER_ENERGY_DRAIN |
		                                    ((scenario & 1) ? TRIGGER_ON : 0) |
		                                    ((scenario & 2) ? TRIGGER_ONE_SHOT : 0)),
		        "actual native checkpoint retains compound actions and independent source state");
		object &robot = Objects[robot_index];
		require(robot.ctype.ai_info.flags[4] == (scenario == 0 || scenario == 5), "checkpoint preserves native hide submode before any robot frame");
		require(PrimaryWeaponPickedUp == scenario % 2 && SecondaryWeaponPickedUp == (scenario / 2) % 2 &&
		            delayed_secondary_autoselect_weapon_index == scenario % 6 - 1,
		        "checkpoint preserves pending pickup and secondary selection state");
		require(delayed_primary_autoselect_weapon_index == (scenario == 6 ?
#ifdef DXX_BUILD_DESCENT_II
		                                                                  LASER_INDEX
		                                                                  :
#else
		                                                                  16
		                                                                  :
#endif
		                                                                  scenario - 1),
		        "checkpoint preserves the pending primary selection, including native quad lasers");
		d_tick_count = 100;
		d_srand(0x1234);
		d_srand_fx(0x5678);
		const auto sim = d_rand_get_call_count(), fx = d_rand_get_stream_call_count(D_RNG_FX);
		json frames = json::array();
		for (int frame = 0; frame < 4; ++frame) {
			if (frame == 2) {
#ifndef DXX_BUILD_DESCENT_II
				require(state_save_all_sub(resume, description), "write a native checkpoint during restored route execution");
#endif
				restore(resume);
				require(asset_snapshot() == fresh_assets, "mid-route checkpoint reload uses the same custom assets");
				require(snapshot_native_triggers() == restored_triggers, "mid-route checkpoint retains native action flags and links");
			}
			do_ai_frame(&robot);
			const ai_static &aip = robot.ctype.ai_info;
			const ai_local &local = Ai_local_info[robot_index];
			// This endpoint reversal is specific to the authored level-1 route
			if (level == 1 && frame == 0 && (scenario == 3 || scenario == 4))
				require(aip.PATH_DIR == (scenario == 3 ? -1 : 1), "restored follow route reverses at a blocked endpoint");
			if (scenario == 0) {
				const vms_vector zero = {};
				require(aip.flags[4] == 1 && std::memcmp(&robot.mtype.phys_info.velocity, &zero, sizeof(zero)) == 0,
				        "restored hiding robot stays hidden across both checkpoint loads");
			}
			json path = json::array();
			for (int i = 0; i < aip.path_length; ++i) {
				require(aip.hide_index >= 0 && aip.hide_index + i < Point_segs_free_ptr - Point_segs, "restored frame route remains in shared path storage");
				const point_seg &point = Point_segs[aip.hide_index + i];
				path.push_back({ { "segment", point.segnum }, { "point", vector(point.point) } });
			}
			frames.push_back({ { "mode", local.mode }, { "behavior", aip.behavior }, { "submode", aip.flags[4] }, { "skip", aip.SKIP_AI_COUNT }, { "state", aip.CURRENT_STATE }, { "goal", aip.GOAL_STATE }, { "position", vector(robot.pos) }, { "velocity", vector(robot.mtype.phys_info.velocity) }, { "forward", vector(robot.orient.fvec) }, { "right", vector(robot.orient.rvec) }, { "up", vector(robot.orient.uvec) }, { "next_fire", local.next_fire }, { "last_seen", local.time_player_seen }, { "path_index", aip.cur_path_index }, { "path_direction", aip.PATH_DIR }, { "path", path }, { "sim_draws", d_rand_get_call_count() - sim }, { "fx_draws", d_rand_get_stream_call_count(D_RNG_FX) - fx } });
			frames.back()["object_physics"] = { robot.size, robot.mtype.phys_info.mass, robot.mtype.phys_info.drag, robot.shields, robot.rtype.pobj_info.model_num };
			GameTime64 += FrameTime;
			++d_tick_count;
		}
		cases.push_back({ { "scenario", scenario }, { "textures", restored_textures }, { "triggers", restored_triggers }, { "assets", restored_assets }, { "frames", frames } });
	}
	json boss_checkpoints = json::array();
	if (level != 1) {
		for (int difficulty = 0; difficulty < NDL; ++difficulty) {
			for (int variant = 0; variant < 6; ++variant) {
				char base[] = "baseline.sav", filename[32];
				restore(base);
				int boss_slot = -1;
				for (int slot = 0; slot <= Highest_object_index; ++slot)
					if (Objects[slot].type == OBJ_ROBOT && Robot_info[Objects[slot].id].boss_flag)
						boss_slot = slot;
				require(boss_slot >= 0, "checkpoint fixture contains a native boss");
				const object original = Objects[boss_slot];
				const fix strength = Robot_info[original.id].strength;
				const fix health[] = { strength * 2, strength, strength / 2, 1, 0, -F1_0 };
				std::snprintf(filename, sizeof(filename), "boss-%d-%d.sav", difficulty, variant);
#ifndef DXX_BUILD_DESCENT_II
				Difficulty_level = difficulty;
				Objects[boss_slot].shields = health[variant];
				Objects[boss_slot].mtype.phys_info.mass += F1_0;
				Objects[boss_slot].mtype.phys_info.drag += 1;
				require(state_save_all_sub(filename, description), "write native boss health and physics checkpoint");
#endif
				restore(filename);
				const object &boss = Objects[boss_slot];
				require(Difficulty_level == difficulty && boss.shields == health[variant],
				        "native checkpoint retains exact boss health at every difficulty, including over-default and dying values");
				require(boss.mtype.phys_info.mass == original.mtype.phys_info.mass + F1_0 &&
				            boss.mtype.phys_info.drag == original.mtype.phys_info.drag + 1 && boss.size == original.size,
				        "native checkpoint retains boss physics instead of copying definition defaults");
				boss_checkpoints.push_back({ difficulty, variant, boss_slot, boss.id, boss.shields,
				                             boss.mtype.phys_info.mass, boss.mtype.phys_info.drag, boss.size });
			}
		}
	}
	FILE *output = std::fopen("frames.json", "wb");
	require(output != nullptr, "open restored frame trace");
	const std::string result = json({ { "fresh_textures", fresh_textures }, { "fresh_triggers", fresh_triggers }, { "fresh_assets", fresh_assets }, { "reactor_guns", fresh_reactor_guns }, { "hidden_reactors", fresh_hidden_reactors }, { "boss_checkpoints", boss_checkpoints }, { "cases", cases } }).dump(2) + "\n";
	require(std::fwrite(result.data(), 1, result.size(), output) == result.size(), "write complete restored frame trace");
	std::fclose(output);
	std::puts("Restored robot frame trace passed");
}

// Exercise actual world transitions and each engine's ordinary save path.
// Timer-generated Return keys dismiss presentation; no production transition is mocked.
static void finish_campaign_game(int death)
{
	// Game-window close returns to the executable's event loop with longjmp
	// Keep its fixture landing frame free of C++ objects needing destruction
	if (!setjmp(LeaveEvents)) {
		if (death) DoPlayerDead();
		else PlayerFinishedLevel(0);
	}
}

static void write_campaign_trace(const char *directory)
{
	using nlohmann::json;
	const std::string hog = std::string(directory) + "/DESCENT.HOG";
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount campaign resources");
	Game_mode = 0;
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	GameArg.SysInputDemoNoRender = 1;
	GameArg.SysWindow = GameCfg.WindowMode = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	GameCfg.TexFilt = 0;
	Game_screen_mode = SM(640, 480);
	digi_select_system(SDLAUDIO_SYSTEM);
#ifdef DXX_BUILD_DESCENT_II
	require(d1_in_d2_init_base_resources(1), "select native campaign resources");
#endif
	load_text();
	require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0 && gr_init(Game_screen_mode) == 0, "initialize campaign renderer");
	key_init();
	mouse_init();
	gr_use_palette_table("palette.256");
	gamefont_init();
	gamedata_init();
#ifdef DXX_BUILD_DESCENT_II
	d1_in_d2_init_startup_bitmaps();
	char mission[] = "descent";
#else
	char mission[] = "";
#endif
	texmerge_init(10);
	init_game();
	require(load_mission_by_name(mission), "select native campaign");
	Player_num = 0;
	N_players = 1;
	std::strcpy(Players[0].callsign, "campaign");
	Difficulty_level = 2;
	const SDL_TimerID timer = SDL_AddTimer(100, [](Uint32 interval, void *) -> Uint32 {
		static bool down = false;
		down = !down;
		SDL_Event event = {};
		event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
		event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
		event.key.keysym.sym = SDLK_RETURN;
		SDL_PushEvent(&event);
		return interval; }, nullptr);
	require(timer != 0, "start presentation dismissal timer");
	json trace = json::array();
	const auto snapshot = [&](const char *phase) {
		const player &p = Players[0];
		trace.push_back({ { "phase", phase }, { "level", Current_level_num }, { "score", p.score }, { "last_score", p.last_score }, { "lives", p.lives }, { "shields", p.shields }, { "energy", p.energy }, { "hostages", p.hostages_on_board }, { "rescued", p.hostages_rescued_total }, { "primary", p.primary_weapon_flags }, { "secondary", p.secondary_weapon_flags }, { "homing", p.secondary_ammo[HOMING_INDEX] }, { "triggers", snapshot_native_triggers() } });
		std::fprintf(stderr, "Campaign phase %s: level %d score %d lives %d\n", phase, Current_level_num, p.score, p.lives);
		std::fflush(stderr);
	};
	for (int secret = 0; secret < -Last_secret_level; ++secret) {
		const int origin = Secret_level_table[secret];
		input_demo_set_skip_level_intro(1);
		StartNewGame(origin);
		Players[0].score = 10234;
		Players[0].last_score = 1000;
		Players[0].shields = 125 * F1_0;
		Players[0].energy = 77 * F1_0;
		Players[0].hostages_on_board = 2;
		Players[0].lives = 3;
		Players[0].primary_weapon_flags = 1 | (1 << PLASMA_INDEX);
		Players[0].secondary_weapon_flags = 1 | (1 << HOMING_INDEX);
		Players[0].secondary_ammo[HOMING_INDEX] = 7;
		int trigger_index = -1;
		for (int i = 0; i < Num_triggers; ++i)
			if (native_trigger_flags(i) & TRIGGER_SECRET_EXIT) trigger_index = i;
		require(trigger_index >= 0, "campaign origin contains native secret trigger");
		input_demo_set_skip_level_intro(1);
		check_trigger_sub(trigger_index, 0, 0);
		require(Current_level_num == -secret - 1, "native secret trigger reaches its exact destination");
		snapshot("secret_entry");
		char checkpoint[] = "campaign.sav", description[21] = "Native travel";
		require(state_save_all_sub(checkpoint, description), "save actual secret world");
		const json triggers_before = snapshot_native_triggers();
		Players[0].energy = 0;
#ifdef DXX_BUILD_DESCENT_II
		require(state_restore_all_sub(checkpoint, 0), "restore imported secret world through ordinary save path");
#else
		require(state_restore_all_sub(checkpoint), "restore native secret world through ordinary save path");
#endif
		require(snapshot_native_triggers() == triggers_before, "ordinary save restores complete native triggers");
		snapshot("secret_restore");
		Control_center_destroyed = 0;
		DoPlayerDead();
		require(Current_level_num == -secret - 1 && Players[0].lives == 2, "intact secret death respawns in that secret");
		snapshot("secret_respawn");
		Control_center_destroyed = 1;
		input_demo_set_skip_level_intro(1);
		DoPlayerDead();
		require(Current_level_num == origin + 1 && Players[0].lives == 1, "destroyed secret death advances to next normal level");
		snapshot("secret_destroyed_death");
		input_demo_set_skip_level_intro(1);
		StartNewGame(-secret - 1);
		input_demo_set_skip_level_intro(1);
		PlayerFinishedLevel(0);
		require(Current_level_num == origin + 1, "ordinary secret exit advances using mission table without entry history");
		snapshot("secret_exit");
		input_demo_set_skip_level_intro(1);
		StartNewGame(-secret - 1);
		Players[0].lives = 1;
		finish_campaign_game(1);
		require(!Game_wind && Players[0].lives == 0, "last life in a secret closes the game");
		snapshot("last_life");
	}
	input_demo_set_skip_level_intro(1);
	StartNewGame(Last_level);
	Players[0].score = Players[0].last_score = 0;
	Players[0].shields = Players[0].energy = 0;
	Players[0].hostages_on_board = 0;
	Players[0].lives = 1;
	finish_campaign_game(0);
	require(!Game_wind, "native campaign completion closes the game");
	snapshot("campaign_end");
	SDL_RemoveTimer(timer);
	const std::string output = trace.dump(2) + "\n";
	write_fixture("campaign.json", bytes(output.begin(), output.end()));
	std::puts("Native campaign transition trace passed");
}

// Drive the real private briefing session through ordinary window events. No
// interpreter fields or production test callbacks are exposed by either engine.
static struct {
	std::string name;
	int pages;
	bool complete;
	bool visited;
	nlohmann::json frames = nlohmann::json::array();
} Briefing_trace;

static int briefing_trace_event(d_event *event)
{
	if (event->type != EVENT_IDLE || Briefing_trace.visited)
		return 0;
	Briefing_trace.visited = true;
	window *wind = window_get_front();
	require(wind != nullptr, "briefing creates its real window");
	struct {
		event_type type;
		int keycode;
	} key = { EVENT_KEY_COMMAND, KEY_ENTER };
	d_event draw = { EVENT_WINDOW_DRAW };
	for (int page = 0; page < Briefing_trace.pages; ++page) {
		require(window_exists(wind), "briefing retains its window until the expected page");
		window_send_event(wind, reinterpret_cast<d_event *>(&key));
		for (int frame = 0; frame < 3; ++frame) {
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			window_send_event(wind, &draw);
			require(window_exists(wind), "valid briefing remains open while drawing");
			bytes pixels(SWIDTH * SHEIGHT * 3);
			glPixelStorei(GL_PACK_ALIGNMENT, 1);
			glReadBuffer(GL_BACK);
			glReadPixels(0, 0, SWIDTH, SHEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
			require(glGetError() == GL_NO_ERROR, "briefing draws with valid graphics resources");
			const std::string stem = Briefing_trace.name + "-" + std::to_string(page) + "-" + std::to_string(frame);
			write_fixture((stem + ".rgb").c_str(), pixels);
#ifdef HAVE_LIBPNG
			png_image image = {};
			image.version = PNG_IMAGE_VERSION;
			image.width = SWIDTH;
			image.height = SHEIGHT;
			image.format = PNG_FORMAT_RGB;
			require(png_image_write_to_file(&image, (stem + ".png").c_str(), 0, pixels.data(), -SWIDTH * 3, nullptr) != 0,
			        "write briefing visual review artifact");
#endif
			Briefing_trace.frames.push_back(stem + ".rgb");
		}
		if (page + 1 == Briefing_trace.pages && !Briefing_trace.complete)
			key.keycode = KEY_ESC;
		window_send_event(wind, reinterpret_cast<d_event *>(&key));
	}
	require(!window_exists(wind), "briefing closes on completion or explicit interruption");
	return 1;
}

#ifdef DXX_BUILD_DESCENT_II
static bool Briefing_failure_closes, Briefing_failure_visited;

static int briefing_failure_event(d_event *event)
{
	if (event->type != EVENT_IDLE) return 0;
	require(!Briefing_failure_visited, "malformed briefing finishes in one driven event");
	Briefing_failure_visited = true;
	window *wind = window_get_front();
	struct {
		event_type type;
		int keycode;
	} key = { EVENT_KEY_COMMAND, KEY_ENTER };
	d_event draw = { EVENT_WINDOW_DRAW };
	window_send_event(wind, reinterpret_cast<d_event *>(&key));
	window_send_event(wind, &draw);
	if (Briefing_failure_closes)
		require(!window_exists(wind), "failed briefing resource closes the owned session");
	else {
		require(window_exists(wind), "bounded malformed command can still be dismissed");
		window_close(wind);
	}
	return 1;
}

static void test_briefing_failures()
{
	set_default_handler(briefing_failure_event);
	auto run = [](const char *filename, bool visited, bool closes) {
		Briefing_failure_visited = false;
		Briefing_failure_closes = closes;
		char name[PATH_MAX];
		std::snprintf(name, sizeof(name), "%s", filename);
		event_flush();
		do_briefing_screens(name, 2);
		require(Briefing_failure_visited == visited && !window_get_front(), "failed or malformed D1 briefing releases its window without D2 fallback");
	};
	run("missing.tex", false, false);
	write_fixture("broken.tex", { '$' });
	run("broken.tex", false, false);
	write_fixture("moon01.pcx", { 1, 2, 3 });
	run("owned.tex", false, false);
	require(PHYSFS_delete("moon01.pcx"), "remove malformed briefing background");
	const std::string portrait = "$S6\n$Bmissing\n$S99\n";
	write_fixture("broken.tex", bytes(portrait.begin(), portrait.end()));
	run("broken.tex", true, true);
	for (const std::string &command : { std::string("$"), std::string("$P"), std::string("$F"), std::string(";comment without newline"),
	                                    std::string("$R999999999999999999999999\nrobot"), std::string("$C999999999999999999999999\ncolor"),
	                                    "$N" + std::string(200, 'a') + "\n", std::string(3000, 'x') }) {
		const std::string script = "$S6\n" + command;
		write_fixture("broken.tex", bytes(script.begin(), script.end()));
		run("broken.tex", true, false);
	}
	set_default_handler(nullptr);
}
#endif

static void write_briefing_trace(const char *directory, const char *d2_directory)
{
	const std::string hog = std::string(directory) + "/DESCENT.HOG";
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount native briefing assets");
	if (d2_directory)
		require(PHYSFS_mount(d2_directory, nullptr, 1), "mount optional D2 installation");
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	GameArg.SysWindow = GameCfg.WindowMode = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	GameCfg.TexFilt = 0;
	Game_screen_mode = SM(640, 480);
	digi_select_system(SDLAUDIO_SYSTEM);
#ifdef DXX_BUILD_DESCENT_II
	require(d1_in_d2_init_base_resources(1), "select D1 briefing content before initialization");
#endif
	load_text();
	require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0 && gr_init(Game_screen_mode) == 0, "initialize briefing renderer");
	key_init();
	mouse_init();
	event_init();
	gr_use_palette_table("palette.256");
	gamefont_init();
	gamedata_init();
#ifdef DXX_BUILD_DESCENT_II
	d1_in_d2_init_startup_bitmaps();
	char mission[] = "descent";
#else
	char mission[] = "";
#endif
	require(load_mission_by_name(mission), "select First Strike briefing metadata");
	set_default_handler(briefing_trace_event);
	auto run = [](const char *name, const char *filename, int level, int pages, bool complete, bool ending = false) {
		require(window_get_front() == nullptr, "prior briefing releases its window");
		Briefing_trace.name = name;
		Briefing_trace.pages = pages;
		Briefing_trace.complete = complete;
		Briefing_trace.visited = false;
		char file[PATH_MAX];
		std::snprintf(file, sizeof(file), "%s", filename);
		event_flush();
		if (ending) do_end_briefing_screens(file);
		else do_briefing_screens(file, level);
		require(Briefing_trace.visited && !window_get_front(), "owned briefing session enters and releases its window");
		std::fprintf(stderr, "Briefing case %s passed\n", name);
	};
	// Authored text exercises native source decoding as well as screen selection.
	run("registered-later", "briefing.txb", 2, 1, false);
	run("registered-secret", "briefing.txb", -1, 1, false);
	std::string script;
	for (int message = 1; message <= 40; ++message) {
		script += "$S" + std::to_string(message) + "\n";
		if (message == 6)
			script += "$R3\nSpider name must remain visible\n$P\n$Bendguy\nPortrait\n$P\n$Ndoor13\nDoor\n$P\n$Odoor13\nLooping bitmap\n$P\n";
		script += "$C2\nScreen " + std::to_string(message) + "\n$T40\nTab\tstop\n;hidden comment\n$$ dollar $; semicolon\n$F\n$F\n";
	}
	script += "$S99\n";
	write_fixture("owned.tex", bytes(script.begin(), script.end()));
	run("first", "owned.tex", 1, 5, true);
	run("commands", "owned.tex", 2, 5, true);
	run("later", "owned.tex", 6, 2, true);
	run("secret", "owned.tex", -2, 1, true);
	run("ending", "owned.tex", 0, 3, true, true);
	run("interrupted", "owned.tex", 2, 2, false);
	run("reopened", "owned.tex", 2, 5, true);
	// A loose mission-art override must beat the base archive, even with D2 present.
	grs_bitmap background = {};
	ubyte palette[768];
	require(pcx_read_bitmap("moon01.pcx", &background, BM_LINEAR, palette) == PCX_ERROR_NONE, "read source briefing art for mission override");
	for (int y = 0; y < background.bm_h; ++y)
		for (int x = 0; x < background.bm_w; ++x)
			background.bm_data[y * background.bm_rowsize + x] = static_cast<ubyte>((x / 8 + y / 8) % 128);
	require(pcx_write_bitmap("moon01.pcx", &background, palette) == PCX_ERROR_NONE, "write mission briefing art override");
	gr_free_bitmap_data(&background);
	run("override", "owned.tex", 2, 1, false);
	require(PHYSFS_delete("moon01.pcx"), "remove mission override before reopening baseline");
	run("after-override", "owned.tex", 2, 1, false);
#ifdef DXX_BUILD_DESCENT_II
	test_briefing_failures();
	set_default_handler(briefing_trace_event);
#endif
	run("after-failures", "owned.tex", 2, 1, false);
	set_default_handler(nullptr);
	const std::string result = Briefing_trace.frames.dump(2) + "\n";
	write_fixture("briefing.json", bytes(result.begin(), result.end()));
	std::puts("Native briefing render/window trace passed");
}

static nlohmann::json exercise_robot_pairs(bool native)
{
	auto result = nlohmann::json::array();
	const robot_info saved[] = { Robot_info[0], Robot_info[1] };
	for (const int first_attack : { 0, 1 })
		for (const int second_attack : { 0, 1 })
			for (const int direction : { -1, 1 })
				for (const int offset : { 0, 3 }) {
					init_test_corridor();
					ConsoleObject->pos = { 8 * F1_0, 8 * F1_0, 0 };
					ConsoleObject->size = F1_0 / 4;
					Robot_info[0].attack_type = first_attack;
					Robot_info[1].attack_type = second_attack;
					vms_vector start = { 0, 0, -6 * direction * F1_0 }, end = { 0, 0, 8 * direction * F1_0 };
					vms_vector point = { offset * F1_0, 0, 5 * direction * F1_0 };
					const int first = obj_create(OBJ_ROBOT, 0, 0, &start, &vmd_identity_matrix, F1_0, CT_AI, MT_NONE, RT_NONE);
					const int second = obj_create(OBJ_ROBOT, 1, 0, &point, &vmd_identity_matrix, F1_0, CT_AI, MT_NONE, RT_NONE);
					require(first > 0 && second > 0, "create actual robot-pair intersection actors");
					fvi_query query = {};
					query.p0 = &start;
					query.p1 = &end;
					query.rad = F1_0;
					query.thisobjnum = first;
					query.flags = FQ_CHECK_OBJS;
					fvi_info hit = {};
					const auto sim = d_rand_get_call_count();
					const int fate = find_vector_intersection(&query, &hit);
					require(fate == (native && first_attack && second_attack && !offset ? HIT_OBJECT : HIT_NONE), "only two native melee robots collide; engine robots remain passable");
					result.push_back({ first_attack, second_attack, direction, offset, fate, hit.hit_pnt.x, hit.hit_pnt.y, hit.hit_pnt.z,
					                   fate == HIT_OBJECT ? hit.hit_object : -1, d_rand_get_call_count() - sim });
#ifdef DXX_BUILD_DESCENT_II
					if (native) {
						Robot_info[1].companion = 1;
						require(find_vector_intersection(&query, &hit) == HIT_NONE, "an optional companion keeps D2 robot-pair eligibility");
						Robot_info[1].companion = 0;
					}
#endif
				}
	Robot_info[0] = saved[0];
	Robot_info[1] = saved[1];
	return result;
}

static nlohmann::json exercise_resource_drops(bool native)
{
	auto result = nlohmann::json::array();
	Game_mode = 0;
	Player_num = 0;
	for (const auto type : { OBJ_PLAYER, OBJ_ROBOT })
		for (const int id : { POW_ENERGY, POW_SHIELD_BOOST })
			for (const int inventory : { 50, 100, 150 })
				for (const unsigned seed : { 1u, 7u, 1000u }) {
					init_test_corridor();
					Players[0].objnum = 0;
					Players[0].energy = Players[0].shields = inventory * F1_0;
					vms_vector point = {};
					const int source = type == OBJ_PLAYER ? 0 : obj_create(type, 0, 0, &point, &vmd_identity_matrix, F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
					require(source >= 0, "create real resource-drop source");
					object &container = Objects[source];
					container.contains_type = OBJ_POWERUP;
					container.contains_id = id;
					container.contains_count = 2;
					container.mtype.phys_info.velocity = { F1_0, 2 * F1_0, 3 * F1_0 };
					d_srand(seed);
					const bool suppress = !native && type != OBJ_PLAYER && inventory >= 100 && d_rand() > 16384;
					d_srand(seed);
					d_rand_reset_call_count();
					const int created = object_create_egg(&container);
					require((created == -1) == suppress, "native resource eggs always spawn while ordinary D2 retains its inventory roll");
					auto objects = nlohmann::json::array();
					for (int i = 1; i <= Highest_object_index; ++i)
						if (Objects[i].type == OBJ_POWERUP) {
							const object &o = Objects[i];
							objects.push_back({ o.id, o.size, o.lifeleft, o.pos.x, o.pos.y, o.pos.z,
							                    o.mtype.phys_info.velocity.x, o.mtype.phys_info.velocity.y, o.mtype.phys_info.velocity.z,
							                    o.mtype.phys_info.mass, o.mtype.phys_info.drag, o.mtype.phys_info.flags });
						}
					require(objects.size() == (suppress ? 0 : 2), "resource operation creates the entire source count");
					result.push_back({ { "source", type }, { "id", id }, { "inventory", inventory }, { "seed", seed }, { "objects", objects }, { "rng", d_rand_get_call_count() } });
				}
	return result;
}

static nlohmann::json exercise_weapon_drops(bool native)
{
	auto result = nlohmann::json::array();
	std::vector<int> ids = { POW_VULCAN_WEAPON };
#ifdef DXX_BUILD_DESCENT_II
	if (!native) {
		ids.push_back(POW_GAUSS_WEAPON);
		ids.push_back(POW_OMEGA_WEAPON);
	}
#endif
	Game_mode = 0;
	Player_num = 0;
	for (const auto type : { OBJ_PLAYER, OBJ_ROBOT })
		for (const int id : ids)
			for (const int count : { 1, 2 }) {
				init_test_corridor();
				vms_vector point = {};
				const int source = type == OBJ_PLAYER ? 0 : obj_create(type, 0, 0, &point, &vmd_identity_matrix, F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
				require(source >= 0, "create weapon-drop source");
				object &container = Objects[source];
				container.contains_type = OBJ_POWERUP;
				container.contains_id = id;
				container.contains_count = count;
				d_srand(123);
				d_srand_stream(D_RNG_FX, 456);
				d_rand_reset_call_count();
				d_rand_reset_stream_call_count(D_RNG_FX);
				const int last = object_create_egg(&container);
				require(last > 0, "real weapon egg creates a pickup");
				auto objects = nlohmann::json::array();
				for (int i = 1; i <= Highest_object_index; ++i) {
					object &pickup = Objects[i];
					if (pickup.type != OBJ_POWERUP) continue;
					int expected = !native && type == OBJ_ROBOT && i == last ? VULCAN_WEAPON_AMMO_AMOUNT : 1;
#ifdef DXX_BUILD_DESCENT_II
					if (id == POW_OMEGA_WEAPON && type == OBJ_ROBOT && i == last) expected = MAX_OMEGA_CHARGE;
#endif
					require(pickup.ctype.powerup_info.count == expected, "D1 eggs keep their original count; ordinary D2 retains weapon-specific contents");
					if (id == POW_VULCAN_WEAPON) {
						Players[0].objnum = 0;
						Players[0].primary_weapon_flags = 1;
						Players[0].primary_weapon = 0;
						Players[0].primary_ammo[VULCAN_INDEX] = 100;
						const int used = do_powerup(&pickup);
						const int gain = native ? VULCAN_WEAPON_AMMO_AMOUNT : expected;
						require(used && Players[0].primary_ammo[VULCAN_INDEX] == 100 + gain, "collect actual dropped Vulcan with original first-acquisition minimum");
						require(pickup.ctype.powerup_info.count == expected - gain, "retain exact collected pickup contents until retirement");
					}
					objects.push_back({ pickup.id, expected, pickup.ctype.powerup_info.count });
				}
				require(objects.size() == static_cast<size_t>(count), "weapon egg preserves the full drop count");
				result.push_back({ type, id, count, objects, d_rand_get_call_count(), d_rand_get_stream_call_count(D_RNG_FX) });
			}
	return result;
}

static nlohmann::json exercise_robot_drops(bool native)
{
	auto result = nlohmann::json::array();
	Game_mode = 0;
	Player_num = 0;
	for (const int id : { 0, 14, 20 })
		for (const int count : { 0, 1, 3 })
			for (const int seed : { 1, 123, 456 }) {
				init_test_corridor();
				Players[0].num_robots_level = Players[0].num_robots_total = 0;
				vms_vector point = { F1_0, 0, 0 }, velocity = { 3 * F1_0, -2 * F1_0, F1_0 };
				d_srand(seed);
				d_srand_stream(D_RNG_FX, 789);
				d_rand_reset_call_count();
				d_rand_reset_stream_call_count(D_RNG_FX);
				const int last = drop_powerup(OBJ_ROBOT, id, count, &velocity, &point, 0);
				require(count ? last > 0 : last == (native ? 0 : -1), "robot egg return value preserves each engine's empty-drop contract");
				auto objects = nlohmann::json::array();
				int powerups = 0;
				for (int i = 1; i <= Highest_object_index; ++i) {
					const object &obj = Objects[i];
					if (obj.type == OBJ_POWERUP) { ++powerups; continue; }
					if (obj.type != OBJ_ROBOT) continue;
#ifdef DXX_BUILD_DESCENT_II
					if (!native) require(obj.size == Polygon_models[Robot_info[id].model_num].rad, "ordinary D2 robot egg uses its own model radius");
#else
					require(obj.size == Polygon_models[Robot_info[ObjId[OBJ_ROBOT]].model_num].rad, "native robot egg uses the original object-table radius");
#endif
					objects.push_back({ obj.id, obj.signature, obj.size, obj.shields, obj.rtype.pobj_info.model_num,
						obj.pos.x, obj.pos.y, obj.pos.z, obj.mtype.phys_info.velocity.x,
						obj.mtype.phys_info.velocity.y, obj.mtype.phys_info.velocity.z,
						obj.mtype.phys_info.mass, obj.mtype.phys_info.drag, obj.mtype.phys_info.flags,
						obj.ctype.ai_info.behavior, obj.ctype.ai_info.CURRENT_STATE,
						obj.ctype.ai_info.GOAL_STATE, obj.ctype.ai_info.REMOTE_OWNER,
						Ai_local_info[i].player_awareness_type, Ai_local_info[i].player_awareness_time });
				}
				require(objects.size() == static_cast<size_t>(count), "robot egg count is exact");
				require(Players[0].num_robots_level == count && Players[0].num_robots_total == count, "robot eggs update both player counts");
				require(d_rand_get_stream_call_count(D_RNG_FX) == 0, "robot eggs do not consume cosmetic RNG");
				if (native) {
					require(powerups == 0, "native robot eggs never add a bonus shield");
					require(d_rand_get_call_count() == static_cast<unsigned>(3 * count), "native robot eggs consume only trajectory draws");
				} else {
					require(powerups <= 1 && d_rand_get_call_count() == static_cast<unsigned>(3 * count + 1 + 4 * powerups), "ordinary D2 keeps its shield chance, trajectory and lifetime draws");
				}
				result.push_back({ id, count, seed, last, objects, powerups, d_rand_get_call_count() });
			}
	return result;
}

static nlohmann::json exercise_secondary_explosions(bool native)
{
	auto result = nlohmann::json::array();
	Game_mode = 0;
	Player_num = 0;
	Difficulty_level = 2;
	for (int id = 0; id < N_robot_types; ++id) {
#ifdef DXX_BUILD_DESCENT_II
		if (native) require(Robot_info[id].badass == 0, "published native robots have no D2 death-blast metadata");
		if (Robot_info[id].companion || Robot_info[id].thief) continue;
		const int blast = Robot_info[id].badass;
#else
		const int blast = 0;
		(void) native;
#endif
		init_test_corridor();
		Players[0].objnum = 0;
		Players[0].flags = 0;
		Players[0].shields = ConsoleObject->shields = 100 * F1_0;
		ConsoleObject->pos = { 0, 0, 2 * F1_0 };
		vms_vector point = {};
		const int index = obj_create(OBJ_ROBOT, id, 0, &point, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
		require(index > 0, "create actual dying source robot");
		Objects[index].mtype.phys_info.mass = F1_0;
		const int contains = Robot_info[id].contains_count;
		Robot_info[id].contains_count = 0; // Drops have their own complete operation comparison
		d_srand(23);
		d_rand_reset_call_count();
		explode_object(&Objects[index], F1_0 / 4);
		const int placeholder = Highest_object_index;
		require(Objects[placeholder].control_type == CT_EXPLOSION && Objects[placeholder].ctype.expl_info.delete_objnum == index, "delayed death creates the real explosion controller");
		Objects[placeholder].lifeleft = 0;
		do_explosion_sequence(&Objects[placeholder]);
		Robot_info[id].contains_count = contains;
		const fix expected_damage = blast ? blast * F1_0 - F1_0 / 2 : 0;
		require(Players[0].shields == 100 * F1_0 - expected_damage, "native secondary explosions are visual; D2 explosive robots retain radial damage");
		const object &explosion = Objects[Highest_object_index];
		require(explosion.type == OBJ_FIREBALL && explosion.ctype.expl_info.delete_objnum == index, "secondary explosion retains the source deletion lifecycle");
		result.push_back({ id, Players[0].shields, Objects[index].flags, explosion.id, explosion.size, explosion.lifeleft,
		                   explosion.ctype.expl_info.delete_time, d_rand_get_call_count() });
	}
	return result;
}

static nlohmann::json exercise_robot_blasts(bool native)
{
	auto result = nlohmann::json::array();
	Game_mode = 0;
	Player_num = 0;
	Current_level_num = 1;
	Difficulty_level = 2;
	FrameTime = F1_0 / 8;
#ifdef DXX_BUILD_DESCENT_II
	const int old_flash = Weapon_info[0].flash;
	Weapon_info[0].flash = 1; // Exercise native rejection even with an optional flash source
#endif
	for (int id = 0; id < N_robot_types; ++id)
		for (const int distance : { 2, 8 })
			for (const int flash : { 0, 1 }) {
				init_test_corridor();
				Players[0].objnum = 0;
				Players[0].flags = PLAYER_FLAGS_INVULNERABLE;
				Players[0].shields = ConsoleObject->shields = 100 * F1_0;
				ConsoleObject->pos = { 8 * F1_0, 0, 0 };
				vms_vector point = { 0, 0, distance * F1_0 };
				const int index = obj_create(OBJ_ROBOT, id, 0, &point, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
				require(index > 0, "create real source-defined blast target");
				object &robot = Objects[index];
				robot.shields = 100 * F1_0;
				robot.mtype.phys_info.mass = F1_0;
				Ai_local_info[index] = {};
				point = {};
				const int source = obj_create(OBJ_WEAPON, 0, 0, &point, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_NONE, RT_NONE);
				require(source > 0, "create actual optional flash source");
				Objects[source].ctype.laser_info.parent_type = OBJ_PLAYER;
				Objects[source].ctype.laser_info.parent_num = 0;
				d_srand(17);
				d_srand_stream(D_RNG_FX, 18);
				d_rand_reset_call_count();
				d_rand_reset_stream_call_count(D_RNG_FX);
				require(object_create_badass_explosion(flash ? &Objects[source] : nullptr, 0, &point, F1_0, VCLIP_SMALL_EXPLOSION,
				                                       20 * F1_0, 10 * F1_0, 0, 0) != nullptr,
				        "execute real robot blast operation");
				const int boss = Robot_info[id].boss_flag;
				const bool resistant = !native && (boss == 25 || boss == 26 || boss == 27);
				const fix expected = (20 - 2 * distance) * F1_0 / (resistant ? 4 : 1);
				require(100 * F1_0 - robot.shields == expected, "D1 bosses take native blast damage; D2 matter-resistant bosses keep quarter damage");
				require(robot.ctype.ai_info.SKIP_AI_COUNT == (!native && flash && !boss ? 2 : 0), "native robots reject flash stun while D2 enemies and companions retain it");
				result.push_back({ id, distance, flash, robot.shields, robot.ctype.ai_info.SKIP_AI_COUNT,
				                   robot.mtype.phys_info.rotthrust.x, robot.mtype.phys_info.rotthrust.y, robot.mtype.phys_info.rotthrust.z,
				                   d_rand_get_call_count(), d_rand_get_stream_call_count(D_RNG_FX) });
			}
#ifdef DXX_BUILD_DESCENT_II
	Weapon_info[0].flash = old_flash;
#endif
	return result;
}

static nlohmann::json exercise_contact_motion(bool native)
{
	using nlohmann::json;
	json motion = json::array(), contacts = json::array();
	Game_mode = 0;
	Player_num = 0;
	N_players = 1;
	Player_is_dead = Player_exploded = 0;
	Difficulty_level = 2;
	FrameTime = F1_0 / 8;
	collide_init();
	const tmap_info original_texture = TmapInfo[1];
	TmapInfo[1].flags = 0;
	TmapInfo[1].damage = 0;
	for (const auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_WEAPON, OBJ_DEBRIS })
		for (const int bounce : { 0, 1 })
			for (const int direction : { -1, 1 })
				for (const int speed : { 32, 128 }) {
					init_test_corridor(4);
					std::memset(&cheats, 0, sizeof(cheats));
					Players[0].objnum = 0;
					Players[0].shields = 100 * F1_0;
					Players[0].flags = PLAYER_FLAGS_INVULNERABLE;
					vms_vector point = { direction * 8 * F1_0, 0, 20 * F1_0 };
					const int id = type == OBJ_WEAPON ? FLARE_ID : 0;
					const int index = type == OBJ_PLAYER ? 0 : obj_create(type, id, 1, &point, &vmd_identity_matrix, F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
					require(index >= 0, "create a real motion actor");
					object &actor = Objects[index];
					actor.pos = actor.last_pos = point;
					obj_relink(index, 1);
					actor.orient = vmd_identity_matrix;
					actor.movement_type = MT_PHYSICS;
					actor.size = F1_0;
					actor.shields = 100 * F1_0;
					actor.mtype.phys_info = {};
					actor.mtype.phys_info.mass = F1_0;
					actor.mtype.phys_info.velocity = { direction * speed * F1_0, 4 * F1_0, 0 };
					actor.mtype.phys_info.flags = bounce ? PF_BOUNCE : 0;
					if (type == OBJ_WEAPON) {
						actor.ctype.laser_info.parent_type = OBJ_PLAYER;
						actor.ctype.laser_info.parent_num = 0;
						actor.ctype.laser_info.parent_signature = ConsoleObject->signature;
					}
					d_srand(123);
					d_srand_stream(D_RNG_FX, 456);
					d_rand_reset_call_count();
					d_rand_reset_stream_call_count(D_RNG_FX);
					json frames = json::array();
					for (int frame = 0; frame < 12; ++frame) {
						GameTime64 = (20 + frame) * F1_0;
						actor.last_pos = actor.pos;
						do_physics_sim(&actor);
						const auto &v = actor.mtype.phys_info.velocity;
						const auto &o = actor.orient;
						frames.push_back({ { "pos", { actor.pos.x, actor.pos.y, actor.pos.z } }, { "velocity", { v.x, v.y, v.z } }, { "orientation", { o.rvec.x, o.rvec.y, o.rvec.z, o.uvec.x, o.uvec.y, o.uvec.z, o.fvec.x, o.fvec.y, o.fvec.z } }, { "segment", actor.segnum }, { "flags", actor.flags }, { "physics_flags", actor.mtype.phys_info.flags }, { "sim", d_rand_get_call_count() }, { "fx", d_rand_get_stream_call_count(D_RNG_FX) } });
						if (actor.flags & OF_SHOULD_BE_DEAD) break;
					}
					if (type == OBJ_WEAPON && bounce) {
						const bool unchanged = !std::memcmp(&actor.orient, &vmd_identity_matrix, sizeof(actor.orient));
						require(unchanged == native, "actual projectile bounce retains native orientation and reorients D2");
					}
					motion.push_back({ { "type", type }, { "bounce", bounce }, { "direction", direction }, { "speed", speed }, { "frames", frames } });
				}
	for (const int exploding : { 0, 1 })
		for (const int invulnerable : { 0, 1 })
			for (const int speed : { 4, 32 }) {
				init_test_corridor();
				Players[0].objnum = 0;
				Players[0].flags = invulnerable ? PLAYER_FLAGS_INVULNERABLE : 0;
				Players[0].shields = ConsoleObject->shields = 100 * F1_0;
				ConsoleObject->size = F1_0;
				ConsoleObject->mtype.phys_info = {};
				ConsoleObject->mtype.phys_info.mass = F1_0;
				ConsoleObject->mtype.phys_info.velocity.z = speed * F1_0;
				vms_vector point = { 0, 0, 2 * F1_0 };
				const int index = obj_create(OBJ_ROBOT, 0, 0, &point, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
				require(index > 0, "create actual contact robot");
				object &robot = Objects[index];
				robot.flags = exploding ? OF_EXPLODING : 0;
				robot.shields = 100 * F1_0;
				robot.mtype.phys_info.mass = F1_0;
				robot.ctype.ai_info.behavior = AIB_NORMAL;
				Ai_local_info[index] = {};
				Num_awareness_events = 0;
				GameTime64 += F1_0;
				d_srand(123);
				d_srand_stream(D_RNG_FX, 456);
				d_rand_reset_call_count();
				d_rand_reset_stream_call_count(D_RNG_FX);
				point.z = F1_0;
				collide_robot_and_player(&robot, ConsoleObject, &point);
				const bool allowed = native || !exploding;
				require(Num_awareness_events == (allowed ? 1 : 0), "real native contact admits exploding robots while D2 rejects them");
				contacts.push_back({ exploding, invulnerable, speed, Players[0].shields, robot.shields, ConsoleObject->mtype.phys_info.velocity.z,
				                     robot.mtype.phys_info.velocity.z, Num_awareness_events, d_rand_get_call_count(), d_rand_get_stream_call_count(D_RNG_FX) });
			}
	TmapInfo[1] = original_texture;
	return { { "motion", motion }, { "contacts", contacts } };
}

// Compare complete shared-engine operations, including wall state and RNG, in
// registered native/imported content. D2 also exercises its own installed bank.
static nlohmann::json exercise_volatile_impacts(bool native)
{
	auto result = nlohmann::json::array();
	const tmap_info original = TmapInfo[1];
	TmapInfo[1].flags = TMI_VOLATILE;
	TmapInfo[1].eclip_num = -1;
	Game_mode = 0;
	Player_num = 0;
	collide_init();
	for (const int id : { LASER_ID_L1, CONCUSSION_ID, MEGA_ID })
		for (int difficulty = 0; difficulty < NDL; ++difficulty)
			for (const int exploding : { 0, 1 }) {
				init_test_corridor();
				Difficulty_level = difficulty;
				Players[0].objnum = 0;
				Players[0].flags = PLAYER_FLAGS_INVULNERABLE;
				Players[0].shields = ConsoleObject->shields = 100 * F1_0;
				ConsoleObject->mtype.phys_info.mass = F1_0;
				vms_vector point = { 0, 0, -5 * F1_0 };
				const int target = obj_create(OBJ_ROBOT, 0, 0, &point, &vmd_identity_matrix, F1_0, CT_NONE, MT_PHYSICS, RT_NONE);
				require(target > 0, "create lava blast target");
				object &robot = Objects[target];
				robot.flags = exploding ? OF_EXPLODING : 0;
				robot.shields = exploding ? -F1_0 : 1000 * F1_0;
				robot.mtype.phys_info.mass = F1_0;
				point.z = -9 * F1_0;
				const int source = obj_create(OBJ_WEAPON, id, 0, &point, &vmd_identity_matrix, F1_0 / 4, CT_WEAPON, MT_NONE, RT_NONE);
				require(source > 0, "create lava impact weapon");
				object &weapon = Objects[source];
				weapon.ctype.laser_info.parent_type = OBJ_PLAYER;
				weapon.ctype.laser_info.parent_num = 0;
				weapon.ctype.laser_info.parent_signature = ConsoleObject->signature;
				weapon.ctype.laser_info.multiplier = F1_0;
				weapon.shields = Weapon_info[id].strength[difficulty];
				weapon.mtype.phys_info.velocity.z = -30 * F1_0;
				d_srand(123);
				d_srand_stream(D_RNG_FX, 456);
				d_rand_reset_call_count();
				d_rand_reset_stream_call_count(D_RNG_FX);
				point.z = -10 * F1_0;
				collide_object_with_wall(&weapon, F1_0, 0, 5, &point);
				const object &explosion = Objects[source + 1];
				require(explosion.type == OBJ_FIREBALL && (weapon.flags & OF_SHOULD_BE_DEAD), "real lava collision creates an explosion and retires the weapon");
				const bool boosted = native || Weapon_info[id].damage_radius < 15 * F1_0;
				require(explosion.size == Weapon_info[id].impact_size + (boosted ? 3 * F1_0 : 0), "native lava keeps its original impact size even for powerful weapons");
				result.push_back({ id, difficulty, exploding, explosion.id, explosion.size, explosion.lifeleft,
				                   explosion.pos.x, explosion.pos.y, explosion.pos.z, robot.shields,
				                   robot.mtype.phys_info.velocity.x, robot.mtype.phys_info.velocity.y, robot.mtype.phys_info.velocity.z,
				                   robot.mtype.phys_info.rotvel.x, robot.mtype.phys_info.rotvel.y, robot.mtype.phys_info.rotvel.z,
				                   d_rand_get_call_count(), d_rand_get_stream_call_count(D_RNG_FX) });
			}
	TmapInfo[1] = original;
	return result;
}

static nlohmann::json exercise_reactor_frames(bool native)
{
	using nlohmann::json;
	json result = json::array();
	Game_mode = 0;
	Player_num = 0;
	Game_suspended = 0;
	cheats.robotfiringsuspended = 0;
	for (const int level : { 1, 14, -1 })
		for (int difficulty = 0; difficulty < NDL; ++difficulty)
			for (const int seed : { 1, 7, 23, 97, 123, 456, 789, 12345 })
				for (int scenario = 0; scenario < 10; ++scenario) {
					init_test_corridor(6);
					for (int vertex = 0; vertex < Num_vertices; ++vertex)
						vm_vec_scale(&Vertices[vertex], 4 * F1_0);
					validate_segment_all();
					Players[0].objnum = 0;
					Players[0].flags = scenario == 1 ? PLAYER_FLAGS_CLOAKED : 0;
					ConsoleObject = Viewer = &Objects[0];
					ConsoleObject->pos = { 0, 0, (scenario == 2 ? 340 : 80) * F1_0 };
					const int player_segment = find_point_seg(&ConsoleObject->pos, 0);
					require(player_segment >= 0, "place reactor target in the test mine");
					obj_relink(0, player_segment);
					vms_vector center = {};
					const int slot = obj_create(OBJ_CNTRLCEN, 0, 0, &center, &vmd_identity_matrix, F1_0, CT_CNTRLCEN, MT_NONE, RT_POLYOBJ);
					require(slot > 0, "create reactor using loaded game definitions");
					object &reactor_object = Objects[slot];
					Current_level_num = level;
					Difficulty_level = difficulty;
#ifdef DXX_BUILD_DESCENT_II
					Reactor_strength = -1;
#endif
					init_controlcen_for_level();
					require(reactor_object.shields == 200 * F1_0 + level * F1_0 * (level >= 0 ? 50 : native ? -100 : -150),
					        "reactor initialization retains original normal and secret-level health");
					if (scenario == 8) {
						Segments[0].children[4] = -1;
						Segments[1].children[5] = -1;
					}
					if (scenario == 9) {
						for (auto &child : Segments[0].children) child = -1;
						Segments[0].children[4] = -2; // Original D1 treats an exit as a non-isolated side
					}
					FrameTime = F1_0 / 64;
					GameTime64 = 10 * F1_0;
					d_tick_count = scenario == 4 ? 1 : 8;
					Control_center_present = scenario != 7;
					Control_center_been_hit = scenario < 3 || scenario == 5 || scenario == 6 || scenario == 8;
					Control_center_player_been_seen = 0;
					Control_center_next_fire_time = scenario == 5 ? 0 : -1;
					Player_is_dead = scenario == 6;
					controlcen_death_silence = scenario == 6 ? 2 * F1_0 : 0;
					Believed_player_pos = scenario == 1 ? vms_vector{ F1_0, 0, 40 * F1_0 } : ConsoleObject->pos;
#ifdef DXX_BUILD_DESCENT_II
					Last_time_cc_vis_check = 0;
#endif
					d_srand(seed);
					d_srand_stream(D_RNG_FX, seed + 1);
					d_rand_reset_call_count();
					d_rand_reset_stream_call_count(D_RNG_FX);
					do_controlcen_frame(&reactor_object);
					json shots = json::array();
					for (int i = 1; i <= Highest_object_index; ++i) {
						const object &shot = Objects[i];
						if (shot.type != OBJ_WEAPON) continue;
						require(shot.id == CONTROLCEN_WEAPON_NUM && shot.ctype.laser_info.parent_num == slot, "reactor creates actual owned projectiles");
						shots.push_back({ shot.id, shot.segnum, shot.pos.x, shot.pos.y, shot.pos.z,
						                  shot.orient.fvec.x, shot.orient.fvec.y, shot.orient.fvec.z,
						                  shot.mtype.phys_info.velocity.x, shot.mtype.phys_info.velocity.y, shot.mtype.phys_info.velocity.z,
						                  shot.lifeleft, shot.shields, shot.ctype.laser_info.parent_type });
					}
					if (scenario < 2 || scenario == 8) {
						require(!shots.empty(), "ready reactor fires at the visible or believed target");
						require(shots.size() <= static_cast<size_t>(native ? 2 : 5), "reactor respects each game's burst limit");
						require(Control_center_next_fire_time == (NDL - difficulty) * F1_0 / 4 + (!native && difficulty == 0 ? F1_0 / 2 : 0), "reactor uses its original difficulty fire delay");
					} else
						require(shots.empty(), "reactor acquisition, distance, cooldown and inactive phases do not fire early");
					if (scenario == 8)
						require(Control_center_been_hit == static_cast<int>(native), "only D2 periodically clears an active reactor's unseen target");
					unsigned sim_state = 0, fx_state = 0;
					const int sim_available = d_rand_get_stream_state(D_RNG_SIM, &sim_state);
					const int fx_available = d_rand_get_stream_state(D_RNG_FX, &fx_state);
					result.push_back({ { "level", level }, { "difficulty", difficulty }, { "seed", seed }, { "scenario", scenario },
					                   { "shots", shots }, { "hit", Control_center_been_hit }, { "seen", Control_center_player_been_seen },
					                   { "shields", reactor_object.shields }, { "sim_state", { sim_available, sim_state } }, { "fx_state", { fx_available, fx_state } },
					                   { "next_fire", Control_center_next_fire_time }, { "death_silence", controlcen_death_silence },
					                   { "sim_draws", d_rand_get_call_count() }, { "fx_draws", d_rand_get_stream_call_count(D_RNG_FX) } });
				}
	Player_is_dead = 0;
	return result;
}

static nlohmann::json exercise_gameplay_rules(bool native)
{
	using nlohmann::json;
	InitWeaponOrdering();
	Game_mode = 0;
	Player_num = 0;
	Player_is_dead = 0;
	json doors = json::array(), pickups = json::array(), damage = json::array(), drops = json::array();
	json vulcan = json::array();
	json object_orientations = json::array();
	json powerup_animation = json::array();
	init_test_corridor();
	for (int index = 0; index < 4; ++index) {
		vms_vector point = {};
		const int slot = obj_create(OBJ_POWERUP, POW_ENERGY, 0, &point, nullptr, F1_0, CT_POWERUP, MT_NONE, RT_POWERUP);
		require(slot > 0, "create animated pickup");
		auto &animation = Objects[slot].rtype.vclip_info;
		animation.vclip_num = Powerup_info[POW_ENERGY].vclip_num;
		const auto &clip = Vclip[animation.vclip_num];
		require(clip.num_frames > 1 && clip.frame_time > 0, "pickup has original animation frames");
		animation.frametime = clip.frame_time;
		animation.framenum = static_cast<sbyte>(clip.num_frames - 1);
		const auto sim_calls = d_rand_get_call_count(), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
		int step = 0;
		for (const fix elapsed : { 0, clip.frame_time, 1, clip.frame_time * 3 + 1 }) {
			FrameTime = elapsed;
			do_powerup_frame(&Objects[slot]);
			if (step == 2)
				require(animation.framenum == (native || !(slot & 1) ? 0 : clip.num_frames - 2), "native pickups advance forward while ordinary D2 keeps alternating direction");
			powerup_animation.push_back({ slot, step++, elapsed, animation.frametime, animation.framenum });
		}
		require(sim_calls == d_rand_get_call_count() && fx_calls == d_rand_get_stream_call_count(D_RNG_FX), "pickup animation consumes neither RNG stream");
	}
	for (const auto type : { OBJ_WEAPON, OBJ_FIREBALL, OBJ_POWERUP, OBJ_ROBOT }) {
		for (const bool supplied : { false, true }) {
			init_test_corridor();
			vms_vector point = {};
			vms_angvec angles = { 1000, 2000, 3000 };
			vms_matrix orientation;
			vm_angles_2_matrix(&orientation, &angles);
			const int index = obj_create(type, type == OBJ_WEAPON ? VULCAN_ID : 0, 0, &point,
				supplied ? &orientation : nullptr, F1_0, CT_NONE, MT_NONE, RT_NONE);
			require(index > 0, "create object with optional orientation");
			const vms_matrix zero = {};
			const vms_matrix &expected = supplied ? orientation : native ? zero : vmd_identity_matrix;
			const auto &actual = Objects[index].orient;
			require(!std::memcmp(&actual, &expected, sizeof(actual)), "object initialization preserves the source orientation default");
			object_orientations.push_back({ type, supplied, actual.rvec.x, actual.rvec.y, actual.rvec.z,
				actual.uvec.x, actual.uvec.y, actual.uvec.z, actual.fvec.x, actual.fvec.y, actual.fvec.z });
		}
	}
	json small_fireballs = json::array();
	json reactor_fireballs = json::array();
	for (const unsigned seed : { 1u, 456u }) {
		init_test_corridor();
		FrameTime = F1_0;
		Control_center_destroyed = 0;
		Countdown_seconds_left = 10;
		vms_vector point = {};
		Dead_controlcen_object_num = obj_create(OBJ_CNTRLCEN, 0, 0, &point, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
		require(Dead_controlcen_object_num > 0, "create burnt reactor");
		d_srand_stream(D_RNG_FX, seed);
		d_rand_reset_stream_call_count(D_RNG_FX);
		const auto sim_calls = d_rand_get_call_count();
		do_controlcen_dead_frame();
		const int index = Objects[Dead_controlcen_object_num].attached_obj;
		require(index > 0 && Objects[index].type == OBJ_FIREBALL, "dead reactor phase creates an attached burn effect");
		const auto &fireball = Objects[index];
		require(fireball.size >= (native ? 3 * F1_0 : F1_0 / 2) && fireball.size < (native ? 9 * F1_0 : 3 * F1_0 / 2),
		        "reactor burn retains native source scale or ordinary D2 scale");
		require(d_rand_get_call_count() == sim_calls, "reactor burn consumes only FX RNG");
		reactor_fireballs.push_back({ seed, fireball.size, fireball.lifeleft, fireball.pos.x, fireball.pos.y, fireball.pos.z,
		                             d_rand_get_stream_call_count(D_RNG_FX) });
	}
	Dead_controlcen_object_num = -1;
	for (const auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_CNTRLCEN })
		for (const fix scale : { F1_0 / 2, F1_0, F1_0 * 3 })
			for (const unsigned seed : { 1u, 456u }) {
				init_test_corridor();
				ConsoleObject->type = type;
				ConsoleObject->id = 0;
				ConsoleObject->size = F1_0;
				d_srand_stream(D_RNG_FX, seed);
				d_rand_reset_stream_call_count(D_RNG_FX);
				const auto sim_calls = d_rand_get_call_count();
				create_small_fireball_on_object(ConsoleObject, scale, 0);
				const int index = ConsoleObject->attached_obj;
				require(index > 0 && Objects[index].type == OBJ_FIREBALL, "small fireball is created and attached to its source");
				const auto &fireball = Objects[index];
				require(d_rand_get_call_count() == sim_calls, "attached fireball consumes only the FX stream");
				require(fireball.size >= (native ? scale : scale / 2) && fireball.size < (native ? scale * 3 : scale * 3 / 2),
				        "native fireball size range retains D1 presentation while ordinary D2 stays smaller");
				small_fireballs.push_back({ type, scale, seed, fireball.size, fireball.lifeleft, fireball.flags,
				                           fireball.pos.x, fireball.pos.y, fireball.pos.z, fireball.rtype.vclip_info.vclip_num,
				                           d_rand_get_stream_call_count(D_RNG_FX) });
			}
	for (const bool owned : { false, true })
		for (const int contents : { 0, VULCAN_AMMO_AMOUNT, VULCAN_WEAPON_AMMO_AMOUNT }) {
			init_test_corridor();
			Players[0].objnum = 0;
			Players[0].flags = 0;
			Players[0].primary_weapon_flags = 1 | (owned ? 1 << VULCAN_INDEX : 0);
			Players[0].primary_weapon = 0;
			Players[0].primary_ammo[VULCAN_INDEX] = 100;
			Players[0].score = 0;
			Players[0].shields = 100 * F1_0;
			object pickup = {};
			pickup.type = OBJ_POWERUP;
			pickup.id = POW_VULCAN_WEAPON;
			pickup.ctype.powerup_info.count = contents;
			const int used = do_powerup(&pickup);
			const int gain = native ? (owned ? VULCAN_AMMO_AMOUNT : VULCAN_WEAPON_AMMO_AMOUNT) : contents;
			require(Players[0].primary_ammo[VULCAN_INDEX] == 100 + gain, "native Vulcan pickup grants its first-weapon minimum or one duplicate ammo box");
			vulcan.push_back({ owned, contents, used, Players[0].primary_ammo[VULCAN_INDEX], pickup.ctype.powerup_info.count, Players[0].score });
		}
	json surfaces = json::array();
	for (int i = 0; i < NumTextures; ++i) {
		const auto &texture = TmapInfo[i];
		if (native) require(!(texture.flags & ~TMI_VOLATILE), "published original surfaces have no D2-only flags");
		surfaces.push_back({ texture.flags, texture.damage, texture.lighting, texture.eclip_num });
	}
	const auto contact_motion = exercise_contact_motion(native);
	const auto robot_blasts = exercise_robot_blasts(native);
	const auto robot_pairs = exercise_robot_pairs(native);
	const auto resource_drops = exercise_resource_drops(native);
	const auto weapon_drops = exercise_weapon_drops(native);
	const auto robot_drops = exercise_robot_drops(native);
	const auto reactor_frames = exercise_reactor_frames(native);
	const auto secondary_explosions = exercise_secondary_explosions(native);
	const auto volatile_impacts = exercise_volatile_impacts(native);
	Game_mode = 0;
	Player_num = 0;
	N_players = 1;
	Player_is_dead = 0;
	Newdemo_state = ND_STATE_NORMAL;
	std::memset(&cheats, 0, sizeof(cheats));
	FrameTime = F1_0 / 16;
	GameTime64 = 10 * F1_0;
	for (const int parts : { 1, 2 })
		for (const int block_part : { 0, 1 })
			for (const int back : { 0, 1 })
				for (const auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_WEAPON, OBJ_FIREBALL })
					for (const fix radius : { 0, F1_0 }) {
						init_test_corridor(4);
						Num_walls = 4;
						std::memset(Walls, 0, 4 * sizeof(*Walls));
						std::memset(ActiveDoors, 0, 2 * sizeof(*ActiveDoors));
						WallAnims[0].num_frames = 8;
						WallAnims[0].play_time = F1_0;
						WallAnims[0].flags = WCF_TMAP1;
						WallAnims[0].open_sound = WallAnims[0].close_sound = -1;
						for (int i = 0; i < 8; ++i) WallAnims[0].frames[i] = i + 1;
						for (int i = 0; i < 4; ++i) {
							Walls[i].segnum = i;
							Walls[i].sidenum = i % 2 ? 5 : 4;
							Walls[i].linked_wall = parts == 2 ? (i + 2) % 4 : -1;
							Walls[i].type = WALL_DOOR;
							Walls[i].flags = WALL_DOOR_AUTO | WALL_DOOR_OPENED;
							Walls[i].state = WALL_DOOR_WAITING;
							Segments[i].sides[Walls[i].sidenum].wall_num = i;
						}
						Num_open_doors = 1;
						ActiveDoors[0].n_parts = parts;
						for (int i = 0; i < parts; ++i) {
							ActiveDoors[0].front_wallnum[i] = i * 2;
							ActiveDoors[0].back_wallnum[i] = i * 2 + 1;
						}
						ActiveDoors[0].time = DOOR_WAIT_TIME - FrameTime;
						vms_vector point = { 0, 0, (block_part * 40 + 10) * F1_0 };
						const int blocker = obj_create(type, 0, block_part * 2 + back, &point, &vmd_identity_matrix, radius, CT_NONE, MT_NONE, RT_NONE);
						require(blocker > 0, "create a real linked-door blocker");
						const bool blocked = radius && block_part == 0 && (native || (type != OBJ_WEAPON && type != OBJ_FIREBALL));
						wall_frame_process();
						require(Walls[0].state == WALL_DOOR_WAITING, "door waits through the exact timeout boundary");
						wall_frame_process();
						require(Walls[0].state == ((!native && blocked) ? WALL_DOOR_WAITING : WALL_DOOR_CLOSING), "native wait ends before obstruction testing; D2 waits for clearance");
						const int wait_state = Walls[0].state;
						for (int i = 0; i < 4; ++i) Walls[i].state = WALL_DOOR_CLOSING;
						ActiveDoors[0].time = 0;
						do_door_close(0);
						require(Walls[0].state == ((!native && blocked) ? WALL_DOOR_OPENING : WALL_DOOR_CLOSING), "obstructed native door pauses while D2 reopens");
						require(!native || !blocked || ActiveDoors[0].time == 0, "native obstruction does not advance animation time");
						json frame = { { "parts", parts }, { "block_part", block_part }, { "back", back }, { "type", type }, { "radius", radius }, { "wait", wait_state }, { "close", Walls[0].state }, { "time", ActiveDoors[0].time } };
						frame["walls"] = json::array();
						for (int i = 0; i < parts * 2; ++i)
							frame["walls"].push_back({ Walls[i].state, Walls[i].flags, Segments[i].sides[Walls[i].sidenum].tmap_num });
						Objects[blocker].size = 0;
						for (int i = 0; i < 4; ++i) Walls[i].state = WALL_DOOR_CLOSING;
						ActiveDoors[0].time = 0;
						for (int frame_num = 0; frame_num < 32 && Num_open_doors; ++frame_num) wall_frame_process();
						require(Num_open_doors == 0 && Walls[0].state == WALL_DOOR_CLOSED, "door finishes closing after its blocker leaves");
						for (int i = 0; i < parts * 2; ++i)
							require(Walls[i].state == WALL_DOOR_CLOSED && !(Walls[i].flags & WALL_DOOR_OPENED), "all linked front/back walls finish closed");
						doors.push_back(frame);
					}
	for (int difficulty = 0; difficulty < NDL; ++difficulty) {
		Difficulty_level = difficulty;
		for (const int id : { POW_ENERGY, POW_SHIELD_BOOST })
			for (const fix start : { 100 * F1_0, MAX_ENERGY - F1_0 / 2, MAX_ENERGY }) {
				init_test_corridor();
				Players[0].objnum = 0;
				Players[0].energy = Players[0].shields = start;
				Players[0].score = 0;
				Players[0].lives = 3;
				Players[0].flags = 0;
				object pickup = {};
				pickup.type = OBJ_POWERUP;
				pickup.id = id;
				const int used = do_powerup(&pickup);
				const fix result = id == POW_ENERGY ? Players[0].energy : Players[0].shields;
				const fix amount = (difficulty == 0 && !native ? 27 : 18 - 3 * difficulty) * F1_0;
				require(result == (std::min) (MAX_ENERGY, start + amount) && used == (start < MAX_ENERGY), "real pickup uses native or D2 difficulty amount and retains saturation/consumption");
				pickups.push_back({ difficulty, id, start, used, result, Players[0].score });
			}
		for (const int invulnerable : { 0, 1 })
			for (int kind = 0; kind < 3; ++kind) {
				init_test_corridor();
				Players[0].objnum = 0;
				Players[0].flags = invulnerable ? PLAYER_FLAGS_INVULNERABLE : 0;
				Players[0].shields = ConsoleObject->shields = 100 * F1_0;
				ConsoleObject->mtype.phys_info.mass = F1_0;
				vms_vector origin = {};
				const int other = obj_create(OBJ_CLUTTER, 0, 0, &origin, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
				require(other > 0, "create contact source");
				d_srand(13);
				if (kind == 0) apply_force_damage(ConsoleObject, 64 * F1_0 + 7, &Objects[other]);
				else if (kind == 1) {
					TmapInfo[1].damage = 20 * F1_0 + 7;
					TmapInfo[1].flags = 0;
					vms_vector point = { 0, 0, -10 * F1_0 };
					scrape_player_on_wall(ConsoleObject, 0, 5, &point);
				} else {
					vms_vector point = { 0, 0, 2 * F1_0 };
					require(object_create_badass_explosion(nullptr, 0, &point, F1_0, VCLIP_SMALL_EXPLOSION, 20 * F1_0, 10 * F1_0, 0, 0) != nullptr, "create actual area-damage explosion");
				}
				const fix delta = 100 * F1_0 - Players[0].shields;
				require(invulnerable ? delta == 0 : delta > 0, "real contact/lava/blast honors invulnerability");
				const auto &physics = ConsoleObject->mtype.phys_info;
				// Equal RNG counts do not prove equal order: lava shove and spin share the stream
				damage.push_back({ difficulty, invulnerable, kind, delta, d_rand_get_call_count(),
				                   { physics.velocity.x, physics.velocity.y, physics.velocity.z },
				                   { physics.rotvel.x, physics.rotvel.y, physics.rotvel.z } });
			}
	}
	for (const int id : { POW_CLOAK, POW_VULCAN_WEAPON, POW_SPREADFIRE_WEAPON, POW_PLASMA_WEAPON, POW_FUSION_WEAPON, POW_QUAD_FIRE, POW_VULCAN_AMMO, POW_ENERGY, POW_EXTRA_LIFE })
		for (int flags = 0; flags < 8; ++flags)
			for (int nearby = -1; nearby < 4; ++nearby)
				for (const unsigned seed : { 1u, 7u, 1000u }) {
					init_test_corridor(4);
					Players[0].objnum = 0;
					Players[0].primary_weapon_flags = flags & 1 ? 0x1f : 1;
					Players[0].flags = flags & 1 ? PLAYER_FLAGS_QUAD_LASERS : 0;
					Players[0].primary_ammo[VULCAN_INDEX] = flags & 2 ? VULCAN_AMMO_MAX : 0;
					Game_mode = flags & 4 ? GM_MULTI : 0;
					if (nearby >= 0) {
						vms_vector point = { 0, 0, nearby * 20 * F1_0 };
						require(obj_create(OBJ_POWERUP, id, nearby, &point, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE) > 0, "create actual nearby pickup");
					}
					for (const int gated : { 0, 1 }) {
						object container = {};
						container.type = OBJ_ROBOT;
						container.segnum = 0;
						container.matcen_creator = gated ? BOSS_GATE_MATCEN_NUM : 0;
						container.contains_type = OBJ_POWERUP;
						container.contains_id = id;
						container.contains_count = 3;
						d_srand(seed);
#ifdef DXX_BUILD_DESCENT_II
						if (!native) {
							const object original = container;
							const auto draws = d_rand_get_call_count();
							require(!d1_in_d2_replace_powerup(&container) && std::memcmp(&original, &container, sizeof(container)) == 0 && d_rand_get_call_count() == draws,
							        "inactive native drop operation leaves D2 object bytes and RNG intact");
						}
#endif
						maybe_replace_powerup_with_energy(&container);
						if (!native && (flags & 1) && !gated &&
						    (id == POW_SPREADFIRE_WEAPON || id == POW_PLASMA_WEAPON || id == POW_FUSION_WEAPON || id == POW_QUAD_FIRE))
							require(container.contains_count == 3 && (container.contains_id == POW_ENERGY || container.contains_id == POW_SHIELD_BOOST),
							        "ordinary D2 duplicate drops retain their count and energy/shield choice");
						drops.push_back({ id, flags, nearby, seed, gated, container.contains_type, container.contains_id, container.contains_count, d_rand_get_call_count() });
					}
				}
	Game_mode = 0;
	return { { "doors", doors }, { "pickups", pickups }, { "vulcan", vulcan }, { "powerup_animation", powerup_animation }, { "object_orientations", object_orientations }, { "small_fireballs", small_fireballs }, { "reactor_fireballs", reactor_fireballs }, { "volatile_impacts", volatile_impacts }, { "damage", damage }, { "drops", drops }, { "surfaces", surfaces }, { "contact_motion", contact_motion }, { "robot_blasts", robot_blasts }, { "robot_pairs", robot_pairs }, { "resource_drops", resource_drops }, { "weapon_drops", weapon_drops }, { "robot_drops", robot_drops }, { "reactor_frames", reactor_frames }, { "secondary_explosions", secondary_explosions } };
}

static void write_gameplay_rules_trace(const char *directory, const char *d2_directory)
{
	const std::string hog = std::string(directory) + "/DESCENT.HOG";
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount native gameplay resources");
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	GameArg.SysWindow = GameCfg.WindowMode = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	Game_screen_mode = SM(640, 480);
	digi_select_system(SDLAUDIO_SYSTEM);
#ifdef DXX_BUILD_DESCENT_II
	require(d1_in_d2_init_base_resources(1), "select native rules before asset initialization");
#endif
	load_text();
	require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0 && gr_init(Game_screen_mode) == 0, "initialize rule fixture renderer");
	gr_use_palette_table("palette.256");
	gamefont_init();
	gamedata_init();
#ifdef DXX_BUILD_DESCENT_II
	d1_in_d2_init_startup_bitmaps();
	char mission[] = "descent";
#else
	char mission[] = "";
	(void) d2_directory;
#endif
	require(load_mission_by_name(mission), "select native gameplay mission");
	const auto native = exercise_gameplay_rules(true);
	const std::string result = native.dump(2) + "\n";
	write_fixture("rules.json", bytes(result.begin(), result.end()));
#ifdef DXX_BUILD_DESCENT_II
	if (d2_directory) {
		const std::string d2_hog = std::string(d2_directory) + "/descent2.hog";
		require(PHYSFS_mount(d2_directory, nullptr, 0) && PHYSFS_mount(d2_hog.c_str(), nullptr, 0), "mount D2 for ordinary-rule regression");
		char d2_mission[] = "d2";
		require(load_mission_by_name(d2_mission) && !d1_in_d2_use_d1_gameplay(), "ordinary D2 rules use an actual installed D2 bank");
		const std::vector<segment2> previous_segments(Segment2s, Segment2s + Highest_segment_index + 1);
		require(!d1_in_d2_initialize_level_ambience() &&
		            std::memcmp(Segment2s, previous_segments.data(), previous_segments.size() * sizeof(segment2)) == 0,
		        "D1 ambience preparation leaves ordinary D2 segment data intact");
		const fix boss_health[] = { 500 * F1_0, 1250 * F1_0, 1500 * F1_0, 1750 * F1_0, 2000 * F1_0 };
		for (int difficulty = 0; difficulty < NDL; ++difficulty)
			require(boss_health_maximum_for_difficulty(2000 * F1_0, difficulty) == boss_health[difficulty],
			        "ordinary D2 retains difficulty-scaled boss health");
		const auto d2 = exercise_gameplay_rules(false);
		for (size_t i = 0; i < native["damage"].size(); ++i) {
			const auto &entry = native["damage"][i];
			const int divisor = entry[0] == 0 ? (entry[2] == 2 ? 4 : 2) : 1;
			require(d2["damage"][i][3] == entry[3].get<int>() / divisor, "ordinary D2 retains its contact and explosion trainee reductions");
		}
		const std::string d2_result = d2.dump(2) + "\n";
		write_fixture("rules-d2.json", bytes(d2_result.begin(), d2_result.end()));
	}
#endif
	std::puts("Gameplay rules trace passed");
}

// Compare CPU collection against the actual draw, then export native/imported lists
static void write_render_candidates_trace(const char *directory)
{
	using nlohmann::json;
	const std::string hog = std::string(directory) + "/DESCENT.HOG";
	require(PHYSFS_mount(directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount original candidate-test resources");
	GameArg.SndNoSound = GameArg.SndNoMusic = 1;
	GameArg.SysWindow = GameCfg.WindowMode = 1;
	GameCfg.AspectX = 4;
	GameCfg.AspectY = 3;
	GameCfg.TexFilt = 0;
	Game_screen_mode = SM(640, 480);
	digi_select_system(SDLAUDIO_SYSTEM);
#ifdef DXX_BUILD_DESCENT_II
	require(d1_in_d2_init_base_resources(1), "select D1 candidate-test resources");
#endif
	load_text();
	require(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0 && gr_init(Game_screen_mode) == 0, "initialize candidate-test renderer");
	gr_use_palette_table("palette.256");
	gamefont_init();
	gamedata_init();
#ifdef DXX_BUILD_DESCENT_II
	d1_in_d2_init_startup_bitmaps();
	char mission[] = "descent";
#else
	char mission[] = "";
#endif
	texmerge_init(10);
	init_game();
	require(load_mission_by_name(mission), "load candidate-test mission");
	Player_num = 0;
	N_players = 1;
	std::strcpy(Players[0].callsign, "viewtest");
	Difficulty_level = 2;
	init_player_stats_game(0);
	json trace = json::array();
	auto check_view = [&](int scene, int pose, int width, int height, int rear, int classic, fix offset) {
		std::fprintf(stderr, "Candidate draw scene=%d pose=%d viewport=%dx%d rear=%d classic=%d offset=%d\n", scene, pose, width, height, rear, classic, offset);
		grs_canvas canvas;
		gr_init_sub_canvas(&canvas, &grd_curscreen->sc_canvas, 0, 0, width, height);
		gr_set_current_canvas(&canvas);
		Rear_view = rear;
		GameCfg.ClassicDepth = classic;
		Endlevel_sequence = 0;
		Player_fired_laser_this_frame = -1;
#ifdef DXX_BUILD_DESCENT_II
		update_rendered_data(0, Viewer, rear, 0);
		render_frame(offset, 0);
		const std::vector<short> drawn(Window_rendered_data[0].rendered_objects,
		    Window_rendered_data[0].rendered_objects + Window_rendered_data[0].num_objects);
#else
		render_frame(offset);
		const std::vector<short> drawn(Ordered_rendered_object_list, Ordered_rendered_object_list + Num_rendered_objects);
#endif
		const std::vector<object> before(Objects, Objects + Highest_object_index + 1);
		const auto sim_calls = d_rand_get_call_count(), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
		unsigned sim_state = 0, fx_state = 0;
		const int has_sim = d_rand_get_state(&sim_state), has_fx = d_rand_get_stream_state(D_RNG_FX, &fx_state);
		short candidates[MAX_RENDERED_OBJECTS];
		const int count = render_collect_view_objects(offset, candidates);
		require(count >= 0 && std::vector<short>(candidates, candidates + count) == drawn, "CPU candidate list equals the actual renderer, including order");
		require(!std::memcmp(before.data(), Objects, before.size() * sizeof(object)), "CPU candidate preparation does not mutate live objects");
		unsigned after_sim = 0, after_fx = 0;
		require(d_rand_get_state(&after_sim) == has_sim && d_rand_get_stream_state(D_RNG_FX, &after_fx) == has_fx &&
		        after_sim == sim_state && after_fx == fx_state && sim_calls == d_rand_get_call_count() && fx_calls == d_rand_get_stream_call_count(D_RNG_FX),
		        "CPU candidate preparation preserves both RNG states and counts");
		trace.push_back({ { "scene", scene }, { "pose", pose }, { "width", width }, { "height", height },
		    { "rear", rear }, { "classic", classic }, { "offset", offset }, { "candidates", drawn },
		    { "segments", std::vector<short>(Render_list, Render_list + N_render_segs) } });
		gr_set_current_canvas(nullptr);
	};
	for (const int level : { 1, 14, 27 }) {
		std::fprintf(stderr, "Candidate level load %d\n", level);
#ifdef DXX_BUILD_DESCENT_II
		input_demo_set_skip_level_intro(1);
		StartNewGame(level);
#else
		StartNewLevelSub(level, 0, 0);
#endif
		Viewer = ConsoleObject = &Objects[Players[Player_num].objnum];
		for (int pose = 0; pose < 8; ++pose) {
			const int segment = pose * Highest_segment_index / 8;
			compute_segment_center(&Viewer->pos, &Segments[segment]);
			obj_relink(static_cast<int>(Viewer - Objects), segment);
			vms_angvec angles = { static_cast<fixang>(pose * 1777), static_cast<fixang>(pose * 991), static_cast<fixang>(pose * 8191) };
			vm_angles_2_matrix(&Viewer->orient, &angles);
			for (const int classic : { 0, 1 })
				for (const int rear : { 0, 1 })
					for (const int viewport : { 0, 1, 2 })
						check_view(level, pose, viewport == 0 ? 640 : 320, viewport == 2 ? 200 : 320, rear, classic, pose & 1 ? F1_0 / 4 : 0);
		}
	}
	// Dense mixed rows hit D1's 49-object sort limit and linked-row migration
	for (const int density : { 8, 48, 64, 96 }) {
		init_test_corridor(3);
		Viewer = ConsoleObject = &Objects[0];
		Players[0].objnum = 0;
		Viewer->type = OBJ_PLAYER;
		Viewer->orient = vmd_identity_matrix;
		for (int i = 0; i < density; ++i) {
			const int segment = density >= 64 ? 0 : i % 3;
			vms_vector position;
			compute_segment_center(&position, &Segments[segment]);
			position.x += (i % 5 - 2) * F1_0 / 4;
			position.z += (i % 7 - 3) * F1_0;
			const auto type = i % 4 == 0 ? OBJ_ROBOT : i % 4 == 1 ? OBJ_FIREBALL : OBJ_WEAPON;
			require(obj_create(type, 0, segment, &position, &vmd_identity_matrix, (i % 3 + 1) * 4 * F1_0, CT_NONE, MT_NONE, RT_NONE) > 0, "create dense candidate scene");
		}
		for (const int classic : { 0, 1 })
			for (const int rear : { 0, 1 })
				check_view(-density, 0, 640, 480, rear, classic, 0);
	}
	short untouched[MAX_RENDERED_OBJECTS];
	std::fill(std::begin(untouched), std::end(untouched), static_cast<short>(123));
	Endlevel_sequence = 1;
	require(render_collect_view_objects(0, untouched) == -1 && untouched[0] == 123, "endlevel retains the prior candidate list");
	Endlevel_sequence = 0;
	const std::string output = trace.dump(2) + "\n";
	write_fixture("candidates.json", bytes(output.begin(), output.end()));
	std::fprintf(stderr, "PASS: %zu CPU/renderer candidate lists match\n", trace.size());
}

void test_autoselect();

int main(int argc, char **argv)
{
	(void) argc;
	mem_init();
	error_init([](const char *message) { std::fprintf(stderr, "%s\n", message); });
	require(PHYSFS_init(argv[0]) != 0, "initialize PhysFS");
	require(PHYSFS_setWriteDir(".") != 0 && PHYSFS_mount(".", nullptr, 1) != 0, "mount isolated fixture directory");
	if (argc == 3 && std::strcmp(argv[1], "--render-candidates-trace") == 0) {
		write_render_candidates_trace(argv[2]);
		return 0;
	}
	if ((argc == 3 || argc == 4) && std::strcmp(argv[1], "--gameplay-rules-trace") == 0) {
		write_gameplay_rules_trace(argv[2], argc == 4 ? argv[3] : nullptr);
		return 0;
	}
	if ((argc == 3 || argc == 4) && std::strcmp(argv[1], "--briefing-trace") == 0) {
		write_briefing_trace(argv[2], argc == 4 ? argv[3] : nullptr);
		return 0;
	}
	if ((argc == 4 || argc == 5) && (std::strcmp(argv[1], "--checkpoint-frame-trace") == 0 || std::strcmp(argv[1], "--custom-checkpoint-frame-trace") == 0)) {
		write_checkpoint_frame_trace(argv[2], argv[3], std::strcmp(argv[1], "--custom-checkpoint-frame-trace") == 0, argc == 5 ? std::atoi(argv[4]) : 1);
		return 0;
	}
	if (argc == 3 && std::strcmp(argv[1], "--campaign-trace") == 0) {
		write_campaign_trace(argv[2]);
		return 0;
	}
	if (argc == 3 && std::strcmp(argv[1], "--robot-frame-trace") == 0) {
		write_robot_frame_trace(argv[2]);
		PHYSFS_deinit();
		return 0;
	}
	std::fprintf(stderr, "Testing model texture seek\n");
	test_seek();
	test_pcx_short_header();
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
	test_object_state_trace();
	test_lives();
	std::fprintf(stderr, "Testing wall crossing, door pixels and rotated RLE overlays\n");
	test_endlevel_flythrough();
	test_wall_crossing();
	std::fprintf(stderr, "Testing native compound triggers and paired crossing state\n");
	test_native_triggers();
#ifdef DXX_BUILD_DESCENT_II
	test_native_trigger_serialization();
#endif
	std::fprintf(stderr, "Testing native robot behavior initialization\n");
	test_robot_initialization();
	test_d1_follow_path_frame();
	std::fprintf(stderr, "Testing native robot perception and cloak state\n");
	test_robot_perception();
	std::fprintf(stderr, "Testing native robot frame scheduling\n");
	test_robot_scheduling();
	std::fprintf(stderr, "Testing native robot entry and awareness phases\n");
	test_robot_frame_entry();
	test_robot_awareness_frame();
	test_world_awareness();
	test_awareness_event_creation();
	test_robot_hit_response();
	test_world_camera_completion();
	test_missile_camera_awareness();
	test_companion_physics();
	test_robot_navigation_preparation();
	test_robot_path_creation();
	test_robot_path_following();
	test_hide_and_brain_frame();
	test_robot_chase_timeout();
	test_fleeing_robot_path_state();
	test_robot_firing();
	test_robot_relative_movement();
	test_boss_preparation_and_gating();
	test_boss_frame_updates();
	std::fprintf(stderr, "Testing native homing acquisition and retention\n");
	test_homing_targets();
	test_projectile_collision_relationships();
	test_stuck_projectiles();
	test_autoselect();
#ifdef DXX_BUILD_DESCENT_II
	std::fprintf(stderr, "Testing mission robot reload\n");
	digi_select_system(SDLAUDIO_SYSTEM);
	test_robot_reload();
	std::fprintf(stderr, "Testing D1 monitor replacement and animation\n");
	test_d1_monitors();
	std::fprintf(stderr, "Testing D1 reactor assets and wreck\n");
	test_d1_reactor();
	std::fprintf(stderr, "Testing embedded D2 sounds after registry replacement\n");
	test_d2_embedded_sound_reload();
	std::fprintf(stderr, "Testing D1 light destruction policy\n");
	test_d1_indestructible_lights();
	std::fprintf(stderr, "Testing D1 simulation operation dispatch\n");
	test_d1_simulation_dispatch();
	std::fprintf(stderr, "Testing D1 weapon creation and smart children\n");
	test_d1_weapon_creation();
	std::fprintf(stderr, "Testing independent original D1 bitmap preparation\n");
	test_d1_native_bitmaps();
	std::fprintf(stderr, "Testing original D1 text resources\n");
	test_d1_text_resources();
	const char *d2_directory = nullptr;
	bool presentation_graphics = false;
	for (int i = 1; i + 1 < argc; ++i)
		if (std::strcmp(argv[i], "--cockpit-dump-prefix") == 0)
			Cockpit_dump_prefix = argv[++i];
	for (int i = 1; i < argc; ++i)
		if (std::strcmp(argv[i], "--presentation-graphics") == 0)
			presentation_graphics = true;
	for (int i = 1; i + 1 < argc; ++i)
		if (std::strcmp(argv[i], "--d2-assets") == 0)
			d2_directory = argv[++i];
	for (int i = 1; i + 1 < argc; ++i)
		if (std::strcmp(argv[i], "--d1-assets") == 0)
			test_d1_registered_bitmaps(argv[++i], d2_directory, presentation_graphics);
#else
	std::fprintf(stderr, "Testing HX1 model loading\n");
	test_hx1();
#endif
	PHYSFS_deinit();
	std::puts("Upstream compatibility integration tests passed");
	return 0;
}
