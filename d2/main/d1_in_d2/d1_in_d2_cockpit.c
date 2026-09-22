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

/* New file, largely derived from the original Parallax/Interplay Descent code */

/* Original D1 cockpit presentation adapted from d1/main/gauges.c
 * Layout, weapon panels and animation belong here; engine drawing remains shared
 * Keep this bounded native-D1 implementation separate from D2's cockpit rules */
#include <stdio.h>
#include <string.h>
#include "inferno.h"
#include "game.h"
#include "screens.h"
#include "gauges.h"
#include "newdemo.h"
#include "gamefont.h"
#include "bm.h"
#include "text.h"
#include "powerup.h"
#include "multi.h"
#include "endlevel.h"
#include "piggy.h"
#include "laser.h"
#include "playsave.h"
#include "rle.h"
#include "byteswap.h"
#include "net_udp.h"
#include "d1_in_d2.h"
#include "d1_in_d2_cockpit.h"
#ifdef OGL
#include "ogl_init.h"
#endif
int get_pnum_for_hud(void);
void game_init_render_sub_buffers(int x, int y, int w, int h);

static int d1_cockpit_hires(void)
{
	return GameBitmaps[cockpit_bitmap[CM_FULL_COCKPIT].index].bm_w >= 640;
}
#define D1_COCKPIT_HIRES d1_cockpit_hires()
//bitmap numbers for gauges
#define GAUGE_SHIELDS			0		//0..9, in decreasing order (100%,90%...0%)
#define GAUGE_INVULNERABLE		10		//10..19
#define N_INVULNERABLE_FRAMES		10
#define GAUGE_SPEED		   	20		//unused
#define GAUGE_ENERGY_LEFT		21
#define GAUGE_ENERGY_RIGHT		22
#define GAUGE_NUMERICAL			23
#define GAUGE_BLUE_KEY			24
#define GAUGE_GOLD_KEY			25
#define GAUGE_RED_KEY			26
#define GAUGE_BLUE_KEY_OFF		27
#define GAUGE_GOLD_KEY_OFF		28
#define GAUGE_RED_KEY_OFF		29
#define SB_GAUGE_BLUE_KEY		30
#define SB_GAUGE_GOLD_KEY		31
#define SB_GAUGE_RED_KEY		32
#define SB_GAUGE_BLUE_KEY_OFF		33
#define SB_GAUGE_GOLD_KEY_OFF		34
#define SB_GAUGE_RED_KEY_OFF		35
#define SB_GAUGE_ENERGY			36
#define GAUGE_LIVES			37
#define GAUGE_SHIPS			38
#define GAUGE_SHIPS_LAST		45
#define RETICLE_CROSS			46
#define RETICLE_PRIMARY			48
#define RETICLE_SECONDARY		51
#define RETICLE_LAST			55
#define GAUGE_HOMING_WARNING_ON	56
#define GAUGE_HOMING_WARNING_OFF	57
#define SML_RETICLE_CROSS		58
#define SML_RETICLE_PRIMARY		60
#define SML_RETICLE_SECONDARY		63
#define SML_RETICLE_LAST		67
#define KEY_ICON_BLUE			68
#define KEY_ICON_YELLOW			69
#define KEY_ICON_RED			70

//Coordinats for gauges
// cockpit keys

#define GAUGE_BLUE_KEY_X_L		45
#define GAUGE_BLUE_KEY_Y_L		152
#define GAUGE_BLUE_KEY_X_H		91
#define GAUGE_BLUE_KEY_Y_H		374
#define GAUGE_BLUE_KEY_X		(D1_COCKPIT_HIRES?GAUGE_BLUE_KEY_X_H:GAUGE_BLUE_KEY_X_L)
#define GAUGE_BLUE_KEY_Y		(D1_COCKPIT_HIRES?GAUGE_BLUE_KEY_Y_H:GAUGE_BLUE_KEY_Y_L)

#define GAUGE_GOLD_KEY_X_L		44
#define GAUGE_GOLD_KEY_Y_L		162
#define GAUGE_GOLD_KEY_X_H		89
#define GAUGE_GOLD_KEY_Y_H		395
#define GAUGE_GOLD_KEY_X		(D1_COCKPIT_HIRES?GAUGE_GOLD_KEY_X_H:GAUGE_GOLD_KEY_X_L)
#define GAUGE_GOLD_KEY_Y		(D1_COCKPIT_HIRES?GAUGE_GOLD_KEY_Y_H:GAUGE_GOLD_KEY_Y_L)

#define GAUGE_RED_KEY_X_L		43
#define GAUGE_RED_KEY_Y_L		172
#define GAUGE_RED_KEY_X_H		87
#define GAUGE_RED_KEY_Y_H		417
#define GAUGE_RED_KEY_X			(D1_COCKPIT_HIRES?GAUGE_RED_KEY_X_H:GAUGE_RED_KEY_X_L)
#define GAUGE_RED_KEY_Y			(D1_COCKPIT_HIRES?GAUGE_RED_KEY_Y_H:GAUGE_RED_KEY_Y_L)

// status bar keys

#define SB_GAUGE_KEYS_X_L		11
#define SB_GAUGE_KEYS_X_H		26
#define SB_GAUGE_KEYS_X			(D1_COCKPIT_HIRES?SB_GAUGE_KEYS_X_H:SB_GAUGE_KEYS_X_L)

#define SB_GAUGE_BLUE_KEY_Y_L		153
#define SB_GAUGE_GOLD_KEY_Y_L		169
#define SB_GAUGE_RED_KEY_Y_L		185

#define SB_GAUGE_BLUE_KEY_Y_H		390
#define SB_GAUGE_GOLD_KEY_Y_H		422
#define SB_GAUGE_RED_KEY_Y_H		454

#define SB_GAUGE_BLUE_KEY_Y		(D1_COCKPIT_HIRES?SB_GAUGE_BLUE_KEY_Y_H:SB_GAUGE_BLUE_KEY_Y_L)
#define SB_GAUGE_GOLD_KEY_Y		(D1_COCKPIT_HIRES?SB_GAUGE_GOLD_KEY_Y_H:SB_GAUGE_GOLD_KEY_Y_L)
#define SB_GAUGE_RED_KEY_Y		(D1_COCKPIT_HIRES?SB_GAUGE_RED_KEY_Y_H:SB_GAUGE_RED_KEY_Y_L)

// cockpit enery gauges

#define LEFT_ENERGY_GAUGE_X_L 	70
#define LEFT_ENERGY_GAUGE_Y_L 	131
#define LEFT_ENERGY_GAUGE_W_L 	64
#define LEFT_ENERGY_GAUGE_H_L 	8

#define LEFT_ENERGY_GAUGE_X_H 	137
#define LEFT_ENERGY_GAUGE_Y_H 	314
#define LEFT_ENERGY_GAUGE_W_H 	133
#define LEFT_ENERGY_GAUGE_H_H 	21

#define LEFT_ENERGY_GAUGE_X 	(D1_COCKPIT_HIRES?LEFT_ENERGY_GAUGE_X_H:LEFT_ENERGY_GAUGE_X_L)
#define LEFT_ENERGY_GAUGE_Y 	(D1_COCKPIT_HIRES?LEFT_ENERGY_GAUGE_Y_H:LEFT_ENERGY_GAUGE_Y_L)
#define LEFT_ENERGY_GAUGE_W 	(D1_COCKPIT_HIRES?LEFT_ENERGY_GAUGE_W_H:LEFT_ENERGY_GAUGE_W_L)
#define LEFT_ENERGY_GAUGE_H 	(D1_COCKPIT_HIRES?LEFT_ENERGY_GAUGE_H_H:LEFT_ENERGY_GAUGE_H_L)

#define RIGHT_ENERGY_GAUGE_X 	(D1_COCKPIT_HIRES?380:190)
#define RIGHT_ENERGY_GAUGE_Y 	(D1_COCKPIT_HIRES?314:131)
#define RIGHT_ENERGY_GAUGE_W 	(D1_COCKPIT_HIRES?133:64)
#define RIGHT_ENERGY_GAUGE_H 	(D1_COCKPIT_HIRES?21:8)

// sb energy gauge

#define SB_ENERGY_GAUGE_X 		(D1_COCKPIT_HIRES?196:98)
#define SB_ENERGY_GAUGE_Y 		(D1_COCKPIT_HIRES?390:155)
#define SB_ENERGY_GAUGE_W 		(D1_COCKPIT_HIRES?32:16)
#define SB_ENERGY_GAUGE_H 		(D1_COCKPIT_HIRES?82:41)

#define SB_ENERGY_NUM_X 		(SB_ENERGY_GAUGE_X+(D1_COCKPIT_HIRES?4:2))
#define SB_ENERGY_NUM_Y 		(D1_COCKPIT_HIRES?457:190)

#define SHIELD_GAUGE_X 			(D1_COCKPIT_HIRES?292:146)
#define SHIELD_GAUGE_Y			(D1_COCKPIT_HIRES?374:155)
#define SHIELD_GAUGE_W 			(D1_COCKPIT_HIRES?70:35)
#define SHIELD_GAUGE_H			(D1_COCKPIT_HIRES?77:32)

#define SHIP_GAUGE_X 			(SHIELD_GAUGE_X+(D1_COCKPIT_HIRES?11:5))
#define SHIP_GAUGE_Y			(SHIELD_GAUGE_Y+(D1_COCKPIT_HIRES?10:5))

#define SB_SHIELD_GAUGE_X 		(D1_COCKPIT_HIRES?247:123)		//139
#define SB_SHIELD_GAUGE_Y 		(D1_COCKPIT_HIRES?395:163)

#define SB_SHIP_GAUGE_X 		(SB_SHIELD_GAUGE_X+(D1_COCKPIT_HIRES?11:5))
#define SB_SHIP_GAUGE_Y 		(SB_SHIELD_GAUGE_Y+(D1_COCKPIT_HIRES?10:5))

#define SB_SHIELD_NUM_X 		(SB_SHIELD_GAUGE_X+(D1_COCKPIT_HIRES?21:12))	//151
#define SB_SHIELD_NUM_Y 		(SB_SHIELD_GAUGE_Y-(D1_COCKPIT_HIRES?16:7))			//156 -- MWA used to be hard coded to 156

#define NUMERICAL_GAUGE_X		(D1_COCKPIT_HIRES?308:154)
#define NUMERICAL_GAUGE_Y		(D1_COCKPIT_HIRES?316:130)
#define NUMERICAL_GAUGE_W		(D1_COCKPIT_HIRES?38:19)
#define NUMERICAL_GAUGE_H		(D1_COCKPIT_HIRES?55:22)

#define PRIMARY_W_PIC_X			(D1_COCKPIT_HIRES?135:64)
#define PRIMARY_W_PIC_Y			(D1_COCKPIT_HIRES?370:154)
#define PRIMARY_W_TEXT_X		HUD_SCALE_X(D1_COCKPIT_HIRES?182:87)
#define PRIMARY_W_TEXT_Y		HUD_SCALE_Y(D1_COCKPIT_HIRES?400:157)
#define PRIMARY_AMMO_X			HUD_SCALE_X(D1_COCKPIT_HIRES?186:93)
#define PRIMARY_AMMO_Y			HUD_SCALE_Y(D1_COCKPIT_HIRES?420:171)

#define SECONDARY_W_PIC_X		(D1_COCKPIT_HIRES?405:234)
#define SECONDARY_W_PIC_Y		(D1_COCKPIT_HIRES?370:154)
#define SECONDARY_W_TEXT_X		HUD_SCALE_X(D1_COCKPIT_HIRES?462:207)
#define SECONDARY_W_TEXT_Y		HUD_SCALE_Y(D1_COCKPIT_HIRES?400:157)
#define SECONDARY_AMMO_X		HUD_SCALE_X(D1_COCKPIT_HIRES?475:213)
#define SECONDARY_AMMO_Y		HUD_SCALE_Y(D1_COCKPIT_HIRES?425:171)

#define SB_LIVES_X			(D1_COCKPIT_HIRES?550:266)
#define SB_LIVES_Y			(D1_COCKPIT_HIRES?450:185)
#define SB_LIVES_LABEL_X		(D1_COCKPIT_HIRES?475:237)
#define SB_LIVES_LABEL_Y		(SB_LIVES_Y+1)

#define SB_SCORE_RIGHT_L		301
#define SB_SCORE_RIGHT_H		605
#define SB_SCORE_RIGHT			(D1_COCKPIT_HIRES?SB_SCORE_RIGHT_H:SB_SCORE_RIGHT_L)

#define SB_SCORE_Y			(D1_COCKPIT_HIRES?398:158)
#define SB_SCORE_LABEL_X		(D1_COCKPIT_HIRES?475:237)

#define SB_SCORE_ADDED_RIGHT		(D1_COCKPIT_HIRES?SB_SCORE_RIGHT_H:SB_SCORE_RIGHT_L)
#define SB_SCORE_ADDED_Y		(D1_COCKPIT_HIRES?413:165)

#define HOMING_WARNING_X		HUD_SCALE_X(D1_COCKPIT_HIRES?13:7)
#define HOMING_WARNING_Y		HUD_SCALE_Y(D1_COCKPIT_HIRES?416:171)

#define BOMB_COUNT_X			(D1_COCKPIT_HIRES?468:210)
#define BOMB_COUNT_Y			(D1_COCKPIT_HIRES?445:186)
#define SB_BOMB_COUNT_X			(D1_COCKPIT_HIRES?342:171)
#define SB_BOMB_COUNT_Y			(D1_COCKPIT_HIRES?458:191)

// defining box boundries for weapon pictures
#define PRIMARY_W_BOX_LEFT_L		63
#define PRIMARY_W_BOX_TOP_L		151		//154
#define PRIMARY_W_BOX_RIGHT_L		(PRIMARY_W_BOX_LEFT_L+58)
#define PRIMARY_W_BOX_BOT_L		(PRIMARY_W_BOX_TOP_L+42)

#define PRIMARY_W_BOX_LEFT_H		121
#define PRIMARY_W_BOX_TOP_H		364
#define PRIMARY_W_BOX_RIGHT_H		241
#define PRIMARY_W_BOX_BOT_H		(PRIMARY_W_BOX_TOP_H+106)		//470

#define PRIMARY_W_BOX_LEFT		(D1_COCKPIT_HIRES?PRIMARY_W_BOX_LEFT_H:PRIMARY_W_BOX_LEFT_L)
#define PRIMARY_W_BOX_TOP		(D1_COCKPIT_HIRES?PRIMARY_W_BOX_TOP_H:PRIMARY_W_BOX_TOP_L)
#define PRIMARY_W_BOX_RIGHT		(D1_COCKPIT_HIRES?PRIMARY_W_BOX_RIGHT_H:PRIMARY_W_BOX_RIGHT_L)
#define PRIMARY_W_BOX_BOT		(D1_COCKPIT_HIRES?PRIMARY_W_BOX_BOT_H:PRIMARY_W_BOX_BOT_L)

#define SECONDARY_W_BOX_LEFT_L		202	//207
#define SECONDARY_W_BOX_TOP_L		151
#define SECONDARY_W_BOX_RIGHT_L		264	//(SECONDARY_W_BOX_LEFT+54)
#define SECONDARY_W_BOX_BOT_L		(SECONDARY_W_BOX_TOP_L+42)

#define SECONDARY_W_BOX_LEFT_H		403
#define SECONDARY_W_BOX_TOP_H		364
#define SECONDARY_W_BOX_RIGHT_H		531
#define SECONDARY_W_BOX_BOT_H		(SECONDARY_W_BOX_TOP_H+106)		//470

#define SECONDARY_W_BOX_LEFT		(D1_COCKPIT_HIRES?SECONDARY_W_BOX_LEFT_H:SECONDARY_W_BOX_LEFT_L)
#define SECONDARY_W_BOX_TOP		(D1_COCKPIT_HIRES?SECONDARY_W_BOX_TOP_H:SECONDARY_W_BOX_TOP_L)
#define SECONDARY_W_BOX_RIGHT		(D1_COCKPIT_HIRES?SECONDARY_W_BOX_RIGHT_H:SECONDARY_W_BOX_RIGHT_L)
#define SECONDARY_W_BOX_BOT		(D1_COCKPIT_HIRES?SECONDARY_W_BOX_BOT_H:SECONDARY_W_BOX_BOT_L)

#define SB_PRIMARY_W_BOX_LEFT_L		34		//50
#define SB_PRIMARY_W_BOX_TOP_L		154
#define SB_PRIMARY_W_BOX_RIGHT_L	(SB_PRIMARY_W_BOX_LEFT_L+55)
#define SB_PRIMARY_W_BOX_BOT_L		(195)

#define SB_PRIMARY_W_BOX_LEFT_H		68
#define SB_PRIMARY_W_BOX_TOP_H		381
#define SB_PRIMARY_W_BOX_RIGHT_H	179
#define SB_PRIMARY_W_BOX_BOT_H		473

#define SB_PRIMARY_W_BOX_LEFT		(D1_COCKPIT_HIRES?SB_PRIMARY_W_BOX_LEFT_H:SB_PRIMARY_W_BOX_LEFT_L)
#define SB_PRIMARY_W_BOX_TOP		(D1_COCKPIT_HIRES?SB_PRIMARY_W_BOX_TOP_H:SB_PRIMARY_W_BOX_TOP_L)
#define SB_PRIMARY_W_BOX_RIGHT		(D1_COCKPIT_HIRES?SB_PRIMARY_W_BOX_RIGHT_H:SB_PRIMARY_W_BOX_RIGHT_L)
#define SB_PRIMARY_W_BOX_BOT		(D1_COCKPIT_HIRES?SB_PRIMARY_W_BOX_BOT_H:SB_PRIMARY_W_BOX_BOT_L)

#define SB_SECONDARY_W_BOX_LEFT_L	169
#define SB_SECONDARY_W_BOX_TOP_L	154
#define SB_SECONDARY_W_BOX_RIGHT_L	(SB_SECONDARY_W_BOX_LEFT_L+54)
#define SB_SECONDARY_W_BOX_BOT_L	(153+42)

#define SB_SECONDARY_W_BOX_LEFT_H	338
#define SB_SECONDARY_W_BOX_TOP_H	381
#define SB_SECONDARY_W_BOX_RIGHT_H	449
#define SB_SECONDARY_W_BOX_BOT_H	473

#define SB_SECONDARY_W_BOX_LEFT		(D1_COCKPIT_HIRES?SB_SECONDARY_W_BOX_LEFT_H:SB_SECONDARY_W_BOX_LEFT_L)	//210
#define SB_SECONDARY_W_BOX_TOP		(D1_COCKPIT_HIRES?SB_SECONDARY_W_BOX_TOP_H:SB_SECONDARY_W_BOX_TOP_L)
#define SB_SECONDARY_W_BOX_RIGHT	(D1_COCKPIT_HIRES?SB_SECONDARY_W_BOX_RIGHT_H:SB_SECONDARY_W_BOX_RIGHT_L)
#define SB_SECONDARY_W_BOX_BOT		(D1_COCKPIT_HIRES?SB_SECONDARY_W_BOX_BOT_H:SB_SECONDARY_W_BOX_BOT_L)

#define SB_PRIMARY_W_PIC_X		(SB_PRIMARY_W_BOX_LEFT+1)	//51
#define SB_PRIMARY_W_PIC_Y		(D1_COCKPIT_HIRES?382:154)
#define SB_PRIMARY_W_TEXT_X		HUD_SCALE_X(SB_PRIMARY_W_BOX_LEFT+(D1_COCKPIT_HIRES?50:24))	//(51+23)
#define SB_PRIMARY_W_TEXT_Y		HUD_SCALE_Y(D1_COCKPIT_HIRES?390:157)
#define SB_PRIMARY_AMMO_X		HUD_SCALE_X(SB_PRIMARY_W_BOX_LEFT+(D1_COCKPIT_HIRES?58:30))	//((SB_PRIMARY_W_BOX_LEFT+33)-3)	//(51+32)
#define SB_PRIMARY_AMMO_Y		HUD_SCALE_Y(D1_COCKPIT_HIRES?410:171)

#define SB_SECONDARY_W_PIC_X		(D1_COCKPIT_HIRES?385:(SB_SECONDARY_W_BOX_LEFT+27))	//(212+27)
#define SB_SECONDARY_W_PIC_Y		(D1_COCKPIT_HIRES?382:154)
#define SB_SECONDARY_W_TEXT_X		HUD_SCALE_X(SB_SECONDARY_W_BOX_LEFT+2)	//212
#define SB_SECONDARY_W_TEXT_Y		HUD_SCALE_Y(D1_COCKPIT_HIRES?390:157)
#define SB_SECONDARY_AMMO_X		HUD_SCALE_X(SB_SECONDARY_W_BOX_LEFT+(D1_COCKPIT_HIRES?(14):11))	//(212+9)
#define SB_SECONDARY_AMMO_Y		HUD_SCALE_Y(D1_COCKPIT_HIRES?414:171)

#define WS_SET				0		//in correct state
#define WS_FADING_OUT			1
#define WS_FADING_IN			2
#define FADE_SCALE			(2*i2f(GR_FADE_LEVELS)/REARM_TIME)		// fade out and back in REARM_TIME, in fade levels per seconds (int)
#define MAX_SHOWN_LIVES 4

#define COCKPIT_PRIMARY_BOX		(!D1_COCKPIT_HIRES?0:4)
#define COCKPIT_SECONDARY_BOX		(!D1_COCKPIT_HIRES?1:5)
#define SB_PRIMARY_BOX			(!D1_COCKPIT_HIRES?2:6)
#define SB_SECONDARY_BOX		(!D1_COCKPIT_HIRES?3:7)

// scaling gauges
#define BASE_WIDTH (D1_COCKPIT_HIRES? 640 : 320)
#define BASE_HEIGHT	(D1_COCKPIT_HIRES? 480 : 200)
#ifdef OGL
#define HUD_SCALE_X(x)		((int) ((double) (x) * ((double)grd_curscreen->sc_w/BASE_WIDTH) + 0.5))
#define HUD_SCALE_Y(y)		((int) ((double) (y) * ((double)grd_curscreen->sc_h/BASE_HEIGHT) + 0.5))
#define HUD_SCALE_X_AR(x)	(HUD_SCALE_X(100) > HUD_SCALE_Y(100) ? HUD_SCALE_Y(x) : HUD_SCALE_X(x))
#define HUD_SCALE_Y_AR(y)	(HUD_SCALE_Y(100) > HUD_SCALE_X(100) ? HUD_SCALE_X(y) : HUD_SCALE_Y(y))
#else
#define HUD_SCALE_X(x)		(x)
#define HUD_SCALE_Y(y)		(y)
#define HUD_SCALE_X_AR(x)	(x)
#define HUD_SCALE_Y_AR(y)	(y)
#endif


static int old_weapon[2] = {-1, -1};
static int invulnerable_frame;
static int weapon_box_states[2];
static fix weapon_box_fade_values[2];
static grs_bitmap deccpt;
static grs_bitmap *WinBoxOverlay[2];
static unsigned char *Decoded_source;
static short Decoded_width, Decoded_height;
typedef struct gauge_box {
	int left,top;
	int right,bot;		//maximal box
} gauge_box;

//first two are primary & secondary
//seconds two are the same for the status bar
static const gauge_box gauge_boxes[] = {

// primary left/right low res
		{PRIMARY_W_BOX_LEFT_L,PRIMARY_W_BOX_TOP_L,PRIMARY_W_BOX_RIGHT_L,PRIMARY_W_BOX_BOT_L},
		{SECONDARY_W_BOX_LEFT_L,SECONDARY_W_BOX_TOP_L,SECONDARY_W_BOX_RIGHT_L,SECONDARY_W_BOX_BOT_L},

//sb left/right low res
		{SB_PRIMARY_W_BOX_LEFT_L,SB_PRIMARY_W_BOX_TOP_L,SB_PRIMARY_W_BOX_RIGHT_L,SB_PRIMARY_W_BOX_BOT_L},
		{SB_SECONDARY_W_BOX_LEFT_L,SB_SECONDARY_W_BOX_TOP_L,SB_SECONDARY_W_BOX_RIGHT_L,SB_SECONDARY_W_BOX_BOT_L},

// primary left/right hires
		{PRIMARY_W_BOX_LEFT_H,PRIMARY_W_BOX_TOP_H,PRIMARY_W_BOX_RIGHT_H,PRIMARY_W_BOX_BOT_H},
		{SECONDARY_W_BOX_LEFT_H,SECONDARY_W_BOX_TOP_H,SECONDARY_W_BOX_RIGHT_H,SECONDARY_W_BOX_BOT_H},

// sb left/right hires
		{SB_PRIMARY_W_BOX_LEFT_H,SB_PRIMARY_W_BOX_TOP_H,SB_PRIMARY_W_BOX_RIGHT_H,SB_PRIMARY_W_BOX_BOT_H},
		{SB_SECONDARY_W_BOX_LEFT_H,SB_SECONDARY_W_BOX_TOP_H,SB_SECONDARY_W_BOX_RIGHT_H,SB_SECONDARY_W_BOX_BOT_H},
	};

//store delta x values from left of box
static const span d1_weapon_window_left[] = {
	{71,114},
	{69,116},
	{68,117},
	{66,118},
	{66,119},
	{66,119},
	{65,119},
	{65,119},
	{65,119},
	{65,119},
	{65,119},
	{65,119},
	{65,119},
	{65,119},
	{65,119},
	{64,119},
	{64,119},
	{64,119},
	{64,119},
	{64,119},
	{64,119},
	{64,119},
	{64,119},
	{64,119},
	{63,119},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,118},
	{63,117},
	{63,117},
	{64,116},
	{65,115},
	{66,113},
	{68,111},
};


//store delta x values from left of box
static const span d1_weapon_window_right[] = {
	{208,255},
	{206,257},
	{205,258},
	{204,259},
	{203,260},
	{203,260},
	{203,260},
	{203,260},
	{203,260},
	{203,261},
	{203,261},
	{203,261},
	{203,261},
	{203,261},
	{203,261},
	{203,261},
	{203,261},
	{203,261},
	{203,262},
	{203,262},
	{203,262},
	{203,262},
	{203,262},
	{203,262},
	{203,262},
	{203,262},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{204,263},
	{205,263},
	{206,262},
	{207,261},
	{208,260},
	{211,255},
};


//store delta x values from left of box
static const span d1_weapon_window_left_hires[] = {
	{141,231},
	{139,234},
	{137,235},
	{136,237},
	{135,238},
	{134,239},
	{133,240},
	{132,240},
	{131,241},
	{131,241},
	{130,242},
	{129,242},
	{129,242},
	{129,243},
	{128,243},
	{128,243},
	{128,243},
	{128,243},
	{128,243},
	{127,243},
	{127,243},
	{127,243},
	{127,243},
	{127,243},
	{127,243},
	{127,243},
	{127,243},
	{127,243},
	{127,243},
	{126,243},
	{126,243},
	{126,243},
	{126,243},
	{126,242},
	{126,242},
	{126,242},
	{126,242},
	{126,242},
	{126,242},
	{125,242},
	{125,242},
	{125,242},
	{125,242},
	{125,242},
	{125,242},
	{125,242},
	{125,242},
	{125,242},
	{125,242},
	{124,242},
	{124,242},
	{124,241},
	{124,241},
	{124,241},
	{124,241},
	{124,241},
	{124,241},
	{124,241},
	{124,241},
	{124,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{123,241},
	{122,241},
	{122,241},
	{122,240},
	{122,240},
	{122,240},
	{122,240},
	{122,240},
	{122,240},
	{122,240},
	{122,240},
	{121,240},
	{121,240},
	{121,240},
	{121,240},
	{121,240},
	{121,240},
	{121,240},
	{121,239},
	{121,239},
	{121,239},
	{121,238},
	{121,238},
	{121,238},
	{122,237},
	{122,237},
	{123,236},
	{123,235},
	{124,234},
	{125,233},
	{126,232},
	{126,231},
	{128,230},
	{130,228},
	{131,226},
	{133,223},
};


//store delta x values from left of box
static const span d1_weapon_window_right_hires[] = {
	{416,509},
	{413,511},
	{412,513},
	{410,514},
	{409,515},
	{408,516},
	{407,517},
	{407,518},
	{406,519},
	{406,519},
	{405,520},
	{405,521},
	{405,521},
	{404,521},
	{404,522},
	{404,522},
	{404,522},
	{404,522},
	{404,522},
	{404,523},
	{404,523},
	{404,523},
	{404,523},
	{404,523},
	{404,523},
	{404,523},
	{404,523},
	{404,523},
	{404,523},
	{404,524},
	{404,524},
	{404,524},
	{404,524},
	{405,524},
	{405,524},
	{405,524},
	{405,524},
	{405,524},
	{405,524},
	{405,525},
	{405,525},
	{405,525},
	{405,525},
	{405,525},
	{405,525},
	{405,525},
	{405,525},
	{405,525},
	{405,525},
	{405,526},
	{405,526},
	{406,526},
	{406,526},
	{406,526},
	{406,526},
	{406,526},
	{406,526},
	{406,526},
	{406,526},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,527},
	{406,528},
	{406,528},
	{407,528},
	{407,528},
	{407,528},
	{407,528},
	{407,528},
	{407,528},
	{407,528},
	{407,529},
	{407,529},
	{407,529},
	{407,529},
	{407,529},
	{407,529},
	{407,529},
	{407,529},
	{408,529},
	{408,529},
	{408,529},
	{409,529},
	{409,529},
	{409,529},
	{410,529},
	{410,528},
	{411,527},
	{412,527},
	{413,526},
	{414,525},
	{415,524},
	{416,524},
	{417,522},
	{419,521},
	{422,519},
	{424,518},
};


#undef WinBoxLeft
#undef WinBoxRight
#define WinBoxLeft (D1_COCKPIT_HIRES?d1_weapon_window_left_hires:d1_weapon_window_left)
#define WinBoxRight (D1_COCKPIT_HIRES?d1_weapon_window_right_hires:d1_weapon_window_right)
#define CLOAK_FADE_WAIT_TIME 0x400
#define INV_FRAME_TIME (f1_0/10)

static void d1_hud_bitblt_free (int x, int y, int w, int h, grs_bitmap *bm)
{
#ifdef OGL
	ogl_ubitmapm_cs (x,y,w,h,bm,-1,F1_0);
#else
	gr_ubitmapm(x, y, bm);
#endif
}

static void d1_hud_bitblt (int x, int y, grs_bitmap *bm)
{
#ifdef OGL
	ogl_ubitmapm_cs (x,y,HUD_SCALE_X (bm->bm_w),HUD_SCALE_Y (bm->bm_h),bm,-1,F1_0);
#else
	gr_ubitmapm(x, y, bm);
#endif
}

static void d1_sb_show_score()
{
	char	score_str[20];
	int x,y;
	int	w, h, aw;

	int pnum = get_pnum_for_hud();

	gr_set_curfont( GAME_FONT );
	gr_set_fontcolor(BM_XRGB(0,20,0),-1 );

	if ( (Game_mode & GM_MULTI) && !((Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS)) )
		gr_printf(HUD_SCALE_X(SB_SCORE_LABEL_X),HUD_SCALE_Y(SB_SCORE_Y),"%s:", TXT_KILLS);
	else
		gr_printf(HUD_SCALE_X(SB_SCORE_LABEL_X),HUD_SCALE_Y(SB_SCORE_Y),"%s:", TXT_SCORE);

	gr_set_curfont( GAME_FONT );
	if ( (Game_mode & GM_MULTI) && !( (Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS) ) )
		sprintf(score_str, "%5d", Players[pnum].net_kills_total);
	else
		sprintf(score_str, "%5d", Players[pnum].score);
	gr_get_string_size(score_str, &w, &h, &aw );

	x = HUD_SCALE_X(SB_SCORE_RIGHT)-w-FSPACX(1);
	y = HUD_SCALE_Y(SB_SCORE_Y);

	//erase old score
	gr_setcolor(BM_XRGB(0,0,0));
	gr_rect(x,y,HUD_SCALE_X(SB_SCORE_RIGHT),y+LINE_SPACING);

	if ( (Game_mode & GM_MULTI) && !((Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS)) )
		gr_set_fontcolor(BM_XRGB(0,20,0),-1 );
	else
		gr_set_fontcolor(BM_XRGB(0,31,0),-1 );

	gr_string(x,y,score_str);
}

static void d1_sb_show_score_added(int *added_score, fix *added_time)
{
	int	color;
	int w, h, aw;
	char	score_str[32];
	static int x;
	static	int last_score_display = -1;

	if ( (Game_mode & GM_MULTI) && !((Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS)) )
		return;

	if ((*added_score) == 0)
		return;

	gr_set_curfont( GAME_FONT );

	(*added_time) -= FrameTime;
	if ((*added_time) > 0) {
		if ((*added_score) != last_score_display)
			last_score_display = (*added_score);

		color = f2i((*added_time) * 20) + 10;

		if (color < 10) color = 10;
		if (color > 31) color = 31;

		if (cheats.enabled)
			sprintf(score_str, "%s", TXT_CHEATER);
		else
			sprintf(score_str, "%5d", (*added_score));

		gr_get_string_size(score_str, &w, &h, &aw );
		x = HUD_SCALE_X(SB_SCORE_ADDED_RIGHT)-w-FSPACX(1);
		gr_set_fontcolor(BM_XRGB(0, color, 0),-1 );
		gr_string(x, HUD_SCALE_Y(SB_SCORE_ADDED_Y), score_str);
	} else {
		//erase old score
		gr_setcolor(BM_XRGB(0,0,0));
		gr_rect(x,HUD_SCALE_Y(SB_SCORE_ADDED_Y),HUD_SCALE_X(SB_SCORE_ADDED_RIGHT),HUD_SCALE_Y(SB_SCORE_ADDED_Y)+LINE_SPACING);
		(*added_time) = 0;
		(*added_score) = 0;
	}
}

static void d1_show_homing_warning(void)
{
	int pnum = get_pnum_for_hud();

	if (Endlevel_sequence)
	{
		PIGGY_PAGE_IN( Gauges[GAUGE_HOMING_WARNING_OFF] );
		d1_hud_bitblt( HOMING_WARNING_X, HOMING_WARNING_Y, &GameBitmaps[Gauges[GAUGE_HOMING_WARNING_OFF].index]);
		return;
	}

	gr_set_current_canvas( NULL );

	if (Players[pnum].homing_object_dist >= 0)
	{
		if (GameTime64 & 0x4000)
		{
			PIGGY_PAGE_IN(Gauges[GAUGE_HOMING_WARNING_ON]);
			d1_hud_bitblt( HOMING_WARNING_X, HOMING_WARNING_Y, &GameBitmaps[Gauges[GAUGE_HOMING_WARNING_ON].index]);
		}
		else
		{
			PIGGY_PAGE_IN(Gauges[GAUGE_HOMING_WARNING_OFF]);
			d1_hud_bitblt( HOMING_WARNING_X, HOMING_WARNING_Y, &GameBitmaps[Gauges[GAUGE_HOMING_WARNING_OFF].index]);
		}
	}
	else
	{
		PIGGY_PAGE_IN(Gauges[GAUGE_HOMING_WARNING_OFF]);
		d1_hud_bitblt( HOMING_WARNING_X, HOMING_WARNING_Y, &GameBitmaps[Gauges[GAUGE_HOMING_WARNING_OFF].index]);
	}
}

static void d1_show_bomb_count(int x,int y,int bg_color,int always_show,int right_align)
{
	int bomb,count,w=0,h=0,aw=0;
	char txt[5],*t;

	int pnum = get_pnum_for_hud();

	bomb = PROXIMITY_INDEX;
	count = Players[pnum].secondary_ammo[bomb];

	count = min(count,99);	//only have room for 2 digits - cheating give 200

	if (always_show && count == 0)		//no bombs, draw nothing on HUD
		return;

	if (count)
		gr_set_fontcolor(gr_find_closest_color(55,0,0),bg_color);
	else
		gr_set_fontcolor(bg_color,bg_color);	//erase by drawing in background color

	snprintf(txt, sizeof(txt), "B:%02d", count);

	while ((t=strchr(txt,'1')) != NULL)
		*t = '\x84';	//convert to wide '1'

	if (right_align)
		gr_get_string_size(txt, &w, &h, &aw );

	gr_string(x-w,y,txt);
}

static void d1_hud_show_weapons_mode(int type,int vertical,int x,int y){
	int i,w,h,aw;
	char weapon_str[10];

	int pnum = get_pnum_for_hud();

	if (vertical){
		y=y+(LINE_SPACING*4);
	}
	if (type==0){
		for (i=4;i>=0;i--){
			if (Players[pnum].primary_weapon==i)
				gr_set_fontcolor(BM_XRGB(20,0,0),-1);
			else{
				if (player_has_weapon(pnum, i, 0) & HAS_WEAPON_FLAG)
					gr_set_fontcolor(BM_XRGB(0,15,0),-1);
				else
					gr_set_fontcolor(BM_XRGB(3,3,3),-1);
			}
			switch(i){
				case 0:
					sprintf(weapon_str,"%c%i",
						(Players[pnum].flags & PLAYER_FLAGS_QUAD_LASERS)?'Q':'L',
						Players[pnum].laser_level+1);
					break;
				case 1:
				if (PlayerCfg.CurrentCockpitMode==CM_FULL_SCREEN)
					sprintf(weapon_str,"V%i", f2i(Players[pnum].primary_ammo[1] * VULCAN_AMMO_SCALE));
				else
					sprintf(weapon_str,"V%i", f2i(Players[pnum].primary_ammo[1] * VULCAN_AMMO_SCALE));
					break;
				case 2:
					sprintf(weapon_str,"S");break;
				case 3:
					sprintf(weapon_str,"P");break;
				case 4:
					sprintf(weapon_str,"F");break;

			}
			gr_get_string_size(weapon_str, &w, &h, &aw );
			if (vertical){
				y-=h+FSPACX(2);
			}else
				 x-=w+FSPACY(3);
			gr_string(x, y, weapon_str);
		}
	} else {
		for (i=4;i>=0;i--){
			if (Players[pnum].secondary_weapon==i)
				gr_set_fontcolor(BM_XRGB(20,0,0),-1);
			else{
				if (Players[pnum].secondary_ammo[i]>0)
					gr_set_fontcolor(BM_XRGB(0,15,0),-1);
				else
					gr_set_fontcolor(BM_XRGB(0,6,0),-1);
			}
			sprintf(weapon_str,"%i",Players[pnum].secondary_ammo[i]);
			gr_get_string_size(weapon_str, &w, &h, &aw );
			if (vertical){
				y-=h+FSPACX(2);
			}else
				 x-=w+FSPACY(3);
			gr_string(x, y, weapon_str);
		}
	}
	gr_set_fontcolor(BM_XRGB(0,31,0),-1 );
}

static void d1_sb_show_lives()
{
	int x,y;

	int pnum = get_pnum_for_hud();

	grs_bitmap * bm = &GameBitmaps[Gauges[GAUGE_LIVES].index];
	x = SB_LIVES_X;
	y = SB_LIVES_Y;

	gr_set_curfont( GAME_FONT );
	gr_set_fontcolor(BM_XRGB(0,20,0),-1 );
	if (Game_mode & GM_MULTI)
		gr_printf(HUD_SCALE_X(SB_LIVES_LABEL_X),HUD_SCALE_Y(y),"%s:", TXT_DEATHS);
	else
		gr_printf(HUD_SCALE_X(SB_LIVES_LABEL_X),HUD_SCALE_Y(y),"%s:", TXT_LIVES);

	if (Game_mode & GM_MULTI)
	{
		char killed_str[20];
		int w, h, aw;
		static int last_x[4] = {SB_SCORE_RIGHT_L,SB_SCORE_RIGHT_L,SB_SCORE_RIGHT_H,SB_SCORE_RIGHT_H};
		int x;

		sprintf(killed_str, "%5d", Players[pnum].net_killed_total);
		gr_get_string_size(killed_str, &w, &h, &aw);
		gr_setcolor(BM_XRGB(0,0,0));
		gr_rect(last_x[D1_COCKPIT_HIRES], HUD_SCALE_Y(y), HUD_SCALE_X(SB_SCORE_RIGHT), HUD_SCALE_Y(y)+LINE_SPACING);
		gr_set_fontcolor(BM_XRGB(0,20,0),-1);
		x = HUD_SCALE_X(SB_SCORE_RIGHT)-w-FSPACX(1);
		gr_string(x, HUD_SCALE_Y(y), killed_str);
		last_x[D1_COCKPIT_HIRES] = x;
		return;
	}

	//erase old icons
	gr_setcolor(BM_XRGB(0,0,0));
	gr_rect(HUD_SCALE_X(x), HUD_SCALE_Y(y), HUD_SCALE_X(SB_SCORE_RIGHT), HUD_SCALE_Y(y+bm->bm_h));

	if (Players[pnum].lives-1 > 0) {
		gr_set_curfont( GAME_FONT );
		gr_set_fontcolor(BM_XRGB(0,20,0),-1 );
		PIGGY_PAGE_IN(Gauges[GAUGE_LIVES]);
		d1_hud_bitblt_free(HUD_SCALE_X(x),HUD_SCALE_Y(y),HUD_SCALE_X_AR(bm->bm_w),HUD_SCALE_Y_AR(bm->bm_h),bm);
		gr_printf(HUD_SCALE_X(x)+HUD_SCALE_X_AR(bm->bm_w), HUD_SCALE_Y(y), " x %d", Players[pnum].lives-1);
	}
}

static void d1_cockpit_decode_alpha(grs_bitmap *bm)
{

	int i=0,x=0,y=0;
	static unsigned char cockpitbuf[1024*1024];

	// check if we processed this bitmap already
	if (Decoded_source==bm->bm_data && Decoded_width == bm->bm_w && Decoded_height == bm->bm_h)
		return;

	if ((size_t)bm->bm_w * bm->bm_h > sizeof(cockpitbuf))
		Error("D1 cockpit bitmap is too large");
	memset(cockpitbuf,0,sizeof(cockpitbuf));

	// decode the bitmap
	if (bm->bm_flags & BM_FLAG_RLE){
		unsigned char * dbits;
		unsigned char * sbits;
		int i, data_offset;

		data_offset = 1;
		if (bm->bm_flags & BM_FLAG_RLE_BIG)
			data_offset = 2;

		sbits = &bm->bm_data[4 + (bm->bm_h * data_offset)];
		dbits = cockpitbuf;

		for (i=0; i < bm->bm_h; i++ )    {
			gr_rle_decode(sbits,dbits);
			if ( bm->bm_flags & BM_FLAG_RLE_BIG )
				sbits += (int)INTEL_SHORT(*((short *)&(bm->bm_data[4+(i*data_offset)])));
			else
				sbits += (int)bm->bm_data[4+i];
			dbits += bm->bm_w;
		}
	}
	else
	{
		memcpy(&cockpitbuf, bm->bm_data, sizeof(unsigned char)*(bm->bm_w*bm->bm_h));
	}

	// add alpha color to the pixels which are inside the window box spans
	for (y=0;y<bm->bm_h;y++)
	{
		for (x=0;x<bm->bm_w;x++)
		{
			if (y >= (D1_COCKPIT_HIRES?364:151) && y <= (D1_COCKPIT_HIRES?469:193) && ((x >= WinBoxLeft[y-(D1_COCKPIT_HIRES?364:151)].l && x <= WinBoxLeft[y-(D1_COCKPIT_HIRES?364:151)].r) ||  (x >=WinBoxRight[y-(D1_COCKPIT_HIRES?364:151)].l && x <= WinBoxRight[y-(D1_COCKPIT_HIRES?364:151)].r)))
				cockpitbuf[i]=TRANSPARENCY_COLOR;
			i++;
		}
	}
#ifdef OGL
	ogl_freebmtexture(&deccpt);
#endif
	gr_init_bitmap (&deccpt, 0, 0, 0, bm->bm_w, bm->bm_h, bm->bm_w, cockpitbuf);
	gr_set_transparent(&deccpt,1);
#ifdef OGL
	ogl_loadbmtexture_f(&deccpt, GameCfg.TexFilt, NULL);
#endif
	if (WinBoxOverlay[0] != NULL)
		gr_free_sub_bitmap(WinBoxOverlay[0]);
	if (WinBoxOverlay[1] != NULL)
		gr_free_sub_bitmap(WinBoxOverlay[1]);
	WinBoxOverlay[0] = gr_create_sub_bitmap(&deccpt,(PRIMARY_W_BOX_LEFT)-2,(PRIMARY_W_BOX_TOP)-2,(PRIMARY_W_BOX_RIGHT-PRIMARY_W_BOX_LEFT+4),(PRIMARY_W_BOX_BOT-PRIMARY_W_BOX_TOP+4));
	WinBoxOverlay[1] = gr_create_sub_bitmap(&deccpt,(SECONDARY_W_BOX_LEFT)-2,(SECONDARY_W_BOX_TOP)-2,(SECONDARY_W_BOX_RIGHT-SECONDARY_W_BOX_LEFT)+4,(SECONDARY_W_BOX_BOT-SECONDARY_W_BOX_TOP)+4);

	Decoded_source = bm->bm_data;
	Decoded_width = bm->bm_w;
	Decoded_height = bm->bm_h;
}

static void d1_draw_wbu_overlay()
{
	grs_bitmap * bm = &GameBitmaps[cockpit_bitmap[PlayerCfg.CurrentCockpitMode].index];

	PIGGY_PAGE_IN(cockpit_bitmap[PlayerCfg.CurrentCockpitMode]);
	d1_cockpit_decode_alpha(bm);

	if (WinBoxOverlay[0] != NULL)
		d1_hud_bitblt(HUD_SCALE_X(PRIMARY_W_BOX_LEFT-2),HUD_SCALE_Y(PRIMARY_W_BOX_TOP-2),WinBoxOverlay[0]);
	if (WinBoxOverlay[1] != NULL)
		d1_hud_bitblt(HUD_SCALE_X(SECONDARY_W_BOX_LEFT-2),HUD_SCALE_Y(SECONDARY_W_BOX_TOP-2),WinBoxOverlay[1]);
}

static void d1_draw_energy_bar(int energy)
{
	int x1, x2, y;
	int not_energy = (D1_COCKPIT_HIRES?(HUD_SCALE_X(125 - (energy*125)/100)):(HUD_SCALE_X(63 - (energy*63)/100)));
	double aplitscale=((double)(HUD_SCALE_X(65)/HUD_SCALE_Y(8))/(65/8)); //scale aplitude of energy bar to current resolution aspect

	// Draw left energy bar
	PIGGY_PAGE_IN(Gauges[GAUGE_ENERGY_LEFT]);
	d1_hud_bitblt (HUD_SCALE_X(LEFT_ENERGY_GAUGE_X), HUD_SCALE_Y(LEFT_ENERGY_GAUGE_Y), &GameBitmaps[Gauges[GAUGE_ENERGY_LEFT].index]);

	gr_setcolor(BM_XRGB(0,0,0));

	if (energy < 100)
		for (y=0; y < HUD_SCALE_Y(LEFT_ENERGY_GAUGE_H); y++) {
			x1 = HUD_SCALE_X(LEFT_ENERGY_GAUGE_H - 2) - y*(aplitscale);
			x2 = HUD_SCALE_X(LEFT_ENERGY_GAUGE_H - 2) - y*(aplitscale) + not_energy;

			if (x2 > HUD_SCALE_X(LEFT_ENERGY_GAUGE_W) - (y*aplitscale)/3)
				x2 = HUD_SCALE_X(LEFT_ENERGY_GAUGE_W) - (y*aplitscale)/3;

			if (x2 > x1) gr_uline( i2f(x1+HUD_SCALE_X(LEFT_ENERGY_GAUGE_X)), i2f(y+HUD_SCALE_Y(LEFT_ENERGY_GAUGE_Y)), i2f(x2+HUD_SCALE_X(LEFT_ENERGY_GAUGE_X)), i2f(y+HUD_SCALE_Y(LEFT_ENERGY_GAUGE_Y)) );
		}

	gr_set_current_canvas( NULL );

	// Draw right energy bar
	PIGGY_PAGE_IN(Gauges[GAUGE_ENERGY_RIGHT]);
	d1_hud_bitblt (HUD_SCALE_X(RIGHT_ENERGY_GAUGE_X), HUD_SCALE_Y(RIGHT_ENERGY_GAUGE_Y), &GameBitmaps[Gauges[GAUGE_ENERGY_RIGHT].index]);

	if (energy < 100)
		for (y=0; y < HUD_SCALE_Y(RIGHT_ENERGY_GAUGE_H); y++) {
			x1 = HUD_SCALE_X(RIGHT_ENERGY_GAUGE_W - RIGHT_ENERGY_GAUGE_H + 2 ) + y*(aplitscale) - not_energy;
			x2 = HUD_SCALE_X(RIGHT_ENERGY_GAUGE_W - RIGHT_ENERGY_GAUGE_H + 2 ) + y*(aplitscale);

			if (x1 < (y*aplitscale)/3)
				x1 = (y*aplitscale)/3;

			if (x2 > x1) gr_uline( i2f(x1+HUD_SCALE_X(RIGHT_ENERGY_GAUGE_X)), i2f(y+HUD_SCALE_Y(RIGHT_ENERGY_GAUGE_Y)), i2f(x2+HUD_SCALE_X(RIGHT_ENERGY_GAUGE_X)), i2f(y+HUD_SCALE_Y(RIGHT_ENERGY_GAUGE_Y)) );
		}

	gr_set_current_canvas( NULL );
}

static void d1_draw_shield_bar(int shield)
{
	int bm_num = shield>=100?9:(shield / 10);

	PIGGY_PAGE_IN(Gauges[GAUGE_SHIELDS+9-bm_num]);
	d1_hud_bitblt (HUD_SCALE_X(SHIELD_GAUGE_X), HUD_SCALE_Y(SHIELD_GAUGE_Y), &GameBitmaps[Gauges[GAUGE_SHIELDS+9-bm_num].index]);
}

static void d1_draw_player_ship(int cloak_state,int x, int y)
{
	static fix cloak_fade_timer=0;
	static int cloak_fade_value=GR_FADE_LEVELS-1;

	int pnum = get_pnum_for_hud();

	grs_bitmap *bm = NULL;

	int color;
#ifdef NETWORK
	if (Game_mode & GM_TEAM)
	{
		color = get_color_for_team(get_team(pnum));
	}
	else
#endif
	{
		color = Netgame.players[pnum].color;
	}
	PIGGY_PAGE_IN(Gauges[GAUGE_SHIPS+color]);
	bm = &GameBitmaps[Gauges[GAUGE_SHIPS+color].index];

	if (cloak_state)
	{
		static int step = 0;

		if (GameTime64-Players[pnum].cloak_time < F1_0)
		{
			step = -2;
		}
		else if (Players[pnum].cloak_time+CLOAK_TIME_MAX-GameTime64 <= F1_0*3)
		{
			if (cloak_fade_value >= (GR_FADE_LEVELS-1))
			{
				step = -2;
			}
			else if (cloak_fade_value <= 0)
			{
				step = 2;
			}
		}
		else
		{
			step = 0;
			cloak_fade_value = 0;
		}

		cloak_fade_timer -= FrameTime;

		while (cloak_fade_timer < 0)
		{
			cloak_fade_timer += CLOAK_FADE_WAIT_TIME;
			cloak_fade_value += step;
		}

		if (cloak_fade_value > (GR_FADE_LEVELS-1))
			cloak_fade_value = (GR_FADE_LEVELS-1);
		if (cloak_fade_value <= 0)
			cloak_fade_value = 0;
	}
	else
	{
		cloak_fade_timer = 0;
		cloak_fade_value = GR_FADE_LEVELS-1;
	}

	gr_set_current_canvas(NULL);
	d1_hud_bitblt( HUD_SCALE_X(x), HUD_SCALE_Y(y), bm);
	gr_settransblend(cloak_fade_value, GR_BLEND_NORMAL);
	gr_rect(HUD_SCALE_X(x-3), HUD_SCALE_Y(y-3), HUD_SCALE_X(x+bm->bm_w+3), HUD_SCALE_Y(y+bm->bm_h+3));
	gr_settransblend(GR_FADE_OFF, GR_BLEND_NORMAL);
	gr_set_current_canvas( NULL );
}

static void d1_draw_numerical_display(int shield, int energy)
{
	int sw,sh,saw,ew,eh,eaw;

	gr_set_curfont( GAME_FONT );
#ifndef OGL
	PIGGY_PAGE_IN(Gauges[GAUGE_NUMERICAL]);
	d1_hud_bitblt (HUD_SCALE_X(NUMERICAL_GAUGE_X), HUD_SCALE_Y(NUMERICAL_GAUGE_Y), &GameBitmaps[Gauges[GAUGE_NUMERICAL].index]);
#endif
	// cockpit is not 100% geometric so we need to divide shield and energy X position by 1.951 which should be most accurate
	// gr_get_string_size is used so we can get the numbers finally in the correct position with sw and ew
	gr_set_fontcolor(BM_XRGB(14,14,23),-1 );
	gr_get_string_size((shield>199)?"200":(shield>99)?"100":(shield>9)?"00":"0",&sw,&sh,&saw);
	gr_printf(	(grd_curscreen->sc_w/1.951)-(sw/2),
			(grd_curscreen->sc_h/1.365),"%d",shield);

	gr_set_fontcolor(BM_XRGB(25,18,6),-1 );
	gr_get_string_size((energy>199)?"200":(energy>99)?"100":(energy>9)?"00":"0",&ew,&eh,&eaw);
	gr_printf(	(grd_curscreen->sc_w/1.951)-(ew/2),
			(grd_curscreen->sc_h/1.5),"%d",energy);

	gr_set_current_canvas( NULL );
}

static void d1_draw_keys()
{
	gr_set_current_canvas( NULL );

	int pnum = get_pnum_for_hud();

	if (Players[pnum].flags & PLAYER_FLAGS_BLUE_KEY )	{
		PIGGY_PAGE_IN(Gauges[GAUGE_BLUE_KEY]);
				d1_hud_bitblt( HUD_SCALE_X(GAUGE_BLUE_KEY_X), HUD_SCALE_Y(GAUGE_BLUE_KEY_Y), &GameBitmaps[Gauges[GAUGE_BLUE_KEY].index]);
	} else {
		PIGGY_PAGE_IN(Gauges[GAUGE_BLUE_KEY_OFF]);
		d1_hud_bitblt( HUD_SCALE_X(GAUGE_BLUE_KEY_X), HUD_SCALE_Y(GAUGE_BLUE_KEY_Y), &GameBitmaps[Gauges[GAUGE_BLUE_KEY_OFF].index]);
	}

	if (Players[pnum].flags & PLAYER_FLAGS_GOLD_KEY)	{
		PIGGY_PAGE_IN(Gauges[GAUGE_GOLD_KEY]);
		d1_hud_bitblt( HUD_SCALE_X(GAUGE_GOLD_KEY_X), HUD_SCALE_Y(GAUGE_GOLD_KEY_Y), &GameBitmaps[Gauges[GAUGE_GOLD_KEY].index]);
	} else {
		PIGGY_PAGE_IN(Gauges[GAUGE_GOLD_KEY_OFF]);
		d1_hud_bitblt( HUD_SCALE_X(GAUGE_GOLD_KEY_X), HUD_SCALE_Y(GAUGE_GOLD_KEY_Y), &GameBitmaps[Gauges[GAUGE_GOLD_KEY_OFF].index]);
	}

	if (Players[pnum].flags & PLAYER_FLAGS_RED_KEY)	{
		PIGGY_PAGE_IN( Gauges[GAUGE_RED_KEY] );
		d1_hud_bitblt( HUD_SCALE_X(GAUGE_RED_KEY_X), HUD_SCALE_Y(GAUGE_RED_KEY_Y), &GameBitmaps[Gauges[GAUGE_RED_KEY].index]);
	} else {
		PIGGY_PAGE_IN(Gauges[GAUGE_RED_KEY_OFF]);
		d1_hud_bitblt( HUD_SCALE_X(GAUGE_RED_KEY_X), HUD_SCALE_Y(GAUGE_RED_KEY_Y), &GameBitmaps[Gauges[GAUGE_RED_KEY_OFF].index]);
	}
}

static void d1_draw_weapon_info_sub(int info_index,const gauge_box *box,int pic_x,int pic_y,char *name,int text_x,int text_y)
{
	grs_bitmap *bm;

	int pnum = get_pnum_for_hud();

	//clear the window
	gr_setcolor(BM_XRGB(0,0,0));
	gr_rect(HUD_SCALE_X(box->left),HUD_SCALE_Y(box->top),HUD_SCALE_X(box->right),HUD_SCALE_Y(box->bot+1));

	bm=&GameBitmaps[Weapon_info[info_index].picture.index];
	Assert(bm != NULL);

	PIGGY_PAGE_IN( Weapon_info[info_index].picture );
	d1_hud_bitblt(HUD_SCALE_X(pic_x), HUD_SCALE_Y(pic_y), bm);

	if (PlayerCfg.HudMode == 0)
	{
		gr_set_fontcolor(BM_XRGB(0,20,0),-1 );

		gr_string(text_x,text_y,name);

		//	For laser, show level and quadness
		if (info_index == LASER_INDEX)
		{
			gr_printf(text_x,text_y+LINE_SPACING, "%s: %i", TXT_LVL, Players[pnum].laser_level+1);
			if (Players[pnum].flags & PLAYER_FLAGS_QUAD_LASERS)
				gr_string(text_x,text_y+(LINE_SPACING*2), TXT_QUAD);
		}
	}
}

static void d1_draw_weapon_info(int weapon_type,int weapon_num)
{
	int x,y;

#ifdef SHAREWARE
	if (Newdemo_state==ND_STATE_RECORDING )
		newdemo_record_player_weapon(weapon_type, weapon_num);
#endif

	if (weapon_type == 0)
	{
		if (PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR)
		{
			d1_draw_weapon_info_sub(Primary_weapon_to_weapon_info[weapon_num],&gauge_boxes[SB_PRIMARY_BOX],SB_PRIMARY_W_PIC_X,SB_PRIMARY_W_PIC_Y,PRIMARY_WEAPON_NAMES_SHORT(weapon_num),SB_PRIMARY_W_TEXT_X,SB_PRIMARY_W_TEXT_Y);
			x=SB_PRIMARY_AMMO_X;
			y=SB_PRIMARY_AMMO_Y;
		}
		else
		{
			d1_draw_weapon_info_sub(Primary_weapon_to_weapon_info[weapon_num],&gauge_boxes[COCKPIT_PRIMARY_BOX],PRIMARY_W_PIC_X,PRIMARY_W_PIC_Y, PRIMARY_WEAPON_NAMES_SHORT(weapon_num),PRIMARY_W_TEXT_X,PRIMARY_W_TEXT_Y);
			x=PRIMARY_AMMO_X;
			y=PRIMARY_AMMO_Y;
		}
	}
	else
	{
		if (PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR)
		{
			d1_draw_weapon_info_sub(Secondary_weapon_to_weapon_info[weapon_num],&gauge_boxes[SB_SECONDARY_BOX],SB_SECONDARY_W_PIC_X,SB_SECONDARY_W_PIC_Y,SECONDARY_WEAPON_NAMES_SHORT(weapon_num),SB_SECONDARY_W_TEXT_X,SB_SECONDARY_W_TEXT_Y);
			x=SB_SECONDARY_AMMO_X;
			y=SB_SECONDARY_AMMO_Y;
		}
		else
		{
			d1_draw_weapon_info_sub(Secondary_weapon_to_weapon_info[weapon_num],&gauge_boxes[COCKPIT_SECONDARY_BOX],SECONDARY_W_PIC_X,SECONDARY_W_PIC_Y,SECONDARY_WEAPON_NAMES_SHORT(weapon_num),SECONDARY_W_TEXT_X,SECONDARY_W_TEXT_Y);
			x=SECONDARY_AMMO_X;
			y=SECONDARY_AMMO_Y;
		}
	}

	if (PlayerCfg.HudMode!=0)
		d1_hud_show_weapons_mode(weapon_type,1,x,y);
}

static void d1_draw_ammo_info(int x,int y,int ammo_count,int primary)
{
	if (PlayerCfg.HudMode!=0)
		d1_hud_show_weapons_mode(!primary,1,x,y);
	else
	{
		gr_setcolor(BM_XRGB(0,0,0));
		gr_set_fontcolor(BM_XRGB(20,0,0),-1 );
		gr_printf(x,y,"%03d",ammo_count);
	}
}

static void d1_draw_primary_ammo_info(int ammo_count)
{
	if (PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR)
		d1_draw_ammo_info(SB_PRIMARY_AMMO_X,SB_PRIMARY_AMMO_Y,ammo_count,1);
	else
		d1_draw_ammo_info(PRIMARY_AMMO_X,PRIMARY_AMMO_Y,ammo_count,1);
}

static void d1_draw_secondary_ammo_info(int ammo_count)
{
	if (PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR)
		d1_draw_ammo_info(SB_SECONDARY_AMMO_X,SB_SECONDARY_AMMO_Y,ammo_count,0);
	else
		d1_draw_ammo_info(SECONDARY_AMMO_X,SECONDARY_AMMO_Y,ammo_count,0);
}

static void d1_draw_weapon_box(int weapon_type,int weapon_num)
{
	gr_set_current_canvas(NULL);

	gr_set_curfont( GAME_FONT );

	if (weapon_num != old_weapon[weapon_type] && weapon_box_states[weapon_type] == WS_SET && (old_weapon[weapon_type] != -1) && !PlayerCfg.HudMode)
	{
		weapon_box_states[weapon_type] = WS_FADING_OUT;
		weapon_box_fade_values[weapon_type]=i2f(GR_FADE_LEVELS-1);
	}

	if (old_weapon[weapon_type] == -1)
	{
		d1_draw_weapon_info(weapon_type,weapon_num);
		old_weapon[weapon_type] = weapon_num;
		weapon_box_states[weapon_type] = WS_SET;
	}

	if (weapon_box_states[weapon_type] == WS_FADING_OUT) {
		d1_draw_weapon_info(weapon_type,old_weapon[weapon_type]);
		weapon_box_fade_values[weapon_type] -= FrameTime * FADE_SCALE;
		if (weapon_box_fade_values[weapon_type] <= 0)
		{
			weapon_box_states[weapon_type] = WS_FADING_IN;
			old_weapon[weapon_type] = weapon_num;
			old_weapon[weapon_type] = weapon_num;
			weapon_box_fade_values[weapon_type] = 0;
		}
	}
	else if (weapon_box_states[weapon_type] == WS_FADING_IN)
	{
		if (weapon_num != old_weapon[weapon_type])
		{
			weapon_box_states[weapon_type] = WS_FADING_OUT;
		}
		else
		{
			d1_draw_weapon_info(weapon_type,weapon_num);
			weapon_box_fade_values[weapon_type] += FrameTime * FADE_SCALE;
			if (weapon_box_fade_values[weapon_type] >= i2f(GR_FADE_LEVELS-1)) {
				weapon_box_states[weapon_type] = WS_SET;
				old_weapon[weapon_type] = -1;
			}
		}
	}
	else
	{
		d1_draw_weapon_info(weapon_type, weapon_num);
		old_weapon[weapon_type] = weapon_num;
	}


	if (weapon_box_states[weapon_type] != WS_SET)         //fade gauge
	{
		int fade_value = f2i(weapon_box_fade_values[weapon_type]);
		int boxofs = (PlayerCfg.CurrentCockpitMode==CM_STATUS_BAR)?SB_PRIMARY_BOX:COCKPIT_PRIMARY_BOX;

		gr_settransblend(fade_value, GR_BLEND_NORMAL);
		gr_rect(HUD_SCALE_X(gauge_boxes[boxofs+weapon_type].left),HUD_SCALE_Y(gauge_boxes[boxofs+weapon_type].top),HUD_SCALE_X(gauge_boxes[boxofs+weapon_type].right),HUD_SCALE_Y(gauge_boxes[boxofs+weapon_type].bot));

		gr_settransblend(GR_FADE_OFF, GR_BLEND_NORMAL);
	}

	gr_set_current_canvas(NULL);

}

static void d1_draw_weapon_boxes()
{
	int pnum = get_pnum_for_hud();

	d1_draw_weapon_box(0,Players[pnum].primary_weapon);

	if (weapon_box_states[0] == WS_SET)
		if (Players[pnum].primary_weapon == VULCAN_INDEX)
		{
			if (Newdemo_state == ND_STATE_RECORDING)
				newdemo_record_primary_ammo(Players[pnum].primary_ammo[Players[pnum].primary_weapon]);
			d1_draw_primary_ammo_info(f2i(VULCAN_AMMO_SCALE * Players[pnum].primary_ammo[Players[pnum].primary_weapon]));
		}

	d1_draw_weapon_box(1,Players[pnum].secondary_weapon);

	if (weapon_box_states[1] == WS_SET)
		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_secondary_ammo(Players[pnum].secondary_ammo[Players[pnum].secondary_weapon]);

	d1_draw_secondary_ammo_info(Players[pnum].secondary_ammo[Players[pnum].secondary_weapon]);

	if(PlayerCfg.HudMode!=0)
	{
		d1_draw_primary_ammo_info(f2i(VULCAN_AMMO_SCALE * Players[pnum].primary_ammo[Players[pnum].primary_weapon]));
		d1_draw_secondary_ammo_info(Players[pnum].secondary_ammo[Players[pnum].secondary_weapon]);
	}
}

static void d1_sb_draw_energy_bar(int energy)
{
	int erase_height;
	int ew, eh, eaw;

	PIGGY_PAGE_IN(Gauges[SB_GAUGE_ENERGY]);
	d1_hud_bitblt( HUD_SCALE_X(SB_ENERGY_GAUGE_X), HUD_SCALE_Y(SB_ENERGY_GAUGE_Y), &GameBitmaps[Gauges[SB_GAUGE_ENERGY].index]);

	erase_height = HUD_SCALE_Y((100 - energy) * SB_ENERGY_GAUGE_H / 100);

	if (erase_height > 0) {
		gr_setcolor( 0 );
		gr_urect(HUD_SCALE_X(SB_ENERGY_GAUGE_X), HUD_SCALE_Y(SB_ENERGY_GAUGE_Y), HUD_SCALE_X(SB_ENERGY_GAUGE_X) + HUD_SCALE_X(SB_ENERGY_GAUGE_W) - 1, HUD_SCALE_Y(SB_ENERGY_GAUGE_Y) + erase_height - 1);
	}

	gr_set_current_canvas( NULL );

	//draw numbers
	gr_set_fontcolor(BM_XRGB(25,18,6),-1 );
	gr_get_string_size((energy>199)?"200":(energy>99)?"100":(energy>9)?"00":"0",&ew,&eh,&eaw);
	gr_printf((grd_curscreen->sc_w/3)-(ew/2),HUD_SCALE_Y(SB_ENERGY_NUM_Y),"%d",energy);
}

static void d1_sb_draw_shield_num(int shield)
{
	int sw, sh, saw;

	//draw numbers
	gr_set_curfont( GAME_FONT );
	gr_set_fontcolor(BM_XRGB(14,14,23),-1 );

	// The status-bar background is drawn afresh before the gauges
	gr_get_string_size((shield>199)?"200":(shield>99)?"100":(shield>9)?"00":"0",&sw,&sh,&saw);
	gr_printf((grd_curscreen->sc_w/2.267)-(sw/2),HUD_SCALE_Y(SB_SHIELD_NUM_Y),"%d",shield);
}

static void d1_sb_draw_shield_bar(int shield)
{
	int bm_num = shield>=100?9:(shield / 10);

	gr_set_current_canvas(NULL);

	PIGGY_PAGE_IN( Gauges[GAUGE_SHIELDS+9-bm_num] );
	d1_hud_bitblt( HUD_SCALE_X(SB_SHIELD_GAUGE_X), HUD_SCALE_Y(SB_SHIELD_GAUGE_Y), &GameBitmaps[Gauges[GAUGE_SHIELDS+9-bm_num].index]);
}

static void d1_sb_draw_keys()
{
	int pnum = get_pnum_for_hud();

	grs_bitmap * bm;
	int flags = Players[pnum].flags;

	gr_set_current_canvas(NULL);
	bm = &GameBitmaps[Gauges[(flags&PLAYER_FLAGS_BLUE_KEY)?SB_GAUGE_BLUE_KEY:SB_GAUGE_BLUE_KEY_OFF].index];
	PIGGY_PAGE_IN(Gauges[(flags&PLAYER_FLAGS_BLUE_KEY)?SB_GAUGE_BLUE_KEY:SB_GAUGE_BLUE_KEY_OFF]);
	d1_hud_bitblt( HUD_SCALE_X(SB_GAUGE_KEYS_X), HUD_SCALE_Y(SB_GAUGE_BLUE_KEY_Y), bm);
	bm = &GameBitmaps[Gauges[(flags&PLAYER_FLAGS_GOLD_KEY)?SB_GAUGE_GOLD_KEY:SB_GAUGE_GOLD_KEY_OFF].index];
	PIGGY_PAGE_IN(Gauges[(flags&PLAYER_FLAGS_GOLD_KEY)?SB_GAUGE_GOLD_KEY:SB_GAUGE_GOLD_KEY_OFF]);
	d1_hud_bitblt( HUD_SCALE_X(SB_GAUGE_KEYS_X), HUD_SCALE_Y(SB_GAUGE_GOLD_KEY_Y), bm);
	bm = &GameBitmaps[Gauges[(flags&PLAYER_FLAGS_RED_KEY)?SB_GAUGE_RED_KEY:SB_GAUGE_RED_KEY_OFF].index];
	PIGGY_PAGE_IN(Gauges[(flags&PLAYER_FLAGS_RED_KEY)?SB_GAUGE_RED_KEY:SB_GAUGE_RED_KEY_OFF]);
	d1_hud_bitblt( HUD_SCALE_X(SB_GAUGE_KEYS_X), HUD_SCALE_Y(SB_GAUGE_RED_KEY_Y), bm);
}

static void d1_draw_invulnerable_ship()
{
	static fix time=0;

	int pnum = get_pnum_for_hud();

	gr_set_current_canvas(NULL);

	if (Players[pnum].invulnerable_time+INVULNERABLE_TIME_MAX-GameTime64 > 0 && (Players[pnum].invulnerable_time+INVULNERABLE_TIME_MAX-GameTime64 > F1_0*4 || GameTime64 & 0x8000))
	{

		if (PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR)	{
			PIGGY_PAGE_IN(Gauges[GAUGE_INVULNERABLE+invulnerable_frame]);
			d1_hud_bitblt( HUD_SCALE_X(SB_SHIELD_GAUGE_X), HUD_SCALE_Y(SB_SHIELD_GAUGE_Y), &GameBitmaps[Gauges[GAUGE_INVULNERABLE+invulnerable_frame].index]);
		} else {
			PIGGY_PAGE_IN(Gauges[GAUGE_INVULNERABLE+invulnerable_frame]);
			d1_hud_bitblt( HUD_SCALE_X(SHIELD_GAUGE_X), HUD_SCALE_Y(SHIELD_GAUGE_Y), &GameBitmaps[Gauges[GAUGE_INVULNERABLE+invulnerable_frame].index]);
		}

		time += FrameTime;

		while (time > INV_FRAME_TIME) {
			time -= INV_FRAME_TIME;
			if (++invulnerable_frame == N_INVULNERABLE_FRAMES)
				invulnerable_frame=0;
		}
	} else if (PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR)
		d1_sb_draw_shield_bar(f2ir(Players[pnum].shields));
	else
		d1_draw_shield_bar(f2ir(Players[pnum].shields));
}

int d1_in_d2_render_gauges(int *added_score, fix *added_time)
{
	if (!d1_in_d2_has_native_assets())
		return 0;
	int pnum = get_pnum_for_hud();

	int energy = f2ir(Players[pnum].energy);
	int shields = f2ir(Players[pnum].shields);
	int cloak = ((Players[pnum].flags&PLAYER_FLAGS_CLOAKED) != 0);

	Assert(PlayerCfg.CurrentCockpitMode==CM_FULL_COCKPIT || PlayerCfg.CurrentCockpitMode==CM_STATUS_BAR);

	if (shields < 0 ) shields = 0;

	gr_set_current_canvas(NULL);
	gr_set_curfont( GAME_FONT );

	d1_draw_weapon_boxes();

	if (PlayerCfg.CurrentCockpitMode == CM_FULL_COCKPIT) {

		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_player_energy(energy);
		d1_draw_energy_bar(energy);
		d1_draw_numerical_display(shields, energy);
		if (!PlayerCfg.HudMode)
			d1_show_bomb_count(HUD_SCALE_X(BOMB_COUNT_X), HUD_SCALE_Y(BOMB_COUNT_Y), gr_find_closest_color(0, 0, 0), 0, 0);
		d1_draw_player_ship(cloak, SHIP_GAUGE_X, SHIP_GAUGE_Y);

		if (Players[pnum].flags & PLAYER_FLAGS_INVULNERABLE)
			d1_draw_invulnerable_ship();
		else
			d1_draw_shield_bar(shields);
		d1_draw_numerical_display(shields, energy);
		if (Newdemo_state == ND_STATE_RECORDING)
		{
			newdemo_record_player_shields(shields);
			newdemo_record_player_flags(Players[pnum].flags);
		}
		d1_draw_keys();

		d1_show_homing_warning();
		d1_draw_wbu_overlay();

	} else if (PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR) {

		if (Newdemo_state == ND_STATE_RECORDING)
			newdemo_record_player_energy(energy);
		d1_sb_draw_energy_bar(energy);
		if (!PlayerCfg.HudMode)
			d1_show_bomb_count(HUD_SCALE_X(SB_BOMB_COUNT_X), HUD_SCALE_Y(SB_BOMB_COUNT_Y), gr_find_closest_color(0, 0, 0), 0, 0);

		d1_draw_player_ship(cloak, SB_SHIP_GAUGE_X, SB_SHIP_GAUGE_Y);

		if (Players[pnum].flags & PLAYER_FLAGS_INVULNERABLE)
			d1_draw_invulnerable_ship();
		else
			d1_sb_draw_shield_bar(shields);
		d1_sb_draw_shield_num(shields);

		if (Newdemo_state==ND_STATE_RECORDING)
		{
			newdemo_record_player_shields(shields);
			newdemo_record_player_flags(Players[pnum].flags);
		}
		d1_sb_draw_keys();

		d1_sb_show_lives();

		if ((Game_mode&GM_MULTI) && !((Game_mode & GM_MULTI_COOP) || (Game_mode & GM_MULTI_ROBOTS)))
		{
			d1_sb_show_score();
		}
		else
		{
			d1_sb_show_score();
			d1_sb_show_score_added(added_score, added_time);
		}
	} else
		d1_draw_player_ship(cloak, SB_SHIP_GAUGE_X, SB_SHIP_GAUGE_Y);
	return 1;
}

void d1_in_d2_close_cockpit(void)
{
	int i;
	for (i = 0; i < 2; ++i) {
		if (WinBoxOverlay[i])
			gr_free_sub_bitmap(WinBoxOverlay[i]);
		WinBoxOverlay[i] = NULL;
	}
#ifdef OGL
	ogl_freebmtexture(&deccpt);
#endif
	memset(&deccpt, 0, sizeof(deccpt));
	Decoded_source = NULL;
	Decoded_width = Decoded_height = 0;
}

int d1_in_d2_cockpit_window_canvas(int window, grs_canvas *canvas)
{
	const gauge_box *box;
	int first;
	if (!d1_in_d2_has_native_assets())
		return 0;
	if (PlayerCfg.CurrentCockpitMode == CM_FULL_COCKPIT)
		first = COCKPIT_PRIMARY_BOX;
	else if (PlayerCfg.CurrentCockpitMode == CM_STATUS_BAR)
		first = SB_PRIMARY_BOX;
	else
		return 0;
	box = &gauge_boxes[first + window];
	gr_init_sub_canvas(canvas, &grd_curscreen->sc_canvas,
		HUD_SCALE_X(box->left), HUD_SCALE_Y(box->top),
		HUD_SCALE_X(box->right - box->left + 1), HUD_SCALE_Y(box->bot - box->top + 1));
	return 1;
}

int d1_in_d2_draw_cockpit_window_overlay(void)
{
	if (!d1_in_d2_has_native_assets())
		return 0;
	d1_draw_wbu_overlay();
	return 1;
}

void d1_in_d2_reset_cockpit(void)
{
	d1_in_d2_close_cockpit();
	old_weapon[0] = old_weapon[1] = -1;
	memset(weapon_box_states, 0, sizeof(weapon_box_states));
	memset(weapon_box_fade_values, 0, sizeof(weapon_box_fade_values));
	invulnerable_frame = 0;
}

int d1_in_d2_init_cockpit(void)
{
	int mode = PlayerCfg.CurrentCockpitMode;
	grs_bitmap *bm;
	int x1, y1, x2, y2;
	if (!d1_in_d2_has_native_assets())
		return 0;
	gr_set_current_canvas(NULL);
#ifndef OGL
	if (Game_screen_mode != (D1_COCKPIT_HIRES ? SM(640,480) : SM(320,200)) && mode != CM_LETTERBOX)
		PlayerCfg.CurrentCockpitMode = mode = CM_FULL_SCREEN;
#endif
	switch (mode) {
		case CM_FULL_COCKPIT:
			game_init_render_sub_buffers(0, 0, SWIDTH, SHEIGHT * 2 / 3);
			break;
		case CM_STATUS_BAR:
			game_init_render_sub_buffers(0, 0, SWIDTH, D1_COCKPIT_HIRES ? SHEIGHT * 2 / 2.6 : SHEIGHT * 2 / 2.72);
			break;
		case CM_REAR_VIEW:
			PIGGY_PAGE_IN(cockpit_bitmap[mode]);
			bm = &GameBitmaps[cockpit_bitmap[mode].index];
			x1 = y1 = 0;
			x2 = bm->bm_w - 1;
			y2 = bm->bm_h - 1;
			gr_bitblt_find_transparent_area(bm, &x1, &y1, &x2, &y2);
			game_init_render_sub_buffers(x1 * ((float)SWIDTH / bm->bm_w), y1 * ((float)SHEIGHT / bm->bm_h),
				(x2 - x1 + 1) * ((float)SWIDTH / bm->bm_w), (y2 - y1 + 2) * ((float)SHEIGHT / bm->bm_h));
			break;
		default:
			return 0; /* Shared fullscreen and letterbox paths */
	}
	gr_set_current_canvas(NULL);
	return 1;
}

int d1_in_d2_draw_cockpit(void)
{
	int mode = PlayerCfg.CurrentCockpitMode;
	grs_bitmap *bm;
	if (!d1_in_d2_has_native_assets())
		return 0;
	if (mode != CM_FULL_COCKPIT && mode != CM_REAR_VIEW && mode != CM_STATUS_BAR)
		return 1;
	PIGGY_PAGE_IN(cockpit_bitmap[mode]);
	bm = &GameBitmaps[cockpit_bitmap[mode].index];
	gr_set_current_canvas(NULL);
#ifdef OGL
	if (mode == CM_STATUS_BAR)
		ogl_ubitmapm_cs(0, D1_COCKPIT_HIRES ? SHEIGHT * 2 / 2.6 : SHEIGHT * 2 / 2.72,
			-1, HUD_SCALE_Y(bm->bm_h), bm, 255, F1_0);
	else
		ogl_ubitmapm_cs(0, 0, -1, SHEIGHT, bm, 255, F1_0);
#else
	gr_ubitmapm(0, mode == CM_STATUS_BAR ? SHEIGHT - bm->bm_h : 0, bm);
#endif
	return 1;
}
