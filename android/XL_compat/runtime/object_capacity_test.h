#include "object_pool.h"
extern "C" int Debris_object_count;
#include "../tests/object_capacity_fixture.h"

static void object_capacity_fixture(const std::filesystem::path &root, unsigned count)
{
	std::ifstream input(root / "neptunxl.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(original)) {
		if (member.name == "neptunxl.rl2") member.bytes = xl_test::expand_objects(member.bytes, count);
		char name[13]{};
		require(member.name.size() < sizeof(name), "Object fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(uint8_t(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "objcap.hog").string().c_str(), output);
	std::ofstream descriptor(root / "objcap.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL objects\nnum_levels = 1\nneptunxl.rl2\n";
}

static void object_reclaim_samples(const std::filesystem::path &root)
{
	std::ofstream output(root / "object-reclaim.txt");
	unsigned random = 0x23845019;
	const auto next = [&] { random = random * 1664525u + 1013904223u; return random; };
	for (int sample = 0; sample < 2048; ++sample) {
		Highest_object_index = 63;
		const int requested = int(next() % 64);
		int occupied = 0;
		output << 64 << ' ' << requested;
		for (int i = 0; i < 64; ++i) {
			auto &object = Objects[i];
			object = {};
			const int types[] = { OBJ_NONE, OBJ_PLAYER, OBJ_ROBOT, OBJ_LIGHT, OBJ_DEBRIS, OBJ_FIREBALL, OBJ_WEAPON, OBJ_POWERUP };
			object.type = static_cast<object_type_t>(types[(next() >> 16) % 8]);
			object.id = (next() >> 16) & 1 ? FLARE_ID : FUSION_ID;
			object.flags = (next() >> 16) % 5 ? 0 : OF_SHOULD_BE_DEAD;
			object.ctype.expl_info.delete_objnum = (next() >> 16) & 1 ? -1 : 1;
			if (object.type != OBJ_NONE && !(object.flags & OF_SHOULD_BE_DEAD)) ++occupied;
			output << ' ' << int(object.type) << ' ' << int(object.id) << ' ' << int(object.flags) << ' ' << object.ctype.expl_info.delete_objnum;
		}
		xl_object_reclaim(requested);
		output << ' ' << occupied;
		for (int i = 0; i < 64; ++i) output << ' ' << int(Objects[i].flags);
		output << '\n';
	}
}

static void object_signature_samples(const std::filesystem::path &root)
{
	std::ofstream output(root / "object-signatures.txt");
	for (int initial : { 32760, 65530, 2147483627 }) {
		object_runtime_state state{};
		object_get_runtime_state(&state);
		state.signature_seed = initial;
		require(object_validate_runtime_state(&state), "Wide XL allocator seed rejected");
		object_set_runtime_state(&state);
		for (int i = 0; i < 19; ++i) {
			const int actual = obj_get_signature();
			output << initial + i << ' ' << actual << '\n';
			require(actual == initial + i + 1, "XL lifetime sequence narrowed before allocation");
		}
	}
	object_runtime_state limit{};
	object_get_runtime_state(&limit);
	limit.signature_seed = 2147483647;
	object_set_runtime_state(&limit);
	int expected = 0;
	for (;;) {
		bool used = false;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE && Objects[i].signature == expected) used = true;
		if (!used) break;
		++expected;
	}
	require(obj_get_signature() == expected, "XL signature overflow did not skip live identities");
	std::cout << "Wide lifetime sequence and defined 32-bit wrap controls passed\n";
}

static void object_pool_allocator_contract()
{
	const int capacity = object_capacity();
	std::vector<int> added;
	while (num_objects < capacity) {
		const int owner = obj_create(OBJ_LIGHT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
		require(owner >= 0, "Cannot fill active object pool");
		added.push_back(owner);
	}
	require(obj_allocate() == -1, "Object allocator exceeded active limit");
	object_runtime_state state{};
	object_get_runtime_state(&state);
	require(object_validate_runtime_state(&state), "Full object allocator state invalid");
	for (int slot : { 999, capacity - 1 }) {
		if (Objects[slot].type == OBJ_NONE || slot == ConsoleObject - Objects) continue;
		const auto signature = Objects[slot].signature;
		obj_delete(slot);
		const int replacement = obj_create(OBJ_LIGHT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
		require(replacement == slot && Objects[replacement].signature != signature, "Full-pool slot reuse lost identity");
	}
	object_get_runtime_state(&state);
	require(object_validate_runtime_state(&state), "Reused object allocator state invalid");
	state.highest_object_index = capacity;
	require(!object_validate_runtime_state(&state), "Over-capacity allocator accepted");
}

static void object_capacity_pressure()
{
	require(num_objects == object_capacity(), "Pressure control requires full pool");
	for (int owner = 32758; owner < 32768; ++owner) {
		auto &object = Objects[owner];
		object.type = OBJ_DEBRIS;
		++Debris_object_count;
	}
	const int made = obj_create(OBJ_LIGHT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
	require(made >= 32758 && num_objects == 32759, "XL allocation pressure did not reclaim and allocate");
	for (int owner = 32758; owner < 32768; ++owner)
		if (owner != made) require(Objects[owner].type == OBJ_NONE, "Reclaimed object remained linked/live");
	object_runtime_state state{};
	object_get_runtime_state(&state);
	require(object_validate_runtime_state(&state), "Reclaimed allocator invalid");
	const auto valid = state;
	for (int variant = 0; variant < 4; ++variant) {
		state = valid;
		if (variant == 0) state.free_obj_list[state.num_objects] = -1;
		if (variant == 1) state.free_obj_list[state.num_objects] = 32768;
		if (variant == 2) state.free_obj_list[state.num_objects] = state.free_obj_list[state.num_objects + 1];
		if (variant == 3) state.free_obj_list[state.num_objects] = made;
		require(!object_validate_runtime_state(&state), "Invalid high free-list identity accepted");
	}
	object_get_runtime_state(&state);
	require(!std::memcmp(&state, &valid, sizeof(state)), "Allocator validation changed live state");
	std::cout << "Actual XL pressure cleanup and malformed high free-list controls passed\n";
}

static void object_capacity_ai_and_event(int robot)
{
	auto &actor = Objects[robot];
	obj_relink(robot, 60);
	compute_segment_center(&actor.pos, &Segments[60]);
	obj_relink(ConsoleObject - Objects, 60);
	ConsoleObject->pos = actor.pos;
	actor.pos.z -= i2f(10);
	ConsoleObject->pos.z += i2f(10);
	actor.orient = vmd_identity_matrix;
	actor.mtype.phys_info.velocity = {};
	actor.ctype.ai_info.CURRENT_STATE = actor.ctype.ai_info.GOAL_STATE = AIS_FIRE;
	auto &local = Ai_local_info[robot];
	local.mode = AIM_CHASE_OBJECT;
	local.next_fire = 0;
	local.player_awareness_type = PA_PLAYER_COLLISION;
	local.player_awareness_time = i2f(10);
	Game_suspended = 0;
	FrameTime = F1_0 / 60;
	int shot = -1;
	for (int frame = 0; frame < 600 && shot < 0; ++frame) {
		GameTime64 += FrameTime;
		++d_tick_count;
		Believed_player_pos = ConsoleObject->pos;
		do_ai_frame_all();
		do_ai_frame(&actor);
		do_physics_sim(&actor);
		for (int owner = 1000; owner <= Highest_object_index; ++owner)
			if (Objects[owner].type == OBJ_WEAPON && Objects[owner].ctype.laser_info.parent_num == robot && Objects[owner].ctype.laser_info.parent_signature == actor.signature) {
				shot = owner;
				break;
			}
	}
	require(shot >= 1000 && local.next_fire > 0, "High robot AI did not fire with retained owner identity");
	const auto shields = Players[Player_num].shields;
	collide_player_and_weapon(ConsoleObject, &Objects[shot], &ConsoleObject->pos);
	require(Players[Player_num].shields < shields, "High AI projectile did not damage player");
	xl_compat::Inventory events;
	xl_compat::ObjectTrigger event{};
	event.object = robot;
	event.action.type = TT_OPEN_WALL;
	event.action.flags = TF_ONE_SHOT;
	event.action.targets = { { Walls[0].segnum, Walls[0].sidenum } };
	events.object_trigger_records.push_back(event);
	xl_object_trigger_load(events);
	Walls[0].type = WALL_CLOSED;
	actor.flags |= OF_SHOULD_BE_DEAD;
	obj_delete_all_that_should_be_dead();
	xl_object_trigger_state consumed{};
	for (int frame = 0; frame < 130 && Walls[0].type != WALL_OPEN; ++frame) {
		FrameTime = F1_0 / 64;
		wall_frame_process();
	}
	require(Objects[robot].type == OBJ_NONE && Walls[0].type == WALL_OPEN && xl_object_trigger_state_get(0, &consumed) && consumed.owner == -1 && consumed.operated, "High owner removal did not execute and consume object event");
	xl_object_trigger_clear();
	std::cout << "High-ID robot AI firing, player damage and ordinary removal event passed\n";
}

static void object_capacity_admission(const std::filesystem::path &root)
{
	PHYSFS_file *file = PHYSFS_openRead("neptunxl.rl2");
	require(file, "Cannot open high object source");
	xl_compat::Bytes original(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, original.data(), original.size()) == PHYSFS_sint64(original.size()) && PHYSFS_close(file), "Cannot read high object source");
	const auto source = xl_compat::inspect_level(original);
	const auto shadow = root / "object-shadow";
	std::filesystem::create_directories(shadow);
	require(PHYSFS_addToSearchPath(shadow.string().c_str(), 0), "Cannot mount object admission controls");
	const int highest = Highest_object_index, count = num_objects;
	for (int variant = 0; variant < 3; ++variant) {
		auto bad = original;
		if (variant == 0) bad = xl_test::expand_objects(original, 16385);
		if (variant == 1) bad.resize(source.objects.back().source_end - 1);
		if (variant == 2) {
			const auto at = source.objects.back().source_offset + (source.game_version == 40 ? 7 : 6);
			bad.at(at) = bad.at(at + 1) = 255;
		}
		write_bytes((shadow / "neptunxl.rl2").string().c_str(), bad);
		const char *owner = PHYSFS_getRealDir("neptunxl.rl2");
		require(owner && std::filesystem::weakly_canonical(owner) == std::filesystem::weakly_canonical(shadow), "Malformed object control does not own the resolved level");
		require(!xl_level_supported("neptunxl.rl2", 0), "Invalid high object source admitted");
		if (!variant) require(std::strstr(xl_level_rejection(), "supported signed-index pool is 32768"), "Object admission error lacks limit");
		require(Highest_object_index == highest && num_objects == count && object_capacity() == 32768, "Read-only object admission changed live world");
	}
	require(PHYSFS_removeFromSearchPath(shadow.string().c_str()) && xl_level_supported("neptunxl.rl2", 0), "Object admission did not recover");
	const auto probe = xl_compat::prepare_gameplay_probe(original);
	write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / "objraw.rl2").string().c_str(), probe.stock_level);
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)), error);
	require(load_level("objraw.rl2") != 0 && object_capacity() == 1000, "Unowned object adapter bypassed classic capacity");
	require(load_base_mission("objcap", error, sizeof(error)) && load_level("neptunxl.rl2") == 0 && object_capacity() == 32768, "Valid XL load failed after object rejections");
	std::cout << "Over-capacity, truncated, invalid and unowned object sources rejected with recovery\n";
}

extern "C" {
void nd_write_object(object *);
void nd_read_object(object *, int);
void newdemo_record_start_demo(void);
void newdemo_write_end(void);
}

static void object_capacity_demo()
{
	char error[256]{};
	require(load_base_mission("neptunxl", error, sizeof(error)), error);
	StartNewGame(1);
	const int owner = projectile_robot();
	const auto read = [](const char *name) {
		auto *file = PHYSFSX_openReadBuffered(name);
		require(file, "Cannot open identity demo");
		xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
		require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) && PHYSFS_close(file), "Cannot read identity demo");
		return bytes;
	};
	const int signatures[] = { 0, 32767, 32768, 65535, 65536, 2147483647 };
	xl_compat::Bytes wide_record;
	for (int signature : signatures) {
		Objects[owner].signature = signature;
		outfile = PHYSFSX_openWriteBuffered("identity-object.bin");
		require(outfile, "Cannot write identity object");
		nd_write_object(&Objects[owner]);
		const uint8_t eof = 0;
		require(PHYSFS_writeBytes(outfile, &eof, 1) == 1, "Cannot frame identity object");
		require(PHYSFS_close(outfile), "Cannot close identity object");
		outfile = nullptr;
		const auto bytes = read("identity-object.bin");
		require((bytes.front() == 255) == (signature > 32767), "Demo wrapper changed classic signature encoding");
		if (signature == 32768) wide_record.assign(bytes.begin(), bytes.end() - 1);
		object decoded{};
		decoded.signature = -999;
		infile = PHYSFSX_openReadBuffered("identity-object.bin");
		require(infile, "Cannot decode identity object");
		nd_read_object(&decoded, 0);
		std::cout << "Object codec signature=" << signature << " decoded=" << decoded.signature << " type=" << int(decoded.type) << " consumed=" << PHYSFS_tell(infile) << "/" << bytes.size() << "\n";
		require(decoded.signature == signature && decoded.type == OBJ_ROBOT && PHYSFS_tell(infile) == PHYSFS_sint64(bytes.size() - 1) && PHYSFS_close(infile), "Demo object lost wide signature or framing");
		infile = nullptr;
		outfile = PHYSFSX_openWriteBuffered("identity-rewrite.bin");
		require(outfile, "Cannot rewrite identity object");
		nd_write_object(&decoded);
		require(PHYSFS_writeBytes(outfile, &eof, 1) == 1, "Cannot frame rewritten identity object");
		require(PHYSFS_close(outfile), "Cannot close rewritten identity object");
		outfile = nullptr;
		require(read("identity-rewrite.bin") == bytes, "Demo identity rewrite changed bytes");
	}
	outfile = PHYSFSX_openWriteBuffered("identity.dem");
	require(outfile, "Cannot record identity demo");
	Newdemo_state = ND_STATE_RECORDING;
	newdemo_record_start_demo();
	for (int signature : signatures) {
		Objects[owner].signature = signature;
		newdemo_record_start_frame(F1_0 / 10);
		newdemo_record_viewer_object(ConsoleObject);
		newdemo_record_render_object(&Objects[owner]);
	}
	newdemo_record_start_frame(F1_0 / 10);
	newdemo_record_viewer_object(ConsoleObject);
	newdemo_write_end();
	require(PHYSFS_close(outfile), "Cannot close complete identity demo");
	outfile = nullptr;
	Newdemo_state = ND_STATE_NORMAL;
	const auto bytes = read("identity.dem");
	const auto directory = std::filesystem::path(PHYSFS_getWriteDir());
	const auto input = (directory / "identity.dem").string();
	const auto output = (directory / "identity.jsonl").string();
	require(newdemo_dump_json(input.c_str(), output.c_str(), error, sizeof(error)), error);
	std::set<int> decoded;
	std::ifstream report(output);
	std::string line;
	while (std::getline(report, line)) {
		const auto row = nlohmann::json::parse(line);
		if (row.contains("objects"))
			for (const auto &object : row["objects"])
				if (object["obj_type"] == OBJ_ROBOT) decoded.insert(object["sig"].get<int>());
	}
	report.close();
	for (int signature : signatures) require(decoded.count(signature) == 1, "Complete frame decoder lost a wide robot signature");
	const auto at = std::search(bytes.begin(), bytes.end(), wide_record.begin(), wide_record.end());
	require(at != bytes.end() && std::search(at + 1, bytes.end(), wide_record.begin(), wide_record.end()) == bytes.end(), "Cannot locate unique wide demo object");
	const auto offset = static_cast<std::size_t>(at - bytes.begin());
	const auto valid_report = read("identity.jsonl");
	for (int variant = 0; variant < 4; ++variant) {
		auto bad = bytes;
		if (variant == 0) bad.resize(offset + 7);
		else if (variant == 1)
			for (unsigned i = 0; i < 4; ++i) bad[offset + 5 + i] = 255;
		else if (variant == 2) {
			bad[offset + 5] = 1;
			bad[offset + 6] = bad[offset + 7] = bad[offset + 8] = 0;
		} else bad[offset + 1] = RT_NONE;
		const auto invalid = (directory / "identity-invalid.dem").string();
		write_bytes(invalid.c_str(), bad);
		require(!newdemo_dump_json(invalid.c_str(), output.c_str(), error, sizeof(error)) && std::strstr(error, "frame decode failed") && read("identity.jsonl") == valid_report, "Malformed wide demo did not fail decoding or changed prior output");
	}
	require(newdemo_dump_json(input.c_str(), output.c_str(), error, sizeof(error)), error);
	std::cout << "Classic/wide object codecs, byte-preserving rewrite, complete frames and malformed identity recovery passed\n";
}

extern "C" void state_object_to_object_rw(object *, object_rw *);
static void object_capacity_saved_counts(char *save)
{
	auto *file = PHYSFSX_openReadBuffered(save);
	require(file, "Cannot open object count save control");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) && PHYSFS_close(file), "Cannot read object count save control");
	object_rw first{};
	state_object_to_object_rw(&Objects[0], &first);
	const auto *begin = reinterpret_cast<const uint8_t *>(&first);
	const auto at = std::search(bytes.begin(), bytes.end(), begin, begin + sizeof(first));
	require(at != bytes.end() && at - bytes.begin() >= 4 && std::search(at + 1, bytes.end(), begin, begin + sizeof(first)) == bytes.end(), "Saved first object is not uniquely located");
	const auto count_at = static_cast<std::size_t>(at - bytes.begin()) - 4;
	const int limit = object_capacity(), highest = Highest_object_index;
	char invalid_name[] = "objects-invalid.sav";
	for (int variant = 0; variant < 4; ++variant) {
		auto bad = bytes;
		if (variant == 3) bad.resize(count_at + 2);
		else {
			const unsigned count = variant == 0 ? unsigned(-1) : variant == 1 ? 0u
			                                                                  : unsigned(limit + 1);
			for (unsigned i = 0; i < 4; ++i) bad[count_at + i] = uint8_t(count >> (8 * i));
		}
		write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / invalid_name).string().c_str(), bad);
		require(!state_restore_all_sub(invalid_name, 0), "Invalid core object count accepted");
		require(Highest_object_index < limit && object_capacity() == limit, "Invalid core count installed or changed pool");
		state_restore_take_menu_request();
		require(state_restore_all_sub(save, 0) && Highest_object_index == highest && object_capacity() == limit, "Valid object save did not recover after rejected core count");
	}
	std::cout << "Negative, zero, oversized and truncated saved object counts rejected for pool " << limit << '\n';
}

static int object_capacity_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		for (unsigned authored : { 999u, 1000u, 1001u, 16384u }) {
			require(load_base_mission("d2", error, sizeof(error)), error);
			object_capacity_fixture(root / "missions", authored);
			require(load_base_mission("objcap", error, sizeof(error)), error);
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			require(object_capacity() == static_cast<int>(xl_object_capacity_for_count(authored)) && Highest_object_index == int(authored - 1), "Authored-count runtime headroom differs after production load");
		}

		require(load_base_mission("objcap", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && object_capacity() == 32768 && Highest_object_index == 16383, "Authored high object count failed native load");
		object_reclaim_samples(root);
		StartNewGame(1);
		object_signature_samples(root);
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		object_pool_allocator_contract();
		object_capacity_pressure();
		std::cout << "Full XL pool exhaustion, identity reuse and invalid allocator controls passed\n";
		// Leave room for impact effects, then create high-ID actual actors
		for (int i = 32728; i < 32768; ++i)
			if (Objects[i].type != OBJ_NONE) obj_delete(i);
		const int robot = projectile_robot();
		const int shot = projectile_test_shot(ConsoleObject - Objects);
		require(robot >= 1000 && shot >= 1000, "High combat fixture allocated only classic identities");
		require(Objects[robot].signature > 32767 && Objects[shot].signature > 32767, "Full-pool reuse did not retain wide lifetime signatures");
		digi_link_sound_to_object(SOUND_SHIP_IN_WATER, short(robot), 1, F1_0);
		require(xl_linked_sound_count(robot, SOUND_SHIP_IN_WATER, 0) == 1, "Wide native audio owner was truncated");
		digi_sync_sounds();
		require(xl_linked_sound_count(robot, SOUND_SHIP_IN_WATER, 0) == 1, "Native sound synchronization lost wide owner");
		digi_kill_sound_linked_to_object(robot);
		collide_robot_and_weapon(&Objects[robot], &Objects[shot], &Objects[robot].pos);
		require(Objects[robot].shields == i2f(60) && xl_projectile_has_hit(shot, robot), "High projectile contact lost damage or history");
		Ai_local_info[robot].player_awareness_time = F1_0;
		Last_afterburner_time[shot] = GameTime64;
		object_runtime_state earned{};
		object_get_runtime_state(&earned);
		require(object_validate_runtime_state(&earned), "High combat allocator invalid");
		char save[] = "objects.sav", description[] = "XL high object state";
		stop_time();
		require(state_save_all_sub(save, description), "Cannot save high object state");
		object_capacity_saved_counts(save);
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && object_capacity() == 1000, "XL capacity leaked into Redux");
		object_pool_allocator_contract();
		object_runtime_state stock{};
		object_get_runtime_state(&stock);
		const int stock_seed = stock.signature_seed;
		stock.signature_seed = 32768;
		require(!object_validate_runtime_state(&stock), "Stock allocator accepted XL signature sequence");
		stock.signature_seed = stock_seed;
		for (int i = object_capacity(); i < MAX_OBJECTS; ++i) stock.free_obj_list[i] = -1;
		object_set_runtime_state(&stock);
		object_get_runtime_state(&stock);
		for (int i = object_capacity(); i < MAX_OBJECTS; ++i)
			require(stock.free_obj_list[i] == i, "Stock restore retained inactive allocator input");
		char stock_save[] = "objects-stock.sav";
		require(state_save_all_sub(stock_save, description), "Cannot save stock full pool");
		object_capacity_saved_counts(stock_save);
		require(state_restore_all_sub(save, 0) && object_capacity() == 32768, "High object disk restore through stock failed");
		start_time();
		object_runtime_state restored{};
		object_get_runtime_state(&restored);
		require(restored.num_objects == earned.num_objects && restored.highest_object_index == earned.highest_object_index && restored.signature_seed == earned.signature_seed && std::equal(restored.free_obj_list, restored.free_obj_list + object_capacity(), earned.free_obj_list), "High allocator order or identity changed through disk");
		require(Objects[robot].shields == i2f(60) && Ai_local_info[robot].player_awareness_time == F1_0 && xl_projectile_has_hit(shot, robot), "High actor state lost through disk");
		collide_robot_and_weapon(&Objects[robot], &Objects[shot], &Objects[robot].pos);
		require(Objects[robot].shields == i2f(60), "Restored high projectile forgot victim");
		route_snapshot_summary snapshot{};
		require(level_metadata_get_canonical_route_snapshot(&snapshot) && snapshot.object_count > 1000, "Metadata snapshot rejected high objects");
		object_capacity_ai_and_event(robot);
		object_capacity_admission(root);
		object_capacity_demo();
		std::cout << "XL object capacity native controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL object capacity controls failed: " << error.what() << '\n';
		return 1;
	}
}
