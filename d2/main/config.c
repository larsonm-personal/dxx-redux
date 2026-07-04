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
 * contains routine(s) to read in the configuration file which contains
 * game configuration stuff like detail level, sound card, etc
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "pstypes.h"
#include "game.h"
#include "songs.h"
#include "kconfig.h"
#include "palette.h"
#include "args.h"
#include "player.h"
#include "mission.h"
#include "physfsx.h"
#ifdef ANDROID
#include "playsave.h"
#include "coop_save.h"
#include "render.h"
#ifdef __ANDROID__
#include "auto_net.h"
#endif
#endif

struct Cfg GameCfg;

static const char DigiVolumeStr[] = "DigiVolume";
static const char MusicVolumeStr[] = "MusicVolume";
static const char ReverseStereoStr[] = "ReverseStereo";
static const char OrigTrackOrderStr[] = "OrigTrackOrder";
static const char MusicTypeStr[] = "MusicType";
static const char CMLevelMusicPlayOrderStr[] = "CMLevelMusicPlayOrder";
static const char CMLevelMusicTrack0Str[] = "CMLevelMusicTrack0";
static const char CMLevelMusicTrack1Str[] = "CMLevelMusicTrack1";
static const char CMLevelMusicPathStr[] = "CMLevelMusicPath";
static const char CMMiscMusic0Str[] = "CMMiscMusic0";
static const char CMMiscMusic1Str[] = "CMMiscMusic1";
static const char CMMiscMusic2Str[] ="CMMiscMusic2";
static const char CMMiscMusic3Str[] = "CMMiscMusic3";
static const char CMMiscMusic4Str[] = "CMMiscMusic4";
static const char GammaLevelStr[] = "GammaLevel";
static const char LastPlayerStr[] = "LastPlayer";
static const char LastMissionStr[] = "LastMission";
static const char ResolutionXStr[] ="ResolutionX";
static const char ResolutionYStr[] ="ResolutionY";
static const char AspectXStr[] ="AspectX";
static const char AspectYStr[] ="AspectY";
static const char WindowModeStr[] ="WindowMode";
static const char TexFiltStr[] ="TexFilt";
static const char MovieTexFiltStr[] ="MovieTexFilt";
static const char MenuTexFiltStr[] ="MenuTexFilt";
static const char HudTexFiltStr[] ="HudTexFilt";
static const char MainViewFovStr[] ="MainViewFov";
static const char CornerTextInsetStr[] ="CornerTextInset";
static const char MovieSubtitlesStr[] ="MovieSubtitles";
static const char VSyncStr[] ="VSync";
static const char MultisampleStr[] ="Multisample";
static const char AnisoLevelStr[] ="AnisoLevel";
static const char MsaaLevelStr[] ="MsaaLevel";
static const char ClassicDepthStr[] ="ClassicDepth";
static const char FPSIndicatorStr[] ="FPSIndicator";
static const char GrabinputStr[] ="GrabInput";
static const char BorderlessWindowStr[] ="BorderlessWindow";
static const char ColorDepthStr[] ="ColorDepth";

#ifdef ANDROID
/*
 * Apply initial settings for Android when the config file doesn't exist yet
 * (first launch).  Settings that live in descent.cfg (like aspect ratio) are
 * handled by SetupActivity before the engine starts.  This function handles
 * settings stored elsewhere (binary .plr files, engine globals, etc.) that
 * can't easily be written from the Kotlin layer.
 *
 * Add any future Android-specific first-run defaults here that require
 * engine internals.
 */
static void android_apply_initial_defaults(void)
{
	/* Default to joystick control for the touch overlay.
	 * ControlType is stored in per-player .plr files (binary), so it
	 * can't be set from the Kotlin layer. */
	PlayerCfg.ControlType = CONTROL_USING_JOYSTICK;

	/* Enable free-flight automap so that pinch-to-thrust on the touch
	 * screen translates through the level instead of just zooming. */
	PlayerCfg.AutomapFreeFlight = 1;

	/* If GOG disc image files are present, default to CD audio */
	if (PHYSFSX_exists("descent_ii.gog", 1) && PHYSFSX_exists("descent_ii.inst", 1))
	{
		GameCfg.MusicType = MUSIC_TYPE_REDBOOK;
		GameCfg.OrigTrackOrder = 1;
	}
}

static const char *android_saved_last_player(void)
{
	if (!GameCfg.LastPlayer[0] || !strcmp(GameCfg.LastPlayer, COOP_AUTOSAVE_CALLSIGN))
		return "";
	return GameCfg.LastPlayer;
}

static int android_should_keep_saved_last_player(const char *last_player)
{
	if (!last_player[0] || !strcmp(last_player, COOP_AUTOSAVE_CALLSIGN))
		return 1;
#ifdef __ANDROID__
	if (auto_net_is_transient_callsign(last_player))
		return 1;
#endif
	return 0;
}
#endif

int ReadConfigFile()
{
	PHYSFS_file *infile;
	char *line, *token, *value, *ptr;

	// set defaults
	GameCfg.DigiVolume = 8;
	GameCfg.MusicVolume = 8;
	GameCfg.ReverseStereo = 0;
	GameCfg.OrigTrackOrder = 0;
#if defined(__APPLE__) && defined(__MACH__)
	GameCfg.MusicType = MUSIC_TYPE_REDBOOK;
#else
	GameCfg.MusicType = MUSIC_TYPE_BUILTIN;
#endif
	GameCfg.CMLevelMusicPlayOrder = MUSIC_CM_PLAYORDER_CONT;
	GameCfg.CMLevelMusicTrack[0] = -1;
	GameCfg.CMLevelMusicTrack[1] = -1;
	memset(GameCfg.CMLevelMusicPath,0,PATH_MAX+1);
	memset(GameCfg.CMMiscMusic[SONG_TITLE],0,PATH_MAX+1);
	memset(GameCfg.CMMiscMusic[SONG_BRIEFING],0,PATH_MAX+1);
	memset(GameCfg.CMMiscMusic[SONG_ENDLEVEL],0,PATH_MAX+1);
	memset(GameCfg.CMMiscMusic[SONG_ENDGAME],0,PATH_MAX+1);
	memset(GameCfg.CMMiscMusic[SONG_CREDITS],0,PATH_MAX+1);
#if defined(__APPLE__) && defined(__MACH__)
	GameCfg.OrigTrackOrder = 1;
	snprintf(GameCfg.CMLevelMusicPath,				PATH_MAX, "%s", "descent2.m3u");
	snprintf(GameCfg.CMMiscMusic[SONG_TITLE],		PATH_MAX, "%s%s", PHYSFS_getUserDir(), "Music/iTunes/iTunes Music/Redbook Soundtrack/Descent II, Macintosh CD-ROM/02 Title.mp3");
	snprintf(GameCfg.CMMiscMusic[SONG_BRIEFING],	PATH_MAX, "%s%s", PHYSFS_getUserDir(), "Music/iTunes/iTunes Music/Insanity/Descent/03 Outerlimits.mp3");
	snprintf(GameCfg.CMMiscMusic[SONG_ENDLEVEL],	PATH_MAX, "%s%s", PHYSFS_getUserDir(), "Music/iTunes/iTunes Music/Insanity/Descent/04 Close Call.mp3");
	snprintf(GameCfg.CMMiscMusic[SONG_ENDGAME],		PATH_MAX, "%s%s", PHYSFS_getUserDir(), "Music/iTunes/iTunes Music/Insanity/Descent/14 Insanity.mp3");
	snprintf(GameCfg.CMMiscMusic[SONG_CREDITS],		PATH_MAX, "%s%s", PHYSFS_getUserDir(), "Music/iTunes/iTunes Music/Redbook Soundtrack/Descent II, Macintosh CD-ROM/03 Crawl.mp3");
#endif
	GameCfg.GammaLevel = 0;
	memset(GameCfg.LastPlayer,0,CALLSIGN_LEN+1);
	memset(GameCfg.LastMission,0,MISSION_NAME_LEN+1);
	GameCfg.ResolutionX = 640;
	GameCfg.ResolutionY = 480;
	GameCfg.AspectX = 3;
	GameCfg.AspectY = 4;
	GameCfg.WindowMode = 0;
	GameCfg.TexFilt = 0;
	GameCfg.MovieTexFilt = 0;
	GameCfg.MenuTexFilt = 0;
	GameCfg.HudTexFilt = 1;
	GameCfg.MainViewFov = 0;
	GameCfg.CornerTextInset = 1;
	GameCfg.MovieSubtitles = 0;
	GameCfg.VSync = 0;
	GameCfg.Multisample = 0;
	GameCfg.AnisoLevel = 0;
	GameCfg.MsaaLevel = 0;
	GameCfg.ClassicDepth = 0;
	GameCfg.FPSIndicator = 0;
	GameCfg.Grabinput = 1;
	GameCfg.BorderlessWindow = 0;
	GameCfg.ColorDepth = 0;

	infile = PHYSFSX_openReadBuffered("descent.cfg");

	if (infile == NULL) {
#ifdef ANDROID
		android_apply_initial_defaults();
#endif
		return 1;
	}

	while (!PHYSFS_eof(infile))
	{
		int max_len = PHYSFS_fileLength(infile); // to be fully safe, assume the whole cfg consists of one big line
		CALLOC(line, char, max_len);
		PHYSFSX_gets(infile, line);
		ptr = &(line[0]);
		while (isspace(*ptr))
			ptr++;
		if (*ptr != '\0') {
			token = strtok(ptr, "=");
			value = strtok(NULL, "=");
			if (!value)
				value = "";
			if (!strcmp(token, DigiVolumeStr))
				GameCfg.DigiVolume = strtol(value, NULL, 10);
			else if (!strcmp(token, MusicVolumeStr))
				GameCfg.MusicVolume = strtol(value, NULL, 10);
			else if (!strcmp(token, ReverseStereoStr))
				GameCfg.ReverseStereo = strtol(value, NULL, 10);
			else if (!strcmp(token, OrigTrackOrderStr))
				GameCfg.OrigTrackOrder = strtol(value, NULL, 10);
			else if (!strcmp(token, MusicTypeStr))
				GameCfg.MusicType = strtol(value, NULL, 10);
			else if (!strcmp(token, CMLevelMusicPlayOrderStr))
				GameCfg.CMLevelMusicPlayOrder = strtol(value, NULL, 10);
			else if (!strcmp(token, CMLevelMusicTrack0Str))
				GameCfg.CMLevelMusicTrack[0] = strtol(value, NULL, 10);
			else if (!strcmp(token, CMLevelMusicTrack1Str))
				GameCfg.CMLevelMusicTrack[1] = strtol(value, NULL, 10);
			else if (!strcmp(token, CMLevelMusicPathStr))	{
				char * p;
				strncpy( GameCfg.CMLevelMusicPath, value, PATH_MAX );
				p = strchr( GameCfg.CMLevelMusicPath, '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, CMMiscMusic0Str))	{
				char * p;
				strncpy( GameCfg.CMMiscMusic[SONG_TITLE], value, PATH_MAX );
				p = strchr( GameCfg.CMMiscMusic[SONG_TITLE], '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, CMMiscMusic1Str))	{
				char * p;
				strncpy( GameCfg.CMMiscMusic[SONG_BRIEFING], value, PATH_MAX );
				p = strchr( GameCfg.CMMiscMusic[SONG_BRIEFING], '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, CMMiscMusic2Str))	{
				char * p;
				strncpy( GameCfg.CMMiscMusic[SONG_ENDLEVEL], value, PATH_MAX );
				p = strchr( GameCfg.CMMiscMusic[SONG_ENDLEVEL], '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, CMMiscMusic3Str))	{
				char * p;
				strncpy( GameCfg.CMMiscMusic[SONG_ENDGAME], value, PATH_MAX );
				p = strchr( GameCfg.CMMiscMusic[SONG_ENDGAME], '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, CMMiscMusic4Str))	{
				char * p;
				strncpy( GameCfg.CMMiscMusic[SONG_CREDITS], value, PATH_MAX );
				p = strchr( GameCfg.CMMiscMusic[SONG_CREDITS], '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, GammaLevelStr)) {
				GameCfg.GammaLevel = strtol(value, NULL, 10);
				gr_palette_set_gamma( GameCfg.GammaLevel );
			}
			else if (!strcmp(token, LastPlayerStr))	{
				char * p;
				strncpy( GameCfg.LastPlayer, value, CALLSIGN_LEN );
				p = strchr( GameCfg.LastPlayer, '\n');
				if ( p ) *p = 0;
#ifdef ANDROID
				if (!strcmp(GameCfg.LastPlayer, COOP_AUTOSAVE_CALLSIGN))
					GameCfg.LastPlayer[0] = '\0';
#endif
			}
			else if (!strcmp(token, LastMissionStr))	{
				char * p;
				strncpy( GameCfg.LastMission, value, MISSION_NAME_LEN );
				p = strchr( GameCfg.LastMission, '\n');
				if ( p ) *p = 0;
			}
			else if (!strcmp(token, ResolutionXStr))
				GameCfg.ResolutionX = strtol(value, NULL, 10);
			else if (!strcmp(token, ResolutionYStr))
				GameCfg.ResolutionY = strtol(value, NULL, 10);
			else if (!strcmp(token, AspectXStr))
				GameCfg.AspectX = strtol(value, NULL, 10);
			else if (!strcmp(token, AspectYStr))
				GameCfg.AspectY = strtol(value, NULL, 10);
			else if (!strcmp(token, WindowModeStr))
				GameCfg.WindowMode = strtol(value, NULL, 10);
			else if (!strcmp(token, TexFiltStr))
				GameCfg.TexFilt = strtol(value, NULL, 10);
			else if (!strcmp(token, MovieTexFiltStr))
				GameCfg.MovieTexFilt = strtol(value, NULL, 10);
			else if (!strcmp(token, MenuTexFiltStr))
				GameCfg.MenuTexFilt = strtol(value, NULL, 10);
			else if (!strcmp(token, HudTexFiltStr))
				GameCfg.HudTexFilt = strtol(value, NULL, 10);
			else if (!strcmp(token, MainViewFovStr))
				GameCfg.MainViewFov = strtol(value, NULL, 10);
			else if (!strcmp(token, CornerTextInsetStr))
				GameCfg.CornerTextInset = strtol(value, NULL, 10);
			else if (!strcmp(token, MovieSubtitlesStr))
				GameCfg.MovieSubtitles = strtol(value, NULL, 10);
			else if (!strcmp(token, VSyncStr))
				GameCfg.VSync = strtol(value, NULL, 10);
			else if (!strcmp(token, MultisampleStr))
				GameCfg.Multisample = strtol(value, NULL, 10);
			else if (!strcmp(token, AnisoLevelStr))
				GameCfg.AnisoLevel = strtol(value, NULL, 10);
			else if (!strcmp(token, MsaaLevelStr))
				GameCfg.MsaaLevel = strtol(value, NULL, 10);
			else if (!strcmp(token, ClassicDepthStr))
				GameCfg.ClassicDepth = strtol(value, NULL, 10);
			else if (!strcmp(token, FPSIndicatorStr))
				GameCfg.FPSIndicator = strtol(value, NULL, 10);
			else if (!strcmp(token, GrabinputStr))
				GameCfg.Grabinput = strtol(value, NULL, 10);
			else if (!strcmp(token, BorderlessWindowStr))
				GameCfg.BorderlessWindow = strtol(value, NULL, 10);
			else if (!strcmp(token, ColorDepthStr))
				GameCfg.ColorDepth = strtol(value, NULL, 10);
		}
		d_free(line);
	}

	PHYSFS_close(infile);

	if ( GameCfg.DigiVolume > 8 ) GameCfg.DigiVolume = 8;
	if ( GameCfg.MusicVolume > 8 ) GameCfg.MusicVolume = 8;

	if (GameCfg.ResolutionX >= 320 && GameCfg.ResolutionY >= 200)
		Game_screen_mode = SM(GameCfg.ResolutionX,GameCfg.ResolutionY);

#ifdef ANDROID
	/* android port: sync config graphics values to runtime OGL globals */
	{
		extern int ogl_aniso_level;
		extern int ogl_msaa_samples;
		extern int g_texfilt_level;
		if (GameCfg.TexFilt < 0)
			GameCfg.TexFilt = 0;
		if (GameCfg.TexFilt > 2)
			GameCfg.TexFilt = 2;
		ogl_aniso_level = GameCfg.AnisoLevel;
		ogl_msaa_samples = GameCfg.MsaaLevel;
		g_texfilt_level = GameCfg.TexFilt;
		android_render_set_main_view_fov(GameCfg.MainViewFov);
		GameCfg.MainViewFov = android_render_get_main_view_fov();
	}
#endif

	return 0;
}

int WriteConfigFile()
{
	PHYSFS_file *infile;
	const char *last_player;

	GameCfg.GammaLevel = gr_palette_get_gamma();
	last_player = Players[Player_num].callsign;
#ifdef ANDROID
	if (android_should_keep_saved_last_player(last_player))
		last_player = android_saved_last_player();
#endif

	infile = PHYSFSX_openWriteBuffered("descent.cfg");

	if (infile == NULL) {
		return 1;
	}

	PHYSFSX_printf(infile, "%s=%d\n", DigiVolumeStr, GameCfg.DigiVolume);
	PHYSFSX_printf(infile, "%s=%d\n", MusicVolumeStr, GameCfg.MusicVolume);
	PHYSFSX_printf(infile, "%s=%d\n", ReverseStereoStr, GameCfg.ReverseStereo);
	PHYSFSX_printf(infile, "%s=%d\n", OrigTrackOrderStr, GameCfg.OrigTrackOrder);
	PHYSFSX_printf(infile, "%s=%d\n", MusicTypeStr, GameCfg.MusicType);
	PHYSFSX_printf(infile, "%s=%d\n", CMLevelMusicPlayOrderStr, GameCfg.CMLevelMusicPlayOrder);
	PHYSFSX_printf(infile, "%s=%d\n", CMLevelMusicTrack0Str, GameCfg.CMLevelMusicTrack[0]);
	PHYSFSX_printf(infile, "%s=%d\n", CMLevelMusicTrack1Str, GameCfg.CMLevelMusicTrack[1]);
	PHYSFSX_printf(infile, "%s=%s\n", CMLevelMusicPathStr, GameCfg.CMLevelMusicPath);
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic0Str, GameCfg.CMMiscMusic[SONG_TITLE]);
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic1Str, GameCfg.CMMiscMusic[SONG_BRIEFING]);
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic2Str, GameCfg.CMMiscMusic[SONG_ENDLEVEL]);
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic3Str, GameCfg.CMMiscMusic[SONG_ENDGAME]);
	PHYSFSX_printf(infile, "%s=%s\n", CMMiscMusic4Str, GameCfg.CMMiscMusic[SONG_CREDITS]);
	PHYSFSX_printf(infile, "%s=%d\n", GammaLevelStr, GameCfg.GammaLevel);
	PHYSFSX_printf(infile, "%s=%s\n", LastPlayerStr, last_player);
	PHYSFSX_printf(infile, "%s=%s\n", LastMissionStr, GameCfg.LastMission);
	PHYSFSX_printf(infile, "%s=%i\n", ResolutionXStr, SM_W(Game_screen_mode));
	PHYSFSX_printf(infile, "%s=%i\n", ResolutionYStr, SM_H(Game_screen_mode));
	PHYSFSX_printf(infile, "%s=%i\n", AspectXStr, GameCfg.AspectX);
	PHYSFSX_printf(infile, "%s=%i\n", AspectYStr, GameCfg.AspectY);
	PHYSFSX_printf(infile, "%s=%i\n", WindowModeStr, GameCfg.WindowMode);
	PHYSFSX_printf(infile, "%s=%i\n", TexFiltStr, GameCfg.TexFilt);
	PHYSFSX_printf(infile, "%s=%i\n", MovieTexFiltStr, GameCfg.MovieTexFilt);
	PHYSFSX_printf(infile, "%s=%i\n", MenuTexFiltStr, GameCfg.MenuTexFilt);
	PHYSFSX_printf(infile, "%s=%i\n", HudTexFiltStr, GameCfg.HudTexFilt);
	PHYSFSX_printf(infile, "%s=%i\n", MainViewFovStr, GameCfg.MainViewFov);
	PHYSFSX_printf(infile, "%s=%i\n", CornerTextInsetStr, GameCfg.CornerTextInset);
	PHYSFSX_printf(infile, "%s=%i\n", MovieSubtitlesStr, GameCfg.MovieSubtitles);
	PHYSFSX_printf(infile, "%s=%i\n", VSyncStr, GameCfg.VSync);
	PHYSFSX_printf(infile, "%s=%i\n", MultisampleStr, GameCfg.Multisample);
	PHYSFSX_printf(infile, "%s=%i\n", AnisoLevelStr, GameCfg.AnisoLevel);
	PHYSFSX_printf(infile, "%s=%i\n", MsaaLevelStr, GameCfg.MsaaLevel);
	PHYSFSX_printf(infile, "%s=%i\n", ClassicDepthStr, GameCfg.ClassicDepth);
	PHYSFSX_printf(infile, "%s=%i\n", FPSIndicatorStr, GameCfg.FPSIndicator);
	PHYSFSX_printf(infile, "%s=%i\n", GrabinputStr, GameCfg.Grabinput);
	PHYSFSX_printf(infile, "%s=%i\n", BorderlessWindowStr, GameCfg.BorderlessWindow);
	PHYSFSX_printf(infile, "%s=%i\n", ColorDepthStr, GameCfg.ColorDepth);

	PHYSFS_close(infile);

	return 0;
}
