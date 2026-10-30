// Actual pinned selection functions with independent files and sample registries
#include "scoped_sound.h"
#include "classic_bank.h"
#include <array>
#include <cstdint>
#include <cctype>
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

static void Error(const char *message)
{
	throw std::runtime_error(message);
}

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
struct FILE {
	const Bytes *bytes = nullptr;
	std::string path;
};
std::map<std::string, FILE> handles;
FILE *open_adapter(const std::string &path)
{
	opens.push_back(path);
	const auto entry = files.find(path);
	if (entry == files.end()) return nullptr;
	auto &handle = handles[path];
	handle = { &entry->second, path };
	return &handle;
}
FILE *GetFileHandle(const char *name, const char *folder, const char *)
{
	return open_adapter(std::string(folder ? folder : "") + name);
}
int fclose(FILE *)
{
	return 0;
}
long ftell(FILE *)
{
	return 0;
}
int ffilelength(FILE *file)
{
	return int(file->bytes->size());
}
int stricmp(const char *a, const char *b)
{
	for (; *a && *b; ++a, ++b)
		if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b)))
			return std::tolower(static_cast<unsigned char>(*a)) - std::tolower(static_cast<unsigned char>(*b));
	return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}
struct {
	FILE *Find(const char *name, int *length, int)
	{
		auto *file = open_adapter(std::string("hog/") + name);
		if (file) *length = ffilelength(file);
		return file;
	}
	const char *AltHogFile() const
	{
		return "mission.hog";
	}
} hogFileManager;
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
struct CFile {
	struct {
		FILE *file = nullptr;
		std::size_t rawPosition = 0;
		int size = 0, libOffset = 0, pBuffer = 0, bufLen = 0;
		char *filename = nullptr;
	} m_info;
	int32_t Open(const char *name, const char *folder, const char *, int);
	static int32_t Exist(const char *name, const char *folder, int);
	int Length() const
	{
		return m_info.size;
	}
	void Close()
	{
		m_info.file = nullptr;
	}
	void Seek(long offset, int origin)
	{
		require(origin == SEEK_SET && offset >= 0 && m_info.file && std::size_t(offset) <= m_info.file->bytes->size(), "Reference bank seek outside adapter");
		m_info.rawPosition = std::size_t(offset);
	}
	long Tell() const
	{
		return static_cast<long>(m_info.rawPosition);
	}
	std::size_t Read(void *out, std::size_t count)
	{
		const auto *bytes = m_info.file ? m_info.file->bytes : nullptr;
		require(bytes && m_info.rawPosition <= bytes->size() && count <= bytes->size() - m_info.rawPosition, "Reference bank read outside adapter");
		std::memcpy(out, bytes->data() + m_info.rawPosition, count);
		m_info.rawPosition += count;
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
		origin = file.m_info.file->path;
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
		bool bD1Mission = false, bNostalgia = false, bDemoData = false, bReadOnly = false;
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
	// CHashTable::Search uses stricmp in the pinned source
	std::string key = name;
	for (char &c : key)
		if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
	const auto found = names.find(key);
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
#include "xl_reference_classic_files.h"
#include "xl_reference_classic_audio.h"
#include "reference_classic_start.h"
#undef USE_OPENAL
#undef HIRES_SOUND_FOLDER

void reset()
{
	files.clear();
	handles.clear();
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
				if (mask & 4) {
					const auto &bytes = files.at(custom);
					const auto selected_bank = xl_compat::select_classic_bank(bytes.data(), bytes.size(), { "laser01", "robot01" });
					require(selected_bank.size() == 2, "Production bank registered an unknown source name");
					for (const auto &[slot, span] : selected_bank)
						require(samples[unsigned(slot)].data[0].bytes == Bytes(bytes.begin() + span.offset, bytes.begin() + span.offset + span.length), "Production bank bytes differ from actual source load");
				}
				++cases;
			}
	for (int rate : { 11025, 44100 })
		for (int ordinal : { 1, 2, -1 })
			for (int mask = 0; mask < 32; ++mask) {
				reset();
				options.sound.audioSampleRate = rate;
				files[std::string("data/") + DefaultSoundFile()] = base;
				require(LoadD2Sounds(false), "Archive selection baseline failed");
				select_mod(ordinal);
				const auto bank_name = std::string("pilot") + (rate >= 22050 ? ".s22" : ".s11");
				const auto level = std::string("mods/pilot/sounds/") + LevelFolder(ordinal) + "laser01.wav";
				const std::string mission = "mods/pilot/sounds/laser01.wav", archived_wave = "hog/laser01.wav";
				const auto folder_bank = "mods/pilot/" + bank_name, archived_bank = "hog/" + bank_name;
				if (mask & 1) files[level] = { 11, 12 };
				if (mask & 2) files[mission] = { 21, 22 };
				if (mask & 4) files[archived_wave] = { 31, 32 };
				if (mask & 8) files[folder_bank] = bank({ { "laser01", { 41, 42 } }, { "robot01", { 4, 5 } } });
				if (mask & 16) files[archived_bank] = bank({ { "laser01", { 51, 52 } }, { "robot01", { 4, 5 } } });
				require(LoadD2Sounds(true), "Archive source selection failed");
				const auto selected = xl_compat::resolve_scoped_sound("pilot", "laser01", ordinal, rate, [&](const std::string &candidate) {
					const auto path = candidate.find('/') == std::string::npos ? "hog/" + candidate : candidate;
					return files.count(path) ? path : std::string{};
				});
				const auto expected = mask & 8 ? folder_bank : mask & 16 ? archived_bank
				                                           : mask & 1    ? level
				                                           : mask & 4    ? archived_wave
				                                           : mask & 2    ? mission
				                                                         : std::string("data/") + DefaultSoundFile();
				require(samples[0].data[0].origin == expected && selected.path == (mask ? expected : "") && selected.bank == bool(mask & 24),
				        "Production folder/HOG precedence differs from actual CFile and sound loader");
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
	for (int variant = 0; variant < 5; ++variant) {
		reset();
		files["data/descent2.s22"] = base;
		require(LoadD2Sounds(false), "Custom bank name-control baseline failed");
		select_mod(-1);
		auto custom = bank({ { "robot01", { 81, 82, 83 } }, { "laser01", { 71, 72 } }, { "LASER01", { 91, 92, 93, 94 } } });
		if (variant == 1) custom[12 + 12] = 0;                 // data_length is not the read length
		if (variant == 2) custom.insert(custom.end(), 19, 77); // trailing bytes are harmless
		if (variant == 3) {
			custom[12 + 16] = 3; // overlapping extents are allowed by the source
			custom[12 + 8] = 2;
		}
		if (variant == 4) {
			custom[12 + 40] = 'X';      // unknown duplicate does not replace laser01
			custom[12 + 40 + 16] = 255; // unknown extents are not consumed
		}
		files["mods/pilot/pilot.s22"] = custom;
		require(LoadD2Sounds(true), "Custom bank name-control source load failed");
		const auto selected = xl_compat::select_classic_bank(custom.data(), custom.size(), { "laser01", "robot01" });
		require(selected.size() == 2 && names.size() == 2, "Custom names changed the native registry");
		for (const auto &[slot, span] : selected)
			require(samples[unsigned(slot)].data[0].bytes == Bytes(custom.begin() + span.offset, custom.begin() + span.offset + span.length), "Reordered/duplicate/case bank differs from actual source");
		++cases;
	}
	unsigned malformed = 0;
	const auto reject = [&](const Bytes &bytes) {
		try {
			xl_compat::select_classic_bank(bytes.data(), bytes.size(), { "laser01", "robot01" });
		} catch (const std::exception &) {
			++malformed;
			return;
		}
		throw std::runtime_error("Malformed selected bank was accepted");
	};
	for (std::size_t end = 0; end < base.size(); ++end) reject(Bytes(base.begin(), base.begin() + end));
	for (unsigned offset : { 0u, 4u, 8u, 12u + 8u, 12u + 16u }) {
		auto bytes = base;
		for (unsigned i = 0; i < 4; ++i) bytes[offset + i] = 255;
		reject(bytes);
	}
	std::cout << "Classic custom banks: full/reordered/case/duplicate/ignored-name source agreement and " << malformed << " malformed extents rejected\n";
	// A partial custom bank reuses omitted samples' old offsets in LoadSounds
	// Record this source defect before implementing safe base-sample retention
	for (bool padded : { false, true }) {
		reset();
		files["data/descent2.s22"] = base;
		require(LoadD2Sounds(false), "Partial custom baseline failed");
		select_mod(1);
		auto custom = bank({ { "laser01", { 31, 32 } } });
		if (padded) custom.resize(64, 99);
		files["mods/pilot/pilot.s22"] = custom;
		bool out_of_bounds = false;
		try {
			require(LoadD2Sounds(true), "Partial custom source load failed");
		} catch (const std::runtime_error &error) {
			out_of_bounds = std::string(error.what()) == "Reference bank seek outside adapter";
			if (!out_of_bounds) throw;
		}
		require(padded ? !out_of_bounds && samples[1].data[0].bytes == Bytes({ 99, 99 }) : out_of_bounds,
		        "Partial custom bank no longer exposes the pinned stale-offset defect");
		require(samples[0].data[0].bytes == Bytes({ 31, 32 }), "Partial source bank lost the explicitly replaced sample");
		++cases;
	}
	std::cout << "Classic audio source: " << cases << " bank suppression, signed scopes, registry, fallback and profile controls\n";
	classic_start_controls();
}

void classic_bank_original(const char *path)
{
	using namespace classic_reference;
	reset();
	const auto bytes = read(path);
	files["data/descent2.s22"] = bytes;
	require(LoadD2Sounds(false), "Original reference bank did not load");
	std::vector<std::string> registry(unsigned(gameData.pigData.sound.nSoundFiles[0]));
	for (const auto &[name, slot] : names) registry.at(unsigned(slot)) = name;
	select_mod(1);
	files["mods/pilot/pilot.s22"] = bytes;
	require(LoadD2Sounds(true), "Original complete custom bank did not load");
	const auto selected = xl_compat::select_classic_bank(bytes.data(), bytes.size(), registry);
	require(selected.size() == registry.size() && !selected.empty(), "Original bank did not retain all named samples");
	for (const auto &[slot, span] : selected)
		require(samples[unsigned(slot)].data[0].bytes == Bytes(bytes.begin() + span.offset, bytes.begin() + span.offset + span.length), "Original DSND sample differs from actual source");
	std::cout << "Original custom bank: " << selected.size() << " registered samples exactly match actual source loading\n";
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
