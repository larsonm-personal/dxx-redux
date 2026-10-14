/* Host CPU fixture: production Android decoder/index, real PhysFS, no renderer */
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <physfs.h>
extern "C" {
#include "pngfile.h"
#include "android_log.h"
#include "android_texture_usage.h"
volatile int debug_log_enabled[DLOG_COUNT]{};
void debug_log(int category, const char *format, ...)
{
	if (category != DLOG_TEXTURE) return;
	va_list args;
	va_start(args, format);
	std::vfprintf(stderr, format, args);
	std::fputc('\n', stderr);
	va_end(args);
}
void debug_log_force(int, const char *, ...) {}
}
using nlohmann::json;
static int current_step = -1;
static std::string current_query;
static void require(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL step=%d query=%s: %s\n", current_step, current_query.c_str(), message);
		std::exit(1);
	}
}
static std::vector<unsigned char> read_bytes(const std::string &path)
{
	std::ifstream in(path, std::ios::binary | std::ios::ate);
	require(in.good(), "independent expected payload opens");
	const auto size = in.tellg();
	require(size >= 0, "independent expected payload size is available");
	std::vector<unsigned char> bytes(static_cast<size_t>(size));
	in.seekg(0);
	in.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	require(in.good(), "independent expected payload reads fully");
	return bytes;
}
int main(int argc, char **argv)
{
	if (argc != 3) return 2;
	std::ifstream input(argv[1]);
	const auto scenario = json::parse(input);
	require(PHYSFS_init(argv[0]), "PhysFS initializes");
	PHYSFS_Version compiled_version{};
	PHYSFS_Version runtime_version{};
	PHYSFS_VERSION(&compiled_version);
	PHYSFS_getLinkedVersion(&runtime_version);
	auto version_string = [](const PHYSFS_Version &version) {
		return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
	};
	require(version_string(compiled_version) == version_string(runtime_version), "compiled and runtime PhysFS versions match");
	if (scenario.contains("expected_physfs_version"))
		require(version_string(runtime_version) == scenario.at("expected_physfs_version"), "runtime PhysFS version matches required pin");
	std::vector<std::string> mounted;
	json result = { { "schema", 1 }, { "seed", scenario.at("seed") }, { "scope", "actual pngfile.c CPU decode and path caches; no ETC2 GPU decompression, upload, rendering or menu wiring" }, { "steps", json::array() } };
	result["physfs_compiled_version"] = version_string(compiled_version);
	result["physfs_runtime_version"] = version_string(runtime_version);
	unsigned ktx_loaded = 0, png_loaded = 0, misses = 0, repeated = 0;
	for (const auto &step : scenario.at("steps")) {
		++current_step;
		if (current_step % 10 == 0) {
			std::fprintf(stderr, "PROGRESS step=%d label=%s\n", current_step, step.at("label").get<std::string>().c_str());
		}
		for (const auto &path : mounted) require(PHYSFS_unmount(path.c_str()), "previous archive unmounts");
		mounted.clear();
		// Input lists highest priority first; real PhysFS append preserves that order
		for (const auto &mount : step.at("mounts")) {
			const auto path = mount.at("path").get<std::string>();
			const auto point = mount.at("point").get<std::string>();
			require(PHYSFS_mount(path.c_str(), point.empty() ? nullptr : point.c_str(), 1), "requested archive mounts");
			mounted.push_back(path);
		}
		if (step.at("clear_cache")) {
			clear_texture_lookup_cache();
			clear_texture_lookup_cache();
		}
		json observations = json::array();
		for (const auto &sample : step.at("samples")) {
			current_query = sample.at("query");
			const auto expected_source = sample.at("source").get<std::string>();
			const char *source = android_texture_source(current_query.c_str());
			if ((source ? std::string(source) : "") != expected_source)
				std::fprintf(stderr, "SOURCE expected=%s actual=%s\n", expected_source.c_str(), source ? source : "(none)");
			require((source ? std::string(source) : "") == expected_source, "source archive matches independent selection");
			json observed = { { "query", current_query }, { "source", source ? source : "" } };
			for (int attempt = 0; attempt < 2; ++attempt) {
				const bool wanted = !sample.at("expected").is_null();
				if (sample.at("kind") == "ktx2") {
					etc2_file_data data{};
					const int loaded = read_ktx2_file(current_query.c_str(), &data);
					require((loaded != 0) == wanted, "KTX load/miss matches expected cache and namespace state");
					if (loaded) {
						const auto expected = read_bytes(sample.at("expected"));
						require(data.width == sample.at("width") && data.height == sample.at("height") &&
						            data.orig_width == sample.at("orig_width") && data.orig_height == sample.at("orig_height") &&
						            data.format == sample.at("format") && data.mip_count == sample.at("mip_count"),
						        "KTX dimensions, original dimensions, format and mip count match ZIP/header oracle");
						require(data.filedata_size == expected.size() &&
						            std::memcmp(data.filedata, expected.data(), expected.size()) == 0,
						        "KTX packed CPU mip payload equals independent archive levels");
						observed.update({ { "width", data.width }, { "height", data.height }, { "orig_width", data.orig_width }, { "orig_height", data.orig_height }, { "format", data.format }, { "mip_count", data.mip_count }, { "payload_bytes", data.filedata_size }, { "payload_sha256", sample.at("sha256") } });
						std::free(data.filedata);
						if (!attempt) ++ktx_loaded;
					} else if (!attempt) ++misses;
				} else {
					png_data data{};
					const int loaded = read_png(current_query.c_str(), &data);
					require((loaded != 0) == wanted, "PNG load/miss matches expected cache and namespace state");
					if (loaded) {
						const auto expected = read_bytes(sample.at("expected"));
						require(data.width == sample.at("width") && data.height == sample.at("height") &&
						            data.channels == sample.at("channels") && data.depth == 8 && data.paletted == 0 &&
						            data.alpha == (data.channels == 2 || data.channels == 4) && data.color == (data.channels >= 3),
						        "PNG dimensions, channels and flags match Pillow oracle");
						const size_t bytes = static_cast<size_t>(data.width) * data.height * data.channels;
						require(bytes == expected.size() && std::memcmp(data.data, expected.data(), expected.size()) == 0,
						        "PNG decoded CPU pixels equal independent Pillow decode");
						observed.update({ { "width", data.width }, { "height", data.height }, { "channels", data.channels }, { "payload_bytes", bytes }, { "payload_sha256", sample.at("sha256") } });
						std::free(data.data);
						if (!attempt) ++png_loaded;
					} else if (!attempt) ++misses;
				}
				if (attempt) ++repeated;
			}
			observations.push_back(observed);
		}
		result["steps"].push_back({ { "index", current_step }, { "label", step.at("label") }, { "clear_cache", step.at("clear_cache") }, { "observations", observations } });
	}
	clear_texture_lookup_cache();
	for (const auto &path : mounted) require(PHYSFS_unmount(path.c_str()), "final archive unmounts");
	require(PHYSFS_deinit(), "PhysFS closes");
	result.update({ { "ktx_loaded", ktx_loaded }, { "png_loaded", png_loaded }, { "misses", misses }, { "repeat_reads", repeated }, { "status", "pass" } });
	std::ofstream out(argv[2]);
	out << result.dump(2) << '\n';
	std::printf("PASS steps=%d KTX=%u PNG=%u misses=%u repeat_reads=%u\n", current_step + 1,
	            ktx_loaded, png_loaded, misses, repeated);
}
