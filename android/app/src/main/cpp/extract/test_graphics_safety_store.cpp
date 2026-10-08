#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <climits>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>
#include "graphics_safety_store.h"
#include "graphics_config_transaction.h"

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define make_dir(path)   _mkdir(path)
#define remove_dir(path) _rmdir(path)
#define current_pid      _getpid
#else
#include <sys/stat.h>
#include <unistd.h>
#define make_dir(path)   mkdir(path, 0700)
#define remove_dir(path) rmdir(path)
#define current_pid      getpid
#endif

static const char *root = "graphics_safety_test";
static void write(const char *leaf, const char *text)
{
	std::ofstream stream(std::string(root) + "/" + leaf);
	stream << text;
	stream.close();
	assert(stream);
}
static std::string read(const char *leaf)
{
	std::ifstream stream(std::string(root) + "/" + leaf);
	assert(stream);
	return std::string(std::istreambuf_iterator<char>(stream), {});
}
static graphics_safety_record record()
{
	graphics_safety_record result;
	assert(graphics_safety_read_record(root, &result));
	return result;
}
static bool accepted_is(const graphics_safety_snapshot &snapshot)
{
	const auto stored = record();
	return graphics_safety_equal(&stored.accepted, &snapshot) != 0;
}

int main()
{
	assert(make_dir(root) == 0);
	assert(make_dir("graphics_safety_test/d1x-redux") == 0);
	assert(make_dir("graphics_safety_test/d2x-redux") == 0);
	write("descent.cfg", "GammaLevel=7\nTexFilt=0\nMsaaLevel=0\n");
	write("d1x-redux/descent.cfg", "MusicVolume=3\n");
	write("d2x-redux/descent.cfg", "MusicVolume=5\n");

	auto bootstrap = record();
	assert(bootstrap.phase == GRAPHICS_SAFE_IDLE);
	assert(graphics_safety_all_off(&bootstrap.accepted));
	auto candidate = bootstrap.accepted;
	candidate.values[GRAPHICS_SAFE_TEXFILT] = 1;
	candidate.values[GRAPHICS_SAFE_MSAA] = 2;
	candidate.values[GRAPHICS_SAFE_WIDTH] = 1280;
	candidate.values[GRAPHICS_SAFE_HEIGHT] = 720;
	candidate.values[GRAPHICS_SAFE_COLOR_DEPTH] = 1;
	assert(graphics_safety_patch_snapshot(root, &candidate));
	graphics_safety_snapshot requested;
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(graphics_safety_equal(&requested, &candidate));
	/* A staged launcher edit survives a restart without being accepted */
	assert(graphics_safety_recover(root, current_pid()) == 1);
	assert(accepted_is(bootstrap.accepted));

	assert(graphics_safety_begin_attempt(root, &candidate, 1, current_pid(), 1));
	assert(graphics_safety_decide(root, 1, 1, 900, "premature_ok") == 0);
	assert(record().phase == GRAPHICS_SAFE_PREPARING);
	assert(graphics_safety_arm(root, 1, 1000));
	assert(!graphics_safety_arm(root, 1, 2000));
	assert(graphics_safety_decide(root, 99, 1, 2000, "stale") == 0);
	assert(graphics_safety_decide(root, 1, 1, 5999, "ok") == 1);
	auto accepted = record();
	assert(accepted.phase == GRAPHICS_SAFE_IDLE);
	assert(graphics_safety_equal(&accepted.accepted, &candidate));

	auto safe = candidate;
	safe.values[GRAPHICS_SAFE_TEXFILT] = 0;
	safe.values[GRAPHICS_SAFE_MSAA] = 0;
	assert(graphics_safety_begin_attempt(root, &safe, 2, current_pid(), 1));
	assert(record().phase == GRAPHICS_SAFE_IDLE);
	assert(accepted_is(candidate));
	assert(graphics_safety_begin_attempt(root, &candidate, 3, current_pid(), 1));
	assert(record().phase == GRAPHICS_SAFE_IDLE);

	auto trial = candidate;
	trial.values[GRAPHICS_SAFE_MSAA] = 4;
	trial.values[GRAPHICS_SAFE_WIDTH] = 1920;
	trial.values[GRAPHICS_SAFE_HEIGHT] = 1080;
	trial.values[GRAPHICS_SAFE_COLOR_DEPTH] = 0;
	assert(graphics_safety_patch_snapshot(root, &trial));
	assert(graphics_safety_begin_attempt(root, &trial, 4, current_pid(), 1));
	assert(graphics_safety_arm(root, 4, 2000));
	/* At deadline equality, timeout wins even when the event says OK */
	assert(graphics_safety_decide(root, 4, 1, 7000, "timeout") == 2);
	assert(record().phase == GRAPHICS_SAFE_RESTORING);
	assert(graphics_safety_decide(root, 4, 1, 6000, "late_ok") == 2);
	assert(graphics_safety_complete_restore(root, 4));
	assert(graphics_safety_read_requested(root, "d1", &requested));
	assert(graphics_safety_equal(&requested, &candidate));
	assert(read("descent.cfg").find("GammaLevel=7\n") != std::string::npos);
	assert(read("d1x-redux/descent.cfg").find("MusicVolume=3\n") != std::string::npos);
	assert(read("d2x-redux/descent.cfg").find("MusicVolume=5\n") != std::string::npos);

	/* A failed batch publish leaves all targets unchanged and a durable repair marker */
	assert(graphics_safety_patch_snapshot(root, &trial));
	assert(graphics_safety_begin_attempt(root, &trial, 5, current_pid(), 1));
	assert(graphics_safety_arm(root, 5, 3000));
	const auto previous = read("graphics_safety.json");
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, 0);
	assert(graphics_safety_decide(root, 5, 1, 3100, "ok") == -1);
	assert(read("graphics_safety.json") == previous);
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, 1);
	assert(graphics_safety_decide(root, 5, 0, 3100, "cancel") == -1);
	assert(record().phase == GRAPHICS_SAFE_RESTORING);
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(graphics_safety_equal(&requested, &trial));
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_NONE, 0);
	assert(graphics_safety_complete_restore(root, 5));
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(graphics_safety_equal(&requested, &candidate));

	/* An abandoned pre-level attempt restores accepted before renderer initialization */
	assert(graphics_safety_patch_snapshot(root, &trial));
	assert(graphics_safety_begin_attempt(root, &trial, 6, INT_MAX, 0));
	assert(graphics_safety_recover(root, current_pid()) == 2);
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(graphics_safety_equal(&requested, &candidate));
	/* A clean exit from native menus keeps the edited values staged for next level */
	assert(graphics_safety_patch_snapshot(root, &trial));
	assert(graphics_safety_begin_attempt(root, &trial, 7, current_pid(), 0));
	assert(graphics_safety_clean_exit(root, current_pid()));
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(graphics_safety_equal(&requested, &trial));
	assert(accepted_is(candidate));

	/* A racing OK and timeout still produces only one durable outcome */
	assert(graphics_safety_begin_attempt(root, &trial, 8, current_pid(), 1));
	assert(graphics_safety_arm(root, 8, 4000));
	int ok_result = 0, timeout_result = 0;
	std::thread ok([&] { ok_result = graphics_safety_decide(root, 8, 1, 8999, "ok"); });
	std::thread timeout([&] { timeout_result = graphics_safety_decide(root, 8, 0, 9000, "timeout"); });
	ok.join();
	timeout.join();
	assert((ok_result == 1 && timeout_result == 0) || (ok_result == 2 && timeout_result == 2));
	if (record().phase == GRAPHICS_SAFE_RESTORING) assert(graphics_safety_complete_restore(root, 8));

	/* Deferred edits merge into the accepted rollback target, not the rejected candidate */
	const auto baseline = record().accepted;
	auto rejected = baseline;
	rejected.values[GRAPHICS_SAFE_TEXFILT] = 2;
	assert(graphics_safety_patch_snapshot(root, &rejected));
	assert(graphics_safety_begin_attempt(root, &rejected, 90, current_pid(), 1));
	assert(record().owner_session != 0);
	assert(graphics_safety_arm(root, 90, 10000));
	graphics_config_update deferred[] = { { "ResolutionX", 800 }, { "ResolutionY", 600 }, { "ColorDepth", 1 } };
	assert(graphics_safety_stage(root, deferred, 3) == 2);
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(graphics_safety_equal(&requested, &rejected));
	assert(graphics_safety_read_staged(root, "d2", &requested));
	assert(requested.values[GRAPHICS_SAFE_WIDTH] == 800 && requested.values[GRAPHICS_SAFE_HEIGHT] == 600);
	assert(graphics_safety_recover(root, INT_MAX) == 1);
	assert(record().phase == GRAPHICS_SAFE_CHALLENGE);
	assert(graphics_safety_decide(root, 90, 0, 11000, "cancel") == 2);
	assert(graphics_safety_recover(root, INT_MAX) == 1);
	assert(record().phase == GRAPHICS_SAFE_RESTORING);
	assert(graphics_safety_complete_restore(root, 90));
	assert(graphics_safety_flush_staged(root) == 2);
	assert(graphics_safety_read_requested(root, "d1", &requested));
	assert(requested.values[GRAPHICS_SAFE_WIDTH] == 800 && requested.values[GRAPHICS_SAFE_HEIGHT] == 600);
	assert(requested.values[GRAPHICS_SAFE_COLOR_DEPTH] == 1);
	assert(requested.values[GRAPHICS_SAFE_TEXFILT] == baseline.values[GRAPHICS_SAFE_TEXFILT]);
	assert(!record().pending_mask && accepted_is(baseline));

	/* A reused PID does not make an abandoned attempt belong to the new process */
	assert(graphics_safety_begin_attempt(root, &rejected, 91, current_pid(), 0));
	auto reused = nlohmann::json::parse(read("graphics_safety.json"));
	reused["attempt"]["owner_session"] = record().owner_session + 1;
	write("graphics_safety.json", reused.dump(2).c_str());
	assert(graphics_safety_recover(root, current_pid()) == 2);
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(graphics_safety_equal(&requested, &baseline));

	/* A failed staged batch stays journaled and repairs all mirrored files on retry */
	graphics_config_update staged[] = { { "TexFilt", 0 } };
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_REPLACE, 1);
	assert(graphics_safety_stage(root, staged, 1) == 0);
	assert(record().pending_mask != 0);
	graphics_config_transaction_set_test_failure(GRAPHICS_CONFIG_TRANSACTION_FAIL_NONE, 0);
	assert(graphics_safety_flush_staged(root) == 2);
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(requested.values[GRAPHICS_SAFE_TEXFILT] == 0 && accepted_is(baseline));
	graphics_config_update invalid_mode[] = { { "ResolutionX", 1 } };
	assert(!graphics_safety_stage(root, invalid_mode, 1));
	assert(!record().pending_mask);
	assert(read("descent.cfg").find("GammaLevel=7\n") != std::string::npos);

	/* Live editing owns a recoverable attempt even when initially equal or all-off */
	assert(graphics_safety_preview(root, &baseline, 101, current_pid(), 0));
	assert(record().phase == GRAPHICS_SAFE_PREVIEW && accepted_is(baseline));
	assert(!graphics_safety_begin_attempt(root, &rejected, 102, current_pid(), 1));
	assert(!graphics_safety_preview(root, &rejected, 102, current_pid(), 0));
	assert(graphics_safety_preview(root, &rejected, 101, current_pid(), 0));
	assert(accepted_is(baseline));
	assert(graphics_safety_preview(root, &baseline, 101, current_pid(), 0));
	assert(record().phase == GRAPHICS_SAFE_PREVIEW);
	assert(graphics_safety_decide(root, 101, 1, 0, "early_ok") == 0);
	assert(graphics_safety_stage(root, staged, 1) == 2);
	assert(graphics_safety_clean_exit(root, current_pid()));
	assert(graphics_safety_read_requested(root, "d2", &requested));
	assert(graphics_safety_equal(&requested, &baseline));
	assert(record().phase == GRAPHICS_SAFE_IDLE && accepted_is(baseline));
	assert(graphics_safety_flush_staged(root) == 2);

	assert(graphics_safety_preview(root, &rejected, 103, current_pid(), 0));
	assert(graphics_safety_preview(root, &rejected, 103, current_pid(), 1));
	assert(record().phase == GRAPHICS_SAFE_PREPARING);
	assert(!graphics_safety_preview(root, &baseline, 103, current_pid(), 0));
	assert(graphics_safety_arm(root, 103, 10000));
	assert(graphics_safety_decide(root, 103, 0, 11000, "cancel_preview") == 2);
	assert(graphics_safety_complete_restore(root, 103));
	assert(accepted_is(baseline));

	assert(graphics_safety_preview(root, &rejected, 105, current_pid(), 0));
	assert(!graphics_safety_finish_safe_preview(root, 105, current_pid()));
	assert(graphics_safety_preview(root, &baseline, 105, current_pid(), 0));
	assert(!graphics_safety_finish_safe_preview(root, 106, current_pid()));
	assert(graphics_safety_finish_safe_preview(root, 105, current_pid()));
	assert(record().phase == GRAPHICS_SAFE_IDLE && accepted_is(baseline));
	auto all_off = baseline;
	for (int i = 0; i < 5; ++i) all_off.values[i] = 0;
	assert(graphics_safety_preview(root, &all_off, 106, current_pid(), 0));
	assert(graphics_safety_finish_safe_preview(root, 106, current_pid()));
	assert(record().phase == GRAPHICS_SAFE_IDLE && accepted_is(baseline));

	/* An abandoned editor rolls back just like an abandoned confirmation */
	assert(graphics_safety_preview(root, &rejected, 104, current_pid(), 0));
	auto abandoned_preview = nlohmann::json::parse(read("graphics_safety.json"));
	abandoned_preview["attempt"]["owner_session"] = record().owner_session + 1;
	write("graphics_safety.json", abandoned_preview.dump(2).c_str());
	assert(graphics_safety_recover(root, current_pid()) == 2);
	assert(accepted_is(baseline) && record().phase == GRAPHICS_SAFE_IDLE);

	/* Corruption is an error, not permission to silently replace the accepted tuple */
	write("graphics_safety.json", "{\"accepted\":{},\"attempt\":null,\"pending\":{}}\n");
	graphics_safety_record invalid;
	assert(!graphics_safety_read_record(root, &invalid));
	assert(!graphics_safety_recover(root, current_pid()));

	for (const char *leaf : { "descent.cfg", "d1x-redux/descent.cfg", "d2x-redux/descent.cfg",
	                          "graphics_safety.json", ".graphics_safety.lock" })
		assert(std::remove((std::string(root) + "/" + leaf).c_str()) == 0);
	assert(remove_dir("graphics_safety_test/d1x-redux") == 0);
	assert(remove_dir("graphics_safety_test/d2x-redux") == 0);
	assert(remove_dir(root) == 0);
	std::puts("graphics safety persistence and recovery tests passed");
}
