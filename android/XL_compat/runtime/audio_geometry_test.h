// Compare loaded-mine spatial outputs with the pinned source in the existing runner
static void classic_audio_geometry_controls(const std::filesystem::path &root)
{
	using namespace xl_compat;
	const auto graph = classic_light_route_graph();
	const auto visibility = classic_segment_visibility(classic_default_lighting_camera());
	ClassicAudioSpatial scene;
	std::ostringstream text;
	text << "XL_AUDIO_GEOMETRY 1 " << graph.centers.size() << '\n';
	for (std::size_t i = 0; i < graph.centers.size(); ++i) {
		for (auto value : graph.centers[i]) text << value << ' ';
		for (const auto &edge : graph.edges[i]) text << edge.child << ' ' << edge.distance << ' ' << edge.cost << ' ' << edge.passable << ' ';
		text << '\n';
	}
	for (const auto &row : visibility) {
		for (bool visible : row) text << visible << ' ';
		text << '\n';
	}
	unsigned cases = 0, audible = 0;
	for (int listener : { 0, int(graph.centers.size() / 2), int(graph.centers.size() - 1), 0 })
		for (unsigned sound = 0; sound < graph.centers.size(); ++sound)
			for (int decay : { 0, 1, 3 })
				for (int reverse : { 0, 1 }) {
					auto a = graph.centers[unsigned(listener)], b = graph.centers[sound];
					a[0] += 8192;
					b[1] -= 8192;
					const FixedLightPoint right = reverse ? FixedLightPoint{ 0, 0, -65536 } : FixedLightPoint{ 65536, 0, 0 }, up{ 0, 65536, 0 };
					const auto output = scene.output(listener, int(sound), a, b, right, up, 65536, 256 * 65536, decay, reverse != 0, 1);
					require(output.volume >= 0 && output.pan >= 0 && output.pan <= 65536, "Loaded spatial audio output is outside its range");
					text << listener << ' ' << sound << " 65536 16777216 " << decay << ' ' << reverse << " 1 ";
					for (const auto &point : { a, b, right, up })
						for (auto value : point) text << value << ' ';
					text << output.volume << ' ' << output.pan << '\n';
					++cases;
					if (output.volume) ++audible;
				}
	require(audible > 0, "Loaded spatial audio controls produced no audible positions");
	const auto path = root / "audio-geometry.txt";
	std::ifstream previous(path, std::ios::binary);
	const std::string old{ std::istreambuf_iterator<char>(previous), std::istreambuf_iterator<char>() };
	previous.close();
	if (old != text.str()) {
		std::ofstream output(path, std::ios::binary);
		output << text.str();
		require(bool(output), "Cannot write loaded audio geometry evidence");
	}
	std::cout << "Loaded audio geometry prepared: " << cases << " positions, " << audible << " audible outputs; source comparison required\n";
}
