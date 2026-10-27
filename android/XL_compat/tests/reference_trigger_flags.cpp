// Recorded wall/audio adapters; no renderer, mixer or full collision comparison
#include "../trigger_flags.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <stdexcept>
#include <vector>
#include "xl_reference_trigger_flag_constants.h"
#define DBG 0
#define Assert(value)                                                  \
	do {                                                               \
		if (!(value)) throw std::runtime_error("Reference assertion"); \
	} while (0)
#define SHOW_DYN_LIGHT   0
#define I2X(value)       ((value) * 65536)
#define TXT_TRIG_SINGLE  "single wall target"
#define TXT_ILLUS_SINGLE "single illusion target"
using CFixVector = std::array<std::int32_t, 3>;
std::vector<std::array<int, 6>> audio_calls;
std::vector<int> stuck_calls;
void PrintLog(int, const char *, ...) {}
void Warning(const char *, ...) {}
struct CWall {
	int nType = WALL_CLOSED;
	unsigned flags = 0;
};
std::array<CWall, 2> oracle_walls;
#define WALLS oracle_walls.data()
void KillStuckObjects(std::ptrdiff_t wall)
{
	stuck_calls.push_back(static_cast<int>(wall));
}
struct CSegment {
	int index = 0;
	std::array<int, 6> m_children{}, wall_indices{};
	struct Side {
		int m_nBaseTex = 0;
	} m_sides[6];
	CWall *Wall(int side)
	{
		return side < 0 || side >= 6 || wall_indices[side] < 0 ? nullptr : &oracle_walls.at(wall_indices[side]);
	}
	int Index() const
	{
		return index;
	}
	int ConnectedSide(CSegment *other) const
	{
		for (int side = 0; side < 6; ++side)
			if (other->m_children[side] == index) return side;
		return -1;
	}
	CFixVector SideCenter(int side) const
	{
		return { index, side, 0 };
	}
	void StartCloak(int) {}
	void StartDecloak(int) {}
	void IllusionOn(int);
	void IllusionOff(int);
};
std::array<CSegment, 2> oracle_segments;
#define SEGMENTS         oracle_segments.data()
#define SEGMENT(index)   (&oracle_segments.at(index))
#define SEG_IDX(segment) ((segment)->index)
struct CTrigger;
struct {
	struct {
		int nSegments = 2;
	} segData;
	struct {
		struct {
			bool IsElement(CTrigger *) const
			{
				return false;
			}
			int Index(CTrigger *) const
			{
				return 0;
			}
		} triggers[2];
	} trigData;
	struct {
		struct {
			struct {
				int flags = TMI_FORCE_FIELD;
			} pTexMapInfo[1];
		} tex;
	} pigData;
} gameData;
struct {
	struct {
		bool bSwitches = false, bWalls = false;
	} legacy;
} options, *gameOpts = &options;
struct {
	void CreateSegmentSound(int sound, int segment, int side, CFixVector, int loop, int volume)
	{
		audio_calls.push_back({ 1, sound, segment, side, loop, volume });
	}
	void DestroySegmentSound(int segment, int side, int sound)
	{
		audio_calls.push_back({ 2, sound, segment, side, 0, 0 });
	}
} audio;
struct {
	void Toggle(int, int, int, int)
	{
		throw std::runtime_error("Unqualified dynamic-light branch");
	}
} lightManager;
struct CTrigger {
	int type = TT_OPEN_WALL, flags = 0, m_nLinks = 1;
	std::array<std::int16_t, 10> m_segments{}, m_sides{};
	int Type() const
	{
		return type;
	}
	int Index() const
	{
		return 0;
	}
	bool Flagged(int mask) const
	{
		return (flags & mask) != 0;
	}
	int DoChangeWalls();
	void DoIllusionOff();
	void DoIllusionOn();
};
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include "xl_reference_trigger_flags.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
static void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
static void reset()
{
	oracle_walls = {};
	for (int i = 0; i < 2; ++i) {
		auto &segment = oracle_segments[i];
		segment = {};
		segment.index = i;
		segment.m_children.fill(-1);
		segment.wall_indices.fill(-1);
		segment.m_children[0] = 1 - i;
		segment.wall_indices[0] = i;
	}
	gameData.pigData.tex.pTexMapInfo[0].flags = TMI_FORCE_FIELD;
	audio_calls.clear();
	stuck_calls.clear();
}
int main()
{
	try {
		static_assert(xl_compat::trigger_flags_mask == (TF_NO_MESSAGE | TF_ONE_SHOT | TF_DISABLED | TF_PERMANENT | TF_ALTERNATE | TF_SET_ORIENT | TF_SILENT | TF_AUTOPLAY));
		int cases = 0;
		for (int flags = 0; flags < 256; ++flags) {
			for (int scenario = 0; scenario < 8; ++scenario)
				for (int type : { TT_OPEN_WALL, TT_CLOSE_WALL, TT_ILLUSORY_WALL }) {
					reset();
					CTrigger action;
					action.flags = flags;
					action.type = type;
					const int desired = type == TT_OPEN_WALL ? WALL_OPEN : type == TT_CLOSE_WALL ? WALL_CLOSED
					                                                                             : WALL_ILLUSION;
					oracle_walls[0].nType = oracle_walls[1].nType = type == TT_CLOSE_WALL ? WALL_OPEN : WALL_CLOSED;
					if (scenario == 1 || scenario == 2) oracle_walls[0].nType = desired;
					if (scenario == 1 || scenario == 3) oracle_walls[1].nType = desired;
					if (scenario == 4) oracle_segments[1].wall_indices[0] = -1;
					if (scenario == 5) oracle_segments[0].wall_indices[0] = -1;
					if (scenario == 6) gameData.pigData.tex.pTexMapInfo[0].flags = 0;
					if (scenario == 7) action.m_nLinks = 2;
					const bool has_front = scenario != 5, has_back = scenario != 4, forcefield = scenario != 6;
					const bool changed = has_front && !(oracle_walls[0].nType == desired && (!has_back || !forcefield || oracle_walls[1].nType == desired));
					const auto result = action.DoChangeWalls();
					require(bool(result) == changed, "Reference wall action changed-state predicate differs");
					const auto count = [&](int kind, int sound) { return std::count_if(audio_calls.begin(), audio_calls.end(), [&](const auto &call) { return call[0] == kind && call[1] == sound; }); };
					const int expected_off = changed && forcefield && type == TT_OPEN_WALL && xl_compat::trigger_cue_enabled(flags, xl_compat::TriggerCue::ForcefieldOff) ? 1 : 0;
					require(count(1, SOUND_FORCEFIELD_OFF) == expected_off, "Production forcefield cue policy differs from pinned function");
					require(count(2, SOUND_FORCEFIELD_HUM) == (changed && forcefield && type == TT_OPEN_WALL ? 1 + int(has_back) : 0), "Silent changed hum removal semantics");
					if (changed && forcefield) require(oracle_walls[0].nType == desired && (!has_back || oracle_walls[1].nType == desired), "Reference forcefield wall state lost");
					++cases;
				}
			reset();
			CTrigger action;
			action.flags = flags;
			action.DoIllusionOff();
			require(audio_calls.size() == (xl_compat::trigger_cue_enabled(flags, xl_compat::TriggerCue::IllusionOff) ? 1u : 0u), "Production illusion cue policy differs from pinned function");
			if (!audio_calls.empty()) require(audio_calls[0] == std::array<int, 6>{ 1, SOUND_WALL_REMOVED, 0, 0, 0, I2X(1) }, "Illusion cue parameters differ");
			require((oracle_walls[0].flags & WALL_ILLUSION_OFF) && (oracle_walls[1].flags & WALL_ILLUSION_OFF), "Paired same-side reference illusion did not turn off");
			audio_calls.clear();
			action.DoIllusionOn();
			require(audio_calls.empty() && !(oracle_walls[0].flags & WALL_ILLUSION_OFF) && !(oracle_walls[1].flags & WALL_ILLUSION_OFF), "Illusion-on flag behavior differs");
			++cases;
		}
		reset();
		oracle_segments[1].m_children[0] = -1;
		oracle_segments[1].wall_indices[0] = -1;
		oracle_segments[1].m_children[2] = 0;
		oracle_segments[1].wall_indices[2] = 1;
		CTrigger action;
		action.DoIllusionOff();
		require((oracle_walls[0].flags & WALL_ILLUSION_OFF) && !(oracle_walls[1].flags & WALL_ILLUSION_OFF), "Pinned same-side illusion-off quirk changed; revisit the intentional connected-side correction");
		std::cout << "Pinned XL trigger flags: " << cases << " leaf cue comparisons passed; connected-side source quirk recorded\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
