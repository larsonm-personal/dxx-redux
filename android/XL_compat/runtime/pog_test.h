#include "pog_resources.h"
#include "texture_pixels.h"
#include "../pog.h"
extern "C" {
#include "gauges.h"
#include "collide.h"
void free_bitmap_replacements(void);
int check_trans_wall(vms_vector *, segment *, int, int);
int check_transparency(segment *, int);
}

namespace pog_control
{
using Resources = std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)>;

static xl_compat::Bytes read(const char *name)
{
	auto *file = PHYSFSX_openReadBuffered(name);
	require(file != nullptr, "Cannot open POG control resource");
	const auto size = PHYSFS_fileLength(file);
	require(size > 0 && size <= PHYSFS_sint64(xl_compat::pog_file_limit), "Invalid POG control resource size");
	xl_compat::Bytes result(static_cast<size_t>(size));
	const auto actual = PHYSFS_readBytes(file, result.data(), result.size());
	require(PHYSFS_close(file) && actual == size, "Cannot read POG control resource");
	return result;
}

static Resources prepare(const char *name)
{
	auto *file = PHYSFSX_openReadBuffered(name);
	require(file != nullptr, "Missing POG control input");
	char error[256]{};
	Resources result(xl_pog_prepare(file, Num_bitmap_files, error, sizeof(error)), xl_pog_free);
	require(PHYSFS_close(file) != 0, "Cannot close POG control input");
	require(bool(result), error);
	return result;
}

static std::vector<xl_compat::PogBitmap> original(const char *name, unsigned count)
{
	const auto bytes = read(name);
	auto images = xl_compat::read_pog(bytes.data(), bytes.size(), Num_bitmap_files);
	require(images.size() == count, "Original POG replacement count changed");
	return images;
}

static void installed(const std::vector<xl_compat::PogBitmap> &images)
{
	for (const auto &image : images) {
		const auto &bitmap = GameBitmaps[image.index];
		if (bitmap.bm_type == BM_RGBA && bitmap.bm_data && bitmap.bm_w == int(image.width) && bitmap.bm_h == int(image.height))
			for (size_t byte = 0; byte < image.pixels.size(); ++byte)
				if (bitmap.bm_data[byte] != image.pixels[byte])
					throw std::runtime_error("Installed POG bitmap " + std::to_string(image.index) + " byte " + std::to_string(byte) +
					                         " is " + std::to_string(bitmap.bm_data[byte]) + " expected " + std::to_string(image.pixels[byte]));
		require(image.truecolor && image.frames == 1 && bitmap.bm_type == BM_RGBA &&
		            bitmap.bm_w == int(image.width) && bitmap.bm_h == int(image.height) &&
		            bitmap.bm_rowsize == int(image.width * 4) && !piggy_bitmap_get_offset(image.index) &&
		            bitmap.bm_data && !std::memcmp(bitmap.bm_data, image.pixels.data(), image.pixels.size()),
		        ("Installed original POG bitmap " + std::to_string(image.index) + ": format=" + std::to_string(bitmap.bm_type) +
		         " dimensions=" + std::to_string(bitmap.bm_w) + "x" + std::to_string(bitmap.bm_h) +
		         " stride=" + std::to_string(bitmap.bm_rowsize) + " offset=" + std::to_string(piggy_bitmap_get_offset(image.index)) +
		         "; expected RGBA " + std::to_string(image.width) + "x" + std::to_string(image.height) + " with exact owned pixels")
		            .c_str());
		require(piggy_bitmap_get_flags(const_cast<grs_bitmap *>(&bitmap)) == int(image.flags & 7), "Installed RGBA semantic flags differ");
	}
}

static xl_compat::Bytes fixture(const std::vector<xl_compat::PogBitmap> &images)
{
	xl_compat::Bytes bytes(12 + images.size() * 20);
	std::memcpy(bytes.data(), "DPOG", 4);
	const auto put = [&](size_t at, unsigned value, unsigned count) {
		for (unsigned i = 0; i < count; ++i) bytes[at + i] = static_cast<unsigned char>(value >> (i * 8));
	};
	put(4, 1, 4);
	put(8, unsigned(images.size()), 4);
	unsigned offset = 0;
	for (size_t i = 0; i < images.size(); ++i) {
		const auto &image = images[i];
		put(12 + i * 2, image.index, 2);
		const auto header = 12 + images.size() * 2 + i * 18;
		bytes[header + 9] = static_cast<unsigned char>(image.width);
		bytes[header + 10] = static_cast<unsigned char>(image.height);
		bytes[header + 11] = static_cast<unsigned char>((image.width >> 8) | ((image.height >> 8) << 4));
		bytes[header + 12] = static_cast<unsigned char>(image.source_flags);
		put(header + 14, offset, 4);
		offset += unsigned(image.pixels.size());
	}
	for (const auto &image : images) bytes.insert(bytes.end(), image.pixels.begin(), image.pixels.end());
	return bytes;
}

static xl_compat::PogBitmap solid(unsigned index, int kind)
{
	xl_compat::PogBitmap result;
	result.index = index;
	result.width = result.height = 1;
	result.source_flags = 128;
	result.truecolor = true;
	result.pixels = kind < 0 ? xl_compat::Bytes{ 120, 88, 128, 255 } : xl_compat::Bytes{ 70, 90, 110, static_cast<unsigned char>(kind) };
	return result;
}

static std::vector<unsigned> bank_state(const std::vector<xl_compat::PogBitmap> &images, const std::vector<unsigned> *expected = nullptr)
{
	std::vector<unsigned> result;
	for (const auto &image : images) {
		if (expected) require(unsigned(piggy_bitmap_get_offset(image.index)) == expected->at(result.size()),
			                  "POG cleanup restored an old PIG offset over the new palette bank");
		bitmap_index index{ static_cast<ushort>(image.index) };
		PIGGY_PAGE_IN(index);
		const auto &bitmap = GameBitmaps[image.index];
		require(bitmap.bm_type == BM_LINEAR && bitmap.bm_data, "PIG bank did not regain its indexed bitmap");
		unsigned bytes = unsigned(bitmap.bm_rowsize) * bitmap.bm_h;
		if (bitmap.bm_flags & BM_FLAG_RLE) std::memcpy(&bytes, bitmap.bm_data, sizeof(bytes));
		require(bytes && bytes <= 1024 * 1024, "PIG bank control has invalid pixel storage");
		unsigned hash = 2166136261u;
		for (unsigned byte = 0; byte < bytes; ++byte) hash = (hash ^ bitmap.bm_data[byte]) * 16777619u;
		for (const auto value : { unsigned(piggy_bitmap_get_offset(image.index)), unsigned(bitmap.bm_w), unsigned(bitmap.bm_h),
		                          unsigned(bitmap.bm_rowsize), unsigned(bitmap.bm_flags), hash }) result.push_back(value);
	}
	return result;
}

static void bank_transitions(const std::vector<xl_compat::PogBitmap> &images, xl_pog_resources *resources)
{
	free_bitmap_replacements();
	std::cout << "POG bank control: capturing clean indexed banks\n";
	char other[] = "water.pig";
	piggy_new_pigfile(other);
	const auto expected_water = bank_state(images);
	for (const char *name : { "alien1.pig", "groupa.pig" }) {
		char selected[FILENAME_LEN];
		std::strcpy(selected, name);
		piggy_new_pigfile(selected);
		const auto expected_bank = bank_state(images);
		require(xl_pog_install(resources), "Cannot install POG before bank transition");
		piggy_new_pigfile(other);
		free_bitmap_replacements();
		require(bank_state(images, &expected_water) == expected_water, "POG cleanup retained the previous palette bank's offsets, flags or pixels");
		require(xl_pog_install(resources), "Cannot install POG over the second bank");
		piggy_new_pigfile(selected);
		free_bitmap_replacements();
		require(bank_state(images, &expected_bank) == expected_bank, "POG cleanup overwrote the new palette bank's offsets, flags or pixels");
	}
	std::cout << "POG bank transitions: original alien1/water/groupa indexed headers and pixels restored exactly\n";
}

static void physics(const std::filesystem::path &root)
{
	auto &room = Segments[187];
	auto &surface = room.sides[1];
	require(surface.wall_num == 17 && room.children[1] >= 0, "Original arena transparency portal changed");
	const auto old_side = surface;
	const auto old_wall = Walls[17];
	const auto old_tmap = TmapInfo[2];
	surface.tmap_num = 1;
	Walls[17].type = WALL_CLOSED;
	Walls[17].flags = Walls[17].cloak_value = 0;
	Walls[17].keys = KEY_NONE;
	Walls[17].trigger = -1;
	TmapInfo[2].eclip_num = -1;
	TmapInfo[2].destroyed = 3;
	vms_vector face, begin, end;
	compute_center_point_on_side(&face, &room, 1);
	vm_vec_scale_add(&begin, &face, &surface.normals[0], F1_0 / 2);
	vm_vec_scale_add(&end, &face, &surface.normals[0], -F1_0 / 2);
	object blower{};
	blower.ctype.laser_info.parent_type = OBJ_PLAYER;
	unsigned rays = 0, effects = 0;
	for (int bottom : { 0, 32, 255 })
		for (int top : { -1, 0, 32, 255 }) {
			write_bytes((root / "control.pog").string().c_str(), fixture({ solid(Textures[1].index, bottom), solid(Textures[2].index, top) }));
			auto resources = prepare("control.pog");
			require(xl_pog_install(resources.get()), "Cannot install native transparency control");
			for (int orientation = 0; orientation < 4; ++orientation) {
				surface.tmap_num2 = short(2 | orientation << 14);
				const bool transparent = top < 0 || (top == 0 && bottom == 0);
				require((check_trans_wall(&face, &room, 1, 0) != 0) == transparent, "Actual wall hit-point transparency differs from XL layering");
				fvi_query query{};
				query.p0 = &begin;
				query.p1 = &end;
				query.startseg = 187;
				query.thisobjnum = -1;
				query.flags = FQ_TRANSPOINT;
				fvi_info hit{};
				require((find_vector_intersection(&query, &hit) == HIT_NONE) == transparent, "Real FVI did not respect RGBA keyed/transparent/opaque layers");
				++rays;
				const int previous_highest = Highest_object_index;
				require(check_effect_blowup(&room, 1, &face, &blower, 0) == (top > 0), "Real destructible-texture hit differs for key or partial alpha");
				require(surface.tmap_num2 == short((top > 0 ? 3 : 2) | orientation << 14), "Destructible texture lost orientation or changed through a hole");
				for (int object = Highest_object_index; object > previous_highest; --object)
					if (Objects[object].type != OBJ_NONE) obj_delete(object);
				++effects;
			}
		}
	surface = old_side;
	Walls[17] = old_wall;
	TmapInfo[2] = old_tmap;
	std::cout << "RGBA native consumers: " << rays << " actual FVI rays and " << effects << " destructible texture hits passed\n";
}

static void rejection(const std::filesystem::path &root, const std::vector<xl_compat::PogBitmap> &images)
{
	const auto path = root / "rtarena.pog";
	const auto energy = Players[Player_num].energy;
	const auto state = arena_state();
	for (int kind = 0; kind < 4; ++kind) {
		auto image = solid(Textures[1].index, 0);
		if (kind == 1) {
			image.height = 2;
			image.pixels.resize(8);
		}
		if (kind == 2) image.index = cockpit_bitmap[0].index;
		if (kind == 3) image.index = unsigned(Num_bitmap_files);
		auto data = fixture({ image });
		if (!kind) data.pop_back();
		write_bytes(path.string().c_str(), data);
		const char *reason = kind == 0 ? "truncated" : kind == 1 ? "animation frames"
		                                           : kind == 2   ? "cockpit/HUD"
		                                                         : "index";
		require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), reason), "POG rejection omitted the actionable encoding reason");
		require(load_level("rtarena.rl2") != 0 && arena_state() == state && Players[Player_num].energy == energy, "Rejected POG changed active gameplay");
		installed(images);
	}
	require(std::filesystem::remove(path), "Cannot remove private rejection fixture");
	require(xl_level_supported("rtarena.rl2", 0), "Original POG did not recover after rejection");
	std::cout << "POG admission: truncated pixels, unsupported strips/HUD and bad indices rejected before live mutation\n";
}
} // namespace pog_control

static int pog_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		using namespace pog_control;
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		new_player_config();
		std::strcpy(Players[Player_num].callsign, "XLPog");
		input_demo_set_skip_level_intro(1);
		require(load_base_mission("d2", error, sizeof(error)), error);
		StartNewGame(1);
		const auto stock_definitions = gameplay_definitions();
		require(load_base_mission("rtarena", error, sizeof(error)), error);
		StartNewGame(1);
		require(xl_level_extensions_active() && !std::strcmp(Gamesave_current_filename, "rtarena.rl2"), "Original RGBA mission did not start normally");
		const auto arena = original("rtarena.pog", 19);
		installed(arena);
		auto arena_resources = prepare("rtarena.pog");
		bank_transitions(arena, arena_resources.get());
		require(xl_pog_install(arena_resources.get()), "Cannot recover original POG after bank transitions");
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-userdir")) / "pog-controls";
		std::filesystem::create_directories(root);
		require(PHYSFS_addToSearchPath(root.string().c_str(), 0), "Cannot mount private POG controls");
		physics(root);
		require(xl_pog_install(arena_resources.get()), "Cannot restore owned original POG");
		installed(arena);
		rejection(root, arena);
		// Prepared resources own their pixels after the backing file changes
		auto rgba = solid(Textures[1].index, 32), indexed = solid(Textures[2].index, 0);
		indexed.source_flags = 3;
		indexed.truecolor = false;
		indexed.pixels = { 254 };
		auto last = rgba;
		last.pixels = { 11, 22, 33, 255 };
		write_bytes((root / "control.pog").string().c_str(), fixture({ rgba, indexed, last }));
		auto mixed = prepare("control.pog");
		write_bytes((root / "control.pog").string().c_str(), { 'B', 'A', 'D' });
		for (int attempt = 0; attempt < 2; ++attempt) {
			require(xl_pog_install(mixed.get()), "Cannot reinstall owned mixed POG");
			const auto &a = GameBitmaps[rgba.index], &b = GameBitmaps[indexed.index];
			require(a.bm_type == BM_RGBA && a.bm_data[0] == 11 && a.bm_data[3] == 255 &&
			            b.bm_type == BM_LINEAR && b.bm_data[0] == 254 && b.bm_flags == 3,
			        "Mixed formats, duplicate ordering or immutable preparation failed");
		}
		require(xl_pog_install(arena_resources.get()), "Cannot recover original resources after mixed POG");
		installed(arena);
		require(PHYSFS_removeFromSearchPath(root.string().c_str()), "Cannot unmount private POG controls");
		if (const auto hog = find_arg_value(argc, argv, "-pumo-hog")) {
			require(PHYSFS_addToSearchPath(hog, 0), "Cannot mount read-only original Pumo resources");
			unsigned count = 0;
			for (const char *name : { "pmlev1.pog", "pmlev2.pog", "pmlev3.pog", "pm-hub.pog", "tulev2.pog" }) {
				const unsigned expected = !std::strcmp(name, "pm-hub.pog") ? 6 : 5;
				const auto images = original(name, expected);
				auto resources = prepare(name);
				require(xl_pog_install(resources.get()), "Cannot install original Pumo POG resources");
				installed(images);
				count += expected;
			}
			require(PHYSFS_removeFromSearchPath(hog) && count == 26, "Original Pumo resource coverage incomplete");
			std::cout << "Original Pumo POG resources: all five files and 26 replacements installed with source-normalized RGBA bytes; campaign not admitted\n";
		}
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		installed(arena);
		char save[] = "pog-owned.sav", description[] = "XL RGBA resources";
		stop_time();
		require(state_save_all_sub(save, description), "Cannot save original RGBA mission");
		require(load_base_mission("d2", error, sizeof(error)), "RGBA-to-stock mission selection failed");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(!xl_level_extensions_active() && gameplay_definitions() == stock_definitions, "RGBA resource scope changed Redux definitions");
		for (int i = 0; i < Num_bitmap_files; ++i) require(GameBitmaps[i].bm_type != BM_RGBA, "RGBA descriptor leaked into Redux registry");
		for (const auto &image : arena) {
			bitmap_index index{ static_cast<ushort>(image.index) };
			PIGGY_PAGE_IN(index);
			require(GameBitmaps[image.index].bm_type == BM_LINEAR && GameBitmaps[image.index].bm_data, "Redux bitmap paging did not recover");
		}
		require(state_restore_all_sub(save, 0) && xl_level_extensions_active(), "RGBA disk restore through stock failed");
		installed(arena);
		start_time();
		std::cout << "Original Arena RGBA: 19 installed replacements, immutable mixed resources, repeated replacement, rejection recovery, disk through stock and paging passed\n";
		return 0;
	} catch (const std::exception &exception) {
		std::cerr << "POG native runtime failed: " << exception.what() << '\n';
		return 1;
	}
}
