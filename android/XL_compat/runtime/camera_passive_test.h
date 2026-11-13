// Real physics must retain the enabled source pairs even without a contact response
static void camera_passive_physics_controls(object &camera, bool stationary = false)
{
	using Point = xl_compat::FixedLightPoint;
	const auto point = [](const vms_vector &v) { return Point{ v.x, v.y, v.z }; };
	const auto saved_actor = *ConsoleObject, saved_camera = camera;
	const auto saved_frame = FrameTime;
	const int owner = int(ConsoleObject - Objects), saved_awareness = Num_awareness_events;
	unsigned cases = 0;
	for (int type : { OBJ_ROBOT, OBJ_FIREBALL })
		for (int behavior : stationary ? std::vector<int>{ 135 } : std::vector<int>{ 128, 129 })
			for (int axis = 0; axis < 3; ++axis)
				for (int direction : { -1, 1 })
					for (int duration : { F1_0 / 16, F1_0 / 8 }) {
						*ConsoleObject = saved_actor;
						camera = saved_camera;
						camera.ctype.ai_info.behavior = behavior;
						camera.flags = 0;
						auto &actor = *ConsoleObject;
						actor.type = type;
						actor.control_type = CT_NONE;
						actor.flags = 0;
						actor.size = F1_0;
						actor.movement_type = MT_PHYSICS;
						actor.mtype.phys_info = {};
						actor.mtype.phys_info.mass = F1_0;
						Point start = point(camera.pos), velocity{};
						start[axis] -= direction * F1_0;
						velocity[axis] = direction * 32 * F1_0;
						actor.pos = { start[0], start[1], start[2] };
						actor.last_pos = actor.pos;
						actor.mtype.phys_info.velocity = { velocity[0], velocity[1], velocity[2] };
						FrameTime = duration;
						xl_camera_hit_state_created(owner);
						timer_update();
						xl_camera_controller_begin_frame();
						vms_vector offset{}, end{}, hit{}, normal{};
						vm_vec_copy_scale(&offset, &actor.mtype.phys_info.velocity, xl_preview_motion_time(actor.segnum, FrameTime));
						vm_vec_add(&end, &actor.pos, &offset);
						require(xl_camera_query(&camera, &actor, &actor.pos, &end, actor.size, &hit, &normal) && point(hit) == start,
						        "Passive camera control requires an inside-start contact");
						xl_compat::CameraUnstickState expected{ start, point(end), point(offset) };
						xl_compat::CameraHitState expected_hits, actual_hits;
						auto position = point(hit);
						xl_compat::camera_sphere_unstick(expected, expected_hits, position, point(camera.pos), velocity,
						                                 actor.size, type, int32_t(xl_camera_controller_ticks()), 2000);
						// Inside-start contact leaves the whole frame for the unchanged-velocity retry
						for (unsigned i = 0; i < 3; ++i) position[i] += point(offset)[i];
						const vms_vector final_point{ position[0], position[1], position[2] };
						require(!get_seg_masks(&final_point, actor.segnum, actor.size, __FILE__, __LINE__).facemask,
						        "Passive camera retry control touches a room wall");
						const auto actor_before = actor, camera_before = camera;
						const auto calls = d_rand_get_call_count();
						const int count = num_objects;
						Num_awareness_events = 0;
						collide_two_objects(&actor, &camera, &hit);
						collide_two_objects(&camera, &actor, &hit);
						require(!std::memcmp(&actor, &actor_before, sizeof(actor)) && !std::memcmp(&camera, &camera_before, sizeof(camera)),
						        "Passive camera dispatcher changed an actor in either order");
						do_physics_sim(&actor);
						require(point(actor.pos) == position && point(actor.mtype.phys_info.velocity) == velocity &&
						            actor.shields == actor_before.shields && actor.flags == actor_before.flags,
						        "Passive camera physics changed unsticking, retry, health or velocity");
						require(num_objects == count && d_rand_get_call_count() == calls && !Num_awareness_events,
						        "Passive camera contact created an effect or consumed gameplay RNG");
						require(xl_camera_hit_state_get(owner, actual_hits) && actual_hits.repaired == expected_hits.repaired &&
						            actual_hits.points == expected_hits.points && actual_hits.times == expected_hits.times &&
						            actual_hits.shield_time == expected_hits.shield_time && actual_hits.next == expected_hits.next,
						        "Passive camera reverse query lost the actor-specific hit state");
						const auto snapshot = xl_camera_hit_snapshot();
						require(xl_camera_hit_restore(snapshot, true), "Passive camera hit state did not restore");
						if (type == OBJ_ROBOT) {
							require(xl_camera_hit_state_get(owner, actual_hits) && actual_hits.repaired == expected_hits.repaired && !actual_hits.next &&
							            actual_hits.points == decltype(actual_hits.points){},
							        "Robot camera restore lost repair time or retained transient points");
							auto invalid = snapshot;
							const auto row = std::find_if(invalid.begin(), invalid.end(), [&](const XlCameraHitSave &entry) { return entry.owner == owner; });
							require(row != invalid.end(), "Robot camera repair state was omitted from the save");
							row->type = OBJ_WEAPON;
							require(!xl_camera_hit_restore(invalid, true), "Robot camera restore accepted a changed actor type");
						}
						++cases;
					}
	*ConsoleObject = saved_actor;
	camera = saved_camera;
	FrameTime = saved_frame;
	Num_awareness_events = saved_awareness;
	std::cout << "Native passive camera physics: " << cases << " robot/fireball frames, both dispatcher orders and robot restore passed\n";
}
