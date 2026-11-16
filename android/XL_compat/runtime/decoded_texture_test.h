#include "pog_resources.h"
#include "texture_pixels.h"
#include "../xl_texture_frame.h"

static nlohmann::json decoded_texture_owner_test(xl_compat::TargetSources &sources, const xl_assets::ObjectDependencies &graph)
{
	const char *path = "mods/boilpnt/textures/metl060.tga";
	const auto bytes = sources.directory_bytes(path, xl_compat::pog_file_limit);
	require(bool(bytes), "Decoded texture control requires original metl060");
	auto original = xl_compat::read_particle_image(bytes->data(), bytes->size());
	require(original.rgb_opaque && original.frames == 1, "Original RGB control format changed");
	int texture = -1;
	for (int id = 0; id < graph.texture_count; ++id) {
		const int bitmap = graph.texture_data[id].index;
		const char *name = graph.resource_banks->bitmap_name(bitmap);
		if (name && !std::strcmp(name, "metl060")) {
			texture = id;
			original.index = bitmap;
			break;
		}
	}
	require(texture >= 0 && texture < NumTextures && Textures[texture].index == original.index,
	        "Original RGB target differs from the initialized native bank");
	const auto saved = GameBitmaps[original.index];
	require(saved.bm_type != BM_RGBA, "Native pixel control requires a stock outgoing bitmap");
	struct Retire {
		~Retire()
		{
			xl_pog_install(nullptr);
		}
	} retire;
	using Owner = std::unique_ptr<xl_pog_resources, decltype(&xl_pog_free)>;
	Owner prepared(xl_pog_prepare_rgba({ original }, Num_bitmap_files), xl_pog_free);
	require(xl_pog_prepare_storage(prepared.get()), "Cannot allocate retained decoded pixels");
	Owner moved(std::move(prepared));
	require(xl_pog_install_prepared(moved.get()), "Cannot install retained original RGB image");
	const auto *pixels = GameBitmaps[original.index].bm_data;
	unsigned char average[3];
	require(xl_pog_average_rgb(original.index, average) && !std::memcmp(average, original.average_rgb.data(), 3),
	        "Native owned RGB average differs from the decoded source");
	require(pixels && GameBitmaps[original.index].bm_rgb_opaque &&
	            !std::memcmp(pixels, original.pixels.data(), original.pixels.size()),
	        "Original RGB bytes changed during native publication");
	require(xl_pog_install_prepared(moved.get()) && GameBitmaps[original.index].bm_data == pixels,
	        "Repeated decoded pixel publication changed storage");
	unsigned installed_bytes = 0, installed_hash = 0;
	require(xl_pog_bitmap_receipt(moved.get(), original.index, &installed_bytes, &installed_hash),
	        "Original decoded pixels lack a native installed receipt");
	unsigned rejected = 0;
	for (int control = 0; control < 3; ++control) {
		auto invalid = original;
		if (control == 0) invalid.pixels.pop_back();
		if (control == 1) invalid.index = Num_bitmap_files;
		if (control == 2) invalid.pixels[3] = 0;
		try {
			Owner candidate(xl_pog_prepare_rgba({ invalid }, Num_bitmap_files), xl_pog_free);
		} catch (const std::runtime_error &) {
			++rejected;
		}
		require(GameBitmaps[original.index].bm_data == pixels, "Rejected decoded input changed native storage");
	}
	require(rejected == 3, "Malformed decoded image was admitted");
	// Explicit synthetic purple controls distinguish 24-bit RGB from 32-bit TGA
	for (bool rgb : { true, false }) {
		auto control = original;
		control.width = control.height = control.frames = 1;
		control.pixels = { 120, 88, 128, 255 };
		control.rgb_opaque = rgb;
		if (!rgb) xl_compat::prepare_pog_rgba(control);
		Owner candidate(xl_pog_prepare_rgba({ control }, Num_bitmap_files), xl_pog_free);
		require(xl_pog_install(candidate.get()), "Cannot install purple pixel control");
		require(xl_texture_pixel_transparency(texture, 0, 0, 0) == (rgb ? 0 : -1),
		        "Native pixel query confused RGB purple with RGBA key");
		grs_bitmap child{};
		gr_init_sub_bitmap(&child, &GameBitmaps[original.index], 0, 0, 1, 1);
		require((child.bm_rgb_opaque != 0) == rgb && gr_ugpixel(&child, 0, 0) ==
		                                                 (rgb ? gr_find_closest_color(120 / 4, 88 / 4, 128 / 4) : 254),
		        "Native subbitmap or indexed consumer lost RGB/RGBA semantics");
	}
	// A whole TGA strip is a different physics view from a static POG frame
	unsigned strip_queries = 0;
	for (bool scoped : { false, true }) {
		auto strip = original;
		strip.width = 2;
		strip.height = 4;
		strip.frames = 2;
		strip.pixels.assign(32, 0);
		for (unsigned pixel = 0; pixel < 4; ++pixel) {
			strip.pixels[pixel * 4] = 80;
			strip.pixels[pixel * 4 + 3] = 255;
		}
		xl_compat::prepare_pog_rgba(strip);
		Owner candidate(xl_pog_prepare_rgba({ strip }, Num_bitmap_files, scoped), xl_pog_free);
		require(xl_pog_install(candidate.get()), "Cannot install independent strip views");
		require(xl_pog_average_rgb(strip.index, average) && !std::memcmp(average, strip.average_rgb.data(), 3),
		        "Native strip average differs from the whole decoded source");
		xl_pog_pixel_view view{};
		require(xl_pog_physics_view(strip.index, &view) && view.width == 2 && view.height == (scoped ? 4u : 2u) &&
		            view.pixels == GameBitmaps[strip.index].bm_data && GameBitmaps[strip.index].bm_h == 2,
		        "Native strip display and physics dimensions were conflated");
		require(unsigned(GameBitmaps[strip.index].bm_flags) == (scoped ? dxx_xl_texture_frame_flags(strip.transparent_frames.data(), strip.super_frames.data(), 0) : strip.flags & ~xl_compat::pog_rgba),
		        "Scoped strip display flags did not select the first frame independently of physics");
		for (int orientation = 0; orientation < 4; ++orientation) {
			const int expected = scoped && (orientation == 0 || orientation == 3) ? 1 : 0;
			require(xl_texture_pixel_transparency(texture, orientation, 16384, 49152) == expected,
			        "Native strip physics query sampled the display frame instead of its source view");
			++strip_queries;
		}
	}
	require(xl_pog_install(nullptr), "Cannot retire decoded pixel controls");
	xl_pog_pixel_view retired_view{};
	require(!xl_pog_physics_view(original.index, &retired_view), "Retired pixel owner still exposes a physics view");
	require(!xl_pog_average_rgb(original.index, average), "Retired pixel owner still exposes an average");
	require(GameBitmaps[original.index].bm_type == saved.bm_type &&
	            GameBitmaps[original.index].bm_rgb_opaque == saved.bm_rgb_opaque,
	        "Decoded pixel retirement lost outgoing bitmap type");
	bitmap_index index{ static_cast<ushort>(original.index) };
	PIGGY_PAGE_IN(index);
	require(GameBitmaps[original.index].bm_data && !(GameBitmaps[original.index].bm_flags & BM_FLAG_PAGED_OUT),
	        "Native paging did not recover after decoded pixels");
	return { { "source", path }, { "source_sha256", picosha2::hash256_hex_string(*bytes) }, { "installed_bytes", installed_bytes }, { "installed_fnv1a", installed_hash }, { "malformed_rejections", rejected }, { "rgb_rgba_key_controls", 2 }, { "strip_physics_queries", strip_queries }, { "retirement_paging", "passed" } };
}
