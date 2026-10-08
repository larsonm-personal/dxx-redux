#include <cstdio>
#include <cstring>
#include <string>
#include <nlohmann/json.hpp>
extern "C" {
#include "args.h"
#include "digi.h"
#include "mission.h"
#ifdef DXX_BUILD_DESCENT_II
#include "bm.h"
#endif
}

static void test_secret_origins(const char *trace_path)
{
#ifdef DXX_BUILD_DESCENT_II
	GameArg.SndDigiSampleRate = SAMPLE_RATE_22K;
	require(properties_init() != 0, "initialize real D2 HAM and sound registry");
#endif
	struct origin_case {
		const char *origins;
		int first;
	};
	// Legacy engines consume only the first origin; launcher admission validates every token
	const origin_case cases[] = {
		{ "1", 1 }, { "2", 2 }, { "1,2", 1 }, { "2,1", 2 }, { " 2 , 1 ", 2 }, { "bad,2", 0 }, { ",2", 0 }, { "0,2", 0 }, { "-1,2", 0 }, { "3,2", 0 }, { "1,bad", 1 }, { "1,,2", 1 }
	};
	nlohmann::json trace = nlohmann::json::array();
	for (const auto &item : cases) {
		nlohmann::json formats = nlohmann::json::array();
		for (const char *extension : {
#ifdef DXX_BUILD_DESCENT_II
		         "mn2",
#endif
		         "msn" }) {
			const char *level_extension = std::strcmp(extension, "mn2") == 0 ? "rl2" : "rdl";
			char descriptor[32];
			std::snprintf(descriptor, sizeof(descriptor), "origins.%s", extension);
			FILE *file = std::fopen(descriptor, "wb");
			require(file != nullptr, "create isolated mission descriptor");
			const int written = std::fprintf(file, "name = Origin fixture\nnum_levels = 2\none.%s\ntwo.%s\nnum_secrets = 1\nsecret.%s,%s\n",
			                                 level_extension, level_extension, level_extension, item.origins);
			const int closed = std::fclose(file);
			require(written > 0 && closed == 0, "write and close mission descriptor");
			require(load_mission_from_current_dir(descriptor) != 0, "load actual mission descriptor");
			require(Last_level == 2, "preserve ordinary mission level list");
			require(Last_secret_level == (item.first ? -1 : 0), "actual secret count respects first origin");
			if (item.first) {
				require(Secret_level_table[0] == item.first, "actual secret table preserves first origin");
				const std::string name = std::string("secret.") + level_extension;
				require(name == Secret_level_names[0], "actual secret name preserved");
			}
			formats.push_back({ { "format", extension }, { "levels", Last_level }, { "secrets", -Last_secret_level }, { "first_origin", item.first } });
			free_mission();
			require(std::remove(descriptor) == 0, "remove owned mission descriptor");
		}
		trace.push_back({ { "origins", item.origins }, { "formats", formats } });
	}
#ifdef DXX_BUILD_DESCENT_II
	gamedata_close();
#endif
	FILE *output = std::fopen(trace_path, "wb");
	require(output != nullptr, "open secret origin trace");
	const auto text = trace.dump(2) + "\n";
	const auto written = std::fwrite(text.data(), 1, text.size(), output);
	const auto closed = std::fclose(output);
	require(written == text.size() && closed == 0, "write complete secret origin trace");
}
