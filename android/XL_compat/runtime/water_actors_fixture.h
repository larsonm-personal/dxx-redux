static void write_water_actors_fixture(const std::filesystem::path &root)
{
	write_equipment_fixture(root);
	std::ifstream original(root / "equip.hog", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() }, output{ 'D', 'H', 'F' };
	for (auto member : xl_compat::read_hog(bytes)) {
		if (member.name == "neptunxl.rl2") {
			const auto source = xl_compat::inspect_level(member.bytes);
			const auto patch = [&](std::size_t offset, std::uint32_t value, int width = 4) {
				for (int byte = 0; byte < width; ++byte) member.bytes.at(offset + byte) = static_cast<unsigned char>(value >> (byte * 8));
			};
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910 + source.segments.size()) * 13;
			for (int segment : { 20, 22, 23, 60, 92 }) patch(properties + segment * 13 + 4, xl_compat::segment_water, 1);
			// Stock robot zero supplies the projectile combat control
			patch(source.sections[6].offset, 1u << 0);
			// A roomy derived fixture lets real-size ships and robots move independently
			for (std::size_t i = 0; i < source.vertices.size(); ++i)
				for (int axis = 0; axis < 3; ++axis) patch(source.mine_offset + 5 + i * 12 + axis * 4, source.vertices[i][axis] * 4);
			for (const auto &object : source.objects)
				for (int axis = 0; axis < 3; ++axis) {
					patch(object.source_offset + 9 + axis * 4, object.position[axis] * 4);
					patch(object.source_offset + 65 + axis * 4, object.last_position[axis] * 4);
				}
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(member.bytes));
			require(blockers.empty(), xl_compat::blocker_message(blockers).c_str());
		}
		char name[13]{};
		require(member.name.size() < sizeof(name), "water actors member name too long");
		std::strcpy(name, member.name.c_str());
		output.insert(output.end(), name, name + sizeof(name));
		for (unsigned shift = 0; shift < 32; shift += 8) output.push_back(static_cast<unsigned char>(member.bytes.size() >> shift));
		output.insert(output.end(), member.bytes.begin(), member.bytes.end());
	}
	write_bytes((root / "wetbots.hog").string().c_str(), output);
	std::ofstream descriptor(root / "wetbots.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived water actors\nnum_levels = 1\nneptunxl.rl2\n";
}
