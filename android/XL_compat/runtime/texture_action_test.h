#include "texture_action_controls.h"
#include "../tests/texture_action_fixture.h"

static int texture_action_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		using namespace xl_texture_controls;
		using xl_texture_controls::require;
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		xl_test::write_texture_action_fixture(root / "missions/neptunxl.hog", root / "missions", { Textures[1].index, Textures[2].index, Textures[3].index, Textures[4].index });
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		for (int ordinal : { 1, 2 }) {
			require(load_base_mission("xtexture", error, sizeof(error)), error);
			StartNewGame(ordinal);
			Game_suspended = SUSP_ROBOTS;
			events();
			const auto before = snapshot();
			const auto original_material = TmapInfo[4];
			TmapInfo[4].eclip_num = short(Num_effects);
			const bool without_action = xl_level_supported("noaction.rl2", 0) != 0;
			const bool with_action = xl_level_supported(ordinal == 1 ? "tex23.rl2" : "tex27.rl2", 0) != 0;
			const std::string dependency_error = xl_level_rejection();
			TmapInfo[4] = original_material;
			require(without_action && !with_action && dependency_error.find("texture action") != std::string::npos && snapshot() == before,
			        "Future texture/effect dependency was not checked before admission");
			for (const auto &bad : { std::array<const char *, 2>{ "badbase.rl2", "trigger texture" }, { "badovl.rl2", "trigger texture" }, { "badlink.rl2", "invalid trigger target" }, { "badobject.rl2", "object trigger texture" } }) {
				require(!xl_level_supported(bad[0], 0) && std::strstr(xl_level_rejection(), bad[1]) && load_level(bad[0]) != 0,
				        "Invalid texture admission lacks its feature/record reason");
				require(snapshot() == before, "Rejected texture source changed active sides");
			}
			const auto pending = prepare();
			const auto earned = snapshot();
			char save[] = "xl-textures.sav", description[] = "XL texture continuation";
			stop_time();
			require(state_save_all_sub(save, description), "Cannot save earned texture state");
			continuation(pending);
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active(), "Texture stock transition failed");
			const auto stock = snapshot();
			trigger action{};
			action.type = 19;
			require(xl_trigger_texture_action(&action) == 1 && snapshot() == stock, "XL texture operation contaminated Redux");
			require(state_restore_all_sub(save, 0) && xl_source_level_version() == (ordinal == 1 ? 23 : 27) && snapshot() == earned,
			        "Disk-through-stock lost texture IDs, orientation or UV/light records");
			continuation(pending);
			require(!game_is_time_paused(), "Native save/restore left the simulation paused");
			std::cout << (ordinal == 1 ? 23 : 27) << "/40 textures: wall/master/object/autoplay, actual collision, rotated clear/restore and delayed disk-through-stock passed\n";
		}
		std::cout << "XL texture action runtime controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
