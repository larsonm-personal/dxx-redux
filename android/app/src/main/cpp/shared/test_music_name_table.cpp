#include "music_name_table.h"

#include <cassert>
#include <string>

static std::string record(const std::string &path, const std::string &alias, const std::string &name)
{
	return "{\"paths\":[\"" + path + "\"],\"aliases\":[\"" + alias + "\"],\"name\":\"" + name + "\"}";
}

static int load_mission(const std::string &json)
{
	return music_name_table_load_mission(json.data(), json.size());
}

int main()
{
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
