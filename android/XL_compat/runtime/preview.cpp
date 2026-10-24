// Explicit static-light preview only; no production mission admission
#include "preview.h"
#include "../probe.h"
#include "../lighting.h"
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "args.h"
#include "console.h"
#include "dxxerror.h"
#include <physfs.h>
int xl_stock_load_level(const char *filename);
}

namespace
{
std::unique_ptr<xl_compat::Inventory> source;
uint64_t vertex_calls = 0;
const char *option(const char *name)
{
	for (int i = 1; i < Num_args; ++i)
		if (!std::strcmp(Args[i], name)) return i + 1 < Num_args ? Args[i + 1] : "";
	return nullptr;
}
} // namespace

extern "C" int load_level(const char *filename)
{
	source.reset();
	vertex_calls = 0;
	const int result = xl_stock_load_level(filename);
	if (result || !option("-xl-static-light-preview")) return result;
	const char *path = option("-xl-source");
	// The development fixture contains one level; unrelated missions remain stock
	const char *base = std::strrchr(filename, '/');
	base = base ? base + 1 : filename;
	if (std::strcmp(base, "xlprobe.rl2")) return result;
	try {
		if (!path || !*path) throw std::runtime_error("-xl-source is required");
		std::ifstream stream(path, std::ios::binary | std::ios::ate);
		if (!stream || stream.tellg() < 0 || stream.tellg() > 64 * 1024 * 1024)
			throw std::runtime_error("cannot read bounded XL source");
		const auto size = stream.tellg();
		stream.seekg(0);
		xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
		if (stream.bad() || bytes.size() != std::size_t(size)) throw std::runtime_error("XL source read failed");
		auto probe = xl_compat::prepare_gameplay_probe(bytes);
		std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> loaded(PHYSFS_openRead(filename), PHYSFS_close);
		if (!loaded || PHYSFS_fileLength(loaded.get()) != PHYSFS_sint64(probe.stock_level.size()))
			throw std::runtime_error("loaded fixture is not the source translation");
		xl_compat::Bytes loaded_bytes(probe.stock_level.size());
		if (PHYSFS_read(loaded.get(), loaded_bytes.data(), 1, PHYSFS_uint32(loaded_bytes.size())) != PHYSFS_sint64(loaded_bytes.size()) || loaded_bytes != probe.stock_level)
			throw std::runtime_error("loaded fixture bytes differ from the source translation");
		auto level = std::make_unique<xl_compat::Inventory>(std::move(probe.source));
		if (level->vertices.size() != std::size_t(Num_vertices) || level->segments.size() != std::size_t(Num_segments))
			throw std::runtime_error("XL source does not match loaded geometry counts");
		for (int i = 0; i < Num_vertices; ++i) {
			const auto &v = level->vertices[i];
			if (v[0] != Vertices[i].x || v[1] != Vertices[i].y || v[2] != Vertices[i].z)
				throw std::runtime_error("XL source does not match loaded vertices");
		}
		for (int i = 0; i < Num_segments; ++i)
			for (unsigned j = 0; j < 8; ++j)
				if (level->segments[i].vertices[j] != Segments[i].verts[j])
					throw std::runtime_error("XL source does not match loaded segment vertices");
		source = std::move(level);
		con_printf(CON_URGENT, "XL STATIC LIGHT PREVIEW: dynamic lighting and face effects are unverified; not production support");
		return result;
	} catch (const std::exception &error) {
		con_printf(CON_URGENT, "XL preview rejected: %s", error.what());
		return 1;
	}
}

extern "C" void xl_preview_vertex(int vertex, int32_t light, int32_t *r, int32_t *g, int32_t *b)
{
	if (!source || vertex < 0 || std::size_t(vertex) >= source->vertex_colors.size()) return;
	try {
		xl_compat::VertexLighting input;
		for (unsigned i = 0; i < 3; ++i) input.ambient[i] = float(source->vertex_colors[vertex].rgb[i]);
		input.static_light = light;
		const auto shaded = xl_compat::shade_vertex(input);
		*r = int32_t(shaded.rgb[0] * 65536);
		*g = int32_t(shaded.rgb[1] * 65536);
		*b = int32_t(shaded.rgb[2] * 65536);
		++vertex_calls;
	} catch (const std::exception &error) {
		Error("XL preview lighting failed: %s", error.what());
	}
}

extern "C" uint64_t xl_preview_vertex_calls(void)
{
	return vertex_calls;
}
