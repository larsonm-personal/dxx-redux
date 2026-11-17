#include "camera_wall_viewer.h"
static void camera_wall_viewer_controls(const std::filesystem::path &root)
{
	std::ofstream receipt(root.parent_path() / "camera-wall.txt");
	const int object_count = Highest_object_index;
	unsigned before, after;
	require(d_rand_get_state(&before), "Wall viewer RNG unavailable");
	for (int segment = 0; segment < Num_segments; ++segment)
		for (int side = 0; side < 6; ++side) {
			const auto geometry = xl_compat::compile_classic_lighting_side(segment, side);
			const auto viewer = xl_compat::camera_wall_viewer(segment, side);
			for (auto value : geometry.fixed_normals[0]) receipt << value << ' ';
			for (auto value : geometry.center) receipt << value << ' ';
			receipt << segment << ' ' << side << ' ' << viewer.pos.x << ' ' << viewer.pos.y << ' ' << viewer.pos.z << ' ';
			for (const auto &column : { viewer.orient.rvec, viewer.orient.uvec, viewer.orient.fvec }) receipt << column.x << ' ' << column.y << ' ' << column.z << ' ';
			receipt << viewer.segnum << ' ' << int(viewer.flags) << '\n';
		}
	require(bool(receipt), "Cannot write wall camera receipt");
	require(d_rand_get_state(&after) && before == after && Highest_object_index == object_count, "Wall viewers altered live object allocation or RNG");
	for (const auto &face : { std::pair<int, int>{ -1, 0 }, { Num_segments, 0 }, { 0, -1 }, { 0, 6 } }) {
		bool rejected = false;
		try {
			xl_compat::camera_wall_viewer(face.first, face.second);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected, "Wall viewer accepted invalid source face");
	}
}
static void camera_wall_trigger_controls(const std::filesystem::path &root)
{
	camera_wall_viewer_controls(root);
	std::ifstream original(root / "trigflag.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto level = xl_compat::inspect_level(member.bytes);
			const auto &section = *std::find_if(level.sections.begin(), level.sections.end(), [](const auto &s) { return s.name == "triggers"; });
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (i * 8));
			};
			const int flags[] = { 0, TF_ONE_SHOT, 16, 8, 128 | TF_DISABLED, 0, 0, 0 };
			const int times[] = { -1, -1, -1, 500, -1, -1000, 500, -1 };
			for (int i = 0; i < 8; ++i) {
				const auto base = section.offset + i * 52;
				patch(base, i == 7 ? 31 : 16, 1);
				patch(base + 1, flags[i], 1);
				patch(base + 8, times[i], 4);
				if (i == 7) {
					patch(base + 12, level.walls[6].segment, 2);
					patch(base + 32, level.walls[6].side, 2);
				}
			}
			require(xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes)).empty(), "camera wall fixture rejected admission");
			auto unsupported = member.bytes;
			unsupported.at(section.offset) = 20;
			require(!xl_compat::runtime_blockers(xl_compat::inspect_level(unsupported)).empty(), "camera admission accidentally enabled smoke actions");
		}
		char name[13]{};
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (int i = 0; i < 4; ++i) output.push_back(static_cast<unsigned char>(member.bytes.size() >> (i * 8)));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "camwall.hog").string().c_str(), output);
	{
		std::ofstream descriptor(root / "camwall.mn2");
		descriptor << "d2x-name = XL camera wall controls\nnum_levels = 1\nneptunxl.rl2\n";
	}
	char error[256]{};
	require(load_base_mission("camwall", error, sizeof(error)), error);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	require(xl_level_extensions_active() && Num_triggers == 8 && xl_object_trigger_count() == 10, "camera fixture lost wall or object bindings");
	const int player = ConsoleObject - Objects;
	const std::vector<wall> walls_before(Walls, Walls + Num_walls);
	const std::vector<object> objects_before(Objects, Objects + Highest_object_index + 1);
	for (int flags = 0; flags < 256; ++flags) {
		xl_wall_trigger_reset();
		Triggers[0].flags = flags;
		d_srand(71);
		unsigned before, after;
		require(d_rand_get_state(&before), "camera RNG unavailable");
		const bool disabled = (flags & TF_DISABLED) != 0;
		require(xl_wall_trigger(0, player, 1) == int(disabled), "camera dispatch result differs");
		require(Triggers[0].type == 16 && Triggers[0].flags == (flags | ((!disabled && (flags & TF_ONE_SHOT)) ? TF_DISABLED : 0)), "camera alternate or one-shot differs");
		xl_trigger_state state{};
		require(xl_wall_trigger_state(0, &state) && bool(state.operated) == !disabled, "camera operation state differs");
		require(d_rand_get_state(&after) && before == after, "camera immediate action consumed RNG");
		require(!std::memcmp(walls_before.data(), Walls, walls_before.size() * sizeof(wall)) &&
		            !std::memcmp(objects_before.data(), Objects, objects_before.size() * sizeof(object)),
		        "camera action mutated world or actor state");
	}
	xl_wall_trigger_reset();
	Triggers[0].flags = 0;
	require(xl_wall_trigger(0, 49, 1) == 1, "wall camera accepted a nonplayer");
	xl_trigger_state untouched{};
	require(xl_wall_trigger_state(0, &untouched) && !untouched.operated, "rejected camera actor changed state");
	Triggers[0].num_links = MAX_WALLS_PER_LINK + 1;
	require(xl_wall_trigger(0, player, 1) == 1, "camera accepted malformed link count");
	Triggers[0].num_links = 1;
	GameTime64 = i2f(10);
	require(xl_wall_trigger(1, player, 1) == 0 && xl_wall_trigger(1, player, 1) == 1, "camera one-shot repeated");
	require(xl_wall_trigger(2, player, 1) == 0 && Triggers[2].type == 16, "camera alternate changed action type");
	require(xl_wall_trigger(3, player, 1) == 1, "camera permanent delay did not arm");
	d_srand(71);
	require(xl_wall_trigger(5, player, 1) == 1 && xl_wall_trigger(7, player, 1) == 0, "camera random delay or master chain did not arm");
	GameTime64 += F1_0 / 32;
	std::array<xl_trigger_state, 8> pending{};
	for (int i = 0; i < 8; ++i) require(xl_wall_trigger_state(i, &pending[i]), "camera pending state absent");
	require(pending[3].delay_ms == 500 && pending[6].delay_ms == 500 && pending[5].delay_ms >= 100 && pending[5].delay_ms <= 1000, "camera selected delay invalid");
	char save[] = "xl-camera-wall.sav", description[] = "Camera wall state";
	stop_time();
	require(state_save_all_sub(save, description), "camera state save failed");
	require(load_base_mission("d2", error, sizeof(error)) && load_level("d2leva-1.rl2") == 0, "camera stock transition failed");
	require(xl_wall_trigger(0, ConsoleObject - Objects, 0) == -1, "camera policy leaked into stock");
	require(state_restore_all_sub(save, 0), "camera state restore failed");
	for (int i : { 3, 5, 6 }) {
		xl_trigger_state restored{};
		require(xl_wall_trigger_state(i, &restored) && restored.elapsed == pending[i].elapsed && restored.delay_ms == pending[i].delay_ms && restored.object == pending[i].object && restored.shot == pending[i].shot, "camera pending state changed across disk");
	}
	Triggers[5].flags |= TF_DISABLED;
	GameTime64 += F1_0 / 2 - pending[6].elapsed - 1;
	triggers_frame_process();
	require(!(Triggers[6].flags & TF_DISABLED), "restored camera fired before boundary");
	++GameTime64;
	triggers_frame_process();
	xl_trigger_state permanent{};
	require((Triggers[6].flags & TF_DISABLED) && Triggers[6].time == 0 && xl_wall_trigger_state(3, &permanent) && !permanent.operated && !(Triggers[3].flags & TF_DISABLED), "restored camera did not complete and rearm at boundary");
	xl_wall_trigger_reset();
	Triggers[6].flags = 0;
	Triggers[6].time = 500;
	require(xl_object_master_action(&Triggers[7], 49) == 0, "attached master did not schedule camera");
	xl_trigger_state attached{};
	require(xl_wall_trigger_state(6, &attached) && attached.operated && attached.attached && attached.object == 49, "camera lost original attached actor");
	obj_delete(49);
	GameTime64 += F1_0 / 2;
	triggers_frame_process();
	require((Triggers[6].flags & TF_DISABLED) && Triggers[6].time == 0, "camera stopped after attached actor removal");
	xl_wall_trigger_reset();
	Triggers[4].flags = 128;
	triggers_frame_process();
	require((Triggers[4].flags & TF_DISABLED) && xl_wall_trigger_state(4, &untouched) && untouched.operated, "camera autoplay did not run once");
	std::cout << "XL camera wall controls passed: 256 flag combinations, inert world/RNG, actor gates, chained delays, disk restore and removed actor continuation\n";
}
