#include "resource_banks.h"
#include "retained_resource.h"
#include "picosha2.h"
#include <stdexcept>
#include <utility>
extern "C" {
#include "piggy_bank.h"
#include "gr.h"
#include "bounded_rle.h"
}

static_assert(sizeof(piggy_bank_bitmap) == 40 && sizeof(piggy_bank_sound) == 20, "Native bank record packing changed");

struct XlResourceBanks::State {
	std::vector<unsigned char> bitmap_bytes, sound_bytes;
	std::vector<piggy_bank_bitmap> bitmaps{ 1 };
	std::vector<piggy_bank_sound> sounds;
	std::string bitmap_hash, sound_hash;
	int rate;
};

XlResourceBanks::XlResourceBanks(std::vector<unsigned char> bitmaps, std::vector<unsigned char> sounds, int sample_rate)
    : state(std::make_unique<State>())
{
	if (sample_rate != 11025 && sample_rate != 22050) throw std::runtime_error("Unsupported native source bank rate");
	state->rate = sample_rate;
	state->bitmap_bytes = std::move(bitmaps);
	state->sound_bytes = std::move(sounds);
	xl_read_retained_resource(state->bitmap_bytes, [&](PHYSFS_file *file) {
		const int count = piggy_read_bitmap_bank(file, state.get(), [](void *context, int id, const piggy_bank_bitmap *record) {
			auto &destination = static_cast<State *>(context)->bitmaps;
			if (id != int(destination.size())) return 0;
			destination.push_back(*record);
			return 1;
		});
		if (count < 0 || count + 1 != int(state->bitmaps.size())) throw std::runtime_error("Invalid retained native bitmap bank");
	});
	for (std::size_t id = 1; id < state->bitmaps.size(); ++id) {
		const auto &record = state->bitmaps[id];
		if ((record.flags & BM_FLAG_RLE) &&
		    !bounded_rle_validate_bitmap(state->bitmap_bytes.data() + record.offset, record.size,
		                                 record.width, record.height, (record.flags & BM_FLAG_RLE_BIG) ? 2 : 1))
			throw std::runtime_error("Malformed retained bitmap payload: " + std::to_string(id));
	}
	xl_read_retained_resource(state->sound_bytes, [&](PHYSFS_file *file) {
		const int count = piggy_read_sound_bank(file, state.get(), [](void *context, int id, const piggy_bank_sound *record) {
			auto &destination = static_cast<State *>(context)->sounds;
			if (id != int(destination.size())) return 0;
			destination.push_back(*record);
			return 1;
		});
		if (count < 0 || count != int(state->sounds.size())) throw std::runtime_error("Invalid retained native sound bank");
	});
	state->bitmap_hash = picosha2::hash256_hex_string(state->bitmap_bytes);
	state->sound_hash = picosha2::hash256_hex_string(state->sound_bytes);
}

XlResourceBanks::~XlResourceBanks() = default;

void XlResourceBanks::bitmap(int id) const
{
	if (id <= 0 || id >= int(state->bitmaps.size())) throw std::runtime_error("object dependency references missing bitmap in target bank");
}

void XlResourceBanks::sound(int id) const
{
	if (id < 0 || id >= int(state->sounds.size()) || state->sounds[id].length <= 0)
		throw std::runtime_error("Dependency references missing target sound sample");
}

int XlResourceBanks::bitmap_count() const
{
	return int(state->bitmaps.size());
}
int XlResourceBanks::sound_count() const
{
	return int(state->sounds.size());
}
int XlResourceBanks::sound_length(int id) const
{
	sound(id);
	return state->sounds[id].length;
}
int XlResourceBanks::sample_rate() const
{
	return state->rate;
}
const std::string &XlResourceBanks::bitmap_hash() const
{
	return state->bitmap_hash;
}
const std::string &XlResourceBanks::sound_hash() const
{
	return state->sound_hash;
}
