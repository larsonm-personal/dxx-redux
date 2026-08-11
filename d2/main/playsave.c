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
 * Functions to load & save player's settings (*.plr file)
 *
 */

#include <stdio.h>
#include <string.h>
#if !defined(_MSC_VER) && !defined(macintosh)
#include <unistd.h>
#endif
#include <errno.h>
#include <ctype.h>

#include "dxxerror.h"
#include "strutil.h"
#include "game.h"
#include "gameseq.h"
#include "player.h"
#include "playsave.h"
#include "joy.h"
#include "digi.h"
#include "newmenu.h"
#include "palette.h"
#include "menu.h"
#include "config.h"
#include "text.h"
#include "state.h"
#include "gauges.h"
#include "screens.h"
#include "powerup.h"
#include "makesig.h"
#include "byteswap.h"
#include "u_mem.h"
#include "strio.h"
#include "physfsx.h"
#include "args.h"
#include "vers_id.h"
#include "newdemo.h"
#include "gauges.h"
#ifdef ANDROID
#include "auto_net.h"
#include "coop_save.h"
#include "playsave_layout.h"
#include "songs.h"
#include "songs_android_shared.h"
#endif

//version 5  ->  6: added new highest level information
//version 6  ->  7: stripped out the old saved_game array.
//version 7  ->  8: added reticle flag, & window size
//version 8  ->  9: removed player_struct_version
//version 9  -> 10: added default display mode
//version 10 -> 11: added all toggles in toggle menu
//version 11 -> 12: added weapon ordering
//version 12 -> 13: added more keys
//version 13 -> 14: took out marker key
//version 14 -> 15: added guided in big window
//version 15 -> 16: added small windows in cockpit
//version 16 -> 17: ??
//version 17 -> 18: save guidebot name
//version 18 -> 19: added automap-highres flag
//version 19 -> 20: added kconfig data for windows joysticks
//version 20 -> 21: save seperate config types for DOS & Windows
//version 21 -> 22: save lifetime netstats 
//version 22 -> 23: ??
//version 23 -> 24: add name of joystick for windows version.

#define SAVE_FILE_ID MAKE_SIG('D','P','L','R')
#define PLAYER_FILE_VERSION 24 //increment this every time the player file changes
#define COMPATIBLE_PLAYER_FILE_VERSION 17

struct player_config PlayerCfg;
int get_lifetime_checksum (int a,int b);
extern void InitWeaponOrdering();
void read_observer_setting(int obs_mode, char *line, char *word);
#ifdef ANDROID
extern volatile int g_headlight_off_by_default_qol;
#endif

#ifdef ANDROID
static const char *android_music_plx_source(void)
{
	switch (GameCfg.MusicType) {
		case MUSIC_TYPE_BUILTIN:
			return android_music_get_prefer_mission_soundtrack() ? "mission" : "midi";
		case MUSIC_TYPE_CUSTOM:
			return "files";
		case MUSIC_TYPE_REDBOOK:
		default:
			return "cd";
	}
}

static void android_apply_music_source(const char *source)
{
	if (!d_strnicmp(source, "mission", 7)) {
		GameCfg.MusicType = MUSIC_TYPE_BUILTIN;
		android_music_set_prefer_mission_soundtrack(1);
	} else if (!d_strnicmp(source, "files", 5)) {
		GameCfg.MusicType = MUSIC_TYPE_CUSTOM;
		android_music_set_prefer_mission_soundtrack(0);
	} else if (!d_strnicmp(source, "midi", 4)) {
		GameCfg.MusicType = MUSIC_TYPE_BUILTIN;
		android_music_set_prefer_mission_soundtrack(0);
	} else if (!d_strnicmp(source, "cd", 2)) {
		GameCfg.MusicType = MUSIC_TYPE_REDBOOK;
		android_music_set_prefer_mission_soundtrack(0);
	}
}

static int android_clamp_int(int value, int min_value, int max_value)
{
	if (value < min_value)
		return min_value;
	if (value > max_value)
		return max_value;
	return value;
}
#endif

int new_player_config()
{
	InitWeaponOrdering (); //setup default weapon priorities
	PlayerCfg.ControlType=0; // Assume keyboard
	memcpy(PlayerCfg.KeySettings, DefaultKeySettings, sizeof(DefaultKeySettings));
	memcpy(PlayerCfg.KeySettingsD2X, DefaultKeySettingsD2X, sizeof(DefaultKeySettingsD2X));
	kc_set_controls();

	PlayerCfg.DefaultDifficulty = 1;
	PlayerCfg.AutoLeveling = 1;
	PlayerCfg.NHighestLevels = 1;
	PlayerCfg.HighestLevels[0].Shortname[0] = 0; //no name for mission 0
	PlayerCfg.HighestLevels[0].LevelNum = 1; //was highest level in old struct
	PlayerCfg.KeyboardSens[0] = PlayerCfg.KeyboardSens[1] = PlayerCfg.KeyboardSens[2] = PlayerCfg.KeyboardSens[3] = PlayerCfg.KeyboardSens[4] = 16;
	PlayerCfg.JoystickSens[0] = PlayerCfg.JoystickSens[1] = PlayerCfg.JoystickSens[2] = PlayerCfg.JoystickSens[3] = PlayerCfg.JoystickSens[4] = PlayerCfg.JoystickSens[5] = 8;
	PlayerCfg.JoystickDead[0] = PlayerCfg.JoystickDead[1] = PlayerCfg.JoystickDead[2] = PlayerCfg.JoystickDead[3] = PlayerCfg.JoystickDead[4] = PlayerCfg.JoystickDead[5] = 0;
	PlayerCfg.JoystickUndercalibrate[0] = PlayerCfg.JoystickUndercalibrate[1] = PlayerCfg.JoystickUndercalibrate[2] = PlayerCfg.JoystickUndercalibrate[3] = PlayerCfg.JoystickUndercalibrate[4] = PlayerCfg.JoystickUndercalibrate[5] = 0;
	PlayerCfg.MouseControlStyle = MOUSE_CONTROL_REBIRTH; /* Old School Mouse */
	PlayerCfg.MouseImpulse = 8;
	PlayerCfg.MouseSens[0] = PlayerCfg.MouseSens[1] = PlayerCfg.MouseSens[2] = PlayerCfg.MouseSens[3] = PlayerCfg.MouseSens[4] = PlayerCfg.MouseSens[5] = 8;
    PlayerCfg.MouseOverrun[0] = PlayerCfg.MouseOverrun[1] = PlayerCfg.MouseOverrun[2] = PlayerCfg.MouseOverrun[3] = PlayerCfg.MouseOverrun[4] = PlayerCfg.MouseOverrun[5] = 0;
	PlayerCfg.MouseFSDead = 0;
	PlayerCfg.MouseFSIndicator = 1;
	PlayerCfg.CurrentCockpitMode = PlayerCfg.PreferredCockpitMode = CM_FULL_COCKPIT;
	PlayerCfg.Cockpit3DView[0]=CV_NONE;
	PlayerCfg.Cockpit3DView[1]=CV_NONE;
	PlayerCfg.ReticleType = RET_TYPE_CLASSIC;
	PlayerCfg.ReticleRGBA[0] = RET_COLOR_DEFAULT_R; PlayerCfg.ReticleRGBA[1] = RET_COLOR_DEFAULT_G; PlayerCfg.ReticleRGBA[2] = RET_COLOR_DEFAULT_B; PlayerCfg.ReticleRGBA[3] = RET_COLOR_DEFAULT_A;
	PlayerCfg.ReticleSize = 0;
	PlayerCfg.MissileViewEnabled = 1;
#ifdef ANDROID
	PlayerCfg.HeadlightActiveDefault = g_headlight_off_by_default_qol ? 0 : 1;
#else
	PlayerCfg.HeadlightActiveDefault = 1;
#endif
	PlayerCfg.GuidedInBigWindow = 0;
	strcpy(PlayerCfg.GuidebotName,"GUIDE-BOT");
	strcpy(PlayerCfg.GuidebotNameReal,"GUIDE-BOT");
	PlayerCfg.HudMode = 0;
	PlayerCfg.EscortHotKeys = 1;
	PlayerCfg.PersistentDebris = 0;
	PlayerCfg.PRShot = 0;
	PlayerCfg.DemoRecordingIndicator = 0;
	PlayerCfg.NoRedundancy = 0;
	PlayerCfg.MultiMessages = 0;
	PlayerCfg.NoRankings = 0;
	PlayerCfg.AutomapFreeFlight = 0;
	PlayerCfg.NoFireAutoselect = 0;
	PlayerCfg.CycleAutoselectOnly = 0;
	PlayerCfg.AlphaEffects = 0;
	PlayerCfg.DynLightColor = 0;
	PlayerCfg.DisableCockpit = 0;  /* DisableCockpit */ 
	PlayerCfg.StickyRearview = 0; /* StickyRearview */ 
	PlayerCfg.SelectAfterFire = 1;  /* SelectAfterFire */
	PlayerCfg.VulcanAmmoWarnings = 1; 
	PlayerCfg.ShieldWarnings = 0; 
	PlayerCfg.AutoDemoSp = 0;
	PlayerCfg.AutoDemoMp = 0;
	PlayerCfg.AutoDemoHideUi = 0;
	PlayerCfg.ShowCustomColors = 1;
	PlayerCfg.PreferMyTeamColors = 0;
	PlayerCfg.QuietPlasma = 1; 
	PlayerCfg.maxFps = GameArg.SysMaxFPS; 
	PlayerCfg.ShipColor = 8;
	PlayerCfg.MissileColor = 8;
	PlayerCfg.MyTeamColor = 8;
	PlayerCfg.OtherTeamColor = 8;
	PlayerCfg.ObsShareSettings = 1;
	for (int obs_mode = 0; obs_mode < NUM_OBS_MODES; obs_mode++) {
		PlayerCfg.ObsTurbo[obs_mode] = 0;
		PlayerCfg.ObsShowCockpit[obs_mode] = 1;
		PlayerCfg.ObsShowScoreboardShieldText[obs_mode] = 1;
		PlayerCfg.ObsShowScoreboardShieldBar[obs_mode] = 1;
		PlayerCfg.ObsShowAmmoBars[obs_mode] = 1;
		PlayerCfg.ObsShowPrimary[obs_mode] = 1;
		PlayerCfg.ObsShowSecondary[obs_mode] = 1;
		PlayerCfg.ObsShowNames[obs_mode] = 0;
		PlayerCfg.ObsShowDamage[obs_mode] = 1;
		PlayerCfg.ObsShowShieldText[obs_mode] = 0;
		PlayerCfg.ObsShowShieldBar[obs_mode] = 1;
		PlayerCfg.ObsShowKillFeed[obs_mode] = 1;
		PlayerCfg.ObsShowDeathSummary[obs_mode] = 0;
		PlayerCfg.ObsShowStreaks[obs_mode] = 0;
		PlayerCfg.ObsShowKillGraph[obs_mode] = 0;
		PlayerCfg.ObsShowBreakdown[obs_mode] = 0;
		PlayerCfg.ObsShowObs[obs_mode] = 1;
		PlayerCfg.ObsChat[obs_mode] = 1;
		PlayerCfg.ObsPlayerChat[obs_mode] = 1;
		PlayerCfg.ObsShowBombTimes[obs_mode] = 0;
		PlayerCfg.ObsTransparentThirdPerson[obs_mode] = 0;
		PlayerCfg.ObsIncreaseThirdPersonDist[obs_mode] = 0;
		PlayerCfg.ObsHideEnergyWeaponMuzzle[obs_mode] = 0;
	}
	PlayerCfg.NoChatSound = 0;
	PlayerCfg.ClassicAutoselectWeapon = 0;
	PlayerCfg.ShowRobotHostageCounts = 0;
	PlayerCfg.ShowBossHealthBar = 1;
	PlayerCfg.OriginalHoming = 0;

	// Default taunt macros
	#ifdef NETWORK
	strcpy(PlayerCfg.NetworkMessageMacro[0], "Why can't we all just get along?");
	strcpy(PlayerCfg.NetworkMessageMacro[1], "Hey, I got a present for ya");
	strcpy(PlayerCfg.NetworkMessageMacro[2], "I got a hankerin' for a spankerin'");
	strcpy(PlayerCfg.NetworkMessageMacro[3], "This one's headed for Uranus");
	PlayerCfg.NetlifeKills=0; PlayerCfg.NetlifeKilled=0;
	#endif
	
	return 1;
}

int read_player_d2x(char *filename)
{
	PHYSFS_file *f;
	int rc = 0;
	char line[50],*word;
	int Stop=0;

	f = PHYSFSX_openReadBuffered(filename);

	if(!f || PHYSFS_eof(f))
		return errno;

	while(!Stop && !PHYSFS_eof(f))
	{
		PHYSFSX_fgets(line,50,f);
		word=splitword(line,':');
		d_strupr(word);
		if (strstr(word,"KEYBOARD"))
		{
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);
	
			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				if(!strcmp(word,"SENSITIVITY0"))
					PlayerCfg.KeyboardSens[0] = atoi(line);
				if(!strcmp(word,"SENSITIVITY1"))
					PlayerCfg.KeyboardSens[1] = atoi(line);
				if(!strcmp(word,"SENSITIVITY2"))
					PlayerCfg.KeyboardSens[2] = atoi(line);
				if(!strcmp(word,"SENSITIVITY3"))
					PlayerCfg.KeyboardSens[3] = atoi(line);
				if(!strcmp(word,"SENSITIVITY4"))
					PlayerCfg.KeyboardSens[4] = atoi(line);
				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
		}
#ifdef ANDROID
		else if (strstr(word,"MUSIC"))
		{
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);

			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				if(!strcmp(word,"SOURCE"))
					android_apply_music_source(line);
				if(!strcmp(word,"PREFERMISSION"))
					android_music_set_prefer_mission_soundtrack(atoi(line) ? 1 : 0);
				if(!strcmp(word,"PLAYORDER"))
					GameCfg.CMLevelMusicPlayOrder = android_clamp_int(atoi(line), 0, 2);
				if(!strcmp(word,"VOLUME")) {
					GameCfg.MusicVolume = (ubyte)android_clamp_int(atoi(line), 0, 8);
					songs_set_volume(GameCfg.MusicVolume);
				}
				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
		}
#endif
		else if (strstr(word,"JOYSTICK"))
		{
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);
	
			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				if(!strcmp(word,"SENSITIVITY0"))
					PlayerCfg.JoystickSens[0] = atoi(line);
				if(!strcmp(word,"SENSITIVITY1"))
					PlayerCfg.JoystickSens[1] = atoi(line);
				if(!strcmp(word,"SENSITIVITY2"))
					PlayerCfg.JoystickSens[2] = atoi(line);
				if(!strcmp(word,"SENSITIVITY3"))
					PlayerCfg.JoystickSens[3] = atoi(line);
				if(!strcmp(word,"SENSITIVITY4"))
					PlayerCfg.JoystickSens[4] = atoi(line);
				if(!strcmp(word,"SENSITIVITY5"))
					PlayerCfg.JoystickSens[5] = atoi(line);
				if(!strcmp(word,"DEADZONE0"))
					PlayerCfg.JoystickDead[0] = atoi(line);
				if(!strcmp(word,"DEADZONE1"))
					PlayerCfg.JoystickDead[1] = atoi(line);
				if(!strcmp(word,"DEADZONE2"))
					PlayerCfg.JoystickDead[2] = atoi(line);
				if(!strcmp(word,"DEADZONE3"))
					PlayerCfg.JoystickDead[3] = atoi(line);
				if(!strcmp(word,"DEADZONE4"))
					PlayerCfg.JoystickDead[4] = atoi(line);
				if(!strcmp(word,"DEADZONE5"))
					PlayerCfg.JoystickDead[5] = atoi(line);
				if(!strcmp(word,"UNDERCALIBRATE0"))
					PlayerCfg.JoystickUndercalibrate[0] = atoi(line);
				if(!strcmp(word,"UNDERCALIBRATE1"))
					PlayerCfg.JoystickUndercalibrate[1] = atoi(line);
				if(!strcmp(word,"UNDERCALIBRATE2"))
					PlayerCfg.JoystickUndercalibrate[2] = atoi(line);
				if(!strcmp(word,"UNDERCALIBRATE3"))
					PlayerCfg.JoystickUndercalibrate[3] = atoi(line);
				if(!strcmp(word,"UNDERCALIBRATE4"))
					PlayerCfg.JoystickUndercalibrate[4] = atoi(line);
				if(!strcmp(word,"UNDERCALIBRATE5"))
					PlayerCfg.JoystickUndercalibrate[5] = atoi(line);					
				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
		}
		else if (strstr(word,"MOUSE"))
		{
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);
	
			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				if(!strcmp(word,"FLIGHTSIM"))
					PlayerCfg.MouseControlStyle = atoi(line);  /* Old School Mouse */
				if(!strcmp(word,"MOUSEIMPULSE"))
					PlayerCfg.MouseImpulse = atoi(line);  /* Old School Mouse */
				if(!strcmp(word,"SENSITIVITY0"))
					PlayerCfg.MouseSens[0] = atoi(line);
				if(!strcmp(word,"SENSITIVITY1"))
					PlayerCfg.MouseSens[1] = atoi(line);
				if(!strcmp(word,"SENSITIVITY2"))
					PlayerCfg.MouseSens[2] = atoi(line);
				if(!strcmp(word,"SENSITIVITY3"))
					PlayerCfg.MouseSens[3] = atoi(line);
				if(!strcmp(word,"SENSITIVITY4"))
					PlayerCfg.MouseSens[4] = atoi(line);
				if(!strcmp(word,"SENSITIVITY5"))
					PlayerCfg.MouseSens[5] = atoi(line);
                if(!strcmp(word,"OVERRUN0"))
                    PlayerCfg.MouseOverrun[0] = atoi(line);
                if(!strcmp(word,"OVERRUN1"))
                    PlayerCfg.MouseOverrun[1] = atoi(line);
                if(!strcmp(word,"OVERRUN2"))
                    PlayerCfg.MouseOverrun[2] = atoi(line);
                if(!strcmp(word,"OVERRUN3"))
                    PlayerCfg.MouseOverrun[3] = atoi(line);
                if(!strcmp(word,"OVERRUN4"))
                    PlayerCfg.MouseOverrun[4] = atoi(line);
                if(!strcmp(word,"OVERRUN5"))
                    PlayerCfg.MouseOverrun[5] = atoi(line);
				if(!strcmp(word,"FSDEAD"))
					PlayerCfg.MouseFSDead = atoi(line);
				if(!strcmp(word,"FSINDI"))
					PlayerCfg.MouseFSIndicator = atoi(line);
				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
		}
		else if (strstr(word,"WEAPON KEYS V2"))
		{
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);
			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				unsigned int kc1=0,kc2=0,kc3=0;
				int i=atoi(word);
				
				if(i==0) i=10;
					i=(i-1)*3;
		
				sscanf(line,"0x%x,0x%x,0x%x",&kc1,&kc2,&kc3);
				PlayerCfg.KeySettingsD2X[i]   = kc1;
				PlayerCfg.KeySettingsD2X[i+1] = kc2;
				PlayerCfg.KeySettingsD2X[i+2] = kc3;
				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
		}
		else if (strstr(word,"COCKPIT"))
		{
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);
	
			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				if(!strcmp(word,"HUD"))
					PlayerCfg.HudMode = atoi(line);
				else if(!strcmp(word,"ROBOTHOSTAGECOUNTS"))
					PlayerCfg.ShowRobotHostageCounts = atoi(line) ? 1 : 0;
				else if(!strcmp(word,"BOSSHEALTHBAR"))
					PlayerCfg.ShowBossHealthBar = atoi(line) ? 1 : 0;
				else if(!strcmp(word,"RETTYPE"))
					PlayerCfg.ReticleType = atoi(line);
				else if(!strcmp(word,"RETRGBA"))
					sscanf(line,"%i,%i,%i,%i",&PlayerCfg.ReticleRGBA[0],&PlayerCfg.ReticleRGBA[1],&PlayerCfg.ReticleRGBA[2],&PlayerCfg.ReticleRGBA[3]);
				else if(!strcmp(word,"RETSIZE"))
					PlayerCfg.ReticleSize = atoi(line);
				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
		}
		else if (strstr(word,"TOGGLES"))
		{
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);
	
			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				if(!strcmp(word,"ESCORTHOTKEYS"))
					PlayerCfg.EscortHotKeys = atoi(line);
				if(!strcmp(word,"PERSISTENTDEBRIS"))
					PlayerCfg.PersistentDebris = atoi(line);
				if(!strcmp(word,"PRSHOT"))
					PlayerCfg.PRShot = atoi(line);
				if (!strcmp(word, "DEMORECORDINGINDICATOR"))
					PlayerCfg.DemoRecordingIndicator = atoi(line);
				if(!strcmp(word,"NOREDUNDANCY"))
					PlayerCfg.NoRedundancy = atoi(line);
				if(!strcmp(word,"MULTIMESSAGES"))
					PlayerCfg.MultiMessages = atoi(line);
				if(!strcmp(word,"NORANKINGS"))
					PlayerCfg.NoRankings = atoi(line);
				if(!strcmp(word,"AUTOMAPFREEFLIGHT"))
					PlayerCfg.AutomapFreeFlight = atoi(line);
				if(!strcmp(word,"DISABLECOCKPIT"))
					PlayerCfg.DisableCockpit = atoi(line); /* DisableCockpit */ 
				if(!strcmp(word,"STICKYREARVIEW"))
					PlayerCfg.StickyRearview = atoi(line); /* StickyRearview */ 
				if(!strcmp(word,"SELECTAFTERFIRE"))
					PlayerCfg.SelectAfterFire = atoi(line); /* SelectAfterFire */ 					
				if(!strcmp(word,"NOFIREAUTOSELECT"))
					PlayerCfg.NoFireAutoselect = atoi(line);
				if(!strcmp(word,"CYCLEAUTOSELECTONLY"))
					PlayerCfg.CycleAutoselectOnly = atoi(line);
				if(!strcmp(word,"VULCANAMMOWARNINGS"))
					PlayerCfg.VulcanAmmoWarnings = atoi(line);	
				if(!strcmp(word,"SHIELDWARNINGS"))
					PlayerCfg.ShieldWarnings = atoi(line);	
				if(!strcmp(word,"AUTODEMO"))
					PlayerCfg.AutoDemoMp = atoi(line);
				if(!strcmp(word,"AUTODEMOSP"))
					PlayerCfg.AutoDemoSp = atoi(line);
				if(!strcmp(word,"AUTODEMOMP"))
					PlayerCfg.AutoDemoMp = atoi(line);
				if(!strcmp(word,"AUTODEMOHIDEUI"))
					PlayerCfg.AutoDemoHideUi = atoi(line);
				if(!strcmp(word,"SHOWCUSTOMCOLORS"))
					PlayerCfg.ShowCustomColors = atoi(line);
				if(!strcmp(word,"SHIPCOLOR"))
					PlayerCfg.ShipColor = atoi(line);
				if(!strcmp(word,"MISSILECOLOR"))
					PlayerCfg.MissileColor = atoi(line);
				if (!strcmp(word, "OVERRIDETEAMCOLORS"))
					PlayerCfg.PreferMyTeamColors = atoi(line);
				if (!strcmp(word, "MYTEAMCOLOR"))
					PlayerCfg.MyTeamColor = atoi(line);
				if (!strcmp(word, "OTHERTEAMCOLOR"))
					PlayerCfg.OtherTeamColor = atoi(line);
				//if(!strcmp(word,"QUIETPLASMA"))
				//	PlayerCfg.QuietPlasma = atoi(line);
				if(!strcmp(word,"MAXFPS")) {
					PlayerCfg.maxFps = atoi(line);
					if(PlayerCfg.maxFps < 25) { PlayerCfg.maxFps = 25; }
					if(PlayerCfg.maxFps > 200) { PlayerCfg.maxFps = 200; }
				}
				if(!strcmp(word,"NOCHATSOUND"))
					PlayerCfg.NoChatSound = atoi(line);
				if(!strcmp(word,"CLASSICAUTOSELECTWEAPON"))
					PlayerCfg.ClassicAutoselectWeapon = atoi(line);
				if(!strcmp(word,"ORIGINALHOMING"))
					PlayerCfg.OriginalHoming = atoi(line) ? 1 : 0;

				// Observer settings - migrate from old version
				// If migrating from an older version, set all observer modes to the same value
				read_observer_setting(-1, line, word);

				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
		}
		else if (strstr(word, "OBSERVER"))
		{
			d_free(word);
			PHYSFSX_fgets(line, 50, f);
			word = splitword(line, '=');
			d_strupr(word);

			while (!strstr(word, "END") && !PHYSFS_eof(f))
			{
				if (!strcmp(word, "OBSSHARESETTINGS"))
					PlayerCfg.ObsShareSettings = atoi(line);
				else if (PlayerCfg.ObsShareSettings)
					read_observer_setting(-1, line, word);
				else
					for (int obs_mode = 0; obs_mode < NUM_OBS_MODES; obs_mode++) {
						// Make an upper-case copy of the mode name
						char mode_name_upper[50];
						strcpy(mode_name_upper, Obs_mode_names[obs_mode]);
						d_strupr(mode_name_upper);

						if (strstr(word, mode_name_upper)) {
							d_free(word);
							PHYSFSX_fgets(line, 50, f);
							word = splitword(line, '=');
							d_strupr(word);

							while (!strstr(word, "END") && !PHYSFS_eof(f)) {
								read_observer_setting(obs_mode, line, word);

								d_free(word);
								PHYSFSX_fgets(line, 50, f);
								word = splitword(line, '=');
								d_strupr(word);
							}
						}
					}

				d_free(word);
				PHYSFSX_fgets(line, 50, f);
				word = splitword(line, '=');
				d_strupr(word);
			}
		}
		else if (strstr(word,"GRAPHICS"))
		{
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);
	
			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				if(!strcmp(word,"ALPHAEFFECTS"))
					PlayerCfg.AlphaEffects = atoi(line);
				if(!strcmp(word,"DYNLIGHTCOLOR"))
					PlayerCfg.DynLightColor = atoi(line);
				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
		}
		else if (strstr(word,"PLX VERSION")) // know the version this pilot was used last with - allow modifications
		{
			int v1=0,v2=0,v3=0;
			d_free(word);
			PHYSFSX_fgets(line,50,f);
			word=splitword(line,'=');
			d_strupr(word);
			while(!strstr(word,"END") && !PHYSFS_eof(f))
			{
				sscanf(line,"%i.%i.%i",&v1,&v2,&v3);
				d_free(word);
				PHYSFSX_fgets(line,50,f);
				word=splitword(line,'=');
				d_strupr(word);
			}
			if (v1 == 0 && v2 == 56 && v3 == 0) // was 0.56.0
				if (DXX_VERSION_MAJORi != v1 || DXX_VERSION_MINORi != v2 || DXX_VERSION_MICROi != v3) // newer (presumably)
				{
					// reset joystick/mouse cycling fields
					PlayerCfg.KeySettings[2][28] = 255;
					PlayerCfg.KeySettings[2][29] = 255;
				}
		}
		else if (strstr(word,"END") || PHYSFS_eof(f))
		{
			Stop=1;
		}
		else
		{
			if(word[0]=='['&&!strstr(word,"D2X OPTIONS"))
			{
				while(!strstr(line,"END") && !PHYSFS_eof(f))
				{
					PHYSFSX_fgets(line,50,f);
					d_strupr(line);
				}
			}
		}

		if(word)
			d_free(word);
	}

	PHYSFS_close(f);

	return rc;
}

#define READ_OBS_SETTING(setting_name, setting) \
	if (!strcmp(word, setting_name)) { \
		if (obs_mode != -1) \
			PlayerCfg.setting[obs_mode] = atoi(line); \
		else \
			for (int i = 0; i < NUM_OBS_MODES; i++) \
				PlayerCfg.setting[i] = atoi(line); \
		return; \
	}

void read_observer_setting(int obs_mode, char* line, char* word)
{
	READ_OBS_SETTING("OBSTURBO", ObsTurbo);
	READ_OBS_SETTING("OBSSHOWCOCKPIT", ObsShowCockpit);
	READ_OBS_SETTING("OBSSHOWSCOREBOARDSHIELDTEXT", ObsShowScoreboardShieldText);
	READ_OBS_SETTING("OBSSHOWSCOREBOARDSHIELDBAR", ObsShowScoreboardShieldBar);
	READ_OBS_SETTING("OBSSHOWAMMOBARS", ObsShowAmmoBars);
	READ_OBS_SETTING("OBSSHOWPRIMARY", ObsShowPrimary);
	READ_OBS_SETTING("OBSSHOWSECONDARY", ObsShowSecondary);
	READ_OBS_SETTING("OBSSHOWNAMES", ObsShowNames);
	READ_OBS_SETTING("OBSSHOWDAMAGE", ObsShowDamage);
	READ_OBS_SETTING("OBSSHOWSHIELDTEXT", ObsShowShieldText);
	READ_OBS_SETTING("OBSSHOWSHIELDBAR", ObsShowShieldBar);
	READ_OBS_SETTING("OBSSHOWKILLFEED", ObsShowKillFeed);
	READ_OBS_SETTING("OBSSHOWDEATHSUMMARY", ObsShowDeathSummary);
	READ_OBS_SETTING("OBSSHOWSTREAKS", ObsShowStreaks);
	READ_OBS_SETTING("OBSSHOWKILLGRAPH", ObsShowKillGraph);
	READ_OBS_SETTING("OBSSHOWBREAKDOWN", ObsShowBreakdown);
	READ_OBS_SETTING("OBSSHOWOBS", ObsShowObs);
	READ_OBS_SETTING("OBSCHAT", ObsChat);
	READ_OBS_SETTING("OBSPLAYERCHAT", ObsPlayerChat);
	READ_OBS_SETTING("OBSSHOWBOMBTIMES", ObsShowBombTimes);
	READ_OBS_SETTING("OBSTRANSPARENTTHIRDPERSON", ObsTransparentThirdPerson);
	READ_OBS_SETTING("OBSINCREASETHIRDPERSONDIST", ObsIncreaseThirdPersonDist);
	READ_OBS_SETTING("OBSHIDEENERGYWEAPONMUZZLE", ObsHideEnergyWeaponMuzzle);
}

int write_player_d2x(char *filename)
{
	PHYSFS_file *fout;
	int rc=0;
	char tempfile[PATH_MAX];

	strcpy(tempfile,filename);
	tempfile[strlen(tempfile)-4]=0;
	strcat(tempfile,".pl$");
	fout=PHYSFSX_openWriteBuffered(tempfile);
	
	if (!fout && GameArg.SysUsePlayersDir)
	{
		PHYSFS_mkdir("Players/");	//try making directory
		fout=PHYSFSX_openWriteBuffered(tempfile);
	}
	
	if(fout)
	{
		PHYSFSX_printf(fout,"[D2X OPTIONS]\n");
		PHYSFSX_printf(fout,"[keyboard]\n");
		PHYSFSX_printf(fout,"sensitivity0=%d\n",PlayerCfg.KeyboardSens[0]);
		PHYSFSX_printf(fout,"sensitivity1=%d\n",PlayerCfg.KeyboardSens[1]);
		PHYSFSX_printf(fout,"sensitivity2=%d\n",PlayerCfg.KeyboardSens[2]);
		PHYSFSX_printf(fout,"sensitivity3=%d\n",PlayerCfg.KeyboardSens[3]);
		PHYSFSX_printf(fout,"sensitivity4=%d\n",PlayerCfg.KeyboardSens[4]);
		PHYSFSX_printf(fout,"[end]\n");
		PHYSFSX_printf(fout,"[joystick]\n");
		PHYSFSX_printf(fout,"sensitivity0=%d\n",PlayerCfg.JoystickSens[0]);
		PHYSFSX_printf(fout,"sensitivity1=%d\n",PlayerCfg.JoystickSens[1]);
		PHYSFSX_printf(fout,"sensitivity2=%d\n",PlayerCfg.JoystickSens[2]);
		PHYSFSX_printf(fout,"sensitivity3=%d\n",PlayerCfg.JoystickSens[3]);
		PHYSFSX_printf(fout,"sensitivity4=%d\n",PlayerCfg.JoystickSens[4]);
		PHYSFSX_printf(fout,"sensitivity5=%d\n",PlayerCfg.JoystickSens[5]);
		PHYSFSX_printf(fout,"deadzone0=%d\n",PlayerCfg.JoystickDead[0]);
		PHYSFSX_printf(fout,"deadzone1=%d\n",PlayerCfg.JoystickDead[1]);
		PHYSFSX_printf(fout,"deadzone2=%d\n",PlayerCfg.JoystickDead[2]);
		PHYSFSX_printf(fout,"deadzone3=%d\n",PlayerCfg.JoystickDead[3]);
		PHYSFSX_printf(fout,"deadzone4=%d\n",PlayerCfg.JoystickDead[4]);
		PHYSFSX_printf(fout,"deadzone5=%d\n",PlayerCfg.JoystickDead[5]);
		PHYSFSX_printf(fout,"undercalibrate0=%d\n",PlayerCfg.JoystickUndercalibrate[0]);
		PHYSFSX_printf(fout,"undercalibrate1=%d\n",PlayerCfg.JoystickUndercalibrate[1]);
		PHYSFSX_printf(fout,"undercalibrate2=%d\n",PlayerCfg.JoystickUndercalibrate[2]);
		PHYSFSX_printf(fout,"undercalibrate3=%d\n",PlayerCfg.JoystickUndercalibrate[3]);
		PHYSFSX_printf(fout,"undercalibrate4=%d\n",PlayerCfg.JoystickUndercalibrate[4]);
		PHYSFSX_printf(fout,"undercalibrate5=%d\n",PlayerCfg.JoystickUndercalibrate[5]);			
		PHYSFSX_printf(fout,"[end]\n");
		PHYSFSX_printf(fout,"[mouse]\n");
		PHYSFSX_printf(fout,"flightsim=%d\n",PlayerCfg.MouseControlStyle);  /* Old School Mouse */
		PHYSFSX_printf(fout,"mouseimpulse=%d\n",PlayerCfg.MouseImpulse);  /* Old School Mouse */
		PHYSFSX_printf(fout,"sensitivity0=%d\n",PlayerCfg.MouseSens[0]);
		PHYSFSX_printf(fout,"sensitivity1=%d\n",PlayerCfg.MouseSens[1]);
		PHYSFSX_printf(fout,"sensitivity2=%d\n",PlayerCfg.MouseSens[2]);
		PHYSFSX_printf(fout,"sensitivity3=%d\n",PlayerCfg.MouseSens[3]);
		PHYSFSX_printf(fout,"sensitivity4=%d\n",PlayerCfg.MouseSens[4]);
		PHYSFSX_printf(fout,"sensitivity5=%d\n",PlayerCfg.MouseSens[5]);
        PHYSFSX_printf(fout,"overrun0=%d\n",PlayerCfg.MouseOverrun[0]);
        PHYSFSX_printf(fout,"overrun1=%d\n",PlayerCfg.MouseOverrun[1]);
        PHYSFSX_printf(fout,"overrun2=%d\n",PlayerCfg.MouseOverrun[2]);
        PHYSFSX_printf(fout,"overrun3=%d\n",PlayerCfg.MouseOverrun[3]);
        PHYSFSX_printf(fout,"overrun4=%d\n",PlayerCfg.MouseOverrun[4]);
        PHYSFSX_printf(fout,"overrun5=%d\n",PlayerCfg.MouseOverrun[5]);
		PHYSFSX_printf(fout,"fsdead=%d\n",PlayerCfg.MouseFSDead);
		PHYSFSX_printf(fout,"fsindi=%d\n",PlayerCfg.MouseFSIndicator);
		PHYSFSX_printf(fout,"[end]\n");
		PHYSFSX_printf(fout,"[weapon keys v2]\n");
		PHYSFSX_printf(fout,"1=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[0],PlayerCfg.KeySettingsD2X[1],PlayerCfg.KeySettingsD2X[2]);
		PHYSFSX_printf(fout,"2=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[3],PlayerCfg.KeySettingsD2X[4],PlayerCfg.KeySettingsD2X[5]);
		PHYSFSX_printf(fout,"3=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[6],PlayerCfg.KeySettingsD2X[7],PlayerCfg.KeySettingsD2X[8]);
		PHYSFSX_printf(fout,"4=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[9],PlayerCfg.KeySettingsD2X[10],PlayerCfg.KeySettingsD2X[11]);
		PHYSFSX_printf(fout,"5=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[12],PlayerCfg.KeySettingsD2X[13],PlayerCfg.KeySettingsD2X[14]);
		PHYSFSX_printf(fout,"6=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[15],PlayerCfg.KeySettingsD2X[16],PlayerCfg.KeySettingsD2X[17]);
		PHYSFSX_printf(fout,"7=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[18],PlayerCfg.KeySettingsD2X[19],PlayerCfg.KeySettingsD2X[20]);
		PHYSFSX_printf(fout,"8=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[21],PlayerCfg.KeySettingsD2X[22],PlayerCfg.KeySettingsD2X[23]);
		PHYSFSX_printf(fout,"9=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[24],PlayerCfg.KeySettingsD2X[25],PlayerCfg.KeySettingsD2X[26]);
		PHYSFSX_printf(fout,"0=0x%x,0x%x,0x%x\n",PlayerCfg.KeySettingsD2X[27],PlayerCfg.KeySettingsD2X[28],PlayerCfg.KeySettingsD2X[29]);
		PHYSFSX_printf(fout,"[end]\n");
		PHYSFSX_printf(fout,"[cockpit]\n");
		PHYSFSX_printf(fout,"hud=%i\n",PlayerCfg.HudMode);
		PHYSFSX_printf(fout,"robothostagecounts=%i\n",PlayerCfg.ShowRobotHostageCounts);
		PHYSFSX_printf(fout,"bosshealthbar=%i\n",PlayerCfg.ShowBossHealthBar);
		PHYSFSX_printf(fout,"rettype=%i\n",PlayerCfg.ReticleType);
		PHYSFSX_printf(fout,"retrgba=%i,%i,%i,%i\n",PlayerCfg.ReticleRGBA[0],PlayerCfg.ReticleRGBA[1],PlayerCfg.ReticleRGBA[2],PlayerCfg.ReticleRGBA[3]);
		PHYSFSX_printf(fout,"retsize=%i\n",PlayerCfg.ReticleSize);
		PHYSFSX_printf(fout,"[end]\n");
		PHYSFSX_printf(fout,"[toggles]\n");
		PHYSFSX_printf(fout,"escorthotkeys=%i\n",PlayerCfg.EscortHotKeys);
		PHYSFSX_printf(fout,"persistentdebris=%i\n",PlayerCfg.PersistentDebris);
		PHYSFSX_printf(fout,"prshot=%i\n",PlayerCfg.PRShot);
		PHYSFSX_printf(fout,"demorecordingindicator=%i\n",PlayerCfg.DemoRecordingIndicator);
		PHYSFSX_printf(fout,"noredundancy=%i\n",PlayerCfg.NoRedundancy);
		PHYSFSX_printf(fout,"multimessages=%i\n",PlayerCfg.MultiMessages);
		PHYSFSX_printf(fout,"norankings=%i\n",PlayerCfg.NoRankings);
		PHYSFSX_printf(fout,"automapfreeflight=%i\n",PlayerCfg.AutomapFreeFlight);
		PHYSFSX_printf(fout,"disablecockpit=%i\n",PlayerCfg.DisableCockpit); /* DisableCockpit */ 
		PHYSFSX_printf(fout,"stickyrearview=%i\n",PlayerCfg.StickyRearview); /* StickyRearview */ 
		PHYSFSX_printf(fout,"selectafterfire=%i\n",PlayerCfg.SelectAfterFire); /* SelectAfterFire */ 		
		PHYSFSX_printf(fout,"nofireautoselect=%i\n",PlayerCfg.NoFireAutoselect);
		PHYSFSX_printf(fout,"cycleautoselectonly=%i\n",PlayerCfg.CycleAutoselectOnly);
		PHYSFSX_printf(fout,"vulcanammowarnings=%i\n",PlayerCfg.VulcanAmmoWarnings);		
		PHYSFSX_printf(fout,"shieldwarnings=%i\n",PlayerCfg.ShieldWarnings);
		PHYSFSX_printf(fout,"autodemosp=%i\n",PlayerCfg.AutoDemoSp);
		PHYSFSX_printf(fout,"autodemomp=%i\n",PlayerCfg.AutoDemoMp);
		PHYSFSX_printf(fout,"autodemohideui=%i\n",PlayerCfg.AutoDemoHideUi);
		PHYSFSX_printf(fout,"showcustomcolors=%i\n",PlayerCfg.ShowCustomColors);
		PHYSFSX_printf(fout,"shipcolor=%i\n",PlayerCfg.ShipColor);	
		PHYSFSX_printf(fout,"missilecolor=%i\n",PlayerCfg.MissileColor);
		PHYSFSX_printf(fout, "overrideteamcolors=%i\n", PlayerCfg.PreferMyTeamColors);
		PHYSFSX_printf(fout, "myteamcolor=%i\n", PlayerCfg.MyTeamColor);
		PHYSFSX_printf(fout, "otherteamcolor=%i\n", PlayerCfg.OtherTeamColor);
		//PHYSFSX_printf(fout,"quietplasma=%i\n",PlayerCfg.QuietPlasma);	
		PHYSFSX_printf(fout,"maxfps=%i\n",PlayerCfg.maxFps);	
		PHYSFSX_printf(fout,"nochatsound=%i\n",PlayerCfg.NoChatSound);
		PHYSFSX_printf(fout,"classicautoselectweapon=%i\n",PlayerCfg.ClassicAutoselectWeapon);
		PHYSFSX_printf(fout,"originalhoming=%i\n",PlayerCfg.OriginalHoming);
		PHYSFSX_printf(fout,"[end]\n");
		PHYSFSX_printf(fout, "[observer]\n");
		PHYSFSX_printf(fout, "obssharesettings=%i\n", PlayerCfg.ObsShareSettings);
		if (PlayerCfg.ObsShareSettings) {
			// Write one set of observer settings for all game modes
			PHYSFSX_printf(fout, "obsturbo=%i\n", PlayerCfg.ObsTurbo[0]);
			PHYSFSX_printf(fout, "obsshowcockpit=%i\n", PlayerCfg.ObsShowCockpit[0]);
			PHYSFSX_printf(fout, "obsshowscoreboardshieldtext=%i\n", PlayerCfg.ObsShowScoreboardShieldText[0]);
			PHYSFSX_printf(fout, "obsshowscoreboardshieldbar=%i\n", PlayerCfg.ObsShowScoreboardShieldBar[0]);
			PHYSFSX_printf(fout, "obsshowammobars=%i\n", PlayerCfg.ObsShowAmmoBars[0]);
			PHYSFSX_printf(fout, "obsshowprimary=%i\n", PlayerCfg.ObsShowPrimary[0]);
			PHYSFSX_printf(fout, "obsshowsecondary=%i\n", PlayerCfg.ObsShowSecondary[0]);
			PHYSFSX_printf(fout, "obsshownames=%i\n", PlayerCfg.ObsShowNames[0]);
			PHYSFSX_printf(fout, "obsshowdamage=%i\n", PlayerCfg.ObsShowDamage[0]);
			PHYSFSX_printf(fout, "obsshowshieldtext=%i\n", PlayerCfg.ObsShowShieldText[0]);
			PHYSFSX_printf(fout, "obsshowshieldbar=%i\n", PlayerCfg.ObsShowShieldBar[0]);
			PHYSFSX_printf(fout, "obsshowkillfeed=%i\n", PlayerCfg.ObsShowKillFeed[0]);
			PHYSFSX_printf(fout, "obsshowdeathsummary=%i\n", PlayerCfg.ObsShowDeathSummary[0]);
			PHYSFSX_printf(fout, "obsshowstreaks=%i\n", PlayerCfg.ObsShowStreaks[0]);
			PHYSFSX_printf(fout, "obsshowkillgraph=%i\n", PlayerCfg.ObsShowKillGraph[0]);
			PHYSFSX_printf(fout, "obsshowbreakdown=%i\n", PlayerCfg.ObsShowBreakdown[0]);
			PHYSFSX_printf(fout, "obsshowobs=%i\n", PlayerCfg.ObsShowObs[0]);
			PHYSFSX_printf(fout, "obschat=%i\n", PlayerCfg.ObsChat[0]);
			PHYSFSX_printf(fout, "obsplayerchat=%i\n", PlayerCfg.ObsPlayerChat[0]);
			PHYSFSX_printf(fout, "obsshowbombtimes=%i\n", PlayerCfg.ObsShowBombTimes[0]);
			PHYSFSX_printf(fout, "obstransparentthirdperson=%i\n", PlayerCfg.ObsTransparentThirdPerson[0]);
			PHYSFSX_printf(fout, "obsincreasethirdpersondist=%i\n", PlayerCfg.ObsIncreaseThirdPersonDist[0]);
			PHYSFSX_printf(fout, "obshideenergyweaponmuzzle=%i\n", PlayerCfg.ObsHideEnergyWeaponMuzzle[0]);
		} else {
			// Write separate observer settings for each game mode
			for (int obs_mode = 0; obs_mode < NUM_OBS_MODES; obs_mode++) {
				// Make a lower-case copy of the mode name
				char mode_name_lower[20];
				strcpy(mode_name_lower, Obs_mode_names[obs_mode]);
				d_strlwr(mode_name_lower);

				PHYSFSX_printf(fout, "[%s]\n", mode_name_lower);
				PHYSFSX_printf(fout, "obsturbo=%i\n", PlayerCfg.ObsTurbo[obs_mode]);
				PHYSFSX_printf(fout, "obsshowcockpit=%i\n", PlayerCfg.ObsShowCockpit[obs_mode]);
				PHYSFSX_printf(fout, "obsshowscoreboardshieldtext=%i\n", PlayerCfg.ObsShowScoreboardShieldText[obs_mode]);
				PHYSFSX_printf(fout, "obsshowscoreboardshieldbar=%i\n", PlayerCfg.ObsShowScoreboardShieldBar[obs_mode]);
				PHYSFSX_printf(fout, "obsshowammobars=%i\n", PlayerCfg.ObsShowAmmoBars[obs_mode]);
				PHYSFSX_printf(fout, "obsshowprimary=%i\n", PlayerCfg.ObsShowPrimary[obs_mode]);
				PHYSFSX_printf(fout, "obsshowsecondary=%i\n", PlayerCfg.ObsShowSecondary[obs_mode]);
				PHYSFSX_printf(fout, "obsshownames=%i\n", PlayerCfg.ObsShowNames[obs_mode]);
				PHYSFSX_printf(fout, "obsshowdamage=%i\n", PlayerCfg.ObsShowDamage[obs_mode]);
				PHYSFSX_printf(fout, "obsshowshieldtext=%i\n", PlayerCfg.ObsShowShieldText[obs_mode]);
				PHYSFSX_printf(fout, "obsshowshieldbar=%i\n", PlayerCfg.ObsShowShieldBar[obs_mode]);
				PHYSFSX_printf(fout, "obsshowkillfeed=%i\n", PlayerCfg.ObsShowKillFeed[obs_mode]);
				PHYSFSX_printf(fout, "obsshowdeathsummary=%i\n", PlayerCfg.ObsShowDeathSummary[obs_mode]);
				PHYSFSX_printf(fout, "obsshowstreaks=%i\n", PlayerCfg.ObsShowStreaks[obs_mode]);
				PHYSFSX_printf(fout, "obsshowkillgraph=%i\n", PlayerCfg.ObsShowKillGraph[obs_mode]);
				PHYSFSX_printf(fout, "obsshowbreakdown=%i\n", PlayerCfg.ObsShowBreakdown[obs_mode]);
				PHYSFSX_printf(fout, "obsshowobs=%i\n", PlayerCfg.ObsShowObs[obs_mode]);
				PHYSFSX_printf(fout, "obschat=%i\n", PlayerCfg.ObsChat[obs_mode]);
				PHYSFSX_printf(fout, "obsplayerchat=%i\n", PlayerCfg.ObsPlayerChat[obs_mode]);
				PHYSFSX_printf(fout, "obsshowbombtimes=%i\n", PlayerCfg.ObsShowBombTimes[obs_mode]);
				PHYSFSX_printf(fout, "obstransparentthirdperson=%i\n", PlayerCfg.ObsTransparentThirdPerson[obs_mode]);
				PHYSFSX_printf(fout, "obsincreasethirdpersondist=%i\n", PlayerCfg.ObsIncreaseThirdPersonDist[obs_mode]);
				PHYSFSX_printf(fout, "obshideenergyweaponmuzzle=%i\n", PlayerCfg.ObsHideEnergyWeaponMuzzle[obs_mode]);
				PHYSFSX_printf(fout, "[end]\n");
			}
		}
		PHYSFSX_printf(fout, "[end]\n");
		PHYSFSX_printf(fout,"[graphics]\n");
		PHYSFSX_printf(fout,"alphaeffects=%i\n",PlayerCfg.AlphaEffects);
		PHYSFSX_printf(fout,"dynlightcolor=%i\n",PlayerCfg.DynLightColor);
		PHYSFSX_printf(fout,"[end]\n");
#ifdef ANDROID
		PHYSFSX_printf(fout,"[music]\n");
		PHYSFSX_printf(fout,"source=%s\n", android_music_plx_source());
		PHYSFSX_printf(fout,"prefermission=%i\n", android_music_get_prefer_mission_soundtrack());
		PHYSFSX_printf(fout,"playorder=%i\n", GameCfg.CMLevelMusicPlayOrder);
		PHYSFSX_printf(fout,"volume=%i\n", GameCfg.MusicVolume);
		PHYSFSX_printf(fout,"[end]\n");
#endif
		PHYSFSX_printf(fout,"[plx version]\n");
		PHYSFSX_printf(fout,"plx version=%s\n", VERSION);
		PHYSFSX_printf(fout,"[end]\n");
		PHYSFSX_printf(fout,"[end]\n");

		PHYSFS_close(fout);
		if(rc==0)
		{
			PHYSFS_delete(filename);
			rc = PHYSFSX_rename(tempfile,filename);
		}
		return rc;
	}
	else
		return errno;

}

ubyte control_type_dos,control_type_win;

//read in the player's saved games.  returns errno (0 == no error)
int read_player_file()
{
	char filename[PATH_MAX];
	PHYSFS_file *file;
	int id, i;
	short player_file_version;
	int rewrite_it=0;
	int swap = 0;

	Assert(Player_num>=0 && Player_num<MAX_PLAYERS);

	memset(filename, '\0', PATH_MAX);
	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%.8s.plr" : "%.8s.plr", Players[Player_num].callsign);
	if (!PHYSFSX_exists(filename,0))
		return ENOENT;

	file = PHYSFSX_openReadBuffered(filename);

	if (!file)
		goto read_player_file_failed;

	new_player_config(); // Set defaults!

	PHYSFS_readSLE32(file, &id);

	if (id!=SAVE_FILE_ID) {
		nm_messagebox(TXT_ERROR, 1, TXT_OK, "Invalid player file");
		PHYSFS_close(file);
		return -1;
	}

	player_file_version = PHYSFSX_readShort(file);

	if (player_file_version > 255) // bigendian file?
		swap = 1;

	if (swap)
		player_file_version = SWAPSHORT(player_file_version);

	if (player_file_version<COMPATIBLE_PLAYER_FILE_VERSION) {
		nm_messagebox(TXT_ERROR, 1, TXT_OK, TXT_ERROR_PLR_VERSION);
		PHYSFS_close(file);
		return -1;
	}

	PHYSFS_seek(file,PHYSFS_tell(file)+2*sizeof(short)); //skip Game_window_w,Game_window_h
	PlayerCfg.DefaultDifficulty = PHYSFSX_readByte(file);
	PlayerCfg.AutoLeveling       = PHYSFSX_readByte(file);
	PHYSFS_seek(file,PHYSFS_tell(file)+sizeof(sbyte)); // skip ReticleOn
	{
		int cockpit_mode = PHYSFSX_readByte(file);
		if (!cockpit_mode_is_persistable(cockpit_mode))
			cockpit_mode = CM_FULL_COCKPIT;
		PlayerCfg.CurrentCockpitMode = PlayerCfg.PreferredCockpitMode = cockpit_mode;
	}
	PHYSFS_seek(file,PHYSFS_tell(file)+sizeof(sbyte)); //skip Default_display_mode
	PlayerCfg.MissileViewEnabled      = PHYSFSX_readByte(file);
	PlayerCfg.HeadlightActiveDefault  = PHYSFSX_readByte(file);
#ifdef ANDROID
	if (g_headlight_off_by_default_qol)
		PlayerCfg.HeadlightActiveDefault = 0;
#endif
	PlayerCfg.GuidedInBigWindow      = PHYSFSX_readByte(file);
	if (player_file_version >= 19)
		PHYSFS_seek(file,PHYSFS_tell(file)+sizeof(sbyte)); //skip Automap_always_hires

	//read new highest level info

	PlayerCfg.NHighestLevels = PHYSFSX_readShort(file);
	if (swap)
		PlayerCfg.NHighestLevels = SWAPSHORT(PlayerCfg.NHighestLevels);
	Assert(PlayerCfg.NHighestLevels <= MAX_MISSIONS);

	if (PHYSFS_read(file, PlayerCfg.HighestLevels, sizeof(hli), PlayerCfg.NHighestLevels) != PlayerCfg.NHighestLevels)
		goto read_player_file_failed;

	//read taunt macros
	{
#ifdef NETWORK
		int i,len;

		len = MAX_MESSAGE_LEN;

		for (i = 0; i < 4; i++)
			if (PHYSFS_read(file, PlayerCfg.NetworkMessageMacro[i], len, 1) != 1)
				goto read_player_file_failed;
#else
		char dummy[4][MAX_MESSAGE_LEN];

		PHYSFS_read(file, dummy, MAX_MESSAGE_LEN, 4);
#endif
	}

	//read kconfig data
	{
		ubyte dummy_joy_sens;

		if (PHYSFS_read(file, &PlayerCfg.KeySettings[0], sizeof(PlayerCfg.KeySettings[0]),1)!=1)
			goto read_player_file_failed;
		if (PHYSFS_read(file, &PlayerCfg.KeySettings[1], sizeof(PlayerCfg.KeySettings[1]),1)!=1)
			goto read_player_file_failed;
		PHYSFS_seek( file, PHYSFS_tell(file)+(sizeof(ubyte)*MAX_CONTROLS*3) ); // Skip obsolete Flightstick/Thrustmaster/Gravis map fields
		if (PHYSFS_read(file, &PlayerCfg.KeySettings[2], sizeof(PlayerCfg.KeySettings[2]),1)!=1)
			goto read_player_file_failed;
		PHYSFS_seek( file, PHYSFS_tell(file)+(sizeof(ubyte)*MAX_CONTROLS) ); // Skip obsolete Cyberman map field
		if (player_file_version>=20)
			PHYSFS_seek( file, PHYSFS_tell(file)+(sizeof(ubyte)*MAX_CONTROLS) ); // Skip obsolete Winjoy map field
		if (PHYSFS_read(file, (ubyte *)&control_type_dos, sizeof(ubyte), 1) != 1)
			goto read_player_file_failed;
		else if (player_file_version >= 21 && PHYSFS_read(file, (ubyte *)&control_type_win, sizeof(ubyte), 1) != 1)
			goto read_player_file_failed;
		else if (PHYSFS_read(file, &dummy_joy_sens, sizeof(ubyte), 1) !=1 )
			goto read_player_file_failed;

		PlayerCfg.ControlType = control_type_dos;
	
		for (i=0;i<11;i++)
		{
			PlayerCfg.PrimaryOrder[i] = PHYSFSX_readByte(file);
			PlayerCfg.SecondaryOrder[i] = PHYSFSX_readByte(file);
		}
		if (!weapon_order_is_valid(PlayerCfg.PrimaryOrder, MAX_PRIMARY_WEAPONS + 1, 0) ||
		    !weapon_order_is_valid(PlayerCfg.SecondaryOrder, MAX_SECONDARY_WEAPONS + 1, 1))
			InitWeaponOrdering();

		if (player_file_version>=16)
		{
			PHYSFS_readSLE32(file, &PlayerCfg.Cockpit3DView[0]);
			PHYSFS_readSLE32(file, &PlayerCfg.Cockpit3DView[1]);
			if (swap)
			{
				PlayerCfg.Cockpit3DView[0] = SWAPINT(PlayerCfg.Cockpit3DView[0]);
				PlayerCfg.Cockpit3DView[1] = SWAPINT(PlayerCfg.Cockpit3DView[1]);
			}
		}
	}

	if (player_file_version>=22)
	{
#ifdef NETWORK
		PHYSFS_readSLE32(file, &PlayerCfg.NetlifeKills);
		PHYSFS_readSLE32(file, &PlayerCfg.NetlifeKilled);
		if (swap) {
			PlayerCfg.NetlifeKills = SWAPINT(PlayerCfg.NetlifeKills);
			PlayerCfg.NetlifeKilled = SWAPINT(PlayerCfg.NetlifeKilled);
		}
#else
		{
			int dummy;
			PHYSFS_readSLE32(file, &dummy);
			PHYSFS_readSLE32(file, &dummy);
		}
#endif
	}
#ifdef NETWORK
	else
	{
		PlayerCfg.NetlifeKills=0; PlayerCfg.NetlifeKilled=0;
	}
#endif

	if (player_file_version>=23)
	{
		PHYSFS_readSLE32(file, &i);
		if (swap)
			i = SWAPINT(i);
#ifdef NETWORK
		if (i!=get_lifetime_checksum (PlayerCfg.NetlifeKills,PlayerCfg.NetlifeKilled))
		{
			PlayerCfg.NetlifeKills=0; PlayerCfg.NetlifeKilled=0;
			nm_messagebox(NULL, 1, "Shame on me", "Trying to cheat eh?");
			rewrite_it=1;
		}
#endif
	}

	//read guidebot name
	if (player_file_version >= 18)
		PHYSFSX_readString(file, PlayerCfg.GuidebotName);
	else
		strcpy(PlayerCfg.GuidebotName,"GUIDE-BOT");

	strcpy(PlayerCfg.GuidebotNameReal,PlayerCfg.GuidebotName);

	{
		char buf[128];

		if (player_file_version >= 24) 
			PHYSFSX_readString(file, buf);			// Just read it in fpr DPS.
	}

	if (!PHYSFS_close(file))
		goto read_player_file_failed;

	if (rewrite_it)
		write_player_file();

	filename[strlen(filename) - 4] = 0;
	strcat(filename, ".plx");
	read_player_d2x(filename);
	#ifdef __ANDROID__
	{
		extern int android_reload_live_gamepad_config(void);
		android_reload_live_gamepad_config();
	}
	#endif

	kc_set_controls();

	return EZERO;

 read_player_file_failed:
	nm_messagebox(TXT_ERROR, 1, TXT_OK, "%s\n\n%s", "Error reading PLR file", PHYSFS_getLastError());
	if (file)
		PHYSFS_close(file);

	return -1;
}


#ifdef ANDROID
static int plr_filename_is_selectable(const char *filename)
{
	const char *base, *dot;
	size_t callsign_len;

	if (!filename || !filename[0])
		return 0;
	base = strrchr(filename, '/');
	base = base ? base + 1 : filename;
	dot = strrchr(base, '.');
	if (!dot || dot == base || d_stricmp(dot, ".plr") || strchr(base, '.') != dot)
		return 0;
	callsign_len = (size_t)(dot - base);
	if (callsign_len > CALLSIGN_LEN)
		return 0;
	if (callsign_len == strlen(COOP_AUTOSAVE_CALLSIGN) &&
	    !d_strnicmp(base, COOP_AUTOSAVE_CALLSIGN, (int)callsign_len))
		return 0;
	return 1;
}

int plr_is_selectable(const char *filename)
{
	PHYSFS_file *file;
	PHYSFS_sint64 file_size;
	int id, n_highest;
	short player_file_version;
	int fixed_header = 18;
	int swap = 0;
	long min_size;

	if (!plr_filename_is_selectable(filename) || !PHYSFSX_exists(filename, 0))
		return 0;

	file = PHYSFSX_openReadBuffered(filename);
	if (!file)
		return 0;

	file_size = PHYSFS_fileLength(file);
	if (file_size < 20) {
		PHYSFS_close(file);
		return 0;
	}

	PHYSFS_readSLE32(file, &id);
	player_file_version = PHYSFSX_readShort(file);
	if (player_file_version > 255)
		swap = 1;
	if (swap)
		player_file_version = SWAPSHORT(player_file_version);

	if (id != SAVE_FILE_ID ||
	    player_file_version < COMPATIBLE_PLAYER_FILE_VERSION) {
		PHYSFS_close(file);
		return 0;
	}

	if (player_file_version >= 19)
		fixed_header = 19;
	if (PHYSFS_seek(file, fixed_header) == 0) {
		PHYSFS_close(file);
		return 0;
	}
	n_highest = PHYSFSX_readShort(file);
	if (swap)
		n_highest = SWAPSHORT(n_highest);
	if (n_highest < 0 || n_highest > MAX_MISSIONS) {
		PHYSFS_close(file);
		return 0;
	}

	min_size = fixed_header + 2 + (long)n_highest * sizeof(hli);
	min_size += 4 * MAX_MESSAGE_LEN;
	min_size += sizeof(PlayerCfg.KeySettings[0]);
	min_size += sizeof(PlayerCfg.KeySettings[1]);
	min_size += sizeof(ubyte) * MAX_CONTROLS * 3;
	min_size += sizeof(PlayerCfg.KeySettings[2]);
	min_size += sizeof(ubyte) * MAX_CONTROLS;
	if (player_file_version >= 20)
		min_size += sizeof(ubyte) * MAX_CONTROLS;
	min_size += 1;
	if (player_file_version >= 21)
		min_size += 1;
	min_size += 1;
	min_size += 11 * 2;
	if (player_file_version >= 16)
		min_size += 2 * sizeof(PHYSFS_sint32);
	if (player_file_version >= 22)
		min_size += 2 * sizeof(PHYSFS_sint32);
	if (player_file_version >= 23)
		min_size += sizeof(PHYSFS_sint32);

	PHYSFS_close(file);
	return file_size >= min_size;
}
#endif

//finds entry for this level in table.  if not found, returns ptr to 
//empty entry.  If no empty entries, takes over last one 
int find_hli_entry()
{
	int i;

	for (i=0;i<PlayerCfg.NHighestLevels;i++)
		if (!d_stricmp(PlayerCfg.HighestLevels[i].Shortname, Current_mission_filename))
			break;

	if (i==PlayerCfg.NHighestLevels) { //not found. create entry

		if (i==MAX_MISSIONS)
			i--; //take last entry
		else
			PlayerCfg.NHighestLevels++;

		strcpy(PlayerCfg.HighestLevels[i].Shortname, Current_mission_filename);
		PlayerCfg.HighestLevels[i].LevelNum = 0;
	}

	return i;
}

//set a new highest level for player for this mission
void set_highest_level(int levelnum)
{
	int ret,i;

	if ((ret=read_player_file()) != EZERO)
		if (ret != ENOENT)		//if file doesn't exist, that's ok
			return;

	i = find_hli_entry();

	if (levelnum > PlayerCfg.HighestLevels[i].LevelNum)
		PlayerCfg.HighestLevels[i].LevelNum = levelnum;

	write_player_file();
}

//gets the player's highest level from the file for this mission
int get_highest_level(void)
{
	int i;
	int highest_saturn_level = 0;
	read_player_file();
#ifndef SATURN
	if (strlen(Current_mission_filename)==0 )	{
		for (i=0;i<PlayerCfg.NHighestLevels;i++)
			if (!d_stricmp(PlayerCfg.HighestLevels[i].Shortname, "DESTSAT")) // Destination Saturn.
				highest_saturn_level = PlayerCfg.HighestLevels[i].LevelNum;
	}
#endif
	i = PlayerCfg.HighestLevels[find_hli_entry()].LevelNum;
	if ( highest_saturn_level > i )
		i = highest_saturn_level;
	return i;
}

static int write_player_file_for_callsign(const char *callsign)
{
	char filename[PATH_MAX];
	PHYSFS_file *file;
	int i;

	memset(filename, '\0', PATH_MAX);
	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%.8s.plx" : "%.8s.plx", callsign);
	write_player_d2x(filename);
	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%.8s.plr" : "%.8s.plr", callsign);
	file = PHYSFSX_openWriteBuffered(filename);

	if (!file)
		return -1;

	//Write out player's info
	PHYSFS_writeULE32(file, SAVE_FILE_ID);
	PHYSFS_writeULE16(file, PLAYER_FILE_VERSION);

	
	PHYSFS_seek(file,PHYSFS_tell(file)+2*(sizeof(PHYSFS_uint16))); // skip Game_window_w, Game_window_h
	PHYSFSX_writeU8(file, PlayerCfg.DefaultDifficulty);
	PHYSFSX_writeU8(file, PlayerCfg.AutoLeveling);
	PHYSFSX_writeU8(file, PlayerCfg.ReticleType==RET_TYPE_NONE?0:1);
	PHYSFSX_writeU8(file, PlayerCfg.PreferredCockpitMode);
	PHYSFS_seek(file,PHYSFS_tell(file)+sizeof(PHYSFS_uint8)); // skip Default_display_mode
	PHYSFSX_writeU8(file, PlayerCfg.MissileViewEnabled);
	PHYSFSX_writeU8(file, PlayerCfg.HeadlightActiveDefault);
	PHYSFSX_writeU8(file, PlayerCfg.GuidedInBigWindow);
	PHYSFS_seek(file,PHYSFS_tell(file)+sizeof(PHYSFS_uint8)); // skip Automap_always_hires

	//write higest level info
	PHYSFS_writeULE16(file, PlayerCfg.NHighestLevels);
	if ((PHYSFS_write(file, PlayerCfg.HighestLevels, sizeof(hli), PlayerCfg.NHighestLevels) != PlayerCfg.NHighestLevels))
		goto write_player_file_failed;

#ifdef NETWORK
	if ((PHYSFS_write(file, PlayerCfg.NetworkMessageMacro, MAX_MESSAGE_LEN, 4) != 4))
		goto write_player_file_failed;
#else
	{
		char dummy[4][MAX_MESSAGE_LEN];	// Pull the messages from a hat! ;-)

		if ((PHYSFS_write(file, dummy, MAX_MESSAGE_LEN, 4) != 4))
			goto write_player_file_failed;
	}
#endif

	//write kconfig info
	{

		ubyte old_avg_joy_sensitivity = 8;
		control_type_dos = PlayerCfg.ControlType;

		if (PHYSFS_write(file, PlayerCfg.KeySettings[0], sizeof(PlayerCfg.KeySettings[0]), 1) != 1)
			goto write_player_file_failed;
		if (PHYSFS_write(file, PlayerCfg.KeySettings[1], sizeof(PlayerCfg.KeySettings[1]), 1) != 1)
			goto write_player_file_failed;
		for (i = 0; i < MAX_CONTROLS*3; i++)
			if (PHYSFS_write(file, "0", sizeof(ubyte), 1) != 1) // Skip obsolete Flightstick/Thrustmaster/Gravis map fields
				goto write_player_file_failed;
		if (PHYSFS_write(file, PlayerCfg.KeySettings[2], sizeof(PlayerCfg.KeySettings[2]), 1) != 1)
			goto write_player_file_failed;
		for (i = 0; i < MAX_CONTROLS*2; i++)
			if (PHYSFS_write(file, "0", sizeof(ubyte), 1) != 1) // Skip obsolete Cyberman/Winjoy map fields
				goto write_player_file_failed;
		if (PHYSFS_write(file, &control_type_dos, sizeof(ubyte), 1) != 1)
			goto write_player_file_failed;
		if (PHYSFS_write(file, &control_type_win, sizeof(ubyte), 1) != 1)
			goto write_player_file_failed;
		if (PHYSFS_write(file, &old_avg_joy_sensitivity, sizeof(ubyte), 1) != 1)
			goto write_player_file_failed;

		for (i = 0; i < 11; i++)
		{
			PHYSFS_write(file, &PlayerCfg.PrimaryOrder[i], sizeof(ubyte), 1);
			PHYSFS_write(file, &PlayerCfg.SecondaryOrder[i], sizeof(ubyte), 1);
		}

		PHYSFS_writeULE32(file, PlayerCfg.Cockpit3DView[0]);
		PHYSFS_writeULE32(file, PlayerCfg.Cockpit3DView[1]);

#ifdef NETWORK
		PHYSFS_writeULE32(file, PlayerCfg.NetlifeKills);
		PHYSFS_writeULE32(file, PlayerCfg.NetlifeKilled);
		i=get_lifetime_checksum (PlayerCfg.NetlifeKills,PlayerCfg.NetlifeKilled);
#else
		PHYSFS_writeULE32(file, 0);
		PHYSFS_writeULE32(file, 0);
		i = get_lifetime_checksum (0, 0);
#endif
		PHYSFS_writeULE32(file, i);
	}

	//write guidebot name
	PHYSFSX_writeString(file, PlayerCfg.GuidebotNameReal);

	{
		char buf[128];
		strcpy(buf, "DOS joystick");
		PHYSFSX_writeString(file, buf);		// Write out current joystick for player.
	}

	if (!PHYSFS_close(file))
		goto write_player_file_failed;

	return EZERO;

 write_player_file_failed:
	nm_messagebox(TXT_ERROR, 1, TXT_OK, "%s\n\n%s", TXT_ERROR_WRITING_PLR, PHYSFS_getLastError());
	if (file)
	{
		PHYSFS_close(file);
		PHYSFS_delete(filename);        //delete bogus file
	}

	return -1;
}

#ifdef __ANDROID__
static int android_should_mirror_player_file(const char *callsign)
{
	return auto_net_is_transient_callsign(callsign) &&
	       GameCfg.LastPlayer[0] &&
	       d_stricmp(GameCfg.LastPlayer, callsign) &&
	       d_stricmp(GameCfg.LastPlayer, COOP_AUTOSAVE_CALLSIGN);
}
#endif

//write out player's saved games.  returns errno (0 == no error)
int write_player_file()
{
	const char *callsign = Players[Player_num].callsign;
	int rc;

	if ( Newdemo_state == ND_STATE_PLAYBACK )
		return -1;

	WriteConfigFile();
	rc = write_player_file_for_callsign(callsign);

#ifdef __ANDROID__
	if (rc == EZERO && android_should_mirror_player_file(callsign))
		(void) write_player_file_for_callsign(GameCfg.LastPlayer);
#endif

	return rc;
}

#ifdef ANDROID
#include "playsave_android_shared.h"

/*
 * Patch the KeySettings and control_type in a single .plr file using stdio.
 * This can be called before PhysFS is initialised (e.g. from the launcher JNI).
 * The binary layout mirrors write_player_file() / read_player_file() above
 *
 * Returns 1 on success, 0 on failure.
 */
static int playsave_android_get_layout(FILE *f,
	struct playsave_binary_layout *layout)
{
	return playsave_d2_get_layout(f, (unsigned)SAVE_FILE_ID,
		COMPATIBLE_PLAYER_FILE_VERSION, MAX_MISSIONS, sizeof(hli),
		MAX_CONTROLS, MAX_MESSAGE_LEN, layout);
}

int plr_patch_keysettings(const char *path,
			 const ubyte *kb, int kb_len,
			 const ubyte *joy, int joy_len,
			 const ubyte *mouse, int mouse_len,
			 int control_type)
{
	struct playsave_binary_layout layout;
	FILE *f;
	int result;

	f = fopen(path, "rb");
	if (!f)
		return 0;

	result = playsave_android_get_layout(f, &layout);
	fclose(f);
	if (result)
		result = playsave_android_patch_keysettings_common(path,
			layout.keysettings, layout.control_dos, kb, kb_len, joy, joy_len,
			mouse, mouse_len, control_type);
	return result;
}

/*
 * Read cockpit mode and AutoLeveling from a D2 .plr file.
 * Returns 1 on success, 0 on failure.
 */
int plr_read_cockpit_autolevel(const char *path,
				   int *cockpit_mode,
				   int *auto_leveling,
				   int *headlight_active_default)
{
	unsigned char buf[4];
	unsigned int id;
	int ver;
	FILE *f;

	f = fopen(path, "rb");
	if (!f) return 0;

	if (fread(buf, 1, 4, f) != 4) { fclose(f); return 0; }
	id = (unsigned)buf[0] | ((unsigned)buf[1] << 8) |
	     ((unsigned)buf[2] << 16) | ((unsigned)buf[3] << 24);
	if (id != (unsigned)SAVE_FILE_ID) { fclose(f); return 0; }

	if (fread(buf, 1, 2, f) != 2) { fclose(f); return 0; }
	ver = buf[0] | (buf[1] << 8);
	if (ver < COMPATIBLE_PLAYER_FILE_VERSION) { fclose(f); return 0; }

	if (fseek(f, 11, SEEK_SET) != 0) { fclose(f); return 0; }
	*auto_leveling = fgetc(f);
	if (*auto_leveling == EOF) { fclose(f); return 0; }

	if (fseek(f, 13, SEEK_SET) != 0) { fclose(f); return 0; }
	*cockpit_mode = fgetc(f);
	if (*cockpit_mode == EOF || !cockpit_mode_is_persistable(*cockpit_mode)) { fclose(f); return 0; }

	if (fseek(f, 16, SEEK_SET) != 0) { fclose(f); return 0; }
	*headlight_active_default = fgetc(f);
	if (*headlight_active_default == EOF) { fclose(f); return 0; }

	fclose(f);
	return 1;
}

/*
 * Patch cockpit mode and AutoLeveling in a D2 .plr file.
 * Returns 1 on success, 0 on failure.
 */
int plr_patch_cockpit_autolevel(const char *path,
				    int cockpit_mode,
				    int auto_leveling,
				    int headlight_active_default)
{
	unsigned char buf[4];
	const long offsets[] = {11, 13, 16};
	unsigned char values[3];
	unsigned int id;
	int ver;
	FILE *f;

	if (!cockpit_mode_is_persistable(cockpit_mode))
		return 0;
	f = fopen(path, "rb");
	if (!f) return 0;

	if (fread(buf, 1, 4, f) != 4) { fclose(f); return 0; }
	id = (unsigned)buf[0] | ((unsigned)buf[1] << 8) |
	     ((unsigned)buf[2] << 16) | ((unsigned)buf[3] << 24);
	if (id != (unsigned)SAVE_FILE_ID) { fclose(f); return 0; }

	if (fread(buf, 1, 2, f) != 2) { fclose(f); return 0; }
	ver = buf[0] | (buf[1] << 8);
	if (ver < COMPATIBLE_PLAYER_FILE_VERSION) { fclose(f); return 0; }

	fclose(f);
	values[0] = auto_leveling ? 1 : 0;
	values[1] = (unsigned char)cockpit_mode;
	values[2] = headlight_active_default ? 1 : 0;
	return playsave_android_patch_u8_values(path, offsets, values, 3);
}
/*
 * Compute the file offset past key settings to where weapon ordering starts.
 * Returns offset on success, -1 on failure.  Leaves file position undefined.
 */
static long plr_weapon_order_offset(FILE *f)
{
	struct playsave_binary_layout layout;

	return playsave_android_get_layout(f, &layout) ? layout.weapon_order : -1;
}

/*
 * Read weapon ordering from a D2 .plr file.
 * Fills primary[prim_len] and secondary[sec_len] from interleaved bytes.
 * Returns 1 on success, 0 on failure.
 */
int plr_read_weapon_order(const char *path,
                          ubyte *primary, int prim_len,
                          ubyte *secondary, int sec_len)
{
	if (!primary || prim_len != MAX_PRIMARY_WEAPONS + 1 ||
	    !secondary || sec_len != MAX_SECONDARY_WEAPONS + 1)
		return 0;
	FILE *f = fopen(path, "rb");
	if (!f) return 0;

	long offset = plr_weapon_order_offset(f);
	if (offset < 0) { fclose(f); return 0; }

	fseek(f, offset, SEEK_SET);
	int n = prim_len > sec_len ? prim_len : sec_len;
	for (int i = 0; i < n; i++) {
		int p = fgetc(f);
		int s = fgetc(f);
		if (p == EOF || s == EOF) { fclose(f); return 0; }
		if (i < prim_len) primary[i] = (ubyte)p;
		if (i < sec_len)  secondary[i] = (ubyte)s;
	}

	fclose(f);
	return weapon_order_is_valid(primary, prim_len, 0) &&
	       weapon_order_is_valid(secondary, sec_len, 1);
}

/*
 * Write weapon ordering to a D2 .plr file.
 * Writes interleaved primary[i], secondary[i] bytes.
 * Returns 1 on success, 0 on failure.
 */
int plr_patch_weapon_order(const char *path,
                           const ubyte *primary, int prim_len,
                           const ubyte *secondary, int sec_len)
{
	if (!weapon_order_is_valid(primary, prim_len, 0) ||
	    !weapon_order_is_valid(secondary, sec_len, 1))
		return 0;
	FILE *f = fopen(path, "rb");
	if (!f) return 0;

	long offset = plr_weapon_order_offset(f);
	if (offset < 0) { fclose(f); return 0; }

	fclose(f);
	return playsave_android_patch_weapon_order(path, offset, primary,
		prim_len, secondary, sec_len);
}
#endif /* ANDROID */

int get_lifetime_checksum (int a,int b)
{
  int num;

  // confusing enough to beat amateur disassemblers? Lets hope so

  num=(a<<8 ^ b);
  num^=(a | b);
  num*=num>>2;
  return (num);
}

// read stored values from ngp file to netgame_info
void read_netgame_profile(netgame_info *ng)
{
	char filename[PATH_MAX];

	memset(filename, '\0', PATH_MAX);
	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%.8s.ngp" : "%.8s.ngp", Players[Player_num].callsign);
	if (!PHYSFSX_exists(filename,0))
		return;
	read_netgame_settings_file(filename, ng, 0);
}

// returns 0 if ok or errno if failed
int read_netgame_settings_file(const char *filename, netgame_info *ng, int no_name)
{
	char line[50], *token, *value, *ptr;
	PHYSFS_file *file;

	file = PHYSFSX_openReadBuffered(filename);

	if (!file)
		return errno;

	// NOTE that we do not set any defaults here or even initialize netgame_info. For flexibility, leave that to the function calling this.
	while (!PHYSFS_eof(file))
	{
		memset(line, 0, 50);
		PHYSFSX_gets(file, line);
		ptr = &(line[0]);
		while (isspace(*ptr))
			ptr++;
		if (*ptr != '\0') {
			token = strtok(ptr, "=");
			value = strtok(NULL, "=");
			if (!value)
				value = "";
			if (!strcmp(token, "game_name") && !no_name)
			{
				char * p;
				strncpy( ng->game_name, value, NETGAME_NAME_LEN+1 );
				p = strchr( ng->game_name, '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, "gamemode"))
				ng->gamemode = strtol(value, NULL, 10);
			else if (!strcmp(token, "RefusePlayers"))
				ng->RefusePlayers = strtol(value, NULL, 10);
			else if (!strcmp(token, "difficulty"))
				ng->difficulty = strtol(value, NULL, 10);
			else if (!strcmp(token, "max_numplayers"))
				ng->max_numplayers = strtol(value, NULL, 10);
			else if (!strcmp(token, "max_numobservers"))
				ng->max_numobservers = strtol(value, NULL, 10);
			else if (!strcmp(token, "game_flags"))
				ng->game_flags = strtol(value, NULL, 10);
			else if (!strcmp(token, "AllowedItems"))
				ng->AllowedItems = strtol(value, NULL, 10);
			else if (!strcmp(token, "Allow_marker_view"))
				ng->Allow_marker_view = strtol(value, NULL, 10);
			else if (!strcmp(token, "AlwaysLighting"))
				ng->AlwaysLighting = strtol(value, NULL, 10);
			else if (!strcmp(token, "ShowEnemyNames"))
				ng->ShowEnemyNames = strtol(value, NULL, 10);
			else if (!strcmp(token, "BrightPlayers"))
				ng->BrightPlayers = strtol(value, NULL, 10);
			else if (!strcmp(token, "SpawnStyle"))
				ng->SpawnStyle = strtol(value, NULL, 10);
			else if (!strcmp(token, "NewSpawnAlgorithm"))
				ng->NewSpawnAlgorithm = strtol(value, NULL, 10);
			else if (!strcmp(token, "GaussAmmoStyle"))
				ng->GaussAmmoStyle = strtol(value, NULL, 10);
			else if (!strcmp(token, "KillGoal"))
				ng->KillGoal = strtol(value, NULL, 10);
			else if (!strcmp(token, "PlayTimeAllowed"))
				ng->PlayTimeAllowed = strtol(value, NULL, 10);
			else if (!strcmp(token, "control_invul_time"))
				ng->control_invul_time = strtol(value, NULL, 10);
			else if (!strcmp(token, "PacketsPerSec"))
				ng->PacketsPerSec = strtol(value, NULL, 10);
			else if (!strcmp(token, "ShortPackets"))
				ng->ShortPackets = strtol(value, NULL, 10);
			else if (!strcmp(token, "NoFriendlyFire"))
				ng->NoFriendlyFire = strtol(value, NULL, 10);
			else if (!strcmp(token, "RetroProtocol"))
				ng->RetroProtocol = strtol(value, NULL, 10);
			else if (!strcmp(token, "RespawnConcs"))
				ng->RespawnConcs = strtol(value, NULL, 10);	
			//else if (!strcmp(token, "DarkSmartBlobs"))
			//	ng->DarkSmartBlobs = strtol(value, NULL, 10);
			else if (!strcmp(token, "LowVulcan"))
				ng->LowVulcan = strtol(value, NULL, 10);
			else if (!strcmp(token, "AllowPreferredColors"))
				ng->AllowPreferredColors = strtol(value, NULL, 10);						
			else if (!strcmp(token, "AllowColoredLighting"))
				ng->AllowColoredLighting = strtol(value, NULL, 10);			
			else if (!strcmp(token, "FairColors"))
				ng->FairColors = strtol(value, NULL, 10);	
			else if (!strcmp(token, "BlackAndWhitePyros"))
				ng->BlackAndWhitePyros = strtol(value, NULL, 10);	
			else if (!strcmp(token, "BornWithBurner"))
				ng->BornWithBurner = strtol(value, NULL, 10);	
			else if (!strcmp(token, "OriginalD1Weapons"))
				ng->OriginalD1Weapons = strtol(value, NULL, 10);
			else if (!strcmp(token, "RebalancedWeapons"))
				ng->RebalancedWeapons = strtol(value, NULL, 10);
			else if (!strcmp(token, "PrimaryDupFactor"))
				ng->PrimaryDupFactor = strtol(value, NULL, 10);
			else if (!strcmp(token, "SecondaryDupFactor"))
				ng->SecondaryDupFactor = strtol(value, NULL, 10);
			else if (!strcmp(token, "SecondaryCapFactor"))
				ng->SecondaryCapFactor = strtol(value, NULL, 10);
			else if (!strcmp(token, "FullDeathSpew"))
				ng->FullDeathSpew = strtol(value, NULL, 10);
			else if (!strcmp(token, "PlayerSpewNoExpire"))
				ng->PlayerSpewNoExpire = strtol(value, NULL, 10);
			else if (!strcmp(token, "DuplicateEnergyShields"))
				ng->DuplicateEnergyShields = strtol(value, NULL, 10);
			else if (!strcmp(token, "obs_delay"))
				ng->obs_delay = strtol(value, NULL, 10);
			else if (!strcmp(token, "obs_min"))
				ng->obs_min = strtol(value, NULL, 10);
			else if (!strcmp(token, "HomingUpdateRate"))
				ng->HomingUpdateRate = strtol(value, NULL, 10);
			else if (!strcmp(token, "OriginalHoming"))
				ng->OriginalHoming = strtol(value, NULL, 10) ? 1 : 0;
			else if (!strcmp(token, "RemoteHitSpark"))
				ng->RemoteHitSpark = strtol(value, NULL, 10);
			else if (!strcmp(token, "AllowCustomModelsTextures"))
				ng->AllowCustomModelsTextures = strtol(value, NULL, 10);
			else if (!strcmp(token, "ReducedFlash"))
				ng->ReducedFlash = strtol(value, NULL, 10);
			else if (!strcmp(token, "DisableGaussSplash"))
				ng->DisableGaussSplash = strtol(value, NULL, 10);
#ifdef USE_TRACKER
			else if (!strcmp(token, "Tracker"))
				ng->Tracker = strtol(value, NULL, 10);
#endif
		}
	}

	PHYSFS_close(file);

	return 0;
}

// write values from netgame_info to ngp file
void write_netgame_profile(netgame_info *ng)
{
	char filename[PATH_MAX];

	memset(filename, '\0', PATH_MAX);
	snprintf(filename, PATH_MAX, GameArg.SysUsePlayersDir? "Players/%.8s.ngp" : "%.8s.ngp", Players[Player_num].callsign);
	write_netgame_settings_file(filename, ng, 0);
}

// returns 0 if ok or errno if failed
int write_netgame_settings_file(const char *filename, netgame_info *ng, int no_name)
{
	PHYSFS_file *file;

	file = PHYSFSX_openWriteBuffered(filename);

	if (!file)
		return errno;

	if (!no_name)
		PHYSFSX_printf(file, "game_name=%s\n", ng->game_name);
	PHYSFSX_printf(file, "gamemode=%i\n", ng->gamemode);
	PHYSFSX_printf(file, "RefusePlayers=%i\n", ng->RefusePlayers);
	PHYSFSX_printf(file, "difficulty=%i\n", ng->difficulty);
	PHYSFSX_printf(file, "max_numplayers=%i\n", ng->max_numplayers);
	PHYSFSX_printf(file, "max_numobservers=%i\n", ng->max_numobservers);
	PHYSFSX_printf(file, "game_flags=%i\n", ng->game_flags);
	PHYSFSX_printf(file, "AllowedItems=%i\n", ng->AllowedItems);
	PHYSFSX_printf(file, "Allow_marker_view=%i\n", ng->Allow_marker_view);
	PHYSFSX_printf(file, "AlwaysLighting=%i\n", ng->AlwaysLighting);
	PHYSFSX_printf(file, "ShowEnemyNames=%i\n", ng->ShowEnemyNames);
	PHYSFSX_printf(file, "BrightPlayers=%i\n", ng->BrightPlayers);
	PHYSFSX_printf(file, "SpawnStyle=%i\n", ng->SpawnStyle);
	PHYSFSX_printf(file, "NewSpawnAlgorithm=%i\n", ng->NewSpawnAlgorithm);
	PHYSFSX_printf(file, "GaussAmmoStyle=%i\n", ng->GaussAmmoStyle);
	PHYSFSX_printf(file, "KillGoal=%i\n", ng->KillGoal);
	PHYSFSX_printf(file, "PlayTimeAllowed=%i\n", ng->PlayTimeAllowed);
	PHYSFSX_printf(file, "control_invul_time=%i\n", ng->control_invul_time);
	PHYSFSX_printf(file, "PacketsPerSec=%i\n", ng->PacketsPerSec);
	PHYSFSX_printf(file, "ShortPackets=%i\n", ng->ShortPackets);
	PHYSFSX_printf(file, "NoFriendlyFire=%i\n", ng->NoFriendlyFire);
	PHYSFSX_printf(file, "RetroProtocol=%i\n", ng->RetroProtocol);
	PHYSFSX_printf(file, "RespawnConcs=%i\n", ng->RespawnConcs);
	//PHYSFSX_printf(file, "DarkSmartBlobs=%i\n", ng->DarkSmartBlobs);
	PHYSFSX_printf(file, "LowVulcan=%i\n", ng->LowVulcan);
	PHYSFSX_printf(file, "AllowPreferredColors=%i\n", ng->AllowPreferredColors);
	PHYSFSX_printf(file, "AllowColoredLighting=%i\n", ng->AllowColoredLighting);
	PHYSFSX_printf(file, "FairColors=%i\n", ng->FairColors);
	PHYSFSX_printf(file, "BlackAndWhitePyros=%i\n", ng->BlackAndWhitePyros);
	PHYSFSX_printf(file, "BornWithBurner=%i\n", ng->BornWithBurner);
	PHYSFSX_printf(file, "OriginalD1Weapons=%i\n", ng->OriginalD1Weapons);
	PHYSFSX_printf(file, "RebalancedWeapons=%i\n", ng->RebalancedWeapons);
	PHYSFSX_printf(file, "PrimaryDupFactor=%i\n", ng->PrimaryDupFactor);
	PHYSFSX_printf(file, "SecondaryDupFactor=%i\n", ng->SecondaryDupFactor);
	PHYSFSX_printf(file, "SecondaryCapFactor=%i\n", ng->SecondaryCapFactor);
	PHYSFSX_printf(file, "FullDeathSpew=%i\n", ng->FullDeathSpew);
	PHYSFSX_printf(file, "PlayerSpewNoExpire=%i\n", ng->PlayerSpewNoExpire);
	PHYSFSX_printf(file, "DuplicateEnergyShields=%i\n", ng->DuplicateEnergyShields);
	PHYSFSX_printf(file, "obs_delay=%i\n", ng->obs_delay);
	PHYSFSX_printf(file, "obs_min=%i\n", ng->obs_min);
	PHYSFSX_printf(file, "HomingUpdateRate=%i\n", ng->HomingUpdateRate);
	PHYSFSX_printf(file, "OriginalHoming=%i\n", ng->OriginalHoming);
	PHYSFSX_printf(file, "RemoteHitSpark=%i\n", ng->RemoteHitSpark);
	PHYSFSX_printf(file, "AllowCustomModelsTextures=%i\n", ng->AllowCustomModelsTextures);
	PHYSFSX_printf(file, "ReducedFlash=%i\n", ng->ReducedFlash);
	PHYSFSX_printf(file, "DisableGaussSplash=%i\n", ng->DisableGaussSplash);
	
#ifdef USE_TRACKER
	PHYSFSX_printf(file, "Tracker=%i\n", ng->Tracker);
#else
	PHYSFSX_printf(file, "Tracker=0\n");
#endif
	PHYSFSX_printf(file, "ngp version=%s\n",VERSION);

	PHYSFS_close(file);

	return 0;
}
