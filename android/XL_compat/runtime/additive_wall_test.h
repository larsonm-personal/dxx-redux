#include "doorway.h"
#include "../tests/hog_fixture.h"
static void additive_wall_binding_controls(bool active)
{
	require(Num_walls > 0, "Additive wall control requires a native wall");
	struct Restore {
		wall saved = Walls[0];
		~Restore()
		{
			Walls[0] = saved;
			xl_preview_end_face();
		}
	} restore;
	auto &wall = Walls[0];
	wall.type = WALL_CLOSED;
	wall.state = WALL_DOOR_CLOSED;
	wall.hps = 0;
	require(Segments[wall.segnum].sides[wall.sidenum].wall_num == 0, "Additive control wall has inconsistent native ownership");
	unsigned cases = 0;
	for (unsigned flags = 0; flags < 256; ++flags) {
		if (flags & xl_compat::wall_render_additive) continue;
		wall.flags = flags;
		for (int cloak : { 0, 6, 30, 31, 32 }) {
			wall.cloak_value = cloak;
			const auto render_doorway = xl_wall_doorway(-1, wall.segnum, wall.sidenum);
			const auto player_doorway = xl_wall_doorway(ConsoleObject - Objects, wall.segnum, wall.sidenum);
			for (bool additive : { true, false }) {
				wall.flags = flags | (additive ? xl_compat::wall_render_additive : 0);
				xl_preview_begin_face(wall.segnum, wall.sidenum);
				require(bool(xl_preview_additive_face()) == (active && additive), "Additive wall binding lost authored or stock scope");
				// Pinned XL palette.h FADE_LEVELS is 31, not the native palette's 34
				const float alpha = active && cloak > 0 && cloak < 31 ? float(31 - cloak) / 31 : 1.f;
				require(xl_preview_alpha(1) == alpha, "Additive wall binding changed authored opacity");
				xl_preview_end_face();
				require(!xl_preview_additive_face() && xl_preview_alpha(1) == 1, "Additive wall state leaked beyond its face");
				if (active) {
					require(xl_wall_doorway(-1, wall.segnum, wall.sidenum) == render_doorway &&
					            xl_wall_doorway(ConsoleObject - Objects, wall.segnum, wall.sidenum) == player_doorway,
					        "Additive wall flag changed native visibility or passability");
				}
				++cases;
			}
		}
	}
	xl_preview_begin_face(-1, 0);
	require(!xl_preview_additive_face(), "Invalid face retained additive wall state");
	std::cout << "XL additive wall binding (" << (active ? "XL" : "stock") << "): " << cases << " opacity, flag, visibility, passability and scope controls passed\n";
}

static void additive_wall_lifecycle_case(const xl_compat::Bytes &original_archive, const std::filesystem::path &root)
{
	using namespace xl_compat;
	auto members = read_hog(original_archive);
	auto found = std::find_if(members.begin(), members.end(), [](const auto &member) { return member.name == "neptunxl.rl2"; });
	require(found != members.end(), "Additive fixture lacks original Neptune geometry");
	const auto inventory = inspect_level(found->bytes);
	const auto section = std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &entry) { return entry.name == "walls"; });
	require(inventory.level_version == 23 && inventory.game_version == 40 && section != inventory.sections.end() && section->count >= 2,
	        "Additive fixture requires audited wide-flag wall records");
	for (unsigned index = 0; index < 2; ++index) {
		const auto offset = std::size_t(section->offset) + index * 25;
		require(offset + 25 <= found->bytes.size(), "Additive fixture wall record is truncated");
		for (unsigned b = 0; b < 4; ++b) found->bytes[offset + 8 + b] = 0;
		found->bytes[offset + 16] = WALL_CLOSED;
		found->bytes[offset + 17] = index ? 28 : 20;
		found->bytes[offset + 18] = 0;
		found->bytes[offset + 19] = WALL_DOOR_CLOSED;
		found->bytes[offset + 20] = 255;
		found->bytes[offset + 21] = 255;
		found->bytes[offset + 22] = 1;
		found->bytes[offset + 23] = 0;
		found->bytes[offset + 24] = 6;
	}
	require(runtime_blockers(inspect_level(found->bytes)).empty(), "Derived Pumo additive wall profile did not pass normal admission");
	Bytes archive{ 'D', 'H', 'F' };
	for (const auto &member : members) xl_test::hog_member(archive, member.name, member.bytes, member.name.size() > 13);
	write_bytes((root / "fxadd.hog").string().c_str(), archive);
	std::ofstream descriptor(root / "fxadd.mn2");
	descriptor << "d2x-name = Additive wall controls\nnum_levels = 1\nneptunxl.rl2\n";
	descriptor.close();
	xl_preview_clear_mission();
	char error[256]{};
	require(load_base_mission("fxadd", error, sizeof(error)), error);
	StartNewGame(1);
	const auto verify = [] {
		require(xl_level_extensions_active() && Num_walls >= 2, "Additive source lost its runtime ownership");
		for (int index = 0; index < 2; ++index) {
			const auto &wall = Walls[index];
			require(wall.type == WALL_CLOSED && wall.flags == (index ? 28 : 20) && wall.cloak_value == 6, "Native translation or restore changed authored wall fields");
			xl_preview_begin_face(wall.segnum, wall.sidenum);
			require(xl_preview_additive_face() && xl_preview_alpha(1) == 25.f / 31, "Loaded additive wall lost its blend or opacity binding");
			xl_preview_end_face();
		}
	};
	verify();
	char save[] = "xl-additive.sav", description[] = "XL additive wall";
	require(!PHYSFS_exists(save), "Additive save control requires an unused isolated path");
	struct SaveGuard {
		~SaveGuard()
		{
			PHYSFS_delete("xl-additive.sav");
		}
	} save_guard;
	Players[Player_num].energy = 77 * F1_0;
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save the derived additive wall mission");
	Walls[0].flags = 0;
	Walls[0].cloak_value = 0;
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Additive control failed to transition to stock");
	xl_preview_begin_face(0, 0);
	require(!xl_preview_additive_face(), "Authored additive mode leaked into the stock mission");
	xl_preview_end_face();
	require(state_restore_all_sub(save, 0), "Additive checkpoint did not restore through a stock transition");
	verify();
	require(Players[Player_num].energy == 77 * F1_0 && !game_is_time_paused(), "Additive checkpoint lost player state or left time paused");
	std::cout << "XL additive wall lifecycle: normal authored load, preserved flags20/28 and opacity6, disk checkpoint, stock isolation and restored binding passed\n";
}
