// File and display storage adapters; the pinned text reader bodies are unchanged
#include "level_text.h"
#include "message_time.h"
#include "hog.h"
#include "inventory.h"
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cmath>
#include <map>

namespace reference
{
constexpr int FILENAME_LEN = 256;
#define NEW        new
#define X2I(value) ((value) >> 16)
constexpr int NORMAL_FONT = 0, GOLD_RGBA = 1, BG_STANDARD = 0;
constexpr int TT_COUNTDOWN = 25, TT_MESSAGE = 29, TT_SOUND = 30;
constexpr const char *TXT_CLOSE = "Close";
float alpha = 0;
int rendered = 0, modal = 0, paused = 0;
class CCanvas
{
  public:
	static CCanvas *Current()
	{
		static CCanvas canvas;
		return &canvas;
	}
	int Width() const
	{
		return 640;
	}
	int Height() const
	{
		return 480;
	}
};
struct CBitmap {
	int Width() const
	{
		return 100;
	}
	int Height() const
	{
		return 20;
	}
	void Render(CCanvas *, int, int, int, int, int, int, int, int, int, int, int, float opacity)
	{
		alpha = opacity;
		++rendered;
	}
};
struct {
	void SetCurrent(int) {}
	void SetColorRGBi(int, int, int, int) {}
} fontManager;
struct {
	void DrawBox(int, int, int, int, int, float, int) {}
} backgroundManager;
struct {
	struct {
		std::int32_t nSDLTicks[1]{};
		bool bD2XLevel = true;
	} app;
	struct {
		int nWindow[1]{};
	} render;
} gameStates;
class CTextData;
struct {
	CTextData *messages = nullptr;
} gameData;
CBitmap *CreateStringBitmap(const char *, int, int, void *, int, int, int, int)
{
	return new CBitmap;
}
void PauseGame()
{
	++paused;
}
void ResumeGame()
{
	--paused;
}
void TextBox(void *, int, int, const char *, const char *)
{
	if (paused != 1) throw std::runtime_error("Source modal did not pause gameplay");
	++modal;
}
class CTrigger
{
  public:
	int type = 29, value = 65536, duration = 3;
	int Type() const
	{
		return type;
	}
	int GetValue() const
	{
		return value;
	}
	int GetTime(int) const
	{
		return duration;
	}
	int32_t DoShowMessage();
	bool IsDelayed();
};
struct {
	struct {
		const char *szData[1]{};
	} game;
} gameFolders;
std::string input;
void PrintLog(int, ...) {}
class CFile
{
  public:
	static void ChangeFilenameExtension(char *out, const char *, const char *extension)
	{
		std::snprintf(out, FILENAME_LEN, "fixture%s", extension);
	}
	int Size(const char *, const char *, int)
	{
		return static_cast<int>(input.size());
	}
	bool Open(const char *, const char *, const char *, int)
	{
		return true;
	}
	void Close() {}
	void Read(void *out, int size, int count)
	{
		if (size != 1 || count != static_cast<int>(input.size())) throw std::runtime_error("Reference text extent mismatch");
		std::memcpy(out, input.data(), input.size());
	}
};
#include "xl_reference_level_text.h"
#undef NEW
#undef X2I
} // namespace reference

namespace
{
std::size_t cases = 0, rows = 0;
void timing()
{
	using namespace reference;
	for (const int seconds : { 0, 1, 2, 100 }) {
		for (const int elapsed : { 0, 1, 249, 250, 499, 500, 750, 999, 1000, 1501, 1999, 2000, 100000 }) {
			CTextData table{};
			input = "1:Message";
			LoadTextData("fixture.rl2", ".msg", &table);
			gameData.messages = &table;
			gameStates.app.nSDLTicks[0] = 10000;
			CTrigger action;
			action.duration = seconds;
			action.value += 32768;
			if (action.IsDelayed() || action.DoShowMessage() != 1 || table.currentMsg != table.index)
				throw std::runtime_error("Source message ID/time dispatch differs");
			gameStates.app.nSDLTicks[0] += elapsed;
			rendered = 0;
			const int visible = ShowGameMessage(&table, -1, 0);
			const auto actual = xl_compat::message_opacity(elapsed, std::int64_t(seconds) * 1000 - elapsed);
			if (visible != (elapsed < seconds * 1000) || (visible && (rendered != 1 || std::abs(actual - alpha) > 0.000001f)) || (!visible && actual != 0))
				throw std::runtime_error("Source message expiry/fade differs");
			FreeTextData(&table);
		}
	}
	for (const int seconds : { -1, -900000, -2147483647 }) {
		CTextData table{};
		input = "1:Modal";
		LoadTextData("fixture.rl2", ".msg", &table);
		gameData.messages = &table;
		CTrigger action;
		action.duration = seconds;
		modal = 0;
		if (action.IsDelayed() || action.DoShowMessage() != 1 || modal != 1 || paused || table.currentMsg)
			throw std::runtime_error("Source modal lifecycle differs");
		FreeTextData(&table);
	}
	gameData.messages = nullptr;
	std::cout << "XL message timing: 52 expiry/fade and three modal source controls passed\n";
}
void compare(const std::string &bytes)
{
	const auto actual = xl_compat::decode_level_text(bytes);
	if (actual.empty()) throw std::runtime_error("Reference comparison requires a defined nonempty source table");
	reference::input = bytes;
	reference::CTextData expected{};
	reference::LoadTextData("fixture.rl2", ".msg", &expected);
	bool equal = expected.nMessages == static_cast<int>(actual.size());
	for (std::size_t i = 0; equal && i < actual.size(); ++i) {
		const auto &entry = expected.index[i];
		equal = actual[i].id == entry.nId && actual[i].lines == entry.nLines && actual[i].text == entry.pszText &&
		        reference::FindTextData(&expected, actual[i].id) == &entry && xl_compat::find_level_text(actual, entry.nId) == &actual[i];
	}
	equal = equal && !reference::FindTextData(&expected, 0) && !xl_compat::find_level_text(actual, 0);
	reference::FreeTextData(&expected);
	if (!equal) throw std::runtime_error("Level text differs from extracted XL source");
	++cases;
	rows += actual.size();
}
void original(const char *path)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) throw std::runtime_error("Original text archive missing");
	const auto length = file.tellg();
	auto read = [&](std::uint64_t offset, unsigned char *out, std::size_t count) {
		file.seekg(offset);
		if (!file.read(reinterpret_cast<char *>(out), count)) throw std::runtime_error("Original text archive read failed");
	};
	unsigned tables = 0;
	const auto members = xl_compat::index_hog(length, read);
	std::map<std::string, xl_compat::LevelTextTable> messages;
	for (const auto &entry : members) {
		const auto name = xl_compat::hog_key(entry.name);
		if (name.size() < 4 || (name.substr(name.size() - 4) != ".msg" && name.substr(name.size() - 4) != ".snd")) continue;
		if (entry.size > 1024 * 1024) throw std::runtime_error("Original text resource exceeds bounds");
		std::string bytes(static_cast<std::size_t>(entry.size), '\0');
		read(entry.offset, reinterpret_cast<unsigned char *>(bytes.data()), bytes.size());
		compare(bytes);
		if (name.substr(name.size() - 4) == ".msg") messages.emplace(name, xl_compat::decode_level_text(bytes));
		++tables;
	}
	if (!tables) throw std::runtime_error("No original MSG/SND tables compared");
	std::cout << tables << " original MSG/SND tables compared\n";
	unsigned levels = 0, actions = 0;
	for (const auto &entry : members) {
		const auto name = xl_compat::hog_key(entry.name);
		if (name.size() < 4 || name.substr(name.size() - 4) != ".rl2") continue;
		if (entry.size > 128 * 1024 * 1024) throw std::runtime_error("Original level exceeds inspection bounds");
		xl_compat::Bytes bytes(static_cast<std::size_t>(entry.size));
		read(entry.offset, bytes.data(), bytes.size());
		const auto inventory = xl_compat::inspect_level(bytes);
		const auto table = messages.find(name.substr(0, name.size() - 4) + ".msg");
		const auto action = [&](const xl_compat::Trigger &trigger) {
			if (trigger.type != 29) return;
			const int id = trigger.value / 65536;
			if (id <= 0 || table == messages.end() || !xl_compat::find_level_text(table->second, id))
				throw std::runtime_error("Original message action lacks supported owned text: " + name);
			++actions;
		};
		for (const auto &trigger : inventory.triggers) action(trigger);
		for (const auto &trigger : inventory.object_trigger_records) action(trigger.action);
		++levels;
	}
	std::cout << levels << " original levels inspected; " << actions << " message action IDs resolve to owned MSG rows\n";
}
} // namespace

int main(int argc, char **argv)
{
	try {
		timing();
		for (const std::string bytes : {
		         "1:one", "1:one\n", "1:one\r\n", "1 one", " 1:leading", "+1:plus", "-1:negative", "\n1:leading blank",
		         "0:ignored\n1:one", "1:one\n# comment\n2:two", "1:line\\nsecond\\nthird", "1:slash\\\\nline", "1:first\n\n2:second" })
			compare(bytes);
		for (int sample = 1; sample <= 2048; ++sample) {
			const auto newline = sample % 2 ? "\r\n" : "\n";
			compare(std::to_string(sample + 4000) + ":Third" + newline + std::to_string(sample) + ":First\\nNext" + newline +
			        std::to_string(sample + 2000) + " Second" + (sample % 3 ? newline : ""));
		}
		if (argc == 2) original(argv[1]);
		else if (argc != 1) throw std::runtime_error("Expected zero arguments or an original HOG");
		std::cout << "XL text source: " << cases << " tables, " << rows << " exact row/lookup comparisons\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
