#include "camera_glass.h"
#include "geometry.h"
static void camera_glass_native_controls(object &camera)
{
	require(Num_walls > 0 && Num_triggers > 0 && NumTextures > 2 && Num_effects > 0, "Glass controls need native assets and trigger storage");
	const std::vector<segment> original_segments(Segments, Segments + Highest_segment_index + 1);
	const std::vector<wall> original_walls(Walls, Walls + Num_walls);
	const std::vector<ubyte> original_lights(Light_subtracted, Light_subtracted + Highest_segment_index + 1);
	const auto original_trigger = Triggers[0];
	std::vector<xl_trigger_state> original_states(Num_triggers);
	for (int i = 0; i < Num_triggers; ++i) require(xl_wall_trigger_state(i, &original_states[i]), "Cannot capture glass trigger state");
	const auto original_texture = TmapInfo[1];
	const auto original_effect = Effects[0];
	const auto original_next = Effects[1];
	const auto original_pilot = Players[Player_num];
	const auto original_player = *ConsoleObject;
	int room = -1, face = -1;
	for (int i = 0; i <= Highest_segment_index && room < 0; ++i) {
		bool child = false;
		int solid = -1;
		for (int side = 0; side < 6; ++side) {
			child |= Segments[i].children[side] >= 0;
			if (Segments[i].children[side] < 0) solid = side;
		}
		if (child && solid >= 0) {
			room = i;
			face = solid;
		}
	}
	require(room >= 0, "Glass fixture needs a room with a portal and solid face");
	int opaque = -1;
	for (int texture = 0; texture < NumTextures; ++texture)
		if (!(piggy_bitmap_get_flags(&GameBitmaps[Textures[texture].index]) & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT))) {
			opaque = texture;
			break;
		}
	require(opaque >= 0, "Glass fixture needs an opaque loaded texture");
	const auto center = xl_compat::compile_classic_lighting_side(room, 0).segment_center;
	ConsoleObject->ctype.laser_info.parent_num = -1;
	object weapon{};
	weapon.type = OBJ_WEAPON;
	weapon.id = EARTHSHAKER_ID;
	weapon.segnum = room;
	weapon.pos = { center[0], center[1], center[2] };
	weapon.ctype.laser_info.parent_num = int(ConsoleObject - Objects);
	unsigned cases = 0, broken = 0, operated = 0;
	for (int mode = 0; mode < 9; ++mode) {
		std::copy(original_segments.begin(), original_segments.end(), Segments);
		std::copy(original_lights.begin(), original_lights.end(), Light_subtracted);
		for (int i = 0; i <= Highest_segment_index; ++i)
			for (auto &side : Segments[i].sides) side.tmap_num2 = 0;
		for (int i = 0; i < Num_walls; ++i) {
			Walls[i].type = mode == 7 ? WALL_CLOSED : WALL_OPEN;
			Walls[i].trigger = -1;
			Walls[i].flags = 0;
			Walls[i].state = WALL_DOOR_CLOSED;
		}
		auto &surface = Segments[room].sides[face];
		surface.tmap_num2 = short(1 | 0x4000);
		surface.wall_num = 0;
		Walls[0].segnum = room;
		Walls[0].sidenum = face;
		Walls[0].trigger = 0;
		if (mode == 7) {
			Walls[0].type = WALL_NORMAL;
			Walls[0].cloak_value = 31;
			for (int i = 0; i <= Highest_segment_index; ++i)
				for (int side = 0; side < 6; ++side)
					if (Segments[i].children[side] >= 0) {
						Segments[i].sides[side].wall_num = 0;
						Segments[i].sides[side].tmap_num = opaque;
					}
		}
		Triggers[0] = {};
		Triggers[0].type = 17;
		Triggers[0].time = -1;
		Triggers[0].value = 10 * F1_0;
		Triggers[0].flags = mode == 1 ? 8 : 0;
		xl_wall_trigger_reset();
		TmapInfo[1].eclip_num = (mode == 2 || mode == 3 || mode == 8) ? 0 : -1;
		TmapInfo[1].destroyed = mode == 4 ? -1 : 2;
		Effects[0].flags = mode == 3 ? EF_ONE_SHOT : 0;
		Effects[0].dest_bm_num = 2;
		Effects[0].dest_eclip = -1;
		Effects[0].dest_size = F1_0;
		Effects[0].dest_vclip = 3;
		if (mode == 8) {
			Effects[0].dest_eclip = 1;
			Effects[0].flags = EF_CRITICAL;
			Effects[1].segnum = -1;
			Effects[1].flags = 64 | EF_STOPPED;
			Effects[1].changing_wall_texture = 3;
			Effects[1].vc.frame_time = 12345;
		}
		ConsoleObject->ctype.laser_info.parent_num = mode == 6 ? int(&camera - Objects) : -1;
		ConsoleObject->shields = Players[Player_num].shields = 100 * F1_0;
		Light_subtracted[room] &= ~(1 << face);
		if (mode == 7)
			for (int side = 0; side < 6; ++side)
				if (Segments[room].children[side] >= 0)
					require(!(WALL_IS_DOORWAY(&Segments[room], side) & WID_RENDPAST_FLAG), "Glass blocked-route fixture retained a transparent portal");
		const int highest = Highest_object_index;
		std::array<int, MAX_OBJECTS> signatures{};
		for (int i = 0; i <= highest; ++i) signatures[i] = Objects[i].signature;
		xl_camera_blast_glass(weapon, mode == 5 ? 1 : 1000 * F1_0);
		const bool expected = mode < 3 || mode == 8;
		xl_trigger_state state{};
		require(xl_wall_trigger_state(0, &state), "Missing glass trigger state");
		if (bool(state.operated) != expected) std::cerr << "Glass mode=" << mode << " operated=" << state.operated << " room=" << room << " side=" << face << '\n';
		require(bool(state.operated) == expected && Players[Player_num].shields == (expected ? 90 : 100) * F1_0, "Glass blast lost linked shield trigger or fired through a gate");
		require(surface.tmap_num2 == short((mode == 8 ? 3 : (expected && mode != 1) ? 2
		                                                                            : 1) |
		                                   0x4000),
		        "Glass blast lost permanent texture or overlay rotation");
		if (mode == 8) require(Effects[1].time_left == 12345 && Effects[1].frame_count == 0 && Effects[1].segnum == room && Effects[1].sidenum == face &&
			                       Effects[1].flags == (EF_STOPPED | EF_ONE_SHOT | EF_CRITICAL) && Effects[1].dest_bm_num == 2,
			                   "Glass blast lost source eclip transition flags or timing");
		require(bool(Light_subtracted[room] & (1 << face)) == (expected && mode != 1), "Glass blast changed permanent light or missed destruction light");
		unsigned effects = 0;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE && (i > highest || Objects[i].signature != signatures[i])) {
				require(Objects[i].type == OBJ_FIREBALL, "Glass blast created unexpected object");
				++effects;
				obj_delete(i);
			}
		require(effects == unsigned(expected), "Glass blast omitted or duplicated its explosion");
		++cases;
		if (expected) {
			++broken;
			++operated;
		}
	}
	std::copy(original_segments.begin(), original_segments.end(), Segments);
	std::copy(original_walls.begin(), original_walls.end(), Walls);
	std::copy(original_lights.begin(), original_lights.end(), Light_subtracted);
	Triggers[0] = original_trigger;
	TmapInfo[1] = original_texture;
	Effects[0] = original_effect;
	Effects[1] = original_next;
	*ConsoleObject = original_player;
	Players[Player_num] = original_pilot;
	require(xl_wall_trigger_restore(original_states.data(), int(original_states.size())), "Cannot restore glass trigger controls");
	std::cout << "Native earthquake glass: " << cases << " gate/permanent/overlay cases, " << broken << " explosions, " << operated << " shield triggers passed\n";
}
