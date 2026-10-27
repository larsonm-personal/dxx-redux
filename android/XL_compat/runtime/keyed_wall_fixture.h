static void write_keyed_wall_fixture(const std::filesystem::path &root)
{
	write_water_actors_fixture(root);
	std::ifstream input(root / "wetbots.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910 + source.segments.size()) * 13;
			member.bytes.at(properties + 15 * 13 + 4) = 4;
			member.bytes.at(properties + 24 * 13) = 9;
			for (int i = 0; i < 6; ++i) {
				const auto wall = source.sections[1].offset + i * 25;
				member.bytes.at(wall + 16) = 5;
				member.bytes.at(wall + 17) = 2;
				member.bytes.at(wall + 18) = 0;
				member.bytes.at(wall + 19) = 0;
				member.bytes.at(wall + 22) = i < 2 ? 2 : i < 4 ? 4
				                                               : 8;
				member.bytes.at(wall + 24) = 9;
			}
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Keyed wall fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "keywall.hog").string().c_str(), output);
	std::ofstream descriptor(root / "keywall.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived keyed walls\nnum_levels = 1\nneptunxl.rl2\n";
}
