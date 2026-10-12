#include "input_demo_codec.h"

static void check_source_bitmaps(const nlohmann::json &rows)
{
	for (const auto &row : rows) {
		const int index = row.at("index");
		require(index > 0 && index < Num_bitmap_files, "source bitmap index registered");
		bitmap_index bi = { static_cast<ushort>(index) };
		PIGGY_PAGE_IN(bi);
		const auto &bm = GameBitmaps[index];
		require(bm.bm_data && bm.bm_w == row.at("width") && bm.bm_h == row.at("height") && bm.bm_rowsize == bm.bm_w, "source dimensions and CPU pixels");
		require(bm.bm_flags == row.at("flags") && bm.avg_color == row.at("average"), "source transparency and average color");
		require(piggy_game_bitmap_name(&GameBitmaps[index]) == row.at("name"), "source runtime bitmap name");
		std::string hash, error;
		require(input_demo_sha256_hex(bm.bm_data, bm.bm_w * bm.bm_h, &hash, &error), "hash runtime CPU pixels");
		if (hash != row.at("sha256")) std::fprintf(stderr, "source mismatch at bitmap %d\n", index);
		require(hash == row.at("sha256"), "every indexed pixel matches independent source decoder");
	}
}
