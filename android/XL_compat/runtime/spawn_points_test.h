#include "spawn_points.h"
#include "spawn_points_fixture.h"
extern "C" {
#include "hudmsg.h"
}
static std::vector<xl_spawn_point> capture_spawns()
{
	std::vector<xl_spawn_point> result(xl_spawn_state_count());
	for (std::size_t slot = 0; slot < result.size(); ++slot) require(xl_spawn_state_get(static_cast<int>(slot), &result[slot]), "spawn capture failed");
	return result;
}
static bool same_spawns(const std::vector<xl_spawn_point> &a, const std::vector<xl_spawn_point> &b)
{
	return a.size() == b.size() && (a.empty() || !std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])));
}
static void spawn_points_lifecycle_test(const std::filesystem::path &root);
static int spawn_points_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		const auto position = ConsoleObject->pos, velocity = ConsoleObject->mtype.phys_info.velocity;
		const auto orientation = ConsoleObject->orient;
		std::ofstream samples(std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "spawn-samples.txt");
		int count = 0;
		for (int segment : { 1, 30, 60, 92 })
			for (int side = 0; side < 6; ++side)
				for (int links : { 1, 2, 3, 10 }) {
					trigger action{};
					action.type = 28;
					action.num_links = links;
					action.flags = TF_NO_MESSAGE | 32 | 64;
					samples << links << ' ';
					for (int link = 0; link < links; ++link) {
						action.seg[link] = (segment + link) % Num_segments;
						action.side[link] = (side + link) % 6;
						const auto geometry = xl_compat::compile_classic_lighting_side(action.seg[link], action.side[link]);
						for (int axis = 0; axis < 3; ++axis) samples << (std::int64_t(geometry.fixed_normals[0][axis]) + geometry.fixed_normals[1][axis]) / 2 << ' ';
						for (int value : geometry.segment_center) samples << value << ' ';
					}
					require(!xl_spawn_action(&action) && xl_spawn_state_count() == XL_SPAWN_SLOTS, "spawn source control rejected");
					const auto points = capture_spawns();
					for (int slot = 0; slot < XL_SPAWN_SLOTS; ++slot) {
						require(points[slot].segment == action.seg[slot % links] && points[slot].side == action.side[slot % links], "spawn links did not cycle across the XL table");
						for (int value : points[slot].position) samples << value << ' ';
						for (int value : points[slot].orientation) samples << value << ' ';
					}
					samples << '\n';
					++count;
				}
		require(!std::memcmp(&position, &ConsoleObject->pos, sizeof(position)) && !std::memcmp(&velocity, &ConsoleObject->mtype.phys_info.velocity, sizeof(velocity)) && !std::memcmp(&orientation, &ConsoleObject->orient, sizeof(orientation)), "spawn configuration teleported the current player");
		std::ofstream cues(std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "spawn-cue-samples.txt");
		for (int flags : { 0, 1, 32, 33, 64, 65, 96, 97 }) {
			trigger action{};
			action.type = 28;
			action.flags = flags;
			action.num_links = 1;
			action.seg[0] = 1;
			HUD_clear_messages();
			require(!xl_spawn_action(&action), "spawn cue control rejected");
			// The existing HUD API returns zero for a duplicate of the current message
			const bool shown = !HUD_init_message_literal(HM_DEFAULT, "New spawn points set!");
			cues << flags << ' ' << shown << '\n';
		}
		spawn_points_lifecycle_test(std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions");
		std::cout << "XL spawn-point source controls passed: " << count << " tables\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL spawn-point control failure: " << error.what() << '\n';
		return 1;
	}
}
