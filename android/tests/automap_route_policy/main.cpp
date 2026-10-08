#include <cstdio>
#include <cstdlib>
#include <string>
#include <type_traits>
#include <nlohmann/json.hpp>
extern "C" {
#include "game.h"
#include "timer.h"
#include "secretarea.h"
#include "automap_metadata_overlay.h"
#include "android_route_metadata.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
}

static int recording, replay, progress, readiness, loads;
static fix64 clock_value;
static unsigned int revision;
static std::string queries;
extern "C" {
int Game_mode;
int input_demo_recorder_is_active(void)
{
	queries += 'C';
	return recording;
}
int input_demo_replay_is_loaded(void)
{
	queries += 'P';
	return replay;
}
fix64 timer_query(void)
{
	queries += 'T';
	return clock_value;
}
int android_route_metadata_get_progress_state(void)
{
	queries += 'S';
	return progress;
}
int level_metadata_get_route_readiness(void)
{
	queries += 'D';
	return readiness;
}
unsigned int level_metadata_get_route_revision(void)
{
	queries += 'V';
	return revision;
}
int level_metadata_try_load_pending_cache(void)
{
	queries += 'L';
	++loads;
	++revision;
	return 1;
}
}

static void require(bool value, const char *message)
{
	if (!value) {
		std::fprintf(stderr, "FAIL: %s queries=%s\n", message, queries.c_str());
		std::exit(1);
	}
}

// The baseline adapter reproduces the old native call; the new entry owns policy
template <typename Update>
static void update_route(Update update)
{
	if constexpr (std::is_invocable_v<Update>) {
		update();
	} else {
		int allow = !input_demo_recorder_is_active() && !input_demo_replay_is_loaded();
#ifdef NETWORK
		if (Game_mode & GM_MULTI) allow = 0;
#endif
		update(0, allow);
	}
}

int main(int argc, char **argv)
{
	if (argc != 2) return 1;
	nlohmann::json results = nlohmann::json::array();
	auto check = [&](int state, int step, bool allowed, bool load, bool refresh) {
		queries.clear();
		const int before_loads = loads;
		const unsigned int before_refresh = automap_metadata_get_route_refresh_count();
		update_route(automap_metadata_update_route);
		const std::string prefix = recording ? "CTSD" : "CPTSD";
		const std::string expected = prefix + (allowed ? (load ? "LV" : "V") : "");
		require(queries == expected, "eligibility/query evaluation order");
		require(loads - before_loads == int(load), "cache adoption cadence and blocking");
		require(automap_metadata_get_route_refresh_count() - before_refresh == unsigned(refresh), "displayed revision accounting");
		results.push_back({ { "state", state }, { "step", step }, { "queries", queries }, { "loads", loads - before_loads }, { "refreshes", automap_metadata_get_route_refresh_count() - before_refresh } });
	};
	for (int state = 0; state < 8; ++state) {
		recording = state & 1;
		replay = (state >> 1) & 1;
		Game_mode = state & 4 ? GM_NETWORK : GM_NORMAL;
		bool allowed = !recording && !replay;
#ifdef NETWORK
		allowed = allowed && !(Game_mode & GM_MULTI);
#endif
		progress = ANDROID_ROUTE_METADATA_USEFUL;
		readiness = LEVEL_METADATA_READINESS_NEXT_READY;
		revision = 10;
		loads = 0;
		automap_metadata_begin();
		clock_value = F1_0;
		check(state, 0, allowed, allowed, allowed);
		clock_value += F1_0 / 2;
		check(state, 1, allowed, false, false);
		clock_value = 2 * F1_0;
		check(state, 2, allowed, allowed, allowed);
		clock_value = F1_0;
		check(state, 3, allowed, allowed, allowed);
		progress = ANDROID_ROUTE_METADATA_CALCULATING;
		++revision;
		check(state, 4, allowed, false, allowed);
		recording = replay = Game_mode = 0;
		progress = ANDROID_ROUTE_METADATA_COMPLETE;
		clock_value = 3 * F1_0;
		check(state, 5, true, true, true);
	}
	for (progress = 0; progress < 4; ++progress)
		for (readiness = 0; readiness < 5; ++readiness) {
			recording = replay = Game_mode = 0;
			automap_metadata_begin();
			++revision;
			clock_value = F1_0;
			const bool load = (progress == ANDROID_ROUTE_METADATA_USEFUL || progress == ANDROID_ROUTE_METADATA_COMPLETE) && readiness != LEVEL_METADATA_READINESS_COMPLETE && readiness != LEVEL_METADATA_READINESS_FAILED;
			check(8 + progress, readiness, true, load, true);
		}
	require(results.size() == 68, "complete route policy matrix");
	FILE *file = std::fopen(argv[1], "wb");
	require(file, "open trace");
	const std::string output = results.dump(2) + "\n";
	require(std::fwrite(output.data(), 1, output.size(), file) == output.size(), "write trace");
	std::fclose(file);
	return 0;
}
