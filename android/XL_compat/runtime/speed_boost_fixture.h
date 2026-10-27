static void write_speed_boost_fixture(const std::filesystem::path &root)
{
	write_object_chain_delay_fixture(root);
	std::ifstream input(root / "objdelay.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int i = 0; i < width; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (8 * i));
			};
			const auto offset = source.sections[3].offset;
			const auto target = [&](int wall) { return std::array<int, 2>{ source.walls[wall].segment, source.walls[wall].side }; };
			const auto action = [&](int index, int type, int flags, int time, int value, std::initializer_list<std::array<int, 2>> links) {
				const auto base = offset + index * 52;
				std::fill(member.bytes.begin() + base, member.bytes.begin() + base + 52, 0);
				patch(base, type, 1);
				patch(base + 1, flags, 1);
				patch(base + 2, static_cast<unsigned>(links.size()), 1);
				patch(base + 4, value);
				patch(base + 8, time);
				int link = 0;
				for (const auto &entry : links) {
					patch(base + 12 + link * 2, entry[0], 2);
					patch(base + 32 + link++ * 2, entry[1], 2);
				}
			};
			action(0, 15, 32 | 8, -1, 5, { { 70, 0 } });
			action(1, 15, 8, -1, 0, {});
			action(2, 31, 8, 20000, 0, { target(3), target(4) });
			action(3, 15, 32 | 8, 1000, 2, { { 84, 0 } });
			action(4, 15, 8, -1, -1, { { 84, 1 } });
			action(5, 15, 8, 1000, 6, { { 70, 1 } });
			// The original first wall supplies a real authored boost-area crossing
			const int area = source.walls[0].segment, side = source.walls[0].side;
			const int child = source.segments[area].children[side];
			require(child >= 0, "boost fixture first wall has no adjoining segment");
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910) * 13 - source.segments.size() * 13;
			require(member.bytes.at(properties + area * 13) == source.segments[area].function, "boost function offset mismatch");
			patch(properties + area * 13, 9, 1);
			const auto wall_offset = source.sections[1].offset;
			patch(wall_offset + 16, WALL_OPEN, 1);
			patch(wall_offset + 17, 0, 2);
			for (int i = 0; i < 6; ++i) {
				if (source.segments[child].children[i] != area) continue;
				const int wall = source.segments[child].sides[i].wall;
				if (wall >= 0) {
					patch(wall_offset + wall * 25 + 16, WALL_OPEN, 1);
					patch(wall_offset + wall * 25 + 17, 0, 2);
				}
			}
			for (int i = 0; i < 10; ++i) {
				const auto base = offset + 6 * 52 + 4 + i * 53;
				patch(base, i == 1 ? 15 : 31, 1);
				patch(base + 1, i == 0 ? 0 : i == 1 ? 128 | 32
				                                    : TF_DISABLED,
				      2);
				patch(base + 3, 1, 1);
				patch(base + 5, i == 1 ? 3 : 0);
				patch(base + 13, i == 1 ? 84 : source.walls[2].segment, 2);
				patch(base + 33, i == 1 ? 0 : source.walls[2].side, 2);
			}
			const auto final = xl_compat::inspect_level(member.bytes);
			const auto blockers = xl_compat::runtime_blockers(final);
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
			write_object_fixture_json(root / "boost-fixture.json", { { "area", area }, { "side", side }, { "child", child }, { "robot", 49 } });
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "boost fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "boost.hog").string().c_str(), output);
	std::ofstream descriptor(root / "boost.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived speed boosts\nnum_levels = 1\nneptunxl.rl2\n";
}
