// Pinned named-audio source contracts; actual output has separate runtime tests
#include "hog.h"
#include "inventory.h"
#include "level_text.h"
#include "named_sound.h"
#include "wave.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

static void require(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}

namespace reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETVAL(value) return value;
#define DBG           0
#include "xl_reference_audio_constants.h"
constexpr int AUDIO_U8 = 8;
struct Mix_Chunk {
	std::uint32_t alen = 0;
};
struct tAddonSound {
	Mix_Chunk *pChunk;
	const char *szSoundFile;
};
struct tTextIndex {
	int nId;
	const char *pszText;
};
struct CTextData {
	tTextIndex *index = nullptr;
	int nMessages = 0;
};

class CTrigger
{
  public:
	struct {
		fix tOperated = -1;
		int nChannel = -1, nObject = -1;
	} m_info;
	int type = TT_SOUND, value = I2X(1), flags = 0;
	std::array<fix, 2> time{ 1, -1 };
	int operations = 0, countdowns = 0, actor = -1, player = -1, object_event = -1;
	bool fly_through = false;
	int Type() const
	{
		return type;
	}
	int GetValue() const
	{
		return value;
	}
	fix GetTime(int i) const
	{
		return time.at(i);
	}
	void SetTime(int i, fix value_)
	{
		time.at(i) = value_;
	}
	bool Flagged(int mask, int match = 0) const
	{
		return match ? (flags & mask) == match : (flags & mask) != 0;
	}
	void SetFlags(int bits)
	{
		flags |= bits;
	}
	void ClearFlags(int bits)
	{
		flags &= ~bits;
	}
	bool IsFlyThrough() const
	{
		return fly_through;
	}
	bool IsDelayed();
	int32_t DoPlaySound(int16_t);
	// The frame oracle checks dispatch arguments; it does not substitute for the
	// separate ordinary wall/object activation tests required by the runtime
	void Operate(int object, int player_, int shot, bool attached)
	{
		require(!shot, "Source autoplay unexpectedly requests a shot");
		++operations;
		actor = object;
		player = player_;
		object_event = attached;
	}
	void Countdown(bool)
	{
		++countdowns;
	}
};
template <class T>
struct CArray {
	std::vector<T> rows;
	T *Buffer()
	{
		return rows.data();
	}
	int Length() const
	{
		return static_cast<int>(rows.size());
	}
};
struct {
	CTextData soundData;
	struct {
		fix xGame = I2X(10);
	} timeData;
	struct {
		CArray<CTrigger> triggers[2];
		int m_nTriggers[2]{};
	} trigData;
} gameData;
struct {
	struct {
		bool bD2XLevel = true, bReadOnly = false;
	} app;
} gameStates;
struct {
	struct {
		int audioSampleRate = 44100;
	} sound;
} options;
auto *gameOpts = &options;
struct {
	int nObject = 7;
} local_player;
bool multiplayer = false, host = true;
bool IAmGameHost()
{
	return host;
}
#define GEOTRIGGERS   gameData.trigData.triggers[0]
#define OBJTRIGGERS   gameData.trigData.triggers[1]
#define LOCALPLAYER   local_player
#define N_LOCALPLAYER 0
#define IsMultiGame   multiplayer

struct {
	struct {
		const char *szData[1]{ "data" };
		const char *szSounds[5]{ "", "", "", "", "addon" };
	} game;
	struct {
		const char *szSounds[2]{ "mission", "level" };
		char szLevel[32]{};
	} mods;
	struct {
		const char *szCache = "cache";
	} var;
} gameFolders;
using ResourceKey = std::pair<std::string, std::string>;
std::map<ResourceKey, Mix_Chunk> files;
Mix_Chunk *extracted_file = nullptr;
ResourceKey selected;
int loads = 0, extracts = 0, exists = 0;
bool load_failure = false;
class CFile
{
  public:
	bool Extract(const char *name, const char *, int, const char *)
	{
		++extracts;
		const auto found = files.find({ "hog", xl_compat::hog_key(name) });
		if (found == files.end()) return false;
		extracted_file = &found->second;
		selected = found->first;
		return true;
	}
	bool Exist(const char *name, const char *folder, int)
	{
		++exists;
		return files.count({ folder, name }) != 0;
	}
};
Mix_Chunk *CFileOpenRWOps(const char *name, const char *folder)
{
	if (std::string(folder) == "cache" && std::string(name) == "d2x-temp.wav") return extracted_file;
	selected = { folder, name };
	const auto found = files.find(selected);
	return found == files.end() ? nullptr : &found->second;
}
Mix_Chunk *Mix_LoadWAV_RW(Mix_Chunk *input, int close)
{
	require(close == 1, "Source audio did not transfer stream ownership");
	++loads;
	return load_failure ? nullptr : input;
}
void Mix_FreeChunk(Mix_Chunk *) {}
Mix_Chunk *LoadAddonSound(const char *, uint8_t *);
class CAudioChannel
{
  public:
	struct {
		Mix_Chunk *pMixChunk = nullptr;
	} m_info;
	fix Duration();
};
struct Audio {
	struct Call {
		int sound, sound_class, volume, pan, looping, first, last, object, speed;
		std::string name;
	};
	std::vector<Call> calls;
	int format = 16, result = 3;
	bool resolve = false;
	CAudioChannel channel;
	int Format() const
	{
		return format;
	}
	CAudioChannel *Channel(int index)
	{
		require(index == result, "Source queried a different audio channel");
		return &channel;
	}
	int StartSound(int sound, int sound_class, int volume, int pan, int looping, int first, int last, int object, int speed, const char *name)
	{
		calls.push_back({ sound, sound_class, volume, pan, looping, first, last, object, speed, name });
		if (resolve) {
			channel.m_info.pMixChunk = LoadAddonSound(name, nullptr);
			if (!channel.m_info.pMixChunk) return -1;
		}
		return result;
	}
} audio;
#include "xl_reference_named_audio.h"

void reset()
{
	for (auto &entry : addonSounds) entry.pChunk = nullptr;
	files.clear();
	extracted_file = nullptr;
	selected = {};
	loads = extracts = exists = 0;
	load_failure = false;
	gameStates.app.bReadOnly = false;
	gameStates.app.bD2XLevel = true;
	gameData = {};
	gameData.timeData.xGame = I2X(10);
	audio = {};
	options.sound.audioSampleRate = 44100;
	multiplayer = false;
	host = true;
}
} // namespace reference

namespace
{
void source_actions()
{
	using namespace reference;
	reset();
	tTextIndex row{ 1, "voice.wav" };
	gameData.soundData = { &row, 1 };
	Mix_Chunk clip{ 44100 * 4 };
	audio.channel.m_info.pMixChunk = &clip;
	require(audio.channel.Duration() == I2X(1) + I2X(1) / 2, "Source duration lost its half-second addition");
	unsigned cases = 0;
	for (int seconds : { -100, -1, 0, 1, 2, 100 }) {
		for (int delta : { -1, 0, 1 }) {
			for (bool failure : { false, true }) {
				CTrigger trigger;
				trigger.time = { seconds, I2X(3) };
				trigger.value += I2X(1) / 2;
				trigger.m_info.tOperated = gameData.timeData.xGame - I2X(3) - delta;
				audio.result = failure ? -1 : 3;
				audio.calls.clear();
				const bool attempt = seconds < 0 || delta > 0;
				require(!trigger.IsDelayed(), "Sound action was interpreted as a scheduler delay");
				require(trigger.DoPlaySound(9) == int(attempt), "Source sound cooldown boundary changed");
				require(audio.calls.size() == std::size_t(attempt), "Source sound attempted an unexpected number of starts");
				require(trigger.Flagged(TF_PLAYING_SOUND) == (attempt && !failure), "Failed source sound acquired playing state");
				if (attempt) {
					const auto &call = audio.calls.back();
					require(call.sound == -1 && call.sound_class == 0 && call.volume == I2X(1) && call.pan == 32767 && call.object == -1 && call.speed == I2X(1) && call.name == "voice.wav", "Source sound request parameters differ");
					require(call.looping == int(seconds < 0) && call.first == (seconds < 0 ? -1 : 0) && call.last == (seconds < 0 ? -1 : seconds - 1), "Source repetition request differs");
					require(MixerLoops(call.looping, call.first, call.last) == (seconds <= 0 ? -1 : seconds - 1), "Source SDL loop count differs");
				}
				const auto expected = attempt && !failure && seconds >= 0 ? audio.channel.Duration() * seconds : I2X(3);
				require(trigger.time[1] == expected, "Source repeat cooldown differs");
				++cases;
			}
		}
	}
	for (bool disabled : { false, true }) {
		CTrigger trigger;
		trigger.flags = disabled ? TF_DISABLED : 0;
		trigger.value = disabled ? I2X(1) : I2X(2);
		audio.calls.clear();
		require(trigger.DoPlaySound(9) == 0 && audio.calls.empty(), "Missing or disabled source sound started");
		++cases;
	}
	for (int format : { AUDIO_U8, 16 }) {
		for (int rate : { 11025, 22050, 44100, 48000 }) {
			options.sound.audioSampleRate = rate;
			audio.format = format;
			clip.alen = rate * (format == AUDIO_U8 ? 2 : 4) * 2;
			require(audio.channel.Duration() == I2X(2) + I2X(1) / 2, "Source converted-byte duration depends on the wrong mixer format");
			++cases;
		}
	}
	audio.channel.m_info.pMixChunk = nullptr;
	require(audio.channel.Duration() == 0, "Source missing chunk has a nonzero duration");
	std::cout << "Named audio source: " << cases + 1 << " action/duration controls\n";
}

void source_resources()
{
	using namespace reference;
	unsigned cases = 0;
	for (int mask = 0; mask < 16; ++mask) {
		for (bool read_only : { false, true }) {
			reset();
			gameStates.app.bReadOnly = read_only;
			const char *folders[] = { "hog", "level", "mission", "addon" };
			int expected = -1;
			for (int i = 3; i >= 0; --i)
				if (mask & (1 << i)) {
					files[{ folders[i], "voice.wav" }] = { 100 };
					if (i || !read_only) expected = i;
				}
			std::uint8_t builtin = 123;
			const auto *chunk = LoadAddonSound("voice.wav", &builtin);
			require(bool(chunk) == (expected >= 0), "Source resource resolution availability differs");
			if (chunk) require(selected == ResourceKey(folders[expected], "voice.wav") && builtin == 0, "Source resource precedence or ownership differs");
			if (read_only) require(extracts == 0, "Read-only source extracted a HOG sound");
			++cases;
		}
	}
	reset();
	files[{ "hog", "voice.wav" }] = { 100 };
	files[{ "level", "voice.wav" }] = { 100 };
	load_failure = true;
	require(!LoadAddonSound("voice.wav", nullptr) && exists == 0 && loads == 1, "Malformed selected source unexpectedly fell back to another resource");
	++cases;
	for (const char *name : { "0", "00:ignored", "missileflight-small.wav", "missileflight-s" }) {
		reset();
		files[{ "hog", "missileflight-small.wav" }] = { 100 };
		std::uint8_t builtin = 0;
		auto *chunk = LoadAddonSound(name, &builtin);
		require(chunk && builtin == 1 && selected.second == "missileflight-small.wav", "Source addon alias/prefix resolution changed");
		require(LoadAddonSound(name, &builtin) == chunk && loads == 1 && extracts == 1, "Source builtin cache was not reused");
		++cases;
	}
	reset();
	require(!LoadAddonSound("43", nullptr) && extracts == 0 && loads == 0, "Out-of-range source builtin index reached storage");
	++cases;
	std::cout << "Named audio source: " << cases << " resource precedence/cache controls\n";
}

void source_scopes()
{
	using namespace reference;
	unsigned cases = 0;
	for (int ordinal : { -9999, -12, -1, 1, 2, 99, 100, 9999 }) {
		for (const char *authored : { "voice.wav", "35", "035:unused", "waterfall", "0", "steam" }) {
			const auto paths = xl_compat::named_sound_paths("custom", authored, ordinal);
			const std::string root = "mods/custom/sounds/";
			const std::string level = root + LevelFolder(ordinal);
			for (int mask = 0; mask < 8; ++mask) {
				reset();
				gameFolders.mods.szSounds[0] = root.c_str();
				gameFolders.mods.szSounds[1] = level.c_str();
				const auto name = xl_compat::named_sound_filename(authored);
				if (mask & 1) files[{ "hog", name }] = { 100 };
				if (mask & 2) files[{ level, name }] = { 200 };
				if (mask & 4) files[{ root, name }] = { 300 };
				const auto *selected_chunk = LoadAddonSound(authored, nullptr);
				std::string candidate;
				for (std::size_t i = 0; i < paths.size(); ++i) {
					const ResourceKey key = i == 0 ? ResourceKey{ "hog", paths[i] } : ResourceKey{ i == 1 ? level : root, paths[i].substr((i == 1 ? level : root).size()) };
					if (files.count(key)) {
						candidate = paths[i];
						break;
					}
				}
				require(bool(selected_chunk) == !candidate.empty(), "Named sound folder availability differs from XL");
				if (selected_chunk) {
					const auto source_path = selected.first == "hog" ? selected.second : selected.first + selected.second;
					require(candidate == source_path, "Named sound folder precedence or ordinal differs from XL");
				}
				// The source unload releases alias caches before the next level's lookup
				FreeAddonSounds();
				for (const auto &addon : addonSounds) require(!addon.pChunk, "Source unload retained an addon from the prior level");
				++cases;
			}
		}
	}
	gameFolders.mods.szSounds[0] = "mission";
	gameFolders.mods.szSounds[1] = "level";
	for (const auto &invalid : { std::string{}, std::string(".."), std::string("other/mission"), std::string("a\\b"), std::string("a:b"), std::string("a\n") }) {
		bool rejected = false;
		try {
			(void) xl_compat::named_sound_paths(invalid, "sound.wav", 1);
		} catch (const std::exception &) {
			rejected = true;
		}
		require(rejected, "Named audio accepted an unsafe mission path component");
	}
	std::cout << "Named audio scopes: " << cases << " source lookup/ordinal/cache comparisons and six invalid identities\n";
}

void source_lifecycle()
{
	using namespace reference;
	unsigned cases = 0;
	for (bool fly : { false, true })
		for (bool once : { false, true })
			for (bool permanent : { false, true })
				for (int delta : { -1, 0, 1 }) {
					reset();
					GEOTRIGGERS.rows.resize(1);
					gameData.trigData.m_nTriggers[0] = 1;
					auto &trigger = GEOTRIGGERS.rows[0];
					trigger.flags = TF_PLAYING_SOUND | (once ? TF_ONE_SHOT : 0) | (permanent ? TF_PERMANENT : 0);
					trigger.fly_through = fly;
					trigger.time[1] = I2X(2);
					trigger.m_info.tOperated = gameData.timeData.xGame - I2X(2) - delta;
					TriggersFrameProcess();
					require(trigger.Flagged(TF_PLAYING_SOUND) == (delta <= 0), "Source playing-state expiry boundary differs");
					require(trigger.Flagged(TF_DISABLED) == (delta > 0 && (fly ? !once : permanent)), "Source completion flags differ");
					require(trigger.countdowns == 1, "Source wall maintenance skipped scheduler callback");
					++cases;
				}
	reset();
	OBJTRIGGERS.rows.resize(1);
	gameData.trigData.m_nTriggers[1] = 1;
	auto &object = OBJTRIGGERS.rows[0];
	object.flags = TF_PLAYING_SOUND;
	object.time[1] = I2X(1);
	object.m_info.tOperated = 0;
	TriggersFrameProcess();
	require(object.Flagged(TF_PLAYING_SOUND) && object.countdowns == 1, "Source object loop unexpectedly applies wall sound-expiry maintenance");
	++cases;
	for (bool attached : { false, true })
		for (bool permanent : { false, true }) {
			reset();
			auto &array = gameData.trigData.triggers[attached];
			array.rows.resize(1);
			gameData.trigData.m_nTriggers[attached] = 1;
			auto &trigger = array.rows[0];
			trigger.flags = TF_AUTOPLAY | (permanent ? TF_PERMANENT : 0);
			trigger.m_info.nObject = 19;
			TriggersFrameProcess();
			const bool operate = attached || !permanent;
			require(trigger.operations == int(operate) && trigger.Flagged(TF_DISABLED) == operate, "Source autoplay/permanent behavior differs");
			if (operate) require(trigger.actor == (attached ? 19 : 7) && trigger.player == (attached ? -1 : 0) && trigger.object_event == int(attached), "Source autoplay dispatch arguments differ");
			++cases;
		}
	for (int seconds : { -1, 0, 1 })
		for (int delta : { 0, 1 })
			for (bool disabled : { false, true }) {
				reset();
				tTextIndex row{ 1, "voice.wav" };
				gameData.soundData = { &row, 1 };
				CArray<CTrigger> triggers;
				triggers.rows.resize(1);
				auto &trigger = triggers.rows[0];
				trigger.flags = TF_PLAYING_SOUND | (disabled ? TF_DISABLED : 0);
				trigger.time = { seconds, I2X(2) };
				trigger.m_info.nChannel = 3;
				trigger.m_info.tOperated = gameData.timeData.xGame - I2X(2) - delta;
				StopTriggeredSounds(triggers);
				require(trigger.m_info.nChannel == -1 && trigger.Flagged(TF_PLAYING_SOUND), "Source stop helper changed flags or retained channel");
				StartTriggeredSounds(triggers);
				require(audio.calls.size() == std::size_t(!disabled && (seconds < 0 || delta > 0)), "Source restore start/cooldown behavior differs");
				++cases;
			}
	std::cout << "Named audio source: " << cases << " frame/autoplay/restore controls\n";
}

void original(const char *path, int expected_levels, int expected_actions, int expected_waves)
{
	using namespace reference;
	reset();
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	require(bool(file), "Original sound archive missing");
	const auto length = file.tellg();
	const auto read = [&](std::uint64_t offset, unsigned char *out, std::size_t count) {
		file.seekg(offset);
		file.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(count));
		require(bool(file), "Short original sound archive read");
	};
	const auto members = xl_compat::index_hog(length, read);
	std::map<std::string, xl_compat::LevelTextTable> tables;
	int waves = 0, actions = 0, levels = 0, table_rows = 0;
	for (const auto &entry : members) {
		const auto name = xl_compat::hog_key(entry.name);
		if (name.size() < 4) continue;
		const auto extension = name.substr(name.size() - 4);
		if (extension != ".wav" && extension != ".snd") continue;
		require(entry.size <= xl_compat::max_wave_bytes, "Original sound resource exceeds bounds");
		xl_compat::Bytes bytes(static_cast<std::size_t>(entry.size));
		read(entry.offset, bytes.data(), bytes.size());
		if (extension == ".snd") {
			auto table = xl_compat::decode_level_text(std::string_view(reinterpret_cast<const char *>(bytes.data()), bytes.size()));
			table_rows += static_cast<int>(table.size());
			tables.emplace(name, std::move(table));
		} else {
			const auto pcm = xl_compat::decode_wave_channels(bytes.data(), bytes.size());
			// Original archive mode currently witnesses native 44.1 kHz stereo PCM
			require(pcm.rate == 44100 && pcm.source_channels == 2 && pcm.source_bits == 16, "Original sound requires a separate mixer-conversion witness");
			files[{ "hog", name }] = { static_cast<std::uint32_t>(pcm.samples.size() * sizeof(std::int16_t)) };
			++waves;
		}
	}
	std::set<std::string> used;
	for (const auto &entry : members) {
		const auto name = xl_compat::hog_key(entry.name);
		if (name.size() < 4 || name.substr(name.size() - 4) != ".rl2") continue;
		require(entry.size <= 128 * 1024 * 1024, "Original sound level exceeds inspection bound");
		xl_compat::Bytes bytes(static_cast<std::size_t>(entry.size));
		read(entry.offset, bytes.data(), bytes.size());
		const auto level = xl_compat::inspect_level(bytes);
		std::vector<tTextIndex> rows;
		const auto table = tables.find(name.substr(0, name.size() - 4) + ".snd");
		if (table != tables.end())
			for (const auto &row : table->second) rows.push_back({ row.id, row.text.c_str() });
		gameData.soundData = { rows.empty() ? nullptr : rows.data(), static_cast<int>(rows.size()) };
		const auto action = [&](const xl_compat::Trigger &record) {
			if (record.type != TT_SOUND) return;
			CTrigger trigger;
			trigger.value = record.value;
			trigger.flags = record.flags & ~TF_DISABLED;
			trigger.time[0] = record.time;
			audio.resolve = true;
			audio.calls.clear();
			selected = {};
			require(trigger.DoPlaySound(-1) == 1 && trigger.Flagged(TF_PLAYING_SOUND) && audio.calls.size() == 1, "Original sound action did not resolve and prepare in source adapter");
			require(selected.first == "hog" && !exists, "Original sound action relied on an external fallback");
			const auto &call = audio.calls.back();
			require(selected.second == xl_compat::hog_key(call.name) && FindAddonSound(call.name.c_str()) < 0 && !std::isdigit(static_cast<unsigned char>(call.name.front())), "Original sound action resolves through an unowned addon alias");
			used.insert(selected.second);
			++actions;
		};
		for (const auto &record : level.triggers) action(record);
		for (const auto &record : level.object_trigger_records) action(record.action);
		++levels;
	}
	require(levels == expected_levels && actions == expected_actions && waves == expected_waves, "Original sound coverage membership differs");
	std::cout << "Original named-audio closure: " << levels << " levels, " << actions << " sound actions, " << tables.size() << " SND tables, " << table_rows << " rows, " << waves << " WAVs, " << used.size() << " action-referenced WAVs\n";
	std::cout << "Source storage/mixer adapters only; audible playback and campaign remain unqualified\n";
}
} // namespace

void classic_audio_source_controls();
void audio_channel_source_controls();
void audio_output_source_controls();
void audio_ownership_source_controls();
void audio_sync_source_controls();
void classic_sample_source_controls();
void audio_route_source_controls();
void audio_spatial_source_controls();
void audio_geometry_source_controls();
void audio_geometry_runtime_controls(const char *);
void audio_creation_runtime_controls(const char *);
void classic_audio_original(const char *bank, const char *folder, int levels);

int main(int argc, char **argv)
{
	try {
		// The public contract owner runs the complete primitive suite separately
		// Runtime comparison needs only the captured mine and its source consumers
		if (argc == 3 && std::string(argv[1]) == "--geometry-runtime") {
			audio_geometry_runtime_controls(argv[2]);
			return 0;
		}
		if (argc == 3 && std::string(argv[1]) == "--creation-runtime") {
			audio_creation_runtime_controls(argv[2]);
			return 0;
		}
		source_actions();
		source_resources();
		source_scopes();
		source_lifecycle();
		classic_audio_source_controls();
		audio_channel_source_controls();
		audio_output_source_controls();
		audio_ownership_source_controls();
		audio_sync_source_controls();
		classic_sample_source_controls();
		audio_route_source_controls();
		audio_spatial_source_controls();
		audio_geometry_source_controls();
		if (argc == 5 && std::string(argv[1]) == "--classic-original") classic_audio_original(argv[2], argv[3], std::stoi(argv[4]));
		else if (argc == 5) original(argv[1], std::stoi(argv[2]), std::stoi(argv[3]), std::stoi(argv[4]));
		else require(argc == 1, "Expected no arguments or original HOG and exact level/action/WAV counts");
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
