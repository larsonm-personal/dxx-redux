#ifndef INPUT_DEMO_HOOKS_SHARED_H
#define INPUT_DEMO_HOOKS_SHARED_H

/*
 * Shared helper bodies for d1/main/input_demo_hooks.c and d2/main/input_demo_hooks.c.
 * Including files provide the game headers that define the referenced globals and types.
 */

static unsigned int input_demo_state_trace_hash_update(unsigned int hash,
	unsigned int value)
{
	hash ^= value + 0x9e3779b9u + (hash << 6) + (hash >> 2);
	return hash;
}

static unsigned int input_demo_state_trace_hash_i64(unsigned int hash,
	int64_t value)
{
	uint64_t bits = (uint64_t)value;

	hash = input_demo_state_trace_hash_update(hash, (unsigned int)bits);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)(bits >> 32));
	return hash;
}

static void input_demo_capture_runtime_state_diag(input_demo_state_trace_diag *diag)
{
	object_runtime_state object_state;
	laser_runtime_state laser_state;
	unsigned int runtime_hash = 0;
	int free_start;

	if (!diag)
		return;

	object_get_runtime_state(&object_state);
	laser_get_runtime_state(&laser_state);
	free_start = object_state.num_objects;
	diag->object_allocator_num_objects = object_state.num_objects;
	diag->object_signature_seed = object_state.signature_seed;
	diag->object_free_head0 = -1;
	diag->object_free_head1 = -1;
	diag->object_free_head2 = -1;
	diag->object_free_head3 = -1;
	diag->object_homer_frame_count = object_state.homer_frame_count;
	diag->object_current_homer_frame_time = object_state.current_homer_frame_time;
	diag->object_do_homer_frame = object_state.do_homer_frame;

	if (free_start >= 0 && free_start <= MAX_OBJECTS) {
		int free_index;

		diag->object_free_list_count = MAX_OBJECTS - free_start;
		for (free_index = free_start; free_index < MAX_OBJECTS; ++free_index) {
			const int head_index = free_index - free_start;
			const short free_slot = object_state.free_obj_list[free_index];

			diag->object_free_list_hash = input_demo_state_trace_hash_update(
				diag->object_free_list_hash, (unsigned int)(unsigned short)free_slot);
			if (head_index == 0)
				diag->object_free_head0 = free_slot;
			else if (head_index == 1)
				diag->object_free_head1 = free_slot;
			else if (head_index == 2)
				diag->object_free_head2 = free_slot;
			else if (head_index == 3)
				diag->object_free_head3 = free_slot;
		}
	} else
		diag->object_free_list_count = -1;

	diag->weapon_next_laser_delta = (int64_t)(Next_laser_fire_time - GameTime64);
	diag->weapon_next_missile_delta = (int64_t)(Next_missile_fire_time - GameTime64);
	diag->weapon_last_laser_delta = (int64_t)(Last_laser_fired_time - GameTime64);
	diag->weapon_next_flare_delta = (int64_t)(Next_flare_fire_time - GameTime64);
	diag->weapon_auto_fusion_delta = (int64_t)(Auto_fire_fusion_cannon_time - GameTime64);
	diag->weapon_last_omega_delta = (int64_t)laser_state.last_omega_fire_time - GameTime64;
	diag->weapon_global_laser_firing_count = Global_laser_firing_count;
	diag->weapon_global_missile_firing_count = Global_missile_firing_count;
	diag->weapon_fusion_charge = laser_state.fusion_charge;
	diag->weapon_spreadfire_toggle = laser_state.spreadfire_toggle;
	diag->weapon_missile_gun = laser_state.missile_gun;
	diag->weapon_proximity_dropped = laser_state.proximity_dropped;
	diag->weapon_helix_orientation = laser_state.helix_orientation;
	diag->weapon_smartmines_dropped = laser_state.smartmines_dropped;

	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->object_allocator_num_objects);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)object_state.highest_object_index);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->object_signature_seed);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->object_free_list_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		diag->object_free_list_hash);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		diag->object_homer_frame_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->object_current_homer_frame_time);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->object_do_homer_frame);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
		diag->weapon_next_laser_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
		diag->weapon_next_missile_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
		diag->weapon_last_laser_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
		diag->weapon_next_flare_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
		diag->weapon_auto_fusion_delta);
	runtime_hash = input_demo_state_trace_hash_i64(runtime_hash,
		diag->weapon_last_omega_delta);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->weapon_global_laser_firing_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->weapon_global_missile_firing_count);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->weapon_fusion_charge);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->weapon_spreadfire_toggle);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->weapon_missile_gun);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->weapon_proximity_dropped);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->weapon_helix_orientation);
	runtime_hash = input_demo_state_trace_hash_update(runtime_hash,
		(unsigned int)diag->weapon_smartmines_dropped);
	diag->runtime_state_hash = runtime_hash;
}

static void input_demo_set_player_weapon_diag_slot(input_demo_state_trace_diag *diag,
	int slot, int objnum, const object *obj)
{
	if (!diag || !obj)
		return;

	switch (slot) {
		case 0:
			diag->player_weapon_obj0 = objnum;
			diag->player_weapon_sig0 = obj->signature;
			diag->player_weapon_id0 = obj->id;
			break;
		case 1:
			diag->player_weapon_obj1 = objnum;
			diag->player_weapon_sig1 = obj->signature;
			diag->player_weapon_id1 = obj->id;
			break;
		case 2:
			diag->player_weapon_obj2 = objnum;
			diag->player_weapon_sig2 = obj->signature;
			diag->player_weapon_id2 = obj->id;
			break;
		case 3:
			diag->player_weapon_obj3 = objnum;
			diag->player_weapon_sig3 = obj->signature;
			diag->player_weapon_id3 = obj->id;
			break;
	}
}

static void input_demo_capture_player_weapon_diag(input_demo_state_trace_diag *diag)
{
	int i;
	int player_objnum;

	if (!diag)
		return;

	diag->player_weapon_obj0 = -1;
	diag->player_weapon_sig0 = -1;
	diag->player_weapon_id0 = -1;
	diag->player_weapon_obj1 = -1;
	diag->player_weapon_sig1 = -1;
	diag->player_weapon_id1 = -1;
	diag->player_weapon_obj2 = -1;
	diag->player_weapon_sig2 = -1;
	diag->player_weapon_id2 = -1;
	diag->player_weapon_obj3 = -1;
	diag->player_weapon_sig3 = -1;
	diag->player_weapon_id3 = -1;
	player_objnum = Players[Player_num].objnum;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];

		if (obj->type != OBJ_WEAPON)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;
		if (obj->ctype.laser_info.parent_type != OBJ_PLAYER)
			continue;
		if (obj->ctype.laser_info.parent_num != player_objnum)
			continue;

		diag->player_weapon_hash = input_demo_state_trace_hash_update(
			diag->player_weapon_hash, (unsigned int)i);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
			diag->player_weapon_hash, (unsigned int)obj->signature);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
			diag->player_weapon_hash, (unsigned int)obj->id);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
			diag->player_weapon_hash, (unsigned int)obj->segnum);
		diag->player_weapon_hash = input_demo_state_trace_hash_update(
			diag->player_weapon_hash,
			(unsigned int)obj->ctype.laser_info.parent_signature);
		if (diag->player_weapon_count < 4)
			input_demo_set_player_weapon_diag_slot(diag,
				diag->player_weapon_count, i, obj);
		diag->player_weapon_count++;
	}
}

static unsigned int input_demo_state_trace_hash_object(unsigned int hash,
	const object *obj)
{
	if (!obj)
		return hash;

	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->signature);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->id);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->segnum);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->control_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->movement_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->render_type);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->flags);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->size);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->shields);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->lifeleft);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->attached_obj);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->pos.x);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->pos.y);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->pos.z);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->last_pos.x);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->last_pos.y);
	hash = input_demo_state_trace_hash_update(hash, (unsigned int)obj->last_pos.z);

	if (obj->movement_type == MT_PHYSICS) {
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.velocity.x);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.velocity.y);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.velocity.z);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.rotvel.x);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.rotvel.y);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.rotvel.z);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.mass);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.drag);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.brakes);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.turnroll);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->mtype.phys_info.flags);
	}

	if (obj->type == OBJ_WEAPON) {
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->ctype.laser_info.parent_type);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->ctype.laser_info.parent_num);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->ctype.laser_info.parent_signature);
	}

	if (obj->render_type == RT_POLYOBJ) {
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->rtype.pobj_info.model_num);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->rtype.pobj_info.subobj_flags);
		hash = input_demo_state_trace_hash_update(hash,
			(unsigned int)obj->rtype.pobj_info.tmap_override);
	}

	return hash;
}

static void input_demo_capture_object_state_diag(input_demo_state_trace_diag *diag)
{
	int i;
	int segnum;

	if (!diag)
		return;
	diag->highest_object_index = Highest_object_index;
	diag->object_slot_bucket_size = INPUT_DEMO_OBJECT_SLOT_BUCKET_SIZE;

	for (i = 0; i <= Highest_object_index; ++i) {
		object *obj = &Objects[i];
		int bucket;

		if (obj->type == OBJ_NONE)
			continue;
		if (obj->flags & OF_SHOULD_BE_DEAD)
			continue;

		bucket = i >> INPUT_DEMO_OBJECT_SLOT_BUCKET_BITS;
		if (bucket >= 0 && bucket < INPUT_DEMO_OBJECT_SLOT_BUCKET_COUNT) {
			diag->object_slot_counts[bucket]++;
			diag->object_slot_hashes[bucket] = input_demo_state_trace_hash_update(
				diag->object_slot_hashes[bucket], (unsigned int)i);
			diag->object_slot_hashes[bucket] = input_demo_state_trace_hash_object(
				diag->object_slot_hashes[bucket], obj);
		}

		diag->live_object_count++;
		diag->live_object_hash = input_demo_state_trace_hash_object(
			diag->live_object_hash, obj);

		switch (obj->type) {
			case OBJ_ROBOT:
				diag->robot_object_count++;
				diag->robot_state_hash = input_demo_state_trace_hash_object(
					diag->robot_state_hash, obj);
				if (INPUT_DEMO_ROBOT_IS_CAMERA_AWAKE(i, obj))
					diag->camera_awake_robots++;
				if (obj->ctype.ai_info.danger_laser_num != -1)
					diag->danger_laser_robots++;
				break;
			case OBJ_WEAPON:
				diag->weapon_object_count++;
				diag->weapon_state_hash = input_demo_state_trace_hash_object(
					diag->weapon_state_hash, obj);
				break;
			case OBJ_FIREBALL:
				diag->fireball_object_count++;
				diag->fireball_state_hash = input_demo_state_trace_hash_object(
					diag->fireball_state_hash, obj);
				break;
			case OBJ_DEBRIS:
				diag->debris_object_count++;
				diag->debris_state_hash = input_demo_state_trace_hash_object(
					diag->debris_state_hash, obj);
				break;
		}
	}

	if (Highest_segment_index < 0)
		return;
	diag->segment_object_list_hash = input_demo_state_trace_hash_update(
		diag->segment_object_list_hash, (unsigned int)Highest_segment_index);
	for (segnum = 0; segnum <= Highest_segment_index; ++segnum) {
		int objnum = Segments[segnum].objects;
		int guard = 0;
		int segment_count = 0;
		unsigned int segment_hash = 0;

		while (objnum != -1) {
			object *obj;

			if (objnum < 0 || objnum > Highest_object_index) {
				diag->segment_object_link_error_count++;
				break;
			}
			if (++guard > MAX_OBJECTS) {
				diag->segment_object_link_error_count++;
				break;
			}
			obj = &Objects[objnum];
			if (obj->type == OBJ_NONE || obj->segnum != segnum)
				diag->segment_object_link_error_count++;
			segment_count++;
			segment_hash = input_demo_state_trace_hash_update(
				segment_hash, (unsigned int)objnum);
			segment_hash = input_demo_state_trace_hash_update(
				segment_hash, (unsigned int)obj->signature);
			segment_hash = input_demo_state_trace_hash_update(
				segment_hash, (unsigned int)obj->type);
			segment_hash = input_demo_state_trace_hash_update(
				segment_hash, (unsigned int)obj->id);
			segment_hash = input_demo_state_trace_hash_update(
				segment_hash, (unsigned int)obj->flags);
			segment_hash = input_demo_state_trace_hash_update(
				segment_hash, (unsigned int)obj->prev);
			segment_hash = input_demo_state_trace_hash_update(
				segment_hash, (unsigned int)obj->next);
			objnum = obj->next;
		}
		if (!segment_count)
			continue;
		diag->segment_object_list_count += segment_count;
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
			diag->segment_object_list_hash, (unsigned int)segnum);
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
			diag->segment_object_list_hash, (unsigned int)segment_count);
		diag->segment_object_list_hash = input_demo_state_trace_hash_update(
			diag->segment_object_list_hash, segment_hash);
	}
}

#endif