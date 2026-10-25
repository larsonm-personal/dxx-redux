// Reproducible developer preview startup; no gameplay assists
#include "preview.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <nlohmann/json.hpp>
#if defined(__ANDROID__) && defined(INTROSPECT_ON)
#include "game_automate.h"
#endif
extern "C" {
#include "args.h"
#include "config.h"
#include "gr.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "wall.h"
#include "inferno.h"
#include "input_demo_start.h"
#include "mission.h"
#include "player.h"
#include <physfs.h>
int input_demo_maybe_start_replay_from_cmdline(void);
extern int Canvas_width, Canvas_height;
extern fix View_zoom;
extern fix Render_zoom;
extern vms_vector View_position, Matrix_scale;
extern vms_matrix Unscaled_matrix;
}

namespace
{
bool running = false;
bool reference_fans = false;
unsigned rendered_ticks = 0;
bool has_option(const char *name)
{
	for (int i = 1; i < Num_args; ++i)
		if (!std::strcmp(Args[i], name)) return true;
	return false;
}
} // namespace

extern "C" int xl_preview_start(void)
{
	const bool option = has_option("-xl-preview-start");
	if (!option) return input_demo_maybe_start_replay_from_cmdline();
	if (!has_option("-xl-static-light-preview") && !has_option("-xl-classic-light-preview") && !has_option("-xl-pilot") && !has_option("-xl-water-physics")) {
		std::fprintf(stderr, "XL preview startup requires a lighting, pilot or fluid profile option\n");
		return 1;
	}
	reference_fans = has_option("-xl-preview-reference-fans");
	if (reference_fans && !has_option("-xl-preview-still")) {
		std::fprintf(stderr, "Reference fan diagnostic requires -xl-preview-still\n");
		return 1;
	}
	fix capture_zoom = 0;
	for (int i = 1; i < Num_args; ++i) {
		if (std::strcmp(Args[i], "-xl-preview-zoom-fixed")) continue;
		char *end = nullptr;
		const long value = i + 1 < Num_args ? std::strtol(Args[i + 1], &end, 10) : 0;
		if (!has_option("-xl-preview-still") || !end || *end || value < F1_0 / 4 || value > 4 * F1_0) {
			std::fprintf(stderr, "XL capture zoom requires -xl-preview-still and fixed zoom 16384..262144\n");
			return 1;
		}
		capture_zoom = static_cast<fix>(value);
	}
	char mission[PATH_MAX];
	const char *selected = has_option("-xl-pilot") ? "d2-xl" : "xlprobe";
	bool explicit_mission = false;
	for (int i = 1; i + 1 < Num_args; ++i)
		if (!std::strcmp(Args[i], "-mission")) {
			selected = Args[i + 1];
			explicit_mission = true;
			break;
		}
	if (((has_option("-xl-water-physics") || has_option("-xl-classic-light-preview")) && !explicit_mission) || std::strlen(selected) >= sizeof(mission)) {
		std::fprintf(stderr, "XL fluid or classic lighting preview requires an explicit bounded -mission name\n");
		return 1;
	}
	std::strcpy(mission, selected);
	if (!load_mission_by_name(mission)) {
		std::fprintf(stderr, "XL preview cannot load mission %s\n", mission);
		return 1;
	}
	std::strcpy(Players[Player_num].callsign, "XLTest");
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	if (has_option("-xl-preview-still")) {
		/* Explicit visual fixture only; never use it as normal gameplay evidence */
		ConsoleObject->mtype.phys_info.flags &= ~PF_WIGGLE;
		select_cockpit(CM_FULL_SCREEN);
		if (capture_zoom) Render_zoom = capture_zoom;
		std::fprintf(stdout, "XL still visual fixture: ship wiggle off, full-screen view; gameplay unqualified\n");
	}
	for (int i = 1; i < Num_args; ++i) {
		if (std::strcmp(Args[i], "-xl-preview-wall")) continue;
		char *end = nullptr;
		const long index = i + 1 < Num_args ? std::strtol(Args[i + 1], &end, 10) : -1;
		if (!end || *end || index < 0 || index >= Num_walls) {
			std::fprintf(stderr, "XL preview wall index invalid\n");
			return 1;
		}
		const wall &w = Walls[index];
		vms_vector target, direction;
		compute_segment_center(&ConsoleObject->pos, &Segments[w.segnum]);
		compute_center_point_on_side(&target, &Segments[w.segnum], w.sidenum);
		vm_vec_sub(&direction, &target, &ConsoleObject->pos);
		vm_vector_2_matrix(&ConsoleObject->orient, &direction, nullptr, nullptr);
		obj_relink(Players[Player_num].objnum, w.segnum);
		std::fprintf(stdout, "XL material camera repositioned to wall %ld; not a gameplay test\n", index);
		break;
	}
	running = true;
	return 0;
}

extern "C" int xl_preview_reference_fans(void)
{
#if defined(__ANDROID__) && defined(INTROSPECT_ON)
	return game_automate_reference_fans_active();
#else
	return running && reference_fans;
#endif
}

extern "C" int xl_preview_split_face_visible(int segment_index, int side_index, int32_t x, int32_t y, int32_t z)
{
	if (segment_index < 0 || segment_index > Highest_segment_index || side_index < 0 || side_index >= 6) return 0;
	segment *seg = &Segments[segment_index];
	const side *face = &seg->sides[side_index];
	if (face->type != SIDE_IS_TRI_02 && face->type != SIDE_IS_TRI_13) return 0;
	const auto &anchor = Vertices[seg->verts[Side_to_verts[side_index][face->type == SIDE_IS_TRI_13 ? 3 : 0]]];
	vms_vector normals[2];
#ifdef COMPACT_SEGS
	get_side_normals(seg, side_index, &normals[0], &normals[1]);
#else
	normals[0] = face->normals[0];
	normals[1] = face->normals[1];
#endif
	for (const auto &normal : normals) {
		const double dot = double(normal.x) * (double(x) - anchor.x) +
		                   double(normal.y) * (double(y) - anchor.y) +
		                   double(normal.z) * (double(z) - anchor.z);
		/* XL truncates dot / 65536 toward zero; avoid normalization near grazing */
		if (dot > -65536.0) return 1;
	}
	return 0;
}

extern "C" void xl_preview_before_swap(void)
{
	if (!running || !xl_preview_vertex_calls()) return;
	if (++rendered_ticks != 30) return;
	/* Capture actual render inputs, not inferred image alignment or gameplay proof */
	const auto vector = [](const vms_vector &v) { return nlohmann::json::array({ v.x, v.y, v.z }); };
	const nlohmann::json context = {
		{ "scope", "Developer visual capture only; not normal gameplay qualification" },
		{ "frame", rendered_ticks },
		{ "still_visual_fixture", has_option("-xl-preview-still") },
		{ "reference_fan_diagnostic", reference_fans },
		{ "visual_profile", (xl_classic_legacy_fans_active() || xl_preview_reference_fans()) ? "legacy_fan" : "compiled_splits" },
		{ "view_position_fixed", vector(View_position) },
		{ "player_position_fixed", vector(ConsoleObject->pos) },
		{ "view_right_fixed", vector(Unscaled_matrix.rvec) },
		{ "view_up_fixed", vector(Unscaled_matrix.uvec) },
		{ "view_forward_fixed", vector(Unscaled_matrix.fvec) },
		{ "matrix_scale_fixed", vector(Matrix_scale) },
		{ "zoom_fixed", View_zoom },
		{ "canvas", { Canvas_width, Canvas_height } },
		{ "screen", { grd_curscreen->sc_w, grd_curscreen->sc_h } },
		{ "gamma", GameCfg.GammaLevel },
		{ "texture_filter", GameCfg.TexFilt }
	};
	const std::string serialized = context.dump(2) + "\n";
	if (PHYSFS_File *file = PHYSFS_openWrite("xl-preview-camera.json")) {
		const bool written = PHYSFS_write(file, serialized.data(), 1, static_cast<PHYSFS_uint32>(serialized.size())) == static_cast<PHYSFS_sint64>(serialized.size());
		const bool closed = PHYSFS_close(file) != 0;
		if (!written || !closed) std::fprintf(stderr, "XL preview camera evidence write failed\n");
	} else std::fprintf(stderr, "XL preview camera evidence file unavailable\n");
	save_screen_shot(0);
	std::fprintf(stdout, "XL preview capture requested after %llu shaded vertices; fidelity and gameplay unverified\n",
	             static_cast<unsigned long long>(xl_preview_vertex_calls()));
	std::fflush(stdout);
	std::fprintf(stdout, "XL dynamic preview unresolved emitters: %u; reference rendering and other lighting modes unverified\n", xl_preview_unresolved_emitters());
	std::fflush(stdout);
}
