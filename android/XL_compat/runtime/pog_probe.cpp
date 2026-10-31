// Android automation: installed pixels, real GPU readback and ordinary save lifecycle
#include "pog_probe.h"
#include "../pog.h"
#include "preview.h"
#include "particle_resources.h"
#include "particle_renderer.h"
#include "particle_objects.h"
#include "skybox_collision.h"
#include "skybox_render.h"
#include "particle_state.h"
#include "transparency_scene.h"
#include "resource_validation.h"
#include "../inventory.h"
#include "../particle_render.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
#include "inferno.h"
#include "args.h"
#include "game.h"
#include "gamesave.h"
#include "gameseq.h"
#include "mission.h"
#include "piggy.h"
#include "textures.h"
#include "player.h"
#include "physfsx.h"
#include "ogl_init.h"
#include "internal.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
#include "input_demo_start_shared.h"
#include "switch.h"
#include "fvi.h"
#include "gameseg.h"
#include "physics.h"
#include "render.h"
#include "automap.h"
#include "render_gameplay_view.h"
#include "robot.h"
void ogl_loadbmtexture(grs_bitmap *bitmap);
void ogl_prepare_framebuffer_readback(void);
#ifndef NDEBUG
extern ubyte object_rendered[MAX_OBJECTS];
#endif
}

namespace
{
std::string failure, report = "null", saved_mission;
std::vector<xl_compat::PogBitmap> expected;
int saved_level = 0;
fix saved_energy = 0;
unsigned checks = 0;

void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}

void verify()
{
	require(xl_level_extensions_active() && !expected.empty() && saved_mission == Current_mission_filename &&
	            saved_level == Current_level_num,
	        "POG control lost the saved mission/resource owner");
	for (const auto &image : expected) {
		const auto &bitmap = GameBitmaps[image.index];
		require(image.truecolor && image.frames == 1 && bitmap.bm_type == BM_RGBA &&
		            bitmap.bm_w == int(image.width) && bitmap.bm_h == int(image.height) &&
		            bitmap.bm_rowsize == int(image.width * 4) && bitmap.bm_data && !piggy_bitmap_get_offset(image.index) &&
		            !std::memcmp(bitmap.bm_data, image.pixels.data(), image.pixels.size()),
		        "Actual Android POG registry differs from its source-normalized RGBA resource");
	}
	++checks;
}

struct Readback {
	GLint read, draw, buffer, alignment, row_length, skip_rows, skip_pixels;
	GLuint framebuffer = 0;
	Readback()
	{
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
		glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &buffer);
		glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
		glGetIntegerv(GL_PACK_ROW_LENGTH, &row_length);
		glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
		glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ROW_LENGTH, 0);
		glPixelStorei(GL_PACK_SKIP_ROWS, 0);
		glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
	}
	~Readback()
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
		glDeleteFramebuffers(1, &framebuffer);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, buffer);
		glPixelStorei(GL_PACK_ALIGNMENT, alignment);
		glPixelStorei(GL_PACK_ROW_LENGTH, row_length);
		glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
		glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
	}
	std::vector<unsigned char> pixels(const ogl_texture *texture, unsigned width, unsigned height)
	{
		require(texture && texture->handle && texture->xl_rgba && texture->tw == int(width) && texture->th == int(height),
		        "Actual Android RGBA texture has missing or resized GPU storage");
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture->handle, 0);
		require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "RGBA texture readback target is incomplete");
		std::vector<unsigned char> result(size_t(width) * height * 4);
		glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, result.data());
		require(glGetError() == GL_NO_ERROR, "RGBA texture readback raised a GPU error");
		return result;
	}
};

#include "particle_gpu_probe.h"
#include "particle_original_probe.h"
#include "skybox_collision_test.h"
#include "skybox_render_probe.h"
#include "particle_secret_probe.h"

void gpu()
{
	verify();
	require(gl_initialized && glGetError() == GL_NO_ERROR, "POG GPU control requires a clean active context");
	unsigned rendered = 0, bytes = 0, masks = 0;
	{
		Readback readback;
		for (const auto &image : expected) {
			auto &bitmap = GameBitmaps[image.index];
			if (bitmap.gltexture && bitmap.gltexture->numrend) ++rendered;
			ogl_loadbmtexture(&bitmap);
			require(readback.pixels(bitmap.gltexture, image.width, image.height) == image.pixels,
			        "Actual Android GPU pixels differ from the owned POG image");
			bytes += unsigned(image.pixels.size());
			if (image.flags & xl_compat::pog_super) {
				const auto mask = readback.pixels(bitmap.gltexture_mask, image.width, image.height);
				for (size_t i = 0; i < mask.size() / 4; ++i)
					require(mask[i * 4 + 3] == (image.transparency(i) < 0 ? 0 : 255), "Actual Android super-transparency mask differs");
				++masks;
			}
		}
	}
	require(glGetError() == GL_NO_ERROR, "POG readback state restoration raised a GPU error");
	report = nlohmann::json{ { "passed", true }, { "bitmaps", expected.size() }, { "bytes", bytes }, { "masks", masks }, { "rendered", rendered }, { "checks", checks } }.dump();
}
} // namespace

const char *xl_pog_probe(const char *operation)
{
	try {
		if (!std::strncmp(operation, "secret_", 7)) {
			particle_secret_probe(operation + 7);
			return nullptr;
		}
		if (!std::strncmp(operation, "original_", 9)) {
			particle_original_probe(operation + 9);
			return nullptr;
		}
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "POG controls require singleplayer");
		if (!std::strcmp(operation, "particle_capture")) {
			require(game_is_time_paused() && xl_level_extensions_active(), "Particle image capture requires a paused XL level");
			xl_compat::Inventory request;
			xl_compat::Object fire{};
			fire.render = 11;
			fire.render_data.resize(13);
			fire.render_data[11] = 3;
			request.objects.push_back(fire);
			particle_probe_image = xl_compat::prepare_particle_image(request);
			require(bool(particle_probe_image), "Particle image capture did not prepare an owned atlas");
		} else if (!std::strcmp(operation, "particle_gpu")) {
			particle_gpu_probe();
		} else if (!std::strcmp(operation, "particle_release")) {
			require(bool(particle_probe_image), "Particle release has no retained image");
			particle_probe_image.reset();
		} else if (!std::strcmp(operation, "capture")) {
			require(game_is_time_paused() && xl_level_extensions_active(), "POG capture requires a paused XL level");
			std::string name = Gamesave_current_filename;
			const auto extension = name.find_last_of('.');
			require(extension != std::string::npos, "POG control has no level extension");
			name.resize(extension);
			name += ".pog";
			auto *file = PHYSFSX_openReadBuffered(name.c_str());
			require(file != nullptr, "POG capture has no original level companion");
			const auto size = PHYSFS_fileLength(file);
			if (size <= 0 || size > PHYSFS_sint64(xl_compat::pog_file_limit)) {
				PHYSFS_close(file);
				throw std::runtime_error("POG capture source exceeds its bound");
			}
			std::vector<unsigned char> source(static_cast<size_t>(size));
			const auto count = PHYSFS_readBytes(file, source.data(), source.size());
			require(PHYSFS_close(file) && count == size, "POG capture source read failed");
			expected = xl_compat::read_pog(source.data(), source.size(), Num_bitmap_files);
			saved_mission = Current_mission_filename;
			saved_level = Current_level_num;
			saved_energy = Players[Player_num].energy;
			verify();
		} else if (!std::strcmp(operation, "verify_saved")) {
			verify();
			require(Players[Player_num].energy == saved_energy, "POG lifecycle did not restore player state");
		} else if (!std::strcmp(operation, "gpu")) {
			gpu();
		} else if (!std::strcmp(operation, "disturb")) {
			verify();
			Players[Player_num].energy -= F1_0;
		} else if (!std::strcmp(operation, "disk_save")) {
			verify();
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL RGBA POG", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save original RGBA mission");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(!expected.empty() && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore original RGBA mission");
			verify();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored RGBA mission");
		} else if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active(), "POG stock control is still XL");
			for (int i = 0; i < Num_bitmap_files; ++i)
				require(GameBitmaps[i].bm_type != BM_RGBA, "RGBA bitmap descriptor leaked into Redux");
			++checks;
		} else throw std::runtime_error("Unknown POG control operation");
		return nullptr;
	} catch (const std::exception &exception) {
		failure = exception.what();
		report = nlohmann::json{ { "passed", false }, { "checks", checks }, { "error", failure } }.dump();
		return failure.c_str();
	}
}

const char *xl_pog_probe_result()
{
	if ((!original_particle_earned.empty() || !secret_particle_base.empty()) && report != "null") {
		auto state = nlohmann::json::parse(report);
		unsigned particles = 0;
		for (const auto &system : xl_compat::particle_systems()) particles += system.emitter.count;
		state["live_particles"] = particles;
		state["particle_systems"] = xl_compat::particle_systems().size();
		xl_particle_scene_result scene{};
		xl_particle_scene_probe_result(&scene);
		state["scene"] = { { "completed", scene.completed }, { "passed", scene.passed }, { "batches", scene.batches }, { "quads", scene.quads }, { "changed", scene.changed }, { "samples", scene.samples }, { "error", xl_particle_scene_probe_error() } };
		report = state.dump();
	}
	return report.c_str();
}
