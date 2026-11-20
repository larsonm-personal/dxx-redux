// Compiled only into a disposable checkout of the pinned original D2X-XL.
// Keep mission selection, file resolution and gameplay in the reference engine.
#include "loadgame.h"
#include "mission.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

static FILE *campaign_receipt;
static int campaign_ordinal;

static bool campaign_requested()
{
	return std::getenv("XL_REFERENCE_DESCRIPTOR") != nullptr;
}

void xl_reference_descriptor(const char *directory, const char *name)
{
	if (campaign_receipt) {
		std::fprintf(campaign_receipt, "descriptor\t%s%s\n", directory, name);
		std::fflush(campaign_receipt);
	}
}

void xl_reference_frame()
{
	if (!campaign_receipt) return;
	static int frames = 0;
	++frames;
	const bool failed = gameStates.app.bPlayerIsDead || missionManager.nCurrentLevel != campaign_ordinal;
	if (!failed && frames < 120) return;
	std::fprintf(campaign_receipt, "frames\t%d\t%d\t%d\n", frames, missionManager.nCurrentLevel, int(gameStates.app.bPlayerIsDead));
	std::fclose(campaign_receipt);
	std::exit(failed ? 23 : 0);
}

static int campaign_reference()
{
	const char *folder = std::getenv("XL_REFERENCE_FOLDER");
	const char *path = std::getenv("XL_REFERENCE_OUT");
	const char *level = std::getenv("XL_REFERENCE_LEVEL");
	const char *descriptor = std::getenv("XL_REFERENCE_DESCRIPTOR");
	if (!folder || !path || !level || !descriptor || std::strlen(folder) >= sizeof(gameFolders.missions.szSubFolder)) return 80;
	campaign_receipt = std::fopen(path, "w");
	if (!campaign_receipt) return 80;
	std::fprintf(campaign_receipt, "initialized\n");
	std::fflush(campaign_receipt);
	// This selects the same directory and entry as the game's mission browser.
	std::strcpy(gameFolders.missions.szSubFolder, folder);
	const int count = missionManager.BuildList(1, -1);
	int selected = -1;
	for (int index = 0; index < count; ++index) {
		const auto &entry = missionManager[index];
		if (!entry.nDescentVersion) continue;
		char filename[FILENAME_LEN];
		std::snprintf(filename, sizeof(filename), "%s.%s", entry.filename, entry.nDescentVersion == 2 ? "mn2" : "msn");
		if (stricmp(filename, descriptor)) continue;
		if (selected >= 0) return 81;
		selected = index;
	}
	std::fprintf(campaign_receipt, "listed\t%d\n", int(selected >= 0));
	std::fflush(campaign_receipt);
	if (selected < 0) return 20;
	const int loaded = missionManager.Load(selected);
	std::fprintf(campaign_receipt, "mission\t%d\n", loaded);
	std::fflush(campaign_receipt);
	if (!loaded) return 21;
	auto record_member = [](int ordinal) {
		CFile member;
		const char *filename = LevelName(ordinal);
		const bool found = member.Open(filename, gameFolders.game.szData[0], "rb", 0);
		std::fprintf(campaign_receipt, "member\t%d\t%s\t%lu\n", ordinal, filename, found ? static_cast<unsigned long>(member.Length()) : 0ul);
	};
	for (int i = 1; i <= missionManager.LastLevel(); ++i) record_member(i);
	for (int i = -1; i >= missionManager.LastSecretLevel(); --i) record_member(i);
	std::fflush(campaign_receipt);
	campaign_ordinal = std::atoi(level);
	if (!campaign_ordinal) return 0;
	if (campaign_ordinal > missionManager.LastLevel() || campaign_ordinal < missionManager.LastSecretLevel()) return 24;
	gameStates.app.nDifficultyLevel = 2;
	std::fprintf(campaign_receipt, "starting\t%d\t%s\n", campaign_ordinal, LevelName(campaign_ordinal));
	std::fflush(campaign_receipt);
	const int started = StartNewGame(campaign_ordinal);
	std::fprintf(campaign_receipt, "started\t%d\t%d\n", started, missionManager.nCurrentLevel);
	std::fflush(campaign_receipt);
	if (!started) return 22;
	MainLoop();
	return 25;
}
