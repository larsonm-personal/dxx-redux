// Actual pinned selection functions with independent files and sample registries
#include "scoped_sound.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace classic_reference
{
using Bytes = std::vector<std::uint8_t>;
void require(bool ok, const char *reason)
{
	if (!ok) throw std::runtime_error(reason);
}
constexpr int FILENAME_LEN = 512, MAX_SOUND_FILES = 256;
constexpr int SAMPLE_RATE_11K = 11025, SAMPLE_RATE_22K = 22050;
constexpr int SNDFILE_ID = 0x444e5344, SNDFILE_VERSION = 1;
#define USE_OPENAL             0
#define HIRES_SOUND_FOLDER(d1) ((d1) ? 3 : gameOpts->UseHiresSound())
template <class... T>
void PrintLog(T...) {}
std::map<std::string, Bytes> files;
std::vector<std::string> opens;
struct CFile {
	const Bytes *bytes = nullptr;
	std::string path;
	std::size_t position = 0;
	int Open(const char *name, const char *folder, const char *, int)
	{
		path = std::string(folder) + name;
		opens.push_back(path);
		const auto it = files.find(path);
		position = 0;
		bytes = it == files.end() ? nullptr : &it->second;
		return bytes != nullptr;
	}
	static int Exist(const char *name, const char *folder, int)
	{
		return files.count(std::string(folder) + name) != 0;
	}
	int Length() const
	{
		return bytes ? static_cast<int>(bytes->size()) : 0;
	}
	void Close()
	{
		bytes = nullptr;
	}
	void Seek(long offset, int origin)
	{
		require(origin == SEEK_SET && offset >= 0 && bytes && std::size_t(offset) <= bytes->size(), "Reference bank seek outside adapter");
		position = std::size_t(offset);
	}
	long Tell() const
	{
		return static_cast<long>(position);
	}
	std::size_t Read(void *out, std::size_t count)
	{
		require(bytes && position <= bytes->size() && count <= bytes->size() - position, "Reference bank read outside adapter");
		std::memcpy(out, bytes->data() + position, count);
		position += count;
		return count;
	}
	int32_t ReadInt()
	{
		std::array<std::uint8_t, 4> v{};
		Read(v.data(), v.size());
		return int32_t(std::uint32_t(v[0]) | std::uint32_t(v[1]) << 8 | std::uint32_t(v[2]) << 16 | std::uint32_t(v[3]) << 24);
	}
};
struct Buffer {
	Bytes bytes;
	std::string origin;
	bool Create(int count, const char * = nullptr)
	{
		require(count >= 0 && count <= 16 * 1024 * 1024, "Reference allocation bound");
		bytes.assign(std::size_t(count), 0);
		origin.clear();
		return true;
	}
	void Destroy()
	{
		bytes.clear();
		origin.clear();
	}
	std::size_t Read(CFile &file, int count)
	{
		require(count >= 0 && std::size_t(count) == bytes.size(), "Reference sample read size differs");
		origin = file.path;
		return file.Read(bytes.data(), std::size_t(count));
	}
};
struct CSoundSample {
	Buffer data[2];
	int nLength[2]{}, nOffset[2]{}, bHires = 0, bCustom = 0;
	char szName[16]{};
};
struct tPIGSoundHeader {
	char name[8];
	int32_t length, data_length, offset;
};
static_assert(sizeof(tPIGSoundHeader) == 20);
void PIGSoundHeaderRead(tPIGSoundHeader *header, CFile &file)
{
	file.Read(header->name, 8);
	header->length = file.ReadInt();
	header->data_length = file.ReadInt();
	header->offset = file.ReadInt();
}
std::array<CSoundSample, MAX_SOUND_FILES> samples;
struct {
	struct {
		struct {
			CSoundSample *pSound = samples.data();
			int nSoundFiles[2]{}, nType = 1;
		} sound;
	} pigData;
} gameData;
struct {
	struct {
		bool bD1Mission = false, bNostalgia = false, bDemoData = false;
	} app;
	struct {
		bool bD1Sound = false;
	} sound;
} gameStates;
struct Options {
	struct {
		int bHires[2]{}, audioSampleRate = 44100, soundSampleRate = 22050;
	} sound;
	int UseHiresSound() const
	{
		return gameStates.app.bNostalgia ? 0 : sound.bHires[0];
	}
} options;
auto *gameOpts = &options;
struct SoundFolders {
	char mission[256]{}, level[512]{};
	char *operator[](unsigned index)
	{
		return index ? level : mission;
	}
};
struct {
	struct {
		SoundFolders szSounds;
		char szName[128]{}, szCurrent[FILENAME_LEN]{}, szLevel[32]{};
	} mods;
	struct {
		char szData[1][FILENAME_LEN]{}, szSounds[5][FILENAME_LEN]{};
	} game;
} gameFolders;
struct {
	int nCurrentLevel = 1;
} missionManager;
struct {
	int resets = 0;
	void Reset()
	{
		++resets;
	}
} audio;
std::map<std::string, int> names;
struct NameTable {
	void Destroy()
	{
		names.clear();
	}
	void Create(int) {}
} soundNames[2];
int PiggyFindSound(const char *name)
{
	const auto found = names.find(name);
	return found == names.end() ? -1 : found->second;
}
void PiggyRegisterSound(const char *name, int, bool)
{
	const int sample = gameData.pigData.sound.nSoundFiles[0]++;
	names.emplace(name, sample);
}
char *DefaultSoundFile()
{
	static char high[] = "descent2.s22", low[] = "descent2.s11";
	return options.sound.audioSampleRate >= 22050 ? high : low;
}
#include "xl_reference_classic_audio.h"
#undef USE_OPENAL
#undef HIRES_SOUND_FOLDER

void reset()
{
	files.clear();
	opens.clear();
	names.clear();
	samples = {};
	options = {};
	gameStates = {};
	gameData.pigData.sound.nSoundFiles[0] = 0;
	gameData.pigData.sound.nType = 1;
	gameFolders = {};
	std::strcpy(gameFolders.game.szData[0], "data/");
	std::strcpy(gameFolders.game.szSounds[1], "global22/");
	std::strcpy(gameFolders.game.szSounds[2], "global44/");
	audio.resets = 0;
	missionManager.nCurrentLevel = 1;
}
void select_mod(int ordinal)
{
	std::strcpy(gameFolders.mods.szName, "pilot");
	std::strcpy(gameFolders.mods.szCurrent, "mods/pilot/");
	std::strcpy(gameFolders.mods.szSounds[0], "mods/pilot/sounds/");
	missionManager.nCurrentLevel = ordinal;
}
void append_int(Bytes &bytes, int value)
{
	for (int shift = 0; shift < 32; shift += 8) bytes.push_back(std::uint8_t(std::uint32_t(value) >> shift));
}
Bytes bank(const std::vector<std::pair<std::string, Bytes>> &entries)
{
	Bytes bytes;
	append_int(bytes, SNDFILE_ID);
	append_int(bytes, 1);
	append_int(bytes, int(entries.size()));
	int offset = 0;
	for (const auto &[name, pcm] : entries) {
		require(name.size() <= 8 && !pcm.empty(), "Invalid derived reference sample");
		for (unsigned c = 0; c < 8; ++c) bytes.push_back(c < name.size() ? std::uint8_t(name[c]) : 0);
		append_int(bytes, int(pcm.size()));
		append_int(bytes, int(pcm.size()));
		append_int(bytes, offset);
		offset += int(pcm.size());
	}
	for (const auto &entry : entries) bytes.insert(bytes.end(), entry.second.begin(), entry.second.end());
	return bytes;
}
Bytes read(const std::filesystem::path &path)
{
	std::ifstream input(path, std::ios::binary);
	require(bool(input), "Cannot read original bank or sound");
	return Bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
}
} // namespace classic_reference

void classic_audio_source_controls()
{
	using namespace classic_reference;
	unsigned cases = 0;
	const auto base = bank({ { "laser01", { 1, 2, 3 } }, { "robot01", { 4, 5 } } });
	for (int rate : { 11025, 22050, 44100, 48000 })
		for (int ordinal : { 1, 2, -1 })
			for (int mask = 0; mask < 16; ++mask) {
				reset();
				options.sound.audioSampleRate = rate;
				files[std::string("data/") + DefaultSoundFile()] = base;
				require(LoadD2Sounds(false) && names.size() == 2, "Base reference sample registry failed");
				select_mod(ordinal);
				const auto root = std::string("mods/pilot/");
				const auto level = root + "sounds/" + LevelFolder(ordinal) + "laser01.wav";
				const auto mission = root + "sounds/laser01.wav";
				const auto custom = root + "pilot" + (rate >= 22050 ? ".s22" : ".s11");
				if (mask & 1) files[level] = { 11, 12, 13, 14 };
				if (mask & 2) files[mission] = { 21, 22, 23 };
				if (mask & 4) files[custom] = bank({ { "laser01", { 31, 32 } }, { "robot01", { 4, 5 } }, { "unknown", { 33 } } });
				// A bank for the other output-rate profile must not suppress WAVs
				if (mask & 8) files[root + "pilot" + (rate >= 22050 ? ".s11" : ".s22")] = { 99 };
				require(LoadD2Sounds(true), "Custom reference load failed");
				const auto selected = xl_compat::resolve_scoped_sound("pilot", "laser01", ordinal, rate,
				                                                      [&](const std::string &path) { return files.count(path) ? path : std::string{}; });
				const auto &sample = samples.at(names.at("laser01"));
				const std::string expected = mask & 4 ? custom : mask & 1 ? level
				                                             : mask & 2   ? mission
				                                                          : std::string("data/") + DefaultSoundFile();
				require(sample.data[0].origin == expected && bool(sample.bHires) == (!(mask & 4) && (mask & 3) != 0), "Extracted source selected wrong bank or WAV");
				require(selected.path == ((mask & 7) ? expected : "") && selected.bank == ((mask & 4) != 0), "Production scope selection differs from extracted source");
				require(samples.at(names.at("robot01")).data[0].bytes == Bytes({ 4, 5 }) && names.size() == 2, "Custom bank lost a named sample or registered an unknown name");
				++cases;
			}
	for (int high : { 1, 2 })
		for (bool complete : { false, true }) {
			reset();
			options.sound.bHires[0] = options.sound.bHires[1] = high;
			files["data/descent2.s22"] = base;
			const auto root = high == 1 ? "global22/" : "global44/";
			files[std::string(root) + "laser01.wav"] = { 61, 62 };
			if (complete) files[std::string(root) + "robot01.wav"] = { 63, 64 };
			require(LoadD2Sounds(false), "Global high-resolution reference profile failed");
			require(options.sound.bHires[0] == (complete ? high : 0) && audio.resets == int(!complete), "Incomplete global bank did not reset to low-resolution defaults");
			require(bool(samples[0].bHires) == complete && bool(samples[1].bHires) == complete, "Global profile left a mixed base bank after retry");
			++cases;
		}
	for (int high : { 0, 1, 2 })
		for (int loaded : { 2, (1 << 16) | 1, 2 << 16 }) {
			reset();
			options.sound.bHires[0] = 0;
			options.sound.bHires[1] = high;
			require(VerifyHiresSound(true, loaded), "Custom high-resolution verification rejected a mixed bank");
			require(options.sound.bHires[0] == ((loaded & 0xffff) ? 0 : high), "Complete custom bank did not restore the preferred global profile");
			++cases;
		}
	// A selected empty WAV suppresses lower-priority WAV lookup in the reference
	reset();
	files["data/descent2.s22"] = base;
	require(LoadD2Sounds(false), "Empty preferred WAV baseline failed");
	select_mod(1);
	files["mods/pilot/sounds/level01/laser01.wav"] = {};
	files["mods/pilot/sounds/laser01.wav"] = { 7, 8 };
	require(LoadD2Sounds(true) && !samples[0].bHires && samples[0].data[0].bytes == Bytes({ 1, 2, 3 }), "Empty preferred reference WAV fell through to a lower folder");
	++cases;
	std::cout << "Classic audio source: " << cases << " bank suppression, signed scopes, registry, fallback and profile controls\n";
}

void classic_audio_original(const char *bank_path, const char *folder, int levels)
{
	using namespace classic_reference;
	require(levels > 0 && levels <= 9999, "Original sound level count outside bound");
	reset();
	files["data/descent2.s22"] = read(bank_path);
	const auto root = std::filesystem::path(folder);
	for (const auto &entry : std::filesystem::recursive_directory_iterator(root)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".wav") continue;
		const auto relative = std::filesystem::relative(entry.path(), root).generic_string();
		files["mods/pilot/sounds/" + relative] = read(entry.path());
	}
	std::set<std::string> used;
	unsigned selections = 0;
	for (int ordinal = 1; ordinal <= levels; ++ordinal) {
		gameFolders.mods = {};
		require(LoadD2Sounds(false), "Original base sound bank failed");
		select_mod(ordinal);
		require(LoadD2Sounds(true), "Original custom sound bank failed");
		for (const auto &[name, sample] : names) {
			const auto &value = samples.at(sample);
			if (!value.bHires) continue;
			require(value.data[0].bytes == files.at(value.data[0].origin), "Original source selection bytes differ");
			const auto path = value.data[0].origin.substr(std::strlen("mods/pilot/sounds/"));
			std::cout << "CLASSIC_SELECTION\t" << ordinal << '\t' << sample << '\t' << name << '\t' << path << '\t' << value.data[0].bytes.size() << '\n';
			used.insert(path);
			++selections;
		}
	}
	std::cout << "Original classic source: " << levels << " levels, " << names.size() << " sample names, " << selections << " selections, " << used.size() << " distinct replacement files; runtime playback unqualified\n";
}
