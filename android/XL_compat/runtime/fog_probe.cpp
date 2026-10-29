// Android automation diagnostics for controlled fog views and actual GPU output
#include "fog_probe.h"
#include "fog.h"
#include "preview.h"
#include "trigger_state.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
#include "inferno.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "mission.h"
#include "player.h"
#include "physfsx.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
}
namespace
{
std::string failure, report = "null";
std::uint64_t saved_identity = 0;
float saved_colors[4][4]{};
fix saved_energy = 0;
xl_fog_diagnostics stock_state{};
bool stock_captured = false;
std::uint64_t context_rebuilds = 0, context_identity = 0;
float context_colors[4][4]{};
void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
void verify_saved()
{
	float colors[4][4]{};
	require(saved_identity && xl_fog_colors(colors) && xl_level_trigger_identity() == saved_identity &&
	            !std::memcmp(colors, saved_colors, sizeof(colors)) && Players[Player_num].energy == saved_energy,
	        "Fog restore lost source ownership, authored colors/distances or player state");
}
} // namespace
const char *xl_fog_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Fog controls require singleplayer");
		if (!std::strcmp(operation, "scene")) {
			require(xl_fog_active(), "Normal fog scene control requires active authored fog");
			ogl_xl_fog_scene_request();
		} else if (!std::strcmp(operation, "gpu")) {
			xl_fog_gpu_result result{};
			const char *error = ogl_xl_fog_gpu_probe(&result);
			report = nlohmann::json{ { "passed", error == nullptr }, { "cases", result.cases }, { "pixels", result.pixels }, { "multisample_cases", result.multisample_cases }, { "state_restored", result.state_restored != 0 }, { "max_error", result.max_error }, { "error", error ? error : "" } }.dump();
			if (error) throw std::runtime_error(error);
		} else if (!std::strcmp(operation, "capture")) {
			require(game_is_time_paused() && xl_fog_active(), "Fog persistence controls require a paused fog level");
			saved_identity = xl_level_trigger_identity();
			saved_energy = Players[Player_num].energy;
			require(xl_fog_colors(saved_colors), "Cannot capture owned fog settings");
			verify_saved();
		} else if (!std::strcmp(operation, "verify_saved")) {
			verify_saved();
		} else if (!std::strcmp(operation, "disturb")) {
			verify_saved();
			Players[Player_num].energy -= F1_0;
		} else if (!std::strcmp(operation, "disk_save")) {
			verify_saved();
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL authored fog", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save authored fog level");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(saved_identity && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore authored fog level");
			verify_saved();
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored fog level");
		} else if (!std::strcmp(operation, "context_capture")) {
			xl_fog_diagnostics state{};
			ogl_xl_fog_diagnostics(&state);
			context_rebuilds = state.context_rebuilds;
			context_identity = xl_level_trigger_identity();
			require(context_identity && xl_fog_colors(context_colors), "Cannot capture fog ownership before context recreation");
		} else if (!std::strcmp(operation, "context_verify")) {
			xl_fog_diagnostics state{};
			ogl_xl_fog_diagnostics(&state);
			float colors[4][4]{};
			require(context_identity == xl_level_trigger_identity() && xl_fog_colors(colors) && !std::memcmp(colors, context_colors, sizeof(colors)) && state.context_rebuilds > context_rebuilds,
			        "Fog context recreation lost authored data or reused old GPU resources");
		} else if (!std::strcmp(operation, "next")) {
			require(xl_source_level_version() == 27 && Current_level_num > 0 && Current_level_num < Last_level, "Fog controls require another level in the derived campaign");
			StartNewLevelSub(Current_level_num + 1, 1, 0);
		} else if (!std::strcmp(operation, "verify")) {
			require(xl_fog_active() && xl_source_level_version() == 27 && xl_source_game_version() == 40 && xl_level_trigger_identity(), "Loaded fog profile/identity missing");
			xl_fog_diagnostics state{};
			ogl_xl_fog_diagnostics(&state);
			require(state.volume_passes && state.composite_passes && state.boundaries && state.width > 0 && state.height > 0 && state.failures == 0, "Normal game did not render fog cleanly");
		} else if (!std::strcmp(operation, "stock_begin")) {
			require(!xl_fog_active(), "Fog ownership leaked into dry/stock level");
			ogl_xl_fog_diagnostics(&stock_state);
			stock_captured = true;
		} else if (!std::strcmp(operation, "stock")) {
			require(!xl_fog_active(), "Fog ownership leaked into dry/stock level");
			if (stock_captured) {
				xl_fog_diagnostics state{};
				ogl_xl_fog_diagnostics(&state);
				require(state.volume_passes == stock_state.volume_passes && state.composite_passes == stock_state.composite_passes, "Inactive fog submitted work in dry/stock frames");
				stock_captured = false;
			}
		} else throw std::runtime_error("Unknown fog control");
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
const char *xl_fog_probe_result()
{
	return report.c_str();
}
