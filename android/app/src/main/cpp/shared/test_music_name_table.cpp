#include "music_name_table.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>

static std::string record(const std::string &path, const std::string &alias, const std::string &name)
{
	return "{\"paths\":[\"" + path + "\"],\"aliases\":[\"" + alias + "\"],\"name\":\"" + name + "\"}";
}

static int load_mission(const std::string &json)
{
	return music_name_table_load_mission(json.data(), json.size());
}

// This focused metadata contract also runs with NDEBUG, independently of legacy asserts
static int schema_version_tests()
{
	const char *rejected[] = { "1.0", "1.5", "1e0", "0", "2", "-1", "4294967297",
		                       "9223372036854775807", "-9223372036854775808",
		                       "18446744073709551615", "true", "null", "\"1\"", "[]", "{}" };
	for (const bool jukebox : { false, true }) {
		const auto load = jukebox ? music_name_table_load_jukebox : music_name_table_load_mission;
		const auto lookup = jukebox ? music_name_table_lookup_jukebox : music_name_table_lookup_mission;
		const std::string prior = "{\"version\":1,\"records\":[" + record("prior.ogg", "prior", "Prior") + "]}";
		const int loaded = jukebox ? load(prior.data(), prior.size()) : load_mission(prior);
		if (!loaded) {
			std::fprintf(stderr, "Supported music sidecar version rejected\n");
			return 1;
		}
		for (const auto version : rejected) {
			const std::string candidate = std::string("{\"version\":") + version + ",\"records\":[]}";
			if (load(candidate.data(), candidate.size())) {
				std::fprintf(stderr, "Unsupported music sidecar version accepted: %s\n", version);
				return 1;
			}
			const char *name = lookup("prior.ogg");
			if (!name || std::strcmp(name, "Prior") != 0) {
				std::fprintf(stderr, "Rejected music sidecar replaced prior table: %s\n", version);
				return 1;
			}
		}
		const std::string missing = "{\"records\":[]}";
		if (load(missing.data(), missing.size()) || !lookup("prior.ogg") ||
		    std::strcmp(lookup("prior.ogg"), "Prior") != 0) {
			std::fprintf(stderr, "Missing music sidecar version changed prior table\n");
			return 1;
		}
		const std::string empty = "{\"version\":1,\"records\":[]}";
		if (!load(empty.data(), empty.size()) || lookup("prior.ogg")) {
			std::fprintf(stderr, "Supported music sidecar did not replace prior table\n");
			return 1;
		}
	}
	std::puts("Music sidecar schema version contract passed for both tables");
	return 0;
}

int main(int argc, char **argv)
{
	if (argc == 2 && std::strcmp(argv[1], "--schema-version") == 0) return schema_version_tests();
	if (argc != 1) return 2;
	if (schema_version_tests()) return 1;
	const std::string collision =
	    "{\"version\":1,\"records\":[" + record("music/a/game01.ogg", "game01.ogg", "First") + "," +
	    record("music/b/game01.ogg", "game01.ogg", "Second") + "]}";
	assert(load_mission(collision));
	assert(std::string(music_name_table_lookup_mission("MUSIC/B/GAME01.OGG")) == "Second");
	assert(music_name_table_lookup_mission("game01.ogg") == nullptr);

	const std::string escaped =
	    "{\"version\":1,\"records\":[{\"paths\":[\"music/track.ogg\"],\"aliases\":[],"
	    "\"name\":\"Line\\nSmile \\u263a \\ud83d\\ude80\"}]}";
	assert(load_mission(escaped));
	assert(std::string(music_name_table_lookup_mission("music/track.ogg")) == "Line\nSmile \xe2\x98\xba \xf0\x9f\x9a\x80");

	assert(!load_mission("{\"version\":1,\"version\":1,\"records\":[]}"));
	assert(std::string(music_name_table_lookup_mission("music/track.ogg")) == "Line\nSmile \xe2\x98\xba \xf0\x9f\x9a\x80");
	assert(!load_mission("{\"version\":1,\"records\":["));
	assert(!load_mission("{\"version\":1,\"records\":[]} trailing"));
	std::string invalid_utf8 =
	    "{\"version\":1,\"records\":[{\"paths\":[\"bad.ogg\"],\"aliases\":[],\"name\":\"";
	invalid_utf8.push_back(static_cast<char>(0xff));
	invalid_utf8 += "\"}]}";
	assert(!load_mission(invalid_utf8));

	const std::string name512(512, 'n');
	const std::string name513(513, 'n');
	assert(load_mission("{\"version\":1,\"records\":[" + record("p", "a", name512) + "]}"));
	assert(!load_mission("{\"version\":1,\"records\":[" + record("q", "b", name513) + "]}"));
	const std::string path1024(1024, 'p');
	const std::string path1025(1025, 'p');
	assert(load_mission("{\"version\":1,\"records\":[" + record(path1024, "a", "ok") + "]}"));
	assert(!load_mission("{\"version\":1,\"records\":[" + record(path1025, "a", "bad") + "]}"));

	std::string records257 = "{\"version\":1,\"records\":[";
	for (int i = 0; i < 257; ++i) {
		if (i) records257 += ',';
		records257 += record("track-" + std::to_string(i) + ".ogg", "alias-" + std::to_string(i),
		                     "Track " + std::to_string(i));
	}
	records257 += "]}";
	assert(load_mission(records257));
	assert(std::string(music_name_table_lookup_mission("track-256.ogg")) == "Track 256");

	std::string too_many = "{\"version\":1,\"records\":[";
	for (int i = 0; i < 4097; ++i) {
		if (i) too_many += ',';
		too_many += record("p" + std::to_string(i), "a" + std::to_string(i), "n");
	}
	too_many += "]}";
	assert(!load_mission(too_many));

	const std::string jukebox =
	    "{\"version\":1,\"records\":[" + record("C:/Music/one.ogg", "one.ogg", "Jukebox") + "]}";
	assert(music_name_table_load_jukebox(jukebox.data(), jukebox.size()));
	assert(std::string(music_name_table_lookup_jukebox("C:\\Music\\one.ogg")) == "Jukebox");
	assert(music_name_table_lookup_jukebox("one.ogg") == nullptr);

	music_name_table_clear_mission();
	assert(music_name_table_lookup_mission("music/track.ogg") == nullptr);
	return 0;
}
