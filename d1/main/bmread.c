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
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/
/*
 *
 * Routines to parse bitmaps.tbl
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "pstypes.h"
#include "inferno.h"
#include "gr.h"
#include "bm.h"
#include "u_mem.h"
#include "dxxerror.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "polyobj.h"
#include "wall.h"
#include "textures.h"
#include "game.h"
#include "multi.h"
#include "iff.h"
#include "hostage.h"
#include "powerup.h"
#include "laser.h"
#include "sounds.h"
#include "piggy.h"
#include "aistruct.h"
#include "robot.h"
#include "weapon.h"
#include "gauges.h"
#include "player.h"
#include "fuelcen.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "args.h"
#include "text.h"
#include "strutil.h"
#include "d1_shareware_table.h"
#ifdef EDITOR
#include "editor/texpage.h"
#endif

#define MAX_BITMAPS_PER_BRUSH 30
extern player_ship only_player_ship;
int TmapList[MAX_TEXTURES];
char Powerup_names[MAX_POWERUP_TYPES][POWERUP_NAME_LENGTH];
char Robot_names[MAX_ROBOT_TYPES][ROBOT_NAME_LENGTH];
static int SuperX = -1;
static int Installed = 0;
extern int d1_read_table_native(int pc_shareware);

#ifdef __ANDROID__
void gamedata_android_reset_tbl(void)
{
 Installed = 0;
 N_robot_types = N_robot_joints = N_weapon_types = N_powerup_types = N_hostage_types = 0;
 Num_cockpits = 0;
 First_multi_bitmap_num = -1;
}
#endif

void remove_char( char * s, char c )
{
	char *p;
	p = strchr(s,c);
	if (p) *p = '\0';
}

//---------------------------------------------------------------
int compute_average_pixel(grs_bitmap *new)
{
	int	row, column, color;
//        char    *pptr;
	int	total_red, total_green, total_blue;

	total_red = 0;
	total_green = 0;
	total_blue = 0;

	for (row=0; row<new->bm_h; row++)
		for (column=0; column<new->bm_w; column++) {
			color = gr_gpixel (new, column, row);
			total_red += gr_palette[color*3];
			total_green += gr_palette[color*3+1];
			total_blue += gr_palette[color*3+2];
		}

	total_red /= (new->bm_h * new->bm_w);
	total_green /= (new->bm_h * new->bm_w);
	total_blue /= (new->bm_h * new->bm_w);

	return BM_XRGB(total_red/2, total_green/2, total_blue/2);
}

//---------------------------------------------------------------
// Loads a bitmap from either the piggy file, a r64 file, or a
// whatever extension is passed.

bitmap_index bm_load_sub(int skip, char * filename )
{
	bitmap_index bitmap_num;
	grs_bitmap * new;
	ubyte newpal[256*3];
	int iff_error;		//reference parm to avoid warning message
	char fname[20];

	bitmap_num.index = 0;

	if (skip) {
		return bitmap_num;
	}

	removeext( filename, fname );

	bitmap_num=piggy_find_bitmap( fname );
	if (bitmap_num.index)	{
		return bitmap_num;
	}

	MALLOC( new, grs_bitmap, 1 );
	iff_error = iff_read_bitmap(filename,new,BM_LINEAR,newpal);
	if (iff_error != IFF_NO_ERROR)		{
		Error("File %s - IFF error: %s",filename,iff_errormsg(iff_error));
	}

	if ( iff_has_transparency )
		gr_remap_bitmap_good( new, newpal, iff_transparent_color, SuperX );
	else
		gr_remap_bitmap_good( new, newpal, -1, SuperX );

	new->avg_color = compute_average_pixel(new);

	bitmap_num = piggy_register_bitmap( new, fname, 0 );
	d_free( new );
	return bitmap_num;
}

void ab_load(int skip, char * filename, bitmap_index bmp[], int *nframes )
{
	grs_bitmap * bm[MAX_BITMAPS_PER_BRUSH];
	bitmap_index bi;
	int i;
	int iff_error;		//reference parm to avoid warning message
	ubyte newpal[768];
	char fname[20];
	char tempname[20];

	if (skip) {
		Assert( bogus_bitmap_initialized != 0 );
		bmp[0] = piggy_register_bitmap(&bogus_bitmap, "bogus", 0);
		*nframes = 1;
		return;
	}


	removeext( filename, fname );
	
	for (i=0; i<MAX_BITMAPS_PER_BRUSH; i++ )	{
		sprintf( tempname, "%s#%d", fname, i );
		bi = piggy_find_bitmap( tempname );
		if ( !bi.index )	
			break;
		bmp[i] = bi;
	}

	if (i) {
		*nframes = i;
		return;
	}

	iff_error = iff_read_animbrush(filename,bm,MAX_BITMAPS_PER_BRUSH,nframes,newpal);
	if (iff_error != IFF_NO_ERROR)	{
		Error("File %s - IFF error: %s",filename,iff_errormsg(iff_error));
	}

	for (i=0;i< *nframes; i++)	{
		bitmap_index new_bmp;
		sprintf( tempname, "%s#%d", fname, i );
		if ( iff_has_transparency )
			gr_remap_bitmap_good( bm[i], newpal, iff_transparent_color, SuperX );
		else
			gr_remap_bitmap_good( bm[i], newpal, -1, SuperX );

		bm[i]->avg_color = compute_average_pixel(bm[i]);

		new_bmp = piggy_register_bitmap( bm[i], tempname, 0 );
		d_free( bm[i] );
		bmp[i] = new_bmp;
	}
}

int ds_load(int skip, char * filename )	{
	int i;
	PHYSFS_file * cfp;
	digi_sound new;
	char fname[20];
	char rawname[100];

	if (skip) {
		// We tell piggy_register_sound it's in the pig file, when in actual fact it's in no file
		// This just tells piggy_close not to attempt to free it
		return piggy_register_sound( &bogus_sound, "bogus", 1 );
	}

	removeext(filename, fname);
	sprintf( rawname, "Sounds/%s.raw", fname );

	i=piggy_find_sound( fname );
	if (i!=255)	{
		return i;
	}

	cfp = PHYSFSX_openReadBuffered(rawname);

	if (cfp!=NULL) {
#ifdef ALLEGRO
		new.len 	= PHYSFS_fileLength( cfp );
		MALLOC( new.data, ubyte, new.len );
		PHYSFS_read( cfp, new.data, 1, new.len );
#else
                new.length      = PHYSFS_fileLength( cfp );
		MALLOC( new.data, ubyte, new.length );
                PHYSFS_read( cfp, new.data, 1, new.length );
#endif
		PHYSFS_close(cfp);
		new.bits = 8;
		new.freq = 11025;
	} else {
		return 255;
	}
	i = piggy_register_sound( &new, fname, 0 );
	return i;
}

bitmap_index d1_table_load_bitmap(int skip, char *filename, int superx)
{
 SuperX = superx;
 return bm_load_sub(skip, filename);
}

void d1_table_load_animation(int skip, char *filename, bitmap_index *bitmaps, int *frames, int superx)
{
 SuperX = superx;
 ab_load(skip, filename, bitmaps, frames);
}

int gamedata_read_tbl(int pc_shareware)
{
 if (Installed) return 1;
 if (d1_read_table_native(pc_shareware) != 0) return 1;
 Installed = 1;
 return 0;
}

void bm_write_all(PHYSFS_file *fp)
{
	int i;

	PHYSFS_write( fp, &NumTextures, sizeof(int), 1);
	PHYSFS_write( fp, Textures, sizeof(bitmap_index), MAX_TEXTURES);
	PHYSFS_write( fp, TmapInfo, sizeof(tmap_info), MAX_TEXTURES);

	PHYSFS_write( fp, Sounds, sizeof(ubyte), MAX_SOUNDS);
	PHYSFS_write( fp, AltSounds, sizeof(ubyte), MAX_SOUNDS);

	PHYSFS_write( fp, &Num_vclips, sizeof(int), 1);
	PHYSFS_write( fp, Vclip, sizeof(vclip), VCLIP_MAXNUM);

	PHYSFS_write( fp, &Num_effects, sizeof(int), 1);
	PHYSFS_write( fp, Effects, sizeof(eclip), MAX_EFFECTS);

	PHYSFS_write( fp, &Num_wall_anims, sizeof(int), 1);
	PHYSFS_write( fp, WallAnims, sizeof(wclip), MAX_WALL_ANIMS);

	PHYSFS_write( fp, &N_robot_types, sizeof(int), 1);
	PHYSFS_write( fp, Robot_info, sizeof(robot_info), MAX_ROBOT_TYPES);

	PHYSFS_write( fp, &N_robot_joints, sizeof(int), 1);
	PHYSFS_write( fp, Robot_joints, sizeof(jointpos), MAX_ROBOT_JOINTS);

	PHYSFS_write( fp, &N_weapon_types, sizeof(int), 1);
	PHYSFS_write( fp, Weapon_info, sizeof(weapon_info), MAX_WEAPON_TYPES);

	PHYSFS_write( fp, &N_powerup_types, sizeof(int), 1);
	PHYSFS_write( fp, Powerup_info, sizeof(powerup_type_info), MAX_POWERUP_TYPES);

	PHYSFS_write( fp, &N_polygon_models, sizeof(int), 1);
	PHYSFS_write( fp, Polygon_models, sizeof(polymodel), N_polygon_models);

	for (i=0; i<N_polygon_models; i++ )	{
		PHYSFS_write( fp, Polygon_models[i].model_data, sizeof(ubyte), Polygon_models[i].model_data_size);
	}

	PHYSFS_write( fp, Gauges, sizeof(bitmap_index), MAX_GAUGE_BMS);

	PHYSFS_write( fp, Dying_modelnums, sizeof(int), MAX_POLYGON_MODELS);
	PHYSFS_write( fp, Dead_modelnums, sizeof(int), MAX_POLYGON_MODELS);

	PHYSFS_write( fp, ObjBitmaps, sizeof(bitmap_index), MAX_OBJ_BITMAPS);
	PHYSFS_write( fp, ObjBitmapPtrs, sizeof(ushort), MAX_OBJ_BITMAPS);

	PHYSFS_write( fp, &only_player_ship, sizeof(player_ship), 1);

	PHYSFS_write( fp, &Num_cockpits, sizeof(int), 1);
	PHYSFS_write( fp, cockpit_bitmap, sizeof(bitmap_index), N_COCKPIT_BITMAPS);

	PHYSFS_write( fp, Sounds, sizeof(ubyte), MAX_SOUNDS);
	PHYSFS_write( fp, AltSounds, sizeof(ubyte), MAX_SOUNDS);

	PHYSFS_write( fp, &Num_total_object_types, sizeof(int), 1);
	PHYSFS_write( fp, ObjType, sizeof(sbyte), MAX_OBJTYPE);
	PHYSFS_write( fp, ObjId, sizeof(sbyte), MAX_OBJTYPE);
	PHYSFS_write( fp, ObjStrength, sizeof(fix), MAX_OBJTYPE);

	PHYSFS_write( fp, &First_multi_bitmap_num, sizeof(int), 1);

	PHYSFS_write( fp, &Reactors[0].n_guns, sizeof(int), 1);
	PHYSFS_write( fp, Reactors[0].gun_points, sizeof(vms_vector), MAX_CONTROLCEN_GUNS);
	PHYSFS_write( fp, Reactors[0].gun_dirs, sizeof(vms_vector), MAX_CONTROLCEN_GUNS);
	PHYSFS_write( fp, &exit_modelnum, sizeof(int), 1);
	PHYSFS_write( fp, &destroyed_exit_modelnum, sizeof(int), 1);
}

