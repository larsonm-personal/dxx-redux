// Exercise the real loader wrapper; no renderer or gameplay certification
#define main stock_metadata_main
#include "../../app/src/main/cpp/headless/headless_metadata_dump_main.cpp"
#undef main
#include "preview.h"
#include "../probe.h"
#include "../lighting.h"
#include <iterator>
#include <iostream>
#include <filesystem>
#include <stdexcept>
#include <cstdlib>

extern "C" void xl_preview_before_swap(void)
{
	std::fputs("Unexpected rendering in headless XL lifecycle test\n", stderr);
	std::abort();
}

namespace
{
void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
void unchanged(int vertex)
{
	int32_t r = 101, g = 202, b = 303;
	xl_preview_vertex(vertex, 65536, &r, &g, &b);
	require(r == 101 && g == 202 && b == 303, "XL colors leaked into inactive state");
}
void colored(const xl_compat::Inventory &source, int vertex)
{
	xl_compat::VertexLighting input;
	for (unsigned i = 0; i < 3; ++i) input.ambient[i] = float(source.vertex_colors[vertex].rgb[i]);
	input.static_light = 65536;
	const auto expected = xl_compat::shade_vertex(input);
	int32_t r = -1, g = -1, b = -1;
	xl_preview_vertex(vertex, 65536, &r, &g, &b);
	require(r == int32_t(expected.rgb[0] * 65536) && g == int32_t(expected.rgb[1] * 65536) && b == int32_t(expected.rgb[2] * 65536), "XL preview colors not applied");
}
void write_bytes(const char *path, const xl_compat::Bytes &bytes)
{
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
	out.close();
	require(bool(out), "cannot write private fixture");
}
} // namespace

int main(int argc, char **argv)
{
	try {
		const auto source_path = find_arg_value(argc, argv, "-xl-source");
		const auto fixture_path = find_arg_value(argc, argv, "-fixture-path");
		require(source_path && fixture_path, "requires original source and private fixture path");
		const auto extra_dir = find_arg_value(argc, argv, "-extra-dir");
		require(extra_dir != nullptr, "requires private fixture directory");
		require(std::filesystem::weakly_canonical(fixture_path) == std::filesystem::weakly_canonical(std::filesystem::path(extra_dir) / "xlprobe.rl2"), "fixture path must identify the mounted test fixture");
		require(!std::filesystem::equivalent(source_path, fixture_path), "original source cannot be the test fixture");
		std::ifstream stream(source_path, std::ios::binary);
		require(bool(stream), "cannot read original source");
		const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
		const auto probe = xl_compat::prepare_gameplay_probe(bytes);
		std::ifstream fixture_stream(fixture_path, std::ios::binary);
		const xl_compat::Bytes fixture_bytes{ std::istreambuf_iterator<char>(fixture_stream), std::istreambuf_iterator<char>() };
		require(fixture_bytes == probe.stock_level, "private fixture must initially match source translation");
		fixture_stream.close();
		int vertex = -1;
		for (unsigned i = 0; i < probe.source.vertex_colors.size(); ++i) {
			const auto &c = probe.source.vertex_colors[i].rgb;
			if (c[0] != c[1] || c[1] != c[2]) {
				vertex = int(i);
				break;
			}
		}
		require(vertex >= 0, "fixture has no chromatic vertex");
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission(find_arg_value(argc, argv, "-mission"), error, sizeof(error)), error);
		unchanged(vertex);
		require(load_level("xlprobe.rl2") == 0, "valid preview load rejected");
		colored(probe.source, vertex);
		require(load_level("stockprobe.rl2") == 0, "stock alias load rejected");
		unchanged(vertex);
		require(load_level("xlprobe.rl2") == 0, "preview reload rejected");
		colored(probe.source, vertex);
		// Change a coordinate bit without changing counts or file size
		auto corrupt = probe.stock_level;
		corrupt.at(probe.source.mine_offset + 5) ^= 1;
		write_bytes(fixture_path, corrupt);
		const int rejected = load_level("xlprobe.rl2");
		write_bytes(fixture_path, probe.stock_level);
		require(rejected != 0, "mismatched source/fixture accepted");
		unchanged(vertex);
		require(load_level("xlprobe.rl2") == 0, "restored preview load rejected");
		colored(probe.source, vertex);
		std::cout << "XL preview lifecycle: activate, clear, reload, reject mismatch, recover passed; rendering/gameplay unverified\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL lifecycle FAILED: " << error.what() << '\n';
		return 1;
	}
}
