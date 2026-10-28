#include "doors.h"
#include "../tests/door_fixture.h"

static void door_animation_matrix(std::ostream &out)
{
	const auto front = Walls[0], back = Walls[1];
	const auto first_side = Segments[front.segnum].sides[front.sidenum];
	const auto second_side = Segments[back.segnum].sides[back.sidenum];
	const auto saved_clip = WallAnims[front.clip_num];
	require(!Num_open_doors, "Animation matrix requires idle doors");
	for (int automatic : { 0, 1 })
		for (int opening : { 0, 1 })
			for (int frames : { 2, 3, 4, 10, 30 })
				for (int duration : { F1_0 / 4, F1_0, 3 * F1_0 }) {
					const int step = duration / frames;
					for (int elapsed : { 0, step - 1, step, 2 * step - 1, 2 * step, (frames / 2) * step, (frames / 2 + 1) * step, (frames - 1) * step - 1, (frames - 1) * step, duration, duration + step }) {
						Walls[0] = front;
						Walls[1] = back;
						auto &clip = WallAnims[front.clip_num];
						clip.num_frames = frames;
						clip.play_time = duration;
						clip.flags = WCF_TMAP1;
						for (int i = 0; i < frames; ++i) clip.frames[i] = 100 + i;
						for (int i : { 0, 1 }) {
							Walls[i].clip_num = front.clip_num;
							Walls[i].state = opening ? WALL_DOOR_OPENING : WALL_DOOR_CLOSING;
							Walls[i].flags = (automatic ? WALL_DOOR_AUTO : 0) | (opening ? 0 : WALL_DOOR_OPENED);
							Segments[Walls[i].segnum].sides[Walls[i].sidenum].tmap_num = 99;
						}
						Num_open_doors = 1;
						ActiveDoors[0] = {};
						ActiveDoors[0].n_parts = 1;
						ActiveDoors[0].front_wallnum[0] = 0;
						ActiveDoors[0].back_wallnum[0] = 1;
						FrameTime = elapsed;
						wall_frame_process();
						const auto &side = Segments[front.segnum].sides[front.sidenum];
						require(Walls[0].state == Walls[1].state && Walls[0].flags == Walls[1].flags && side.tmap_num == Segments[back.segnum].sides[back.sidenum].tmap_num, "Symmetric animated faces diverged");
						out << automatic << ' ' << opening << ' ' << frames << ' ' << duration << ' ' << elapsed << ' ' << int(Walls[0].state) << ' ' << int(Walls[0].flags) << ' ' << side.tmap_num - 100 << ' ' << Num_open_doors << ' ' << (Num_open_doors ? ActiveDoors[0].time : 0) << '\n';
					}
				}
	Walls[0] = front;
	Walls[1] = back;
	WallAnims[front.clip_num] = saved_clip;
	Segments[front.segnum].sides[front.sidenum] = first_side;
	Segments[back.segnum].sides[back.sidenum] = second_side;
	Num_open_doors = 0;
	std::cout << "660 actual XL animation frame/flag/state/timer samples written\n";
}

static void write_door_lifecycle_fixture(const std::filesystem::path &root)
{
	std::ifstream input(root / "close.hog", std::ios::binary);
	const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(original)) {
		if (member.name == "neptunxl.rl2") {
			member.bytes = xl_test::add_door_pairs(member.bytes, 91);
			const auto level = xl_compat::inspect_level(member.bytes);
			require(xl_compat::runtime_blockers(level).empty(), "Door lifecycle fixture was not admitted");
			auto invalid = level;
			invalid.walls[0].clip = -1;
			require(xl_compat::blocker_message(xl_compat::runtime_blockers(invalid)).find("door has no wall animation") != std::string::npos, "Missing door animation was silently admitted");
			invalid = level;
			invalid.walls[1].type = WALL_CLOSED;
			invalid.walls[1].clip = -1;
			require(xl_compat::blocker_message(xl_compat::runtime_blockers(invalid)).find("opposite wall 1") != std::string::npos, "Missing opposite animation lacks a useful admission error");
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Door lifecycle member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<uint8_t>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "doorlife.hog").string().c_str(), output);
	std::ofstream descriptor(root / "doorlife.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL door lifecycle\nnum_levels = 1\nneptunxl.rl2\n";
}

static void door_pair_variants()
{
	const auto front = Walls[0], back = Walls[1];
	const auto front_side = Segments[front.segnum].sides[front.sidenum];
	const auto back_side = Segments[back.segnum].sides[back.sidenum];
	const int a = front.clip_num, b = (a + 1) % Num_wall_anims;
	require(a != b, "Paired door controls need two clip slots");
	const auto clip_a = WallAnims[a], clip_b = WallAnims[b];
	const auto reset = [&] {
		Walls[0] = front;
		Walls[1] = back;
		Segments[front.segnum].sides[front.sidenum] = front_side;
		Segments[back.segnum].sides[back.sidenum] = back_side;
		Walls[0].clip_num = a;
		Walls[1].clip_num = b;
		for (int i : { 0, 1 }) {
			Walls[i].flags = 0;
			Walls[i].state = WALL_DOOR_CLOSED;
			Walls[i].linked_wall = -1;
		}
		WallAnims[a] = clip_a;
		WallAnims[b] = clip_b;
		WallAnims[a].play_time = F1_0;
		WallAnims[b].play_time = 2 * F1_0;
		WallAnims[a].num_frames = WallAnims[b].num_frames = 4;
		WallAnims[a].flags = WallAnims[b].flags = WCF_TMAP1;
		for (int i = 0; i < 4; ++i) {
			WallAnims[a].frames[i] = 100 + i;
			WallAnims[b].frames[i] = 200 + i;
		}
		Num_open_doors = 0;
	};
	for (int automatic : { 0, 1 }) {
		reset();
		if (automatic) Walls[0].flags = Walls[1].flags = WALL_DOOR_AUTO;
		wall_open_door(&Segments[front.segnum], front.sidenum);
		FrameTime = F1_0 / 4;
		for (int i = 0; i < 3; ++i) wall_frame_process();
		require(Num_open_doors == 1 && Walls[0].state == (automatic ? WALL_DOOR_WAITING : WALL_DOOR_OPEN) && Walls[1].state == WALL_DOOR_OPENING && xl_door_state_valid(ActiveDoors, 1), "Faster door face retired its opening partner");
		for (int i = 0; i < 3; ++i) wall_frame_process();
		require(Num_open_doors == automatic && Walls[1].state == Walls[0].state && Segments[front.segnum].sides[front.sidenum].tmap_num == 103 && Segments[back.segnum].sides[back.sidenum].tmap_num == 203, "Asymmetric door faces lost clips or completion");
		if (automatic) require(ActiveDoors[0].time == 0, "Asymmetric opening reset wait timer before both faces finished");
	}
	for (int type : { WALL_DOOR, WALL_ILLUSION, WALL_CLOSED, WALL_OPEN, WALL_BLASTABLE })
		for (int automatic = 0; automatic < 4; ++automatic) {
			reset();
			Walls[1].type = type;
			Walls[0].flags = automatic & 1 ? WALL_DOOR_AUTO : 0;
			Walls[1].flags = automatic & 2 ? WALL_DOOR_AUTO : 0;
			wall_open_door(&Segments[front.segnum], front.sidenum);
			require(Num_open_doors == 1 && xl_door_state_valid(ActiveDoors, 1), "Different opposite wall type prevented door activation");
			FrameTime = F1_0 / 4;
			for (int i = 0; i < 6; ++i) {
				wall_frame_process();
				require(xl_door_state_valid(ActiveDoors, Num_open_doors), "Mixed door flags produced unrestorable animation state");
			}
			require(Walls[1].type == type && Walls[1].state == Walls[0].state && Num_open_doors == (automatic & 1), "Mixed door flags stranded a partner or changed its wall type");
			if (automatic & 1) {
				require(ActiveDoors[0].time == 0, "Mixed door flags did not start a fresh waiting period");
				FrameTime = DOOR_WAIT_TIME + 1;
				wall_frame_process();
			} else wall_close_door(&Segments[front.segnum], front.sidenum);
			require(Num_open_doors == 1 && xl_door_state_valid(ActiveDoors, 1), "Mixed door pair did not enter closing");
			FrameTime = 2 * F1_0;
			wall_frame_process();
			require(!Num_open_doors && Walls[0].state == WALL_DOOR_CLOSED && Walls[1].state == WALL_DOOR_CLOSED && Walls[1].type == type, "Mixed door pair did not finish closing");
		}
	reset();
	Walls[0].linked_wall = 1;
	Walls[1].linked_wall = 0;
	wall_open_door(&Segments[front.segnum], front.sidenum);
	require(Num_open_doors == 1 && ActiveDoors[0].n_parts == 2 && ActiveDoors[0].front_wallnum[1] == 1 && ActiveDoors[0].back_wallnum[1] == 0 && xl_door_state_valid(ActiveDoors, 1), "Linked door retained a side number as a wall identity");
	FrameTime = F1_0 / 16;
	wall_frame_process();
	require(ActiveDoors[0].time == F1_0 / 8, "Linked door lost source per-part timing");
	FrameTime = 2 * F1_0;
	wall_frame_process();
	require(!Num_open_doors && Walls[0].state == WALL_DOOR_OPEN && Walls[1].state == WALL_DOOR_OPEN, "Linked door did not retire as one entry");
	reset();
	Walls[0].linked_wall = 1;
	Walls[1].linked_wall = 0;
	Walls[0].flags = Walls[1].flags = WALL_DOOR_AUTO;
	wall_open_door(&Segments[front.segnum], front.sidenum);
	FrameTime = F1_0 / 4;
	for (int i = 0; i < 3; ++i) wall_frame_process();
	require(Num_open_doors == 1 && Walls[0].state == WALL_DOOR_WAITING && Walls[1].state == WALL_DOOR_WAITING && !ActiveDoors[0].time && xl_door_state_valid(ActiveDoors, 1), "Linked door completing in its second part retained opening time as waiting time");
	for (int rotation : { 0, 0x4000, 0x8000, 0xc000 })
		for (int overlay : { 0, 33 }) {
			reset();
			auto &face = Segments[front.segnum].sides[front.sidenum];
			WallAnims[a].flags = 0;
			face.tmap_num = 7;
			face.tmap_num2 = rotation | overlay;
			wall_open_door(&Segments[front.segnum], front.sidenum);
			FrameTime = F1_0 / 4;
			wall_frame_process();
			require(overlay ? (face.tmap_num == 7 && static_cast<uint16_t>(face.tmap_num2) == (rotation | 101)) : (face.tmap_num == 101 && static_cast<uint16_t>(face.tmap_num2) == rotation), "XL animation changed overlay rotation or ignored absent-overlay base fallback");
		}
	reset();
	WallAnims[a].play_time = 0;
	bool rejected = false;
	try {
		xl_assets::ObjectDependencies graph;
		graph.wall_clip(a);
	} catch (const std::exception &error) {
		rejected = std::string(error.what()).find("duration") != std::string::npos;
	}
	require(rejected, "Zero frame duration escaped asset validation");
	Walls[0] = front;
	Walls[1] = back;
	WallAnims[a] = clip_a;
	WallAnims[b] = clip_b;
	Segments[front.segnum].sides[front.sidenum] = front_side;
	Segments[back.segnum].sides[back.sidenum] = back_side;
	Num_open_doors = 0;
	std::cout << "Asymmetric and linked faces, twenty mixed type/automatic pairs, per-part timing, texture-layer/rotation and invalid duration controls passed\n";
}

static void door_saved_rejections(char *save)
{
	auto *file = PHYSFSX_openReadBuffered(save);
	require(file, "Cannot read door save controls");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
	require(PHYSFS_readBytes(file, bytes.data(), bytes.size()) == PHYSFS_sint64(bytes.size()) && PHYSFS_close(file), "Cannot capture door save controls");
	const auto *first = reinterpret_cast<const uint8_t *>(ActiveDoors);
	const auto *last = first + Num_open_doors * sizeof(active_door);
	const auto found = std::search(bytes.begin(), bytes.end(), first, last);
	require(found != bytes.end() && found - bytes.begin() >= 4 && std::search(found + 1, bytes.end(), first, last) == bytes.end(), "Cannot uniquely locate full door table");
	const auto table = static_cast<std::size_t>(found - bytes.begin());
	char invalid[] = "bad-door.sav";
	for (int variant = 0; variant < 10; ++variant) {
		auto bad = bytes;
		const auto patch = [&](std::size_t offset, unsigned value, unsigned width) {
			for (unsigned b = 0; b < width; ++b) bad.at(offset + b) = static_cast<uint8_t>(value >> (8 * b));
		};
		switch (variant) {
			case 0: patch(table - 4, unsigned(-1), 4); break;
			case 1: patch(table - 4, 91, 4); break;
			case 2: bad.resize(table + 90 * sizeof(active_door) - 1); break;
			case 3: patch(table, 0, 4); break;
			case 4: patch(table, 3, 4); break;
			case 5: patch(table + offsetof(active_door, front_wallnum), Num_walls, 2); break;
			case 6: patch(table + offsetof(active_door, back_wallnum), ActiveDoors[1].back_wallnum[0], 2); break;
			case 7: patch(table + offsetof(active_door, time), unsigned(-1), 4); break;
			case 8: std::copy_n(bad.begin() + table, sizeof(active_door), bad.begin() + table + sizeof(active_door)); break;
			case 9: patch(table - 4, 0, 4); break;
		}
		write_bytes((std::filesystem::path(PHYSFS_getWriteDir()) / invalid).string().c_str(), bad);
		require(!state_restore_all_sub(invalid, 0), "Malformed full door save was accepted");
		require(state_restore_all_sub(save, 0) && Num_open_doors == 90 && xl_door_state_valid(ActiveDoors, Num_open_doors), "Valid full door save did not recover");
	}
	std::cout << "Ten malformed door count/payload/topology/ownership/time saves rejected with valid recovery\n";
}

static void door_concurrency(const std::filesystem::path &output)
{
	write_door_lifecycle_fixture(output / "missions");
	char error[256]{};
	require(load_base_mission("doorlife", error, sizeof(error)) && !load_level("neptunxl.rl2"), "Cannot load authored door lifecycle fixture");
	const int first = Num_walls - 182;
	require(first > 1, "Door fixture lost original walls");
	for (int i = Highest_object_index; i >= 0; --i)
		if (Objects[i].type != OBJ_NONE && i != ConsoleObject - Objects) obj_delete(i);
	obj_relink(ConsoleObject - Objects, 0);
	compute_segment_center(&ConsoleObject->pos, &Segments[0]);
	ConsoleObject->size = F1_0 / 16;
	const auto open = [&](int index) { wall_open_door(&Segments[Walls[index].segnum], Walls[index].sidenum); };
	const auto close = [&](int index) { require(xl_authored_close_door(Walls[index].segnum, Walls[index].sidenum), "Authored close selected Redux"); };
	for (int i = 0; i < 89; ++i) open(first + 2 * i);
	require(Num_open_doors == 89, "89 doors did not allocate");
	open(first + 178);
	require(Num_open_doors == 90, "90th door did not allocate");
	const auto before = Walls[first + 180];
	open(first + 180);
	require(Num_open_doors == 90 && !std::memcmp(&before, &Walls[first + 180], sizeof(wall)), "91st door partially mutated or allocated");
	FrameTime = F1_0 / 32;
	wall_frame_process();
	for (int i = 0; i < 90; ++i) require(ActiveDoors[i].time == FrameTime, "Concurrent door lost its frame");
	close(first);
	require(Num_open_doors == 90 && Walls[first].state == WALL_DOOR_CLOSING, "Full-pool reversal allocated or failed");
	open(first);
	require(Num_open_doors == 90 && Walls[first].state == WALL_DOOR_OPENING, "Full-pool reopening failed");
	require(xl_door_state_valid(ActiveDoors, Num_open_doors), "Full door state is invalid");
	const std::vector<active_door> earned(ActiveDoors, ActiveDoors + Num_open_doors);
	char save[] = "doors-full.sav", description[] = "XL active doors";
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save full active doors");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Door stock transition failed");
	require(!xl_door_open(Walls[0].segnum, Walls[0].sidenum) && !xl_door_frame_process(), "XL door handler claimed stock");
	require(state_restore_all_sub(save, 0) && Num_open_doors == 90 && !std::memcmp(earned.data(), ActiveDoors, earned.size() * sizeof(active_door)), "Full door identities/timers lost through stock");
	door_saved_rejections(save);
	start_time();
	FrameTime = WallAnims[Walls[first].clip_num].play_time;
	wall_frame_process();
	require(Num_open_doors == 0, "Simultaneous completion skipped compacted door entries");
	for (int i = 0; i < 180; ++i) require(Walls[first + i].state == WALL_DOOR_OPEN && (Walls[first + i].flags & WALL_DOOR_OPENED), "Concurrent completion lost a door face");
	open(first + 180);
	require(Num_open_doors == 1, "Freed capacity was not reusable");
	wall_frame_process();
	require(Num_open_doors == 0, "Reused door failed completion");
	// Restore all doors, including source ownership, before testing automatic timing
	require(state_restore_all_sub(save, 0), "Cannot reset concurrent door controls");
	for (int i = 0; i < 180; ++i) Walls[first + i].flags |= WALL_DOOR_AUTO;
	FrameTime = WallAnims[Walls[first].clip_num].play_time;
	wall_frame_process();
	require(Num_open_doors == 90, "Automatic doors retired while waiting");
	for (int i = 0; i < 90; ++i) require(ActiveDoors[i].time == 0 && Walls[first + i * 2].state == WALL_DOOR_WAITING && Walls[first + i * 2 + 1].state == WALL_DOOR_WAITING, "Door did not reset its own wait timer or both faces");
	FrameTime = DOOR_WAIT_TIME;
	wall_frame_process();
	for (int i = 0; i < 180; ++i) require(Walls[first + i].state == WALL_DOOR_WAITING, "Door closed at the strict wait boundary");
	FrameTime = 1;
	wall_frame_process();
	for (int i = 0; i < 90; ++i) require(Walls[first + i * 2].state == WALL_DOOR_CLOSING && ActiveDoors[i].time == 0, "Unblocked door failed wait deadline");
	FrameTime = WallAnims[Walls[first].clip_num].play_time;
	wall_frame_process();
	require(Num_open_doors == 0, "Concurrent automatic closure skipped an entry");
	for (int i = 0; i < 180; ++i) require(Walls[first + i].state == WALL_DOOR_CLOSED && !(Walls[first + i].flags & WALL_DOOR_OPENED), "Closed automatic door retained passage");
	std::cout << "Authored 91-door fixture: 90-entry boundary, full reuse, compaction, wait deadlines and disk-through-stock passed\n";
}
