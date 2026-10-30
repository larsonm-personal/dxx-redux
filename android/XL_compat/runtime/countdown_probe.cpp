// Android controlled countdown output, terminal death and ordinary persistence
#include <nlohmann/json.hpp>
#include <SDL.h>
#include "countdown_probe.h"
#include "countdown_controls.h"
extern "C" {
#include "gr.h"
#include "ogl_init.h"
#include "digi.h"
#include "songs.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
#include "android_audio_capture.h"
void render_countdown_gauge(void);
void ogl_prepare_framebuffer_readback(void);
}
namespace
{
using xl_countdown_controls::require;
std::string failure, report = "null";
xl_countdown_state earned{};
int version = 0, lives = 0, energy = 0;
unsigned verifies = 0, hud_pixels = 0, frames_checked = 0, terminal_deaths = 0;
std::size_t audio_samples = 0, audible_samples = 0;
bool audio_invalid = false;
unsigned initial_deaths = 0;
void verify()
{
	require(version && xl_source_level_version() == version && xl_countdown_controls::equal(earned, xl_countdown_controls::snapshot()) && Players[Player_num].energy == energy,
	        "Android countdown restore lost exact timer, duration, displayed value, pending cadence or player state");
	++verifies;
}
void audio_capture(const SDL_AudioSpec *spec, const Uint8 *data, const Uint8 *music, int length, long long)
{
	if (!music || spec->format != AUDIO_S16SYS || spec->channels != 2 || length < 0 || length % 4) {
		audio_invalid = true;
		return;
	}
	const auto *mixed = reinterpret_cast<const std::int16_t *>(data), *background = reinterpret_cast<const std::int16_t *>(music);
	for (int i = 0; i < length / 2; ++i) audible_samples += mixed[i] != background[i];
	audio_samples += std::size_t(length / 2);
}
void audio_output()
{
	SDL_PauseAudio(1);
	digi_stop_all_channels();
	songs_set_volume(0);
	digi_set_digi_volume(32768);
	audio_samples = audible_samples = 0;
	audio_invalid = false;
	androidaud_set_capture_callback(audio_capture);
	xl_countdown_clear();
	require(xl_countdown_controls::fire(7) == 0, "Cannot start audible countdown");
	const auto before = xl_countdown_sound_queries(114);
	xl_countdown_controls::step(F1_0);
	require(xl_countdown_sound_queries(114) == before + 1, "Countdown did not dispatch the actual activation warning");
	SDL_PauseAudio(0);
	const auto start = SDL_GetTicks();
	for (;;) {
		SDL_LockAudio();
		const bool done = audio_samples >= 32768;
		SDL_UnlockAudio();
		if (done) break;
		require(SDL_GetTicks() - start < 3500, "Countdown audio output timed out");
		SDL_Delay(1);
	}
	SDL_PauseAudio(1);
	androidaud_set_capture_callback(nullptr);
	require(!audio_invalid && audible_samples > 1000, "Countdown warning produced no PCM beyond background music");
	SDL_PauseAudio(0);
}
void hud()
{
	require(game_is_time_paused() && gl_initialized && Control_center_destroyed && Countdown_seconds_left >= 0, "Countdown HUD requires a paused active countdown");
	const auto canvas = grd_curcanv;
	gr_set_current_canvas(nullptr);
	GLint viewport[4], read, pack, alignment, row, skip_rows, skip_pixels;
	glGetIntegerv(GL_VIEWPORT, viewport);
	glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
	glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
	glGetIntegerv(GL_PACK_ALIGNMENT, &alignment);
	glGetIntegerv(GL_PACK_ROW_LENGTH, &row);
	glGetIntegerv(GL_PACK_SKIP_ROWS, &skip_rows);
	glGetIntegerv(GL_PACK_SKIP_PIXELS, &skip_pixels);
	require(viewport[2] > 0 && viewport[3] > 0 && viewport[2] <= 8192 && viewport[3] <= 8192, "Invalid countdown HUD viewport");
	const auto pixels = [&] {
		ogl_prepare_framebuffer_readback();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glPixelStorei(GL_PACK_ROW_LENGTH, 0);
		glPixelStorei(GL_PACK_SKIP_ROWS, 0);
		glPixelStorei(GL_PACK_SKIP_PIXELS, 0);
		std::vector<unsigned char> result(std::size_t(viewport[2]) * viewport[3] * 4);
		glReadPixels(viewport[0], viewport[1], viewport[2], viewport[3], GL_RGBA, GL_UNSIGNED_BYTE, result.data());
		return result;
	};
	gr_clear_canvas(BM_XRGB(0, 0, 0));
	const auto blank = pixels();
	render_countdown_gauge();
	const auto shown = pixels();
	Control_center_destroyed = 0;
	gr_clear_canvas(BM_XRGB(0, 0, 0));
	render_countdown_gauge();
	const auto absent = pixels();
	Control_center_destroyed = 1;
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
	glPixelStorei(GL_PACK_ALIGNMENT, alignment);
	glPixelStorei(GL_PACK_ROW_LENGTH, row);
	glPixelStorei(GL_PACK_SKIP_ROWS, skip_rows);
	glPixelStorei(GL_PACK_SKIP_PIXELS, skip_pixels);
	gr_set_current_canvas(canvas);
	require(glGetError() == GL_NO_ERROR && absent == blank, "Inactive countdown drew a HUD gauge or GPU readback failed");
	unsigned changed = 0;
	for (std::size_t i = 0; i < shown.size(); i += 4)
		if (shown[i + 1] > blank[i + 1] && shown[i + 1] > shown[i] && shown[i + 1] > shown[i + 2]) ++changed;
	require(changed > 20, "Normal countdown HUD rendered no green glyph pixels");
	hud_pixels += changed;
}
} // namespace
const char *xl_countdown_report()
{
	return report.c_str();
}
const char *xl_countdown_probe(const char *operation)
{
	try {
		using namespace xl_countdown_controls;
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Countdown controls require running singleplayer");
		if (!std::strcmp(operation, "stock")) {
			const auto before = snapshot();
			trigger action{};
			action.type = 25;
			action.time = 99;
			require(!xl_level_extensions_active() && !xl_countdown_action(&action), "XL countdown action ran in Redux");
			xl_countdown_frame();
			require(equal(before, snapshot()), "XL countdown frame changed Redux");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(version && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android countdown");
			verify();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored countdown");
		} else if (!std::strcmp(operation, "terminal_verify")) {
			require(Current_level_num == 2 && xl_source_level_version() == 27 && Players[Player_num].lives == lives - 1 && !Player_is_dead && xl_countdown_death_queries() == initial_deaths + 1,
			        "Countdown terminal death failed to consume one life and advance normally");
			++terminal_deaths;
		} else {
			fixture();
			if (!std::strcmp(operation, "events")) events();
			else if (!std::strcmp(operation, "frames")) {
				frames();
				audio_output();
				++frames_checked;
			} else if (!std::strcmp(operation, "hud")) hud();
			else if (!std::strcmp(operation, "prepare")) {
				require(game_is_time_paused(), "Countdown capture requires paused gameplay");
				earned = prepare();
				version = xl_source_level_version();
				energy = Players[Player_num].energy;
				verify();
				for (const char *name : { "badcount.rl2", "badobject.rl2", "badbase.rl2" }) require(!xl_level_supported(name, 0), "Android admitted invalid countdown duration");
			} else if (!std::strcmp(operation, "verify")) verify();
			else if (!std::strcmp(operation, "disturb")) {
				verify();
				continuation(earned);
				Players[Player_num].energy -= F1_0;
			} else if (!std::strcmp(operation, "disk_save")) {
				verify();
				require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL countdown continuation", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android countdown");
			} else if (!std::strcmp(operation, "terminal_arm")) {
				require(!game_is_time_paused() && Current_level_num == 1 && Last_level == 2, "Terminal countdown requires the running first fixture level");
				lives = Players[Player_num].lives;
				initial_deaths = xl_countdown_death_queries();
				require(lives > 1, "Terminal countdown needs a surviving life");
				xl_countdown_clear();
				require(fire(7) == 0, "Cannot arm terminal countdown");
				Countdown_timer = -3 * F1_0;
				Countdown_seconds_left = -3;
			} else require(false, "Unknown countdown control operation");
		}
		report = nlohmann::json{ { "passed", true }, { "verifies", verifies }, { "hud_pixels", hud_pixels }, { "frame_sequences", frames_checked }, { "audio_samples", audio_samples }, { "audible_samples", audible_samples }, { "terminal_deaths", terminal_deaths } }.dump();
		return nullptr;
	} catch (const std::exception &error) {
		androidaud_set_capture_callback(nullptr);
		SDL_PauseAudio(0);
		failure = error.what();
		return failure.c_str();
	}
}
