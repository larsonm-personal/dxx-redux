#ifndef DXX_XL_DOOR_OBSTRUCTION_CONTROL_H
#define DXX_XL_DOOR_OBSTRUCTION_CONTROL_H
#include <array>
#include <cstring>
namespace xl_door_obstruction_control
{
template <class Check>
void run(Check check, bool ignore_marker = false)
{
	check(xl_level_extensions_active() && !Num_open_doors && Num_walls >= 2 &&
	          Walls[0].segnum == 24 && Walls[0].sidenum == 5 && Walls[1].segnum == 21 &&
	          Walls[0].state == WALL_DOOR_CLOSED && Walls[1].state == WALL_DOOR_CLOSED,
	      "Obstruction controls require the closed derived door pair");
	struct Guard {
		wall front = Walls[0], back = Walls[1];
		side front_side = Segments[24].sides[5], back_side = Segments[21].sides[4];
		fix frame = FrameTime, size = ConsoleObject->size;
		vms_vector position = ConsoleObject->pos;
		int segment = ConsoleObject->segnum, object = -1;
		std::array<active_door, MAX_DOORS> doors;
		Guard()
		{
			std::memcpy(doors.data(), ActiveDoors, sizeof(ActiveDoors));
		}
		~Guard()
		{
			if (object >= 0) obj_delete(object);
			Walls[0] = front;
			Walls[1] = back;
			Segments[24].sides[5] = front_side;
			Segments[21].sides[4] = back_side;
			std::memcpy(ActiveDoors, doors.data(), sizeof(ActiveDoors));
			Num_open_doors = 0;
			FrameTime = frame;
			obj_relink(ConsoleObject - Objects, segment);
			ConsoleObject->pos = position;
			ConsoleObject->size = size;
		}
	} guard;
	obj_relink(ConsoleObject - Objects, 0);
	compute_segment_center(&ConsoleObject->pos, &Segments[0]);
	ConsoleObject->size = F1_0 / 16;
	FrameTime = F1_0 / 64;
	for (auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_POWERUP, OBJ_MARKER, OBJ_WEAPON, OBJ_FIREBALL, object_type_t(19), object_type_t(20) })
		for (int from : { 24, 21 }) {
			vms_vector face;
			const int side_index = from == 24 ? 5 : 4;
			compute_center_point_on_side(&face, &Segments[from], side_index);
			vm_vec_scale_add2(&face, &Segments[from].sides[side_index].normals[0], F1_0 / 16);
			guard.object = obj_create(type, 0, from, &face, &vmd_identity_matrix, F1_0 / 8, type == OBJ_WEAPON ? CT_WEAPON : CT_NONE, MT_NONE, RT_NONE);
			check(guard.object >= 0, "Cannot create obstruction control actor");
			const bool ignored = from == 21 || type == OBJ_WEAPON || type == OBJ_FIREBALL || type == 19 || type == 20 || (ignore_marker && type == OBJ_MARKER);
			for (int phase = 0; phase < 3; ++phase) {
				Walls[0] = guard.front;
				Walls[1] = guard.back;
				Walls[0].flags |= WALL_DOOR_AUTO;
				Walls[1].flags |= WALL_DOOR_AUTO;
				Walls[0].state = Walls[1].state = phase == 2 ? WALL_DOOR_CLOSING : WALL_DOOR_WAITING;
				Num_open_doors = 1;
				ActiveDoors[0] = {};
				ActiveDoors[0].n_parts = 1;
				ActiveDoors[0].front_wallnum[0] = 0;
				ActiveDoors[0].back_wallnum[0] = 1;
				ActiveDoors[0].front_wallnum[1] = ActiveDoors[0].back_wallnum[1] = -1;
				ActiveDoors[0].time = phase == 2 ? 0 : DOOR_WAIT_TIME;
				if (!phase) check(xl_door_close(24, 5, 1) == 1, "XL obstruction selected stock close");
				else wall_frame_process();
				const int expected = ignored ? WALL_DOOR_CLOSING : phase == 2 ? WALL_DOOR_OPENING
				                                                              : WALL_DOOR_WAITING;
				check(Walls[0].state == expected && Walls[1].state == expected && Num_open_doors == 1,
				      "XL actor/effect obstruction differs across authored, waiting or closing phases");
			}
			obj_delete(guard.object);
			guard.object = -1;
		}
}
} // namespace xl_door_obstruction_control
#endif
