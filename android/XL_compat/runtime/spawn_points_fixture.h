static void write_spawn_points_fixture(const std::filesystem::path &root)
{
	write_speed_boost_fixture(root);
	std::ifstream input(root / "boost.hog", std::ios::binary);
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
			action(0, 28, 8, -1, 0, { { 60, 0 }, { 84, 1 }, { 30, 2 } });
			action(1, 28, 8 | 32, -1, 0, { { 84, 0 } });
			action(2, 31, 8, 20000, 0, { target(3) });
			action(3, 28, 8, 1000, 0, { { 92, 4 } });
			action(4, 15, 8, -1, 2, { { 70, 0 } });
			action(5, 28, 8, 1000, 0, { { 30, 1 } });
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910 + source.segments.size()) * 13;
			patch(properties + source.walls[0].segment * 13, 0, 1);
			for (int i = 0; i < 10; ++i) {
				const auto base = offset + 6 * 52 + 4 + i * 53;
				patch(base, i == 1 ? 28 : 31, 1);
				patch(base + 1, i == 0 ? 0 : i == 1 ? 128 | 2
				                                    : TF_DISABLED,
				      2);
				patch(base + 3, 1, 1);
				patch(base + 5, 0);
				patch(base + 13, i == 1 ? 30 : source.walls[2].segment, 2);
				patch(base + 33, i == 1 ? 0 : source.walls[2].side, 2);
			}
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "spawn fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "spawn.hog").string().c_str(), output);
	std::ofstream descriptor(root / "spawn.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived spawn points\nnum_levels = 1\nneptunxl.rl2\n";
}
