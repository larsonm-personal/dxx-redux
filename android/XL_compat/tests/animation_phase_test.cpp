#include "../../app/src/main/cpp/shared/xl_animation_phase.h"
#include "../scoped_texture.h"
#include <cstdio>
#include <cstdlib>
#include <climits>
static void require(bool value)
{
	if (!value) {
		std::fputs("Opaque animation contract failed\n", stderr);
		std::exit(1);
	}
}
int main()
{
	for (const int stock : { 1, 4, 8, 30 })
		for (const int source : { 1, 2, 6, 128 }) {
			int frame = 0, left = 12, seen = -1, transitions = 0;
			for (int tick = 0; tick <= stock * 12; ++tick) {
				const int selected = dxx_xl_animation_frame(stock, 12, frame, left, 0, source);
				require(selected >= 0 && selected < source);
				if (selected != seen) {
					++transitions;
					seen = selected;
				}
				require(selected == (tick ? tick - 1 : 0) * source / (stock * 12));
				// Existing effect loop: equality does not advance, strict negative does.
				if (--left < 0) {
					left += 12;
					frame = (frame + 1) % stock;
				}
			}
			require(seen == source - 1 || source > stock * 12);
			require(transitions > 0);
			require(dxx_xl_animation_frame(stock, 12, 0, 0, 1, source) == 0);
		}
	require(dxx_xl_animation_frame(8, INT_MAX, 7, 0, 0, 128) == 127);
	for (int invalid : { -1, 0, 129 }) require(dxx_xl_animation_frame(8, 12, 0, 12, 0, invalid) == -1);
	require(dxx_xl_animation_frame(31, 12, 0, 12, 0, 6) == -1);
	require(dxx_xl_animation_frame(8, 0, 0, 0, 0, 6) == -1);
	require(dxx_xl_animation_frame(8, 12, 8, 12, 0, 6) == -1);
	require(dxx_xl_animation_frame(8, 12, 0, -1, 0, 6) == -1);
	require(dxx_xl_animation_frame(8, 12, 0, 13, 0, 6) == -1);
	for (int stock = 1; stock <= 50; ++stock)
		for (int source = 1; source <= stock; ++source) {
			int previous = -1;
			for (int frame = 0; frame < stock; ++frame) {
				const int selected = dxx_xl_wclip_frame(stock, frame, source);
				require(selected >= 0 && selected < source && selected >= previous && selected <= previous + 1);
				previous = selected;
			}
			require(dxx_xl_wclip_frame(stock, 0, source) == 0 && previous == source - 1);
			require(dxx_xl_wclip_frame(stock, 0, stock + 1) == -1);
		}
	for (int invalid : { -1, 0, 51, INT_MAX }) require(dxx_xl_wclip_frame(invalid, 0, 1) == -1);
	require(dxx_xl_wclip_frame(5, -1, 4) == -1 && dxx_xl_wclip_frame(5, 5, 4) == -1);
	require(dxx_xl_wclip_frame(5, 0, 0) == -1);
	using xl_compat::animation_mission_texture_path;
	require(animation_mission_texture_path("missions/BOILPNT.HOG", "force01#127") == "mods/BOILPNT/textures/force01#127.tga");
	for (const char *name : { "force01", "force01#", "force01#128", "force01#-1", "force01#0/evil", "force01#0#1", "../force01#0" })
		require(animation_mission_texture_path("boilpnt.hog", name).empty());
	std::puts("Bounded animation names, authoritative strict-negative phase, stopped and overflow controls passed");
}
