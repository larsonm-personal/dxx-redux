static void write_blocked_fixture(const std::filesystem::path &root)
{
	write_water_actors_fixture(root);
	std::ifstream input(root / "wetbots.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			require(source.segments.size() > 60, "Blocked fixture lacks matrix rooms");
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910 + source.segments.size()) * 13;
			for (int bits = 0; bits < 16; ++bits) member.bytes.at(properties + (40 + bits) * 13 + 4) = bits;
			member.bytes.at(properties + 60 * 13 + 4) = 4;
			member.bytes.at(properties + 22 * 13 + 4) = 6;
			member.bytes.at(properties + 23 * 13 + 4) = 5;
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Blocked fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "blocked.hog").string().c_str(), output);
	std::ofstream descriptor(root / "blocked.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived blocked volumes\nnum_levels = 1\nneptunxl.rl2\n";
}
