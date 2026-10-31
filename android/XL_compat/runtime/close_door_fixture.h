static void write_close_door_fixture(const std::filesystem::path &root, unsigned flags = WALL_DOOR_AUTO, const char *mission = "close")
{
	write_doorway_fixture(root);
	std::ifstream input(root / "doorway.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			for (int i : { 0, 1 }) {
				member.bytes.at(source.sections[1].offset + i * 25 + 17) = static_cast<unsigned char>(flags);
				member.bytes.at(source.sections[1].offset + i * 25 + 18) = static_cast<unsigned char>(flags >> 8);
			}
			require(xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes)).empty(), "Close-door fixture failed admission");
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "Close-door fixture member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / (std::string(mission) + ".hog")).string().c_str(), output);
	std::ofstream descriptor(root / (std::string(mission) + ".mn2"), std::ios::binary);
	descriptor << "d2x-name = Derived XL close doors\nnum_levels = 1\nneptunxl.rl2\n";
}
