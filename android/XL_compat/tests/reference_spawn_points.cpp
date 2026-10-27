#include "reference_motion.h"
#include <filesystem>
#include "xl_reference_teleport.h"
constexpr int MAX_PLAYERS = 16, N_LOCALPLAYER = 0;
constexpr int TF_NO_MESSAGE = 1;
int messages = 0;
void HUDInitMessage(const char *text, const char *)
{
	if (std::string(text) != "New spawn points set!") throw std::runtime_error("Unexpected spawn cue");
	++messages;
}
struct {
	struct {
		struct {
			tObjTransformation position;
			int nSegment = -1, nSegType = -1;
		} playerInit[MAX_PLAYERS];
	} multiplayer;
} gameData;
struct {
	void MoveSpawnPoint(tObjTransformation *, int) {}
	int SpawnIndex(int)
	{
		return -1;
	}
	int SetHighlight(int index)
	{
		return index;
	}
	void Delete(int)
	{
		throw std::runtime_error("unexpected marker in transform oracle");
	}
} markerManager;
struct CTrigger {
	int m_nLinks = 0, m_segments[10]{}, m_sides[10]{};
	int flags = 0;
	bool Flagged(int mask)
	{
		return (flags & mask) != 0;
	}
	int32_t DoSetSpawnPoints();
	void PrintMessage(int32_t player, int32_t shot, const char *message);
};
#include "xl_reference_spawn_points.h"
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected runtime spawn samples");
		std::ifstream input(argv[1]);
		unsigned count = 0;
		CTrigger action;
		while (input >> action.m_nLinks) {
			if (action.m_nLinks < 1 || action.m_nLinks > 10) throw std::runtime_error("Invalid link count");
			for (int link = 0; link < action.m_nLinks; ++link) {
				fix values[6];
				for (auto &value : values)
					if (!(input >> value)) throw std::runtime_error("Truncated spawn geometry");
				segments[link].normal.Set(values[0], values[1], values[2]);
				segments[link].center.Set(values[3], values[4], values[5]);
				action.m_segments[link] = link;
			}
			action.DoSetSpawnPoints();
			for (int slot = 0; slot < MAX_PLAYERS; ++slot) {
				const auto &spawn = gameData.multiplayer.playerInit[slot];
				if (spawn.nSegment != slot % action.m_nLinks) throw std::runtime_error("Spawn link order mismatch");
				for (const auto *vector : { &spawn.position.vPos, &spawn.position.mOrient.m.dir.r, &spawn.position.mOrient.m.dir.u, &spawn.position.mOrient.m.dir.f })
					for (const fix expected : { vector->v.coord.x, vector->v.coord.y, vector->v.coord.z }) {
						fix actual;
						if (!(input >> actual) || actual != expected) throw std::runtime_error("Spawn transform mismatch at table " + std::to_string(count) + " slot " + std::to_string(slot));
					}
			}
			++count;
		}
		if (count < 96) throw std::runtime_error("Insufficient spawn samples");
		std::cout << count * MAX_PLAYERS << " extracted-reference spawn transforms passed\n";
		std::ifstream cues(std::filesystem::path(argv[1]).parent_path() / "spawn-cue-samples.txt");
		int shown, cue_count = 0;
		while (cues >> action.flags >> shown) {
			for (int player : { -1, N_LOCALPLAYER }) {
				messages = 0;
				action.PrintMessage(player, 1, "New spawn points set!");
				if (messages != shown) throw std::runtime_error("Spawn cue differs from source for player " + std::to_string(player));
				++cue_count;
			}
		}
		if (cue_count != 16) throw std::runtime_error("Missing spawn cue controls");
		std::cout << cue_count << " extracted-reference player/object spawn cues passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
