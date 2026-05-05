/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Code for controlling player movement
 *
 */


#include <stdio.h>
#include <stdlib.h>

#include "pstypes.h"
#include "key.h"
#include "joy.h"
#include "timer.h"
#include "dxxerror.h"
#include "inferno.h"
#include "game.h"
#include "object.h"
#include "player.h"
#include "controls.h"
#include "render.h"
#include "args.h"
#include "palette.h"
#include "mouse.h"
#include "kconfig.h"
#include "laser.h"
#include "newdemo.h"
#include "input_demo_energy_trace.h"
#include "input_demo_debug_logging.h"
#include "input_demo_replay.h"
#include "input_demo_recorder.h"
#ifdef NETWORK
#include "multi.h"
#endif
#include "vclip.h"
#include "fireball.h"

//look at keyboard, mouse, joystick, CyberMan, whatever, and set 
//physics vars rotvel, velocity

#define AFTERBURNER_USE_SECS	3				//use up in 3 seconds
#define DROP_DELTA_TIME			(f1_0/15)	//drop 3 per second

extern int Drop_afterburner_blob_flag;		//ugly hack

extern fix	Seismic_tremor_magnitude;

static int input_demo_trace_player_control_active(void)
{
	return input_demo_debug_activity_probe_active();
}

static unsigned int input_demo_trace_player_control_frame_index(void)
{
	return input_demo_debug_frame_index();
}

static const char *input_demo_trace_player_control_mode_name(void)
{
	return input_demo_debug_activity_mode_name();
}

static int input_demo_player_weapon_threat_now(object *obj)
{
	int i;

	if (!input_demo_trace_player_control_active() ||
		!ConsoleObject ||
		obj != ConsoleObject ||
		obj->type != OBJ_PLAYER ||
		obj->id != Player_num)
		return 0;

	for (i = 0; i <= Highest_object_index; i++) {
		object *weapon = &Objects[i];

		if (weapon->type != OBJ_WEAPON)
			continue;
		if (weapon->flags & (OF_SHOULD_BE_DEAD | OF_HARMLESS))
			continue;
		if (weapon->ctype.laser_info.parent_type != OBJ_ROBOT)
			continue;
		if (weapon->segnum != obj->segnum)
			continue;
		return 1;
	}

	return 0;
}

static int input_demo_player_control_probe_active(object *obj)
{
	static unsigned int last_frame = (unsigned int)-1;
	static int frames_remaining = 0;
	const unsigned int frame = input_demo_trace_player_control_frame_index();
	const int threat_now = input_demo_player_weapon_threat_now(obj);

	if (frame != last_frame) {
		if (frames_remaining > 0)
			frames_remaining--;
		last_frame = frame;
	}
	if (threat_now)
		frames_remaining = 2;

	return threat_now || frames_remaining > 0;
}

static unsigned int input_demo_player_control_state_key(object *obj, fix resolved_forward_thrust_time)
{
	unsigned int key = (unsigned int)(obj->segnum & 0xffff);

	key = key * 131u + (unsigned int)resolved_forward_thrust_time;
	key = key * 131u + (unsigned int)Controls.pitch_time;
	key = key * 131u + (unsigned int)Controls.heading_time;
	key = key * 131u + (unsigned int)Controls.bank_time;
	key = key * 131u + (unsigned int)Controls.forward_thrust_time;
	key = key * 131u + (unsigned int)Controls.sideways_thrust_time;
	key = key * 131u + (unsigned int)Controls.vertical_thrust_time;
	key = key * 131u + (unsigned int)obj->mtype.phys_info.thrust.x;
	key = key * 131u + (unsigned int)obj->mtype.phys_info.thrust.y;
	key = key * 131u + (unsigned int)obj->mtype.phys_info.thrust.z;
	key = key * 131u + (unsigned int)obj->mtype.phys_info.rotthrust.x;
	key = key * 131u + (unsigned int)obj->mtype.phys_info.rotthrust.y;
	key = key * 131u + (unsigned int)obj->mtype.phys_info.rotthrust.z;
	key = key * 131u + (unsigned int)obj->mtype.phys_info.flags;
	key = key * 131u + (unsigned int)Players[Player_num].flags;
	key = key * 131u + (unsigned int)Controls.afterburner_state;
	key = key * 131u + (unsigned int)Players[Player_num].afterburner_charge;

	return key;
}

static int input_demo_player_control_probe_should_log(unsigned int state_key)
{
	static unsigned int last_frame = (unsigned int)-1;
	static unsigned int last_state_key = 0;
	const unsigned int frame = input_demo_trace_player_control_frame_index();

	if (last_frame != (unsigned int)-1 && frame == last_frame + 1 && last_state_key == state_key)
		return 0;

	last_frame = frame;
	last_state_key = state_key;
	return 1;
}

static void input_demo_log_player_control_probe(object *obj,
	const vms_vector *pre_scale_thrust,
	const vms_vector *pre_scale_rotthrust,
	fix resolved_forward_thrust_time)
{
	const unsigned int state_key = input_demo_player_control_state_key(obj, resolved_forward_thrust_time);

	if (!input_demo_player_control_probe_active(obj))
		return;
	if (!input_demo_player_control_probe_should_log(state_key))
		return;

	con_printf(CON_NORMAL,
		"Input demo player control probe: mode=%s frame=%u gt=%lld step=read_flying_controls seg=%d resolved_forward=%d controls=(%d,%d,%d,%d,%d,%d) pre_thrust=(%d,%d,%d) thrust=(%d,%d,%d) pre_rot=(%d,%d,%d) rot=(%d,%d,%d) vel=(%d,%d,%d) orient_f=(%d,%d,%d) orient_r=(%d,%d,%d) orient_u=(%d,%d,%d) phys_flags=0x%x player_flags=0x%x ab=(%d,%d)\n",
		input_demo_trace_player_control_mode_name(),
		input_demo_trace_player_control_frame_index(),
		(long long)GameTime64,
		obj->segnum,
		resolved_forward_thrust_time,
		Controls.pitch_time,
		Controls.heading_time,
		Controls.bank_time,
		Controls.forward_thrust_time,
		Controls.sideways_thrust_time,
		Controls.vertical_thrust_time,
		pre_scale_thrust ? pre_scale_thrust->x : 0,
		pre_scale_thrust ? pre_scale_thrust->y : 0,
		pre_scale_thrust ? pre_scale_thrust->z : 0,
		obj->mtype.phys_info.thrust.x,
		obj->mtype.phys_info.thrust.y,
		obj->mtype.phys_info.thrust.z,
		pre_scale_rotthrust ? pre_scale_rotthrust->x : 0,
		pre_scale_rotthrust ? pre_scale_rotthrust->y : 0,
		pre_scale_rotthrust ? pre_scale_rotthrust->z : 0,
		obj->mtype.phys_info.rotthrust.x,
		obj->mtype.phys_info.rotthrust.y,
		obj->mtype.phys_info.rotthrust.z,
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		obj->orient.fvec.x,
		obj->orient.fvec.y,
		obj->orient.fvec.z,
		obj->orient.rvec.x,
		obj->orient.rvec.y,
		obj->orient.rvec.z,
		obj->orient.uvec.x,
		obj->orient.uvec.y,
		obj->orient.uvec.z,
		obj->mtype.phys_info.flags,
		Players[Player_num].flags,
		Controls.afterburner_state,
		Players[Player_num].afterburner_charge);
}

static void input_demo_log_player_wiggle_probe(object *obj,
	const vms_vector *velocity_before_wiggle,
	const vms_vector *wiggle_delta,
	fix raw_swiggle,
	fix scaled_swiggle,
	fix wiggle_amount,
	int wiggle_applied)
{
	if (!input_demo_player_control_probe_active(obj))
		return;
	if (!wiggle_applied &&
		(!wiggle_delta ||
		(wiggle_delta->x == 0 && wiggle_delta->y == 0 && wiggle_delta->z == 0)))
		return;

	con_printf(CON_NORMAL,
		"Input demo player wiggle probe: mode=%s frame=%u gt=%lld seg=%d applied=%d raw=%d scaled=%d amount=%d ship_wiggle=%d vel_before=(%d,%d,%d) wiggle_delta=(%d,%d,%d) vel_after=(%d,%d,%d) uvec=(%d,%d,%d) phys_flags=0x%x ft=%d\n",
		input_demo_trace_player_control_mode_name(),
		input_demo_trace_player_control_frame_index(),
		(long long)GameTime64,
		obj->segnum,
		wiggle_applied,
		raw_swiggle,
		scaled_swiggle,
		wiggle_amount,
		Player_ship ? Player_ship->wiggle : 0,
		velocity_before_wiggle ? velocity_before_wiggle->x : 0,
		velocity_before_wiggle ? velocity_before_wiggle->y : 0,
		velocity_before_wiggle ? velocity_before_wiggle->z : 0,
		wiggle_delta ? wiggle_delta->x : 0,
		wiggle_delta ? wiggle_delta->y : 0,
		wiggle_delta ? wiggle_delta->z : 0,
		obj->mtype.phys_info.velocity.x,
		obj->mtype.phys_info.velocity.y,
		obj->mtype.phys_info.velocity.z,
		obj->orient.uvec.x,
		obj->orient.uvec.y,
		obj->orient.uvec.z,
		obj->mtype.phys_info.flags,
		FrameTime);
}

void read_flying_controls( object * obj )
{
	fix	forward_thrust_time;
	vms_vector pre_scale_thrust = {0, 0, 0};
	vms_vector pre_scale_rotthrust = {0, 0, 0};
	vms_vector velocity_before_wiggle = {0, 0, 0};
	vms_vector wiggle_delta = {0, 0, 0};
	fix raw_swiggle = 0;
	fix scaled_swiggle = 0;
	fix wiggle_amount = 0;
	int wiggle_applied = 0;

	Assert(FrameTime > 0); 		//Get MATT if hit this!

// this section commented and moved to the bottom by WraithX
//	if (Player_is_dead) {
//		vm_vec_zero(&obj->mtype.phys_info.rotthrust);
//		vm_vec_zero(&obj->mtype.phys_info.thrust);
//		return;
//	}
// end of section to be moved.

	
	if (Guided_missile[Player_num] && Guided_missile[Player_num]->signature==Guided_missile_sig[Player_num]) {
		vms_angvec rotangs;
		vms_matrix rotmat,tempm;
		fix speed;

		//this is a horrible hack.  guided missile stuff should not be
		//handled in the middle of a routine that is dealing with the player

		vm_vec_zero(&obj->mtype.phys_info.rotthrust);

		rotangs.p = Controls.pitch_time / 2 + Seismic_tremor_magnitude/64;
		rotangs.b = Controls.bank_time / 2 + Seismic_tremor_magnitude/16;
		rotangs.h = Controls.heading_time / 2 + Seismic_tremor_magnitude/64;

		vm_angles_2_matrix(&rotmat,&rotangs);

		vm_matrix_x_matrix(&tempm,&Guided_missile[Player_num]->orient,&rotmat);

		Guided_missile[Player_num]->orient = tempm;

		speed = Weapon_info[Guided_missile[Player_num]->id].speed[Difficulty_level];

		vm_vec_copy_scale(&Guided_missile[Player_num]->mtype.phys_info.velocity,&Guided_missile[Player_num]->orient.fvec,speed);
#ifdef NETWORK
		if (Game_mode & GM_MULTI)
			multi_send_guided_info (Guided_missile[Player_num],0);
#endif

	}
	else {
		obj->mtype.phys_info.rotthrust.x = Controls.pitch_time;
		obj->mtype.phys_info.rotthrust.y = Controls.heading_time;
		obj->mtype.phys_info.rotthrust.z = Controls.bank_time;
	}

#ifdef NETWORK
	if((Game_mode & GM_NETWORK) && (Netgame.SpawnStyle == SPAWN_STYLE_PREVIEW) && Player_is_dead && Player_exploded) {
		fix	ft = FrameTime;

		if ((ft < F1_0/2) && (ft << 15 <= Player_ship->max_rotthrust)) {
			ft = (Player_ship->max_thrust >> 15) + 1;
		}

		vm_vec_scale( &obj->mtype.phys_info.rotthrust, fixdiv(Player_ship->max_rotthrust,ft) );
	}
#endif
	
	//references to player_ship require that this obj be the player
	if ((obj->type != OBJ_PLAYER && !object_is_observer(obj)) || (obj->id != Player_num))
		return;


	forward_thrust_time = Controls.forward_thrust_time;

	if (Players[Player_num].flags & PLAYER_FLAGS_AFTERBURNER)
	{
		if (Controls.afterburner_state) {			//player has key down
			//if (forward_thrust_time >= 0) { 		//..and isn't moving backward
			{
				fix afterburner_scale;
				int old_count,new_count;
	
				//add in value from 0..1
				afterburner_scale = f1_0 + min(f1_0/2, Players[Player_num].afterburner_charge) * 2;
	
				forward_thrust_time = fixmul(FrameTime,afterburner_scale);	//based on full thrust
	
				old_count = (Players[Player_num].afterburner_charge / (DROP_DELTA_TIME/AFTERBURNER_USE_SECS));

				Players[Player_num].afterburner_charge -= FrameTime/AFTERBURNER_USE_SECS;

				if (Players[Player_num].afterburner_charge < 0)
					Players[Player_num].afterburner_charge = 0;

				new_count = (Players[Player_num].afterburner_charge / (DROP_DELTA_TIME/AFTERBURNER_USE_SECS));

				if (old_count != new_count) {
					Drop_afterburner_blob_flag = 1;	//drop blob (after physics called)

					if (Game_mode & GM_MULTI)
						multi_send_ship_status();
				}
			}
		}
		else {
			fix afterburner_before = Players[Player_num].afterburner_charge;
			fix cur_energy,charge_up;
			fix energy_before = Players[Player_num].energy;
	
			//charge up to full
			charge_up = min(FrameTime/8,f1_0 - Players[Player_num].afterburner_charge);	//recharge over 8 seconds
	
			cur_energy = max(Players[Player_num].energy-i2f(10),0);	//don't drop below 10

			//maybe limit charge up by energy
			charge_up = min(charge_up,cur_energy/10);
	
			Players[Player_num].afterburner_charge += charge_up;
	
			Players[Player_num].energy -= charge_up * 100 / 10;	//full charge uses 10% of energy

			if (charge_up > 0) {
				char extra_json[160];
				char extra_log[160];

				snprintf(extra_json, sizeof(extra_json), ",\"charge_up\":%d,\"afterburner_before\":%d,\"afterburner_after\":%d", charge_up, afterburner_before, Players[Player_num].afterburner_charge);
				snprintf(extra_log, sizeof(extra_log), " charge_up=%d afterburner_before=%d afterburner_after=%d", charge_up, afterburner_before, Players[Player_num].afterburner_charge);
				input_demo_trace_energy_change("afterburner_recharge", energy_before, Players[Player_num].energy, extra_json, extra_log);
			}

			if (charge_up > 0 && (Game_mode & GM_MULTI))
				multi_send_ship_status();
		}
	}

	// Set object's thrust vector for forward/backward
	vm_vec_copy_scale(&obj->mtype.phys_info.thrust,&obj->orient.fvec, forward_thrust_time );
	
	// slide left/right
	vm_vec_scale_add2(&obj->mtype.phys_info.thrust,&obj->orient.rvec, Controls.sideways_thrust_time );

	// slide up/down
	vm_vec_scale_add2(&obj->mtype.phys_info.thrust,&obj->orient.uvec, Controls.vertical_thrust_time );

	velocity_before_wiggle = obj->mtype.phys_info.velocity;

	if (!is_observer() && obj->mtype.phys_info.flags & PF_WIGGLE)
	{
		fix_fastsincos(((fix)GameTime64), &raw_swiggle, NULL);
		scaled_swiggle = raw_swiggle;
		if (FrameTime < F1_0) // Only scale wiggle if getting at least 1 FPS, to avoid causing the opposite problem.
			scaled_swiggle = fixmul(raw_swiggle*30, FrameTime); //make wiggle fps-independent (based on pre-scaled amount of wiggle at 30 FPS)
		wiggle_amount = fixmul(scaled_swiggle,Player_ship->wiggle);
		vm_vec_copy_scale(&wiggle_delta,&obj->orient.uvec,wiggle_amount);
		vm_vec_add2(&obj->mtype.phys_info.velocity,&wiggle_delta);
		wiggle_applied = 1;
	}

	input_demo_log_player_wiggle_probe(obj,
		&velocity_before_wiggle,
		&wiggle_delta,
		raw_swiggle,
		scaled_swiggle,
		wiggle_amount,
		wiggle_applied);

	newdemo_dump_note_player_wiggle(obj,
		&velocity_before_wiggle,
		&wiggle_delta,
		raw_swiggle,
		scaled_swiggle,
		wiggle_amount,
		wiggle_applied);

	pre_scale_thrust = obj->mtype.phys_info.thrust;
	pre_scale_rotthrust = obj->mtype.phys_info.rotthrust;

	// As of now, obj->mtype.phys_info.thrust & obj->mtype.phys_info.rotthrust are 
	// in units of time... In other words, if thrust==FrameTime, that
	// means that the user was holding down the Max_thrust key for the
	// whole frame.  So we just scale them up by the max, and divide by
	// FrameTime to make them independant of framerate

	//	Prevent divide overflows on high frame rates.
	//	In a signed divide, you get an overflow if num >= div<<15
	{
		fix	ft = FrameTime;

		//	Note, you must check for ft < F1_0/2, else you can get an overflow  on the << 15.
		if ((ft < F1_0/2) && (ft << 15 <= Player_ship->max_thrust)) {
			ft = (Player_ship->max_thrust >> 15) + 1;
		}

		vm_vec_scale( &obj->mtype.phys_info.thrust, fixdiv(Player_ship->max_thrust,ft) );

		if ((ft < F1_0/2) && (ft << 15 <= Player_ship->max_rotthrust)) {
			ft = (Player_ship->max_thrust >> 15) + 1;
		}

		vm_vec_scale( &obj->mtype.phys_info.rotthrust, fixdiv(Player_ship->max_rotthrust,ft) );
	}

	input_demo_log_player_control_probe(obj,
		&pre_scale_thrust,
		&pre_scale_rotthrust,
		forward_thrust_time);

	newdemo_record_player_control_trace(obj,
		&pre_scale_thrust,
		&pre_scale_rotthrust,
		&velocity_before_wiggle,
		&wiggle_delta,
		raw_swiggle,
		scaled_swiggle,
		wiggle_amount,
		forward_thrust_time,
		wiggle_applied);

	// moved here by WraithX
	if (Player_is_dead)
	{
		//vm_vec_zero(&obj->mtype.phys_info.rotthrust); // let dead players rotate, changed by WraithX
		vm_vec_zero(&obj->mtype.phys_info.thrust);  // don't let dead players move, changed by WraithX
		return;
	}// end if

}
