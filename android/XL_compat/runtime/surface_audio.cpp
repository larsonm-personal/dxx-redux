#include "gameplay_audio.h"
#include "linked_audio.h"
#include "ambient_objects.h"
#include "preview.h"
#include <array>
extern "C" {
#include "inferno.h"
#include "segment.h"
#include "gameseg.h"
#include "wall.h"
#include "bm.h"
#include "textures.h"
#include "effects.h"
#include "digi.h"
}
namespace
{
int surface_sound(int segment, int side)
{
	auto &room = Segments[segment];
	const int passable = WALL_IS_DOORWAY(&room, side);
	if (!(passable & WID_RENDER_FLAG)) return -1;
	const auto &face = room.sides[side];
	const int overlay = face.tmap_num2 & 0x3fff;
	const int base = face.tmap_num;
	if (overlay >= NumTextures || base < 0 || base >= NumTextures) return -1;
	int effect = overlay ? TmapInfo[overlay].eclip_num : -1;
	if (effect < 0) effect = TmapInfo[base].eclip_num;
	if (effect < 0 || effect >= Num_effects) return -1;
	const int sound = Effects[effect].sound_num;
	if (sound < 0) return -1;
	const int child = room.children[side];
	if (child >= 0 && child < segment && (passable & (WID_FLY_FLAG | WID_RENDPAST_FLAG))) {
		const int opposite = find_connect_side(&room, &Segments[child]);
		// XL stores overlay orientation separately from its texture identity
		if (opposite >= 0 && (Segments[child].sides[opposite].tmap_num2 & 0x3fff) == overlay) return -1;
	}
	return sound;
}
} // namespace
extern "C" int xl_gameplay_surface_audio(void)
{
	if (!xl_level_extensions_active()) return 0;
	digi_init_sounds();
	xl_audio_defer_objects(1);
	for (int segment = 0; segment <= Highest_segment_index; ++segment) {
		std::array<int, 6> sounds;
		int count = 0;
		for (int side = 0; side < 6; ++side) {
			sounds[unsigned(side)] = surface_sound(segment, side);
			count += sounds[unsigned(side)] >= 0;
		}
		for (int side = 0; side < 6; ++side) {
			if (sounds[unsigned(side)] < 0) continue;
			vms_vector center;
			compute_center_point_on_side(&center, &Segments[segment], side);
			xl_gameplay_sound_position(sounds[unsigned(side)], segment, side, &center, 1, F1_0 / (2 * count), 256 * F1_0);
		}
	}
	xl_ambient_objects_start();
	xl_audio_defer_objects(0);
	return 1;
}
