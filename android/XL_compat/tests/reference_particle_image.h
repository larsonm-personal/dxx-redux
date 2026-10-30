// Reuse the actual CTGA header/ReadData/SetProperties bodies and bitmap adapters
static void compare_particle_image(const std::vector<uint8_t> &bytes)
{
	const auto actual = xl_compat::read_particle_image(bytes.data(), bytes.size());
	CFile input{ bytes };
	CBitmap bitmap;
	CTGA source{ &bitmap, {} };
	require(source.m_header.Read(input, &bitmap) && source.ReadData(input, -1, 1.0, 0, source.m_header.m_data.yStart != 0), "Source particle TGA did not load");
	require(actual.width == unsigned(bitmap.Width()) && actual.height == unsigned(bitmap.Height()) && actual.flags == unsigned(bitmap.Flags()), "Particle TGA dimensions or flags differ from source");
	for (std::size_t i = 0; i < actual.pixels.size() / 4; ++i)
		for (unsigned c = 0; c < 4; ++c) {
			const auto expected = c == 3 && bitmap.BPP() == 3 ? 255 : bitmap.pixels[i * bitmap.BPP() + c];
			require(actual.pixels[i * 4 + c] == expected, "Particle TGA row/channel/alpha conversion differs from source");
		}
	for (unsigned c = 0; c < 3; ++c) require(actual.average_rgb[c] == bitmap.average.values[c], "Particle TGA average color differs from source");
	for (unsigned frame = 0; frame < 4; ++frame)
		require(actual.transparent_frames[frame] == uint32_t(bitmap.transparent[frame]) && actual.super_frames[frame] == uint32_t(bitmap.super[frame]), "Particle TGA transparency frames differ from source");
}

static std::vector<uint8_t> particle_image_fixture(unsigned channels, unsigned height, unsigned marker, unsigned descriptor, unsigned identifier, unsigned alpha)
{
	std::vector<uint8_t> bytes(18 + identifier + 4 * height * channels, 0);
	bytes[0] = uint8_t(identifier);
	bytes[2] = 2;
	bytes[10] = uint8_t(marker);
	bytes[12] = 4;
	bytes[14] = uint8_t(height);
	bytes[16] = uint8_t(channels * 8);
	bytes[17] = uint8_t(descriptor | (channels == 4 ? 8 : 0));
	for (unsigned i = 0; i < 4 * height; ++i) {
		auto *pixel = bytes.data() + 18 + identifier + i * channels;
		pixel[0] = uint8_t(i * 17 + 9);
		pixel[1] = uint8_t(i * 11 + 51);
		pixel[2] = uint8_t(i * 3 + 72);
		if (channels == 4) pixel[3] = uint8_t(i % 3 ? alpha : 255);
		if (i % 7 == 1) {
			pixel[0] = marker ? 120 : 128;
			pixel[1] = 88;
			pixel[2] = marker ? 128 : 120;
		}
	}
	return bytes;
}

static void particle_image_controls()
{
	unsigned comparisons = 0;
	for (unsigned channels : { 3u, 4u })
		for (unsigned height : { 2u, 4u, 8u })
			for (unsigned marker : { 0u, 1u })
				for (unsigned descriptor : { 0u, 16u, 32u, 48u })
					for (unsigned identifier : { 0u, 7u })
						for (unsigned alpha : { 0u, 1u, 223u, 224u, 255u }) {
							compare_particle_image(particle_image_fixture(channels, height, marker, descriptor, identifier, alpha));
							++comparisons;
						}
	// TGA permits width 4096 although packed POG headers stop at 4095. The
	// shared pixel normalizer must not impose the POG header's encoding limit
	auto wide = particle_image_fixture(4, 2, 0, 0, 0, 128);
	wide[12] = 0;
	wide[13] = 16;
	wide.resize(18 + 4096 * 2 * 4);
	compare_particle_image(wide);
	++comparisons;
	unsigned rejected = 0;
	for (unsigned variant = 0; variant < 14; ++variant) {
		auto bytes = particle_image_fixture(4, 4, 0, 0, 0, 128);
		if (variant == 0) bytes.clear();
		if (variant == 1) bytes.resize(17);
		if (variant == 2) bytes.pop_back();
		if (variant == 3) bytes[1] = 1;
		if (variant == 4) bytes[2] = 10;
		if (variant == 5) bytes[16] = 16;
		if (variant == 6) bytes[12] = 0;
		if (variant == 7) bytes[14] = 0;
		if (variant == 8) bytes[13] = 17;
		if (variant == 9) bytes[15] = 128;
		if (variant == 10) bytes[8] = 5;
		if (variant == 11) bytes[10] = 5;
		if (variant == 12) bytes[0] = 128;
		if (variant == 13) bytes[14] = 5;
		try {
			(void) xl_compat::read_particle_image(bytes.data(), bytes.size());
		} catch (const std::runtime_error &) {
			++rejected;
			continue;
		}
		throw std::runtime_error("Malformed or unsupported particle image accepted");
	}
	std::cout << "Pinned particle TGA: " << comparisons << " header/pixel/flag comparisons, " << rejected << " malformed/unsupported rejections passed\n";
}
