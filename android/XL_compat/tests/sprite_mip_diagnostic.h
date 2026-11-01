// Independent double-precision filtering calculation for diagnostic readbacks
struct SpriteMipDiagnostic {
	struct Level {
		int width, height;
		std::vector<unsigned char> pixels;
	};
	std::vector<Level> levels;
	explicit SpriteMipDiagnostic(const Bitmap &image)
	{
		for (int width = image.bitmap.bm_w, height = image.bitmap.bm_h, level = 0;; ++level) {
			Level current{ width, height, std::vector<unsigned char>(std::size_t(width) * height * 4) };
			glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, current.pixels.data());
			levels.push_back(std::move(current));
			if (width == 1 && height == 1) break;
			width = (std::max) (1, width / 2);
			height = (std::max) (1, height / 2);
		}
	}
	std::array<double, 4> sample(unsigned level, double u, double v, bool linear) const
	{
		const auto &mip = levels[level];
		u = std::clamp(u, 0., 1.);
		v = std::clamp(v, 0., 1.);
		std::array<double, 4> result{};
		if (!linear) {
			const int x = (std::min) (mip.width - 1, int(u * mip.width));
			const int y = (std::min) (mip.height - 1, int(v * mip.height));
			for (unsigned c = 0; c < 4; ++c) result[c] = mip.pixels[(y * mip.width + x) * 4 + c] / 255.;
			return result;
		}
		const double px = u * mip.width - .5, py = v * mip.height - .5;
		const int ix = int(std::floor(px)), iy = int(std::floor(py));
		const double fx = px - ix, fy = py - iy;
		for (int dy = 0; dy < 2; ++dy)
			for (int dx = 0; dx < 2; ++dx) {
				const int x = ix + dx, y = iy + dy;
				if (x < 0 || y < 0 || x >= mip.width || y >= mip.height) continue;
				const double weight = (dx ? fx : 1 - fx) * (dy ? fy : 1 - fy);
				for (unsigned c = 0; c < 4; ++c) result[c] += weight * mip.pixels[(y * mip.width + x) * 4 + c] / 255.;
			}
		return result;
	}
	bool matches(const xl_compat::ParticleQuad &quad, unsigned pixel, GLenum min_filter, GLenum mag_filter,
	             bool mesa_lod, const std::array<unsigned char, 64 * 64 * 4> &actual) const
	{
		const double left = (double(quad.vertices[0].position[0]) + 1) * 32;
		const double right = (double(quad.vertices[1].position[0]) + 1) * 32;
		const double top = (double(quad.vertices[0].position[1]) + 1) * 32;
		const double bottom = (double(quad.vertices[3].position[1]) + 1) * 32;
		const double u = (double(pixel % 64) + .5 - left) / (right - left);
		const double v = (top - double(pixel / 64) - .5) / (top - bottom);
		const double footprint = (std::max) (levels[0].width / (right - left), levels[0].height / (top - bottom));
		double lod = std::log2(footprint);
		if (mesa_lod) {
			const double squared = footprint * footprint, exponent = std::floor(std::log2(squared));
			lod = .5 * (exponent - 1 + squared / std::exp2(exponent));
		}
		const double crossover = mag_filter == GL_LINEAR && min_filter == GL_NEAREST_MIPMAP_NEAREST ? .5 : 0.;
		std::array<double, 4> result{};
		if (lod <= crossover) result = sample(0, u, v, mag_filter == GL_LINEAR);
		else if (min_filter == GL_NEAREST || min_filter == GL_LINEAR) result = sample(0, u, v, min_filter == GL_LINEAR);
		else {
			lod = std::clamp(lod, 0., double(levels.size() - 1));
			const bool linear = min_filter == GL_LINEAR_MIPMAP_NEAREST || min_filter == GL_LINEAR_MIPMAP_LINEAR;
			if (min_filter == GL_NEAREST_MIPMAP_NEAREST || min_filter == GL_LINEAR_MIPMAP_NEAREST)
				result = sample(unsigned(std::floor(lod + .5)), u, v, linear);
			else {
				const unsigned lower = unsigned(std::floor(lod));
				const auto a = sample(lower, u, v, linear);
				const auto b = sample((std::min) (lower + 1, unsigned(levels.size() - 1)), u, v, linear);
				for (unsigned c = 0; c < 4; ++c) result[c] = a[c] * (1 - (lod - lower)) + b[c] * (lod - lower);
			}
		}
		for (unsigned c = 0; c < 4; ++c) result[c] *= quad.vertices[0].color[c];
		const std::array<double, 4> background{ .2, .4, .6, 1 };
		for (unsigned c = 0; c < 4; ++c) {
			const int expected = int(std::lround(255 * (result[c] * result[3] + background[c] * (1 - result[3]))));
			if (std::abs(expected - int(actual[pixel * 4 + c])) > 1) return false;
		}
		return true;
	}
};
