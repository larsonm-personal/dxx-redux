// Android texture events, normal renderer readback and ordinary persistence
#include <nlohmann/json.hpp>
#include "texture_action_probe.h"
#include "texture_action_controls.h"
extern "C" {
#include "gameseq.h"
#include "render.h"
#include "ogl_init.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
void ogl_prepare_framebuffer_readback(void);
}

namespace
{
using xl_texture_controls::require;
std::string failure, report = "null";
std::vector<int> earned;
xl_trigger_state pending{};
int version = 0;
fix energy = 0;
unsigned scenes = 0, checked_pixels = 0, verifies = 0;

void verify()
{
	require(!earned.empty() && xl_level_extensions_active() && xl_source_level_version() == version &&
	            Players[Player_num].energy == energy && xl_texture_controls::snapshot() == earned,
	        "Android texture restore lost IDs, rotation, UVs, lighting or player state");
	xl_trigger_state current{};
	require(xl_wall_trigger_state(4, &current) && current.elapsed == pending.elapsed && current.delay_ms == pending.delay_ms && current.operated,
	        "Android texture restore lost its pending delayed action");
	++verifies;
}

std::vector<unsigned char> pixels()
{
	// Use the game's normal renderer and normal MSAA resolve for each observation
	render_frame(0, 0);
	ogl_prepare_framebuffer_readback();
	GLint viewport[4], read, pack, alignment, row, skip_rows, skip_pixels;
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
	glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
	require(viewport[2] >= 64 && viewport[3] >= 64, "Texture scene viewport is too small");
	glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_PACK_ROW_LENGTH, 0);
	glPixelStorei(GL_PACK_SKIP_ROWS, 0);
	glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
	std::vector<unsigned char> result(32 * 32 * 4);
	glReadPixels(viewport[0] + viewport[2] / 2 - 16, viewport[1] + viewport[3] / 2 - 16, 32, 32, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
	const auto error = glGetError();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
	glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, row);
	glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
	require(error == GL_NO_ERROR && glGetError() == GL_NO_ERROR, "Texture scene GPU readback failed");
	checked_pixels += 32 * 32;
	return result;
}

void scene()
{
	xl_texture_controls::fixture();
	require(game_is_time_paused() && gl_initialized, "Texture scene control requires paused gameplay and an active context");
	const auto old_position = ConsoleObject->pos;
	const auto old_orientation = ConsoleObject->orient;
	const int old_segment = ConsoleObject->segnum;
	const auto old_viewer = Viewer;
	const auto old_canvas = grd_curcanv;
	const auto old_frame_time = FrameTime;
	const auto old_front = xl_texture_controls::face();
	const auto restore = [&] {
		xl_texture_controls::face() = old_front;
		ConsoleObject->pos = old_position;
		ConsoleObject->orient = old_orientation;
		obj_relink(Players[Player_num].objnum, old_segment);
		Viewer = old_viewer;
		FrameTime = old_frame_time;
		gr_set_current_canvas(old_canvas);
	};
	try {
		const auto &wall = Walls[Num_walls - 2];
		vms_vector center, direction;
		compute_center_point_on_side(&center, &Segments[wall.segnum], wall.sidenum);
		direction = xl_texture_controls::face().normals[0];
		vm_vec_scale_add(&ConsoleObject->pos, &center, &direction, 4 * F1_0);
		vm_vec_negate(&direction);
		vm_vector_2_matrix(&ConsoleObject->orient, &direction, nullptr, nullptr);
		obj_relink(Players[Player_num].objnum, wall.segnum);
		require(!get_seg_masks(&ConsoleObject->pos, wall.segnum, 0, nullptr, 0).centermask, "Controlled texture view lies outside its segment");
		Viewer = ConsoleObject;
		FrameTime = 0;
		gr_set_current_canvas(nullptr);
		require(xl_trigger_texture_action(&Triggers[0]) == 0, "Cannot render authored overlay");
		pixels();
		const auto overlay = pixels();
		require(pixels() == overlay, "Repeated texture view is not stable");
		require(xl_trigger_texture_action(&Triggers[1]) == 0, "Cannot render authored overlay clear");
		const auto clear = pixels();
		xl_texture_controls::face().tmap_num2 = 0;
		require(pixels() == clear, "Cleared rotated overlay still renders a texture-zero layer");
		xl_texture_controls::face().tmap_num2 = short(0xc000);
		require(xl_trigger_texture_action(&Triggers[0]) == 0 && pixels() == overlay, "Restored overlay rendered with different rotation or pixels");
		unsigned changed = 0;
		for (unsigned i = 0; i < clear.size(); i += 4)
			if (!std::equal(clear.begin() + i, clear.begin() + i + 3, overlay.begin() + i)) ++changed;
		require(changed >= 512, "Texture scene lacks an actual visible material change");
		++scenes;
		report = nlohmann::json{ { "passed", true }, { "scenes", scenes }, { "checked_pixels", checked_pixels }, { "changed_pixels", changed }, { "verifies", verifies } }.dump();
		restore();
	} catch (...) {
		restore();
		throw;
	}
}
} // namespace

const char *xl_texture_action_report()
{
	return report.c_str();
}

const char *xl_texture_action_probe(const char *operation)
{
	try {
		using namespace xl_texture_controls;
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Texture controls require running singleplayer");
		if (!std::strcmp(operation, "stock")) {
			const auto before = snapshot();
			trigger action{};
			action.type = 19;
			require(!xl_level_extensions_active() && xl_trigger_texture_action(&action) == 1 && snapshot() == before, "Texture action contaminated Redux");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(!earned.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android texture state");
			verify();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored texture state");
		} else {
			fixture();
			if (!std::strcmp(operation, "events")) events();
			else if (!std::strcmp(operation, "scene")) scene();
			else if (!std::strcmp(operation, "prepare")) {
				require(game_is_time_paused(), "Texture capture requires paused gameplay");
				pending = prepare();
				earned = snapshot();
				version = xl_source_level_version();
				energy = Players[Player_num].energy;
				verify();
				for (const char *name : { "badbase.rl2", "badovl.rl2", "badlink.rl2", "badobject.rl2" }) {
					require(!xl_level_supported(name, 0), "Android admitted an invalid texture source");
					verify();
				}
			} else if (!std::strcmp(operation, "verify")) verify();
			else if (!std::strcmp(operation, "disturb")) {
				verify();
				continuation(pending);
				Players[Player_num].energy -= F1_0;
				require(snapshot() != earned, "Texture continuation did not change material state");
			} else if (!std::strcmp(operation, "disk_save")) {
				verify();
				require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL texture continuation", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android texture state");
			} else if (!std::strcmp(operation, "next")) {
				require(!game_is_time_paused() && Current_level_num == 1 && Last_level == 2, "Texture transition requires the running two-level fixture");
				StartNewLevelSub(2, 1, 0);
				require(Current_level_num == 2 && xl_source_level_version() == 27, "Texture transition did not select the next layout");
			} else require(false, "Unknown texture control operation");
		}
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
