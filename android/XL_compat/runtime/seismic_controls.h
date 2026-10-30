#ifndef DXX_XL_SEISMIC_CONTROLS_H
#define DXX_XL_SEISMIC_CONTROLS_H
#include "seismic_audio_test.h"
#include "preview.h"
#include "object_trigger.h"
#include "trigger_state.h"
extern "C" {
#include "player.h"
#include "wall.h"
#include "switch.h"
void obj_delete_all_that_should_be_dead(void);
}
namespace xl_seismic_controls
{
using xl_compat::seismic_gameplay_snapshot;
using xl_seismic_audio_control::check;
inline void fixture()
{
	check(xl_level_extensions_active() && Num_triggers == 8 && xl_object_trigger_count() == 2 && Triggers[0].type == 36 && Triggers[0].time == 99 && Triggers[7].time == 3 && Triggers[7].value == 3 * F1_0, "Seismic controls require the labeled authored fixture");
}
inline int fire(int index)
{
	xl_wall_trigger_reset();
	return check_trigger_sub(index, Player_num, 1);
}
inline void events()
{
	fixture();
	xl_wall_trigger_frame();
	xl_object_trigger_state autoplay{};
	check((Triggers[5].flags & TF_DISABLED) && xl_object_trigger_state_get(1, &autoplay) && autoplay.operated && (autoplay.flags & TF_DISABLED), "Seismic wall/object autoplay did not operate");
	const auto once = seismic_gameplay_snapshot();
	xl_wall_trigger_frame();
	check(seismic_gameplay_snapshot() == once, "Seismic autoplay repeated");
	xl_seismic_clear();
	const auto &front = Walls[Num_walls - 2];
	xl_wall_trigger_reset();
	check_trigger(&Segments[front.segnum], short(front.sidenum), short(Players[Player_num].objnum), 0);
	check(seismic_gameplay_snapshot()[0] == 3 * F1_0 && seismic_gameplay_snapshot()[1] == 99 * F1_0, "Mine-shake wall lost strength or seconds");
	check(fire(1) == 0 && seismic_gameplay_snapshot()[1] == 1000 * F1_0 && fire(1) == 1, "Long seismic action delayed or one-shot repeated");
	xl_trigger_state schedule{};
	check(xl_wall_trigger_state(1, &schedule) && schedule.delay_ms == -1, "Mine-shake seconds entered the millisecond scheduler");
	check(fire(2) == 0 && seismic_gameplay_snapshot()[0] == 0 && seismic_gameplay_snapshot()[1] == 0, "Negative seismic strength or zero duration differs");
	check(fire(3) == 0 && seismic_gameplay_snapshot()[1] == 10 * F1_0, "Negative seismic time lost its ten-second default");
	check(fire(4) == 0 && seismic_gameplay_snapshot()[1] == 99 * F1_0, "Master did not dispatch seismic child");
	const auto disabled = seismic_gameplay_snapshot();
	check(fire(6) == 1 && seismic_gameplay_snapshot() == disabled, "Disabled seismic action changed live state");
	check(fire(7) == 0 && seismic_gameplay_snapshot()[0] == 196608 && seismic_gameplay_snapshot()[1] == 3 * F1_0 && fire(7) == 1, "Original Pumo action parameters failed source one-shot behavior");
	int owner = -1;
	for (int i = 0; i < xl_object_trigger_count(); ++i) {
		xl_object_trigger_state state{};
		check(xl_object_trigger_state_get(i, &state), "Missing seismic object owner");
		if (!(state.flags & 128)) owner = state.owner;
	}
	check(owner >= 0 && Objects[owner].type == OBJ_POWERUP, "Seismic removal owner missing");
	Objects[owner].flags |= OF_SHOULD_BE_DEAD;
	obj_delete_all_that_should_be_dead();
	check(Objects[owner].type == OBJ_NONE && seismic_gameplay_snapshot()[0] == 5 * F1_0 && seismic_gameplay_snapshot()[1] == 120 * F1_0, "Attached mine-shake lost authored time during removal");
}
inline nlohmann::json prepare()
{
	check(!game_is_time_paused(), "Seismic continuation must earn state in running simulation");
	xl_seismic_clear();
	trigger action{};
	action.type = 36;
	action.value = INT32_MAX;
	action.time = 10;
	check(xl_seismic_action(&action), "Cannot start high-strength seismic save control");
	auto position = ConsoleObject->pos;
	position.x += 400 * F1_0;
	smega_rock_stuff_at(&position);
	const auto frame = FrameTime;
	FrameTime = F1_0 / 32;
	GameTime64 += FrameTime;
	xl_seismic_audio_control::checked_frame();
	action.value = 3 * F1_0;
	action.time = 3;
	check(xl_seismic_action(&action), "Cannot queue second mine disturbance");
	FrameTime = F1_0 / 100;
	GameTime64 += FrameTime;
	xl_seismic_audio_control::checked_frame();
	FrameTime = frame;
	const auto earned = seismic_gameplay_snapshot();
	check(earned[1] == 3 * F1_0 && !earned[2].is_null() && earned[3][0][1] == 0.5f && earned[5].get<int>() > 0 && std::abs(earned[4].get<std::int64_t>()) > F1_0, "Seismic save lacks active/pending disturbance, scaled detonation, wide magnitude or partial tick");
	return earned;
}
inline void paused()
{
	check(game_is_time_paused(), "Seismic pause control requires paused simulation");
	const auto state = seismic_gameplay_snapshot(), audio = xl_compat::named_audio_snapshot();
	const auto rotation = ConsoleObject->mtype.phys_info.rotvel;
	const unsigned sim = d_rand_get_stream_call_count(D_RNG_SIM), fx = d_rand_get_stream_call_count(D_RNG_FX);
	const auto frame = FrameTime;
	FrameTime = F1_0;
	do_seismic_stuff();
	FrameTime = frame;
	check(seismic_gameplay_snapshot() == state && xl_compat::named_audio_snapshot() == audio && !std::memcmp(&rotation, &ConsoleObject->mtype.phys_info.rotvel, sizeof(rotation)) && d_rand_get_stream_call_count(D_RNG_SIM) == sim && d_rand_get_stream_call_count(D_RNG_FX) == fx, "Paused seismic frame changed physics, cadence, RNG or audio");
}
inline void continuation()
{
	check(!game_is_time_paused(), "Seismic continuation requires running simulation");
	const auto frame = FrameTime;
	for (int i = 0; i < 8; ++i) {
		FrameTime = i == 0 ? F1_0 / 50 : 2 * F1_0;
		GameTime64 += FrameTime;
		xl_seismic_audio_control::checked_frame();
	}
	FrameTime = frame;
	check(seismic_gameplay_snapshot()[1] == 0 && Seismic_tremor_volume == 0 && Seismic_tremor_magnitude == 0, "Restored seismic queue did not complete and expire");
}
} // namespace xl_seismic_controls
#endif
