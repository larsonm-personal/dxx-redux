/* Destination views for the single native HAM decoder */
#ifndef DXX_BM_HAM_H
#define DXX_BM_HAM_H

#include "bm.h"
#include "vclip.h"
#include "effects.h"
#include "wall.h"
#include "robot.h"
#include "weapon.h"
#include "powerup.h"
#include "polyobj.h"
#include "player.h"
#include "cntrlcen.h"

/* Callers supply native-capacity tables and own all model payloads
 * Count and scalar destinations must remain valid throughout decoding */
typedef struct bm_ham_destination {
	int *NumTextures;
	bitmap_index *Textures;
	tmap_info *TmapInfo;
	ubyte *Sounds;
	ubyte *AltSounds;
	int *Num_vclips;
	vclip *Vclip;
	int *Num_effects;
	eclip *Effects;
	int *Num_wall_anims;
	wclip *WallAnims;
	int *N_robot_types;
	robot_info *Robot_info;
	int *N_robot_joints;
	jointpos *Robot_joints;
	int *N_weapon_types;
	weapon_info *Weapon_info;
	int *N_powerup_types;
	powerup_type_info *Powerup_info;
	int *N_polygon_models;
	polymodel *Polygon_models;
	int *Dying_modelnums;
	int *Dead_modelnums;
	bitmap_index *Gauges;
	bitmap_index *Gauges_hires;
	int *N_ObjBitmaps;
	bitmap_index *ObjBitmaps;
	ushort *ObjBitmapPtrs;
	player_ship *only_player_ship;
	int *Num_cockpits;
	bitmap_index *cockpit_bitmap;
	int *First_multi_bitmap_num;
	int *Num_reactors;
	reactor *Reactors;
	int *Marker_model_num;
	int *exit_modelnum;
	int *destroyed_exit_modelnum;
	ushort *GameBitmapXlat;
} bm_ham_destination;

#define N_D2_ROBOT_TYPES		66
#define N_D2_ROBOT_JOINTS		1145
#define N_D2_POLYGON_MODELS     166
#define N_D2_OBJBITMAPS			422
#define N_D2_OBJBITMAPPTRS		502
#define N_D2_WEAPON_TYPES		62

#ifdef DXX_XL_RUNTIME
/* Decode a validated version-3 mission HAM into independent destinations
 * No global patch, Android capture or live definition publication occurs
 * The model reader owns allocation/error handling and returns zero on failure */
int bm_read_xl_ham_into(PHYSFS_file *fp, const bm_ham_destination *destination,
                      int (*read_model)(polymodel *, PHYSFS_file *));
/* Publish a completely decoded native-capacity destination without I/O or
 * allocation. Model payload ownership transfers to the native tables */
void bm_install_xl_ham(bm_ham_destination *source);
/* Extra-HAM destinations are native-capacity arrays with empty extra model slots */
int bm_read_xl_extra_ham_into(PHYSFS_file *fp, const bm_ham_destination *destination,
                             int (*read_model)(polymodel *, PHYSFS_file *),
                             int *bitmap_count, int *pointer_count);
#endif
#endif
