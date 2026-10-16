#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using Uint32 = unsigned;
static void test_log(const char *, ...) {}
#define LOGI(...) test_log(__VA_ARGS__)
#define LOGE(...) test_log(__VA_ARGS__)
#include "automation_types.inc"
#include "automation_request.inc"

static std::vector<auto_step> g_steps;
static int g_active, g_failed, g_current_step, g_key_phase, g_select_phase, g_select_delta;
static int g_skip_briefing_last_game_wind_seen, g_inject_axis_logged;
static Uint32 g_step_start, g_script_start, g_repeat_start;
static void *g_skip_briefing_last_front;
static int g_axis_probe;
static FILE *g_log_fp;
static char g_active_script[512], g_active_run_id[80], g_automate_dir[512] = ".";
static automation_load_request g_pending_load;
static std::mutex g_pending_load_mutex;
static std::atomic<bool> g_load_requested{false};
static Uint32 SDL_GetTicks() { return 100; }
static void clear_held_axes() {}
static void open_log_file() {}
static void log_append(const char *, const char *, const char *) {}
static const char *step_type_name(step_type) { return "test"; }
#include "automation_parser.inc"
#include "automation_result.inc"
#include "automation_stale.inc"
#include "automation_terminal.inc"

static void process_pending_load()
{
#include "automation_load.inc"
}

static void request(const char *text, int start_step = 0)
{
	std::ofstream("automation-input.json") << text;
	std::strcpy(g_pending_load.script_path, "automation-input.json");
	std::strcpy(g_pending_load.run_id, "new-run");
	g_pending_load.start_step = start_step;
	g_load_requested = true;
	process_pending_load();
}

static json result()
{
	std::ifstream input("automation_result.json");
	assert(input.good());
	return json::parse(input);
}

int main()
{
	// An ordinary tick without an automation request must not publish or reset anything
	std::remove("automation_result.json");
	assert(!g_load_requested && !g_active);
	process_pending_load();
	assert(!g_active && !g_failed && g_steps.empty());
	assert(!std::ifstream("automation_result.json").good());

	for (const char *invalid : {
			 R"([{"action":"log"},{"action":"assert_typo"}])",
			 R"([{"action":"log"},{}])",
			 R"([{"action":"log"},null])",
			 R"([{"action":"log"},{"action":5}])",
			 "[{", "{}", "[]", R"([{"_info":{}}])"}) {
		// Begin with a still-active prior script and a stale successful result
		request(R"([{"action":"log"},{"action":"wait_ms","ms":1000}])");
		assert(g_active && g_steps.size() == 2);
		write_result_file("PASS", nullptr);
		request(invalid);
		assert(!g_active && g_failed && g_steps.empty());
		const auto failed = result();
		assert(failed.at("result") == "FAIL" && failed.at("run_id") == "new-run");
		assert(failed.at("total_steps") == 0 && failed.at("steps_completed") == 0);
		advance_step();
		assert(result().at("result") == "FAIL");
	}
	request(R"([{"action":"log"}])");
	std::strcpy(g_pending_load.script_path, "missing-automation-input.json");
	g_load_requested = true;
	process_pending_load();
	assert(!g_active && g_failed && g_steps.empty() && result().at("result") == "FAIL");

	request(R"([{"_info":{}},{"action":"enter_launcher"},{"action":"assert_button","text":"Play"},{"action":"enter_game"},{"action":"log"}])", 3);
	assert(g_active && !g_failed && g_steps.size() == 4 && g_current_step == 3);
	assert(g_steps[1].type == STEP_ASSERT_BUTTON && g_steps[2].type == STEP_ENTER_GAME);
	advance_step();
	const auto passed = result();
	assert(passed.at("result") == "PASS" && passed.at("total_steps") == 4 && passed.at("steps_completed") == 4);
	std::remove("automation-input.json");
	std::remove("automation_result.json");
	std::puts("PASS: automation input rejects malformed replacements and preserves handoff indices");
}
