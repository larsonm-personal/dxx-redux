#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
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

bitmap_index d1_table_load_bitmap(int skip, char *filename, int superx);
void d1_table_load_animation(int skip, char *filename, bitmap_index *bitmaps, int *frames, int superx);
int ds_load(int skip, char *filename);
extern int TmapList[MAX_TEXTURES];
}
#include "d1_shareware_definitions.hpp"

namespace
{
class NativeD1TableParser final : public D1TableParser
{
  public:
	NativeD1TableParser()
	{
		Textures = ::Textures;
		Gauges = ::Gauges;
		cockpit_bitmap = ::cockpit_bitmap;
		ObjBitmaps = ::ObjBitmaps;
		TmapInfo = ::TmapInfo;
		Vclip = ::Vclip;
		Effects = ::Effects;
		WallAnims = ::WallAnims;
		Robot_info = ::Robot_info;
		Weapon_info = ::Weapon_info;
		Powerup_info = ::Powerup_info;
		Polygon_models = ::Polygon_models;
		Player_ship = ::Player_ship;
		Reactors = ::Reactors;
		GameBitmaps = ::GameBitmaps;
		Sounds = ::Sounds;
		AltSounds = ::AltSounds;
		ObjBitmapPtrs = ::ObjBitmapPtrs;
		ObjType = ::ObjType;
		ObjId = ::ObjId;
		ObjStrength = ::ObjStrength;
		Dying_modelnums = ::Dying_modelnums;
		Dead_modelnums = ::Dead_modelnums;
		TmapList = ::TmapList;
		Hostage_vclip_num = ::Hostage_vclip_num;
		Robot_names = ::Robot_names;
		Powerup_names = ::Powerup_names;
		gauge_limit = MAX_GAUGE_BMS;
	}
	void publish_counts()
	{
		::NumTextures = NumTextures;
		::Num_tmaps = Num_tmaps;
		::Num_effects = Num_effects;
		::Num_vclips = Num_vclips;
		::Num_wall_anims = Num_wall_anims;
		::N_robot_types = N_robot_types;
		::N_weapon_types = N_weapon_types;
		::N_powerup_types = N_powerup_types;
		::N_hostage_types = N_hostage_types;
		::Num_cockpits = Num_cockpits;
		::Num_total_object_types = Num_total_object_types;
		::First_multi_bitmap_num = First_multi_bitmap_num;
		::exit_modelnum = exit_modelnum;
		::destroyed_exit_modelnum = destroyed_exit_modelnum;
	}

  private:
	bitmap_index bm_load_sub(int skip, char *filename) override
	{
		return d1_table_load_bitmap(skip, filename, SuperX);
	}
	void ab_load(int skip, char *filename, bitmap_index *bitmaps, int *frames) override
	{
		d1_table_load_animation(skip, filename, bitmaps, frames, SuperX);
	}
	int ds_load(int skip, char *filename) override
	{
		return ::ds_load(skip, filename);
	}
	int load_polygon_model(char *filename, int textures, int first, robot_info *robot) override
	{
		return ::load_polygon_model(filename, textures, first, robot);
	}
	int read_model_guns(char *filename, vms_vector *points, vms_vector *directions, int *models) override
	{
		return ::read_model_guns(filename, points, directions, models);
	}
	char *texture_name(int index) override
	{
		return ::TmapInfo[index].filename;
	}
};
int read_byte(void *source)
{
	unsigned char value;
	PHYSFS_sint64 result = PHYSFS_read(static_cast<PHYSFS_file *>(source), &value, 1, 1);
	return result == 1 ? value : result == 0 ? -1
	                                         : -2;
}
} // namespace

extern "C" int d1_read_table_native(int pc_shareware)
{
	PHYSFS_file *file = PHYSFSX_openReadBuffered("BITMAPS.TBL");
	int encoded = 0;
	if (!file) {
		file = PHYSFSX_openReadBuffered("BITMAPS.BIN");
		encoded = 1;
	}
	if (!file) Error("Missing BITMAPS.TBL and BITMAPS.BIN file");
	d1_shareware_table_reader input;
	d1_shareware_table_init(&input, read_byte, file, encoded);
	try {
		NativeD1TableParser parser;
		parser.read(input, pc_shareware);
		parser.publish_counts();
	} catch (const std::exception &error) {
		PHYSFS_close(file);
		Error("%s", error.what());
		return 1;
	}
	PHYSFS_close(file);
	return 0;
}
