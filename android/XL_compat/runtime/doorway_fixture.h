static void write_doorway_fixture(const std::filesystem::path &root)
{
	write_keyed_wall_fixture(root);
	std::ifstream input(root / "keywall.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			require(source.triggers.size() == 1, "Doorway fixture expects the original single trigger");
			// Add one close-door action, rebasing sections after the expanded table
			const auto patch = [&](std::size_t offset, std::uint32_t value) {
				for (int i = 0; i < 4; ++i) member.bytes.at(offset + i) = static_cast<unsigned char>(value >> (8 * i));
			};
			member.bytes.insert(member.bytes.begin() + source.sections[3].offset, 52, 0);
			for (std::size_t i = 0; i < source.sections.size(); ++i)
				if (source.sections[i].offset > source.sections[3].offset) patch(source.game_offset + 35 + i * 12, source.sections[i].offset + 52);
			patch(source.game_offset + 75, 2);
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910 + source.segments.size()) * 13;
			member.bytes.at(properties + 15 * 13 + 4) = 0;
			member.bytes.at(properties + 24 * 13) = 0;
			for (int i = 0; i < 6; ++i) {
				const auto wall = source.sections[1].offset + i * 25;
				member.bytes.at(wall + 16) = i < 2 ? WALL_DOOR : i < 4 ? WALL_CLOAKED
				                                                       : WALL_NORMAL;
				member.bytes.at(wall + 17) = i < 2 ? 0 : WALL_DOOR_OPENED;
				member.bytes.at(wall + 18) = 0;
				member.bytes.at(wall + 19) = 0;
				member.bytes.at(wall + 22) = KEY_NONE;
				member.bytes.at(wall + 24) = 31;
			}
			for (std::size_t i = 0; i < source.walls.size(); ++i)
				member.bytes.at(source.sections[1].offset + i * 25 + 20) = 255;
			for (std::size_t i = 0; i < 2; ++i) {
				const auto base = source.sections[3].offset + i * 52;
				std::fill(member.bytes.begin() + base, member.bytes.begin() + base + 52, 0);
				member.bytes.at(base) = i == 1 ? TT_CLOSE_DOOR : TT_OPEN_DOOR;
				member.bytes.at(base + 2) = 1;
				for (int j = 0; j < 4; ++j) member.bytes.at(base + 8 + j) = 255;
				member.bytes.at(base + 12) = 24;
				member.bytes.at(base + 32) = 5;
			}
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Doorway fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "doorway.hog").string().c_str(), output);
	std::ofstream descriptor(root / "doorway.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL doorways\nnum_levels = 1\nneptunxl.rl2\n";
}
