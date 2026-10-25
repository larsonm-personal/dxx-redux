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
 * Code to handle multiple missions
 *
 */

#include "input_demo_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "pstypes.h"
#include "strutil.h"
#include "inferno.h"
#include "mission.h"
#include "gameseq.h"
#include "titles.h"
#include "songs.h"
#include "dxxerror.h"
#include "config.h"
#include "newmenu.h"
#include "text.h"
#include "u_mem.h"
#include "ignorecase.h"
#include "mission_descriptor_safe.h"
#ifdef __ANDROID__
#include "physfsx_android_shared.h"
#include "android_mission_assets.h"
#endif
#include "bm.h"
#include "piggy.h"
#include "../../android/XL_compat/mission_label.h"
#include "d1_in_d2/d1_in_d2.h"
#ifdef DXX_XL_RUNTIME
#include "resource_validation.h"
#include "preview.h"
#endif

//values that describe where a mission is located
enum mle_loc
{
	ML_CURDIR = 0,
	ML_MISSIONDIR = 1
};

//mission list entry
typedef struct mle {
	char    *filename;          // filename without extension
	int     builtin_hogsize;    // if it's the built-in mission, used for determining the version
	char    mission_name[MISSION_NAME_LEN+1];
	ubyte   descent_version;    // descent 1 or descent 2?
	ubyte   requires_xl;
	ubyte   anarchy_only_flag;  // if true, mission is anarchy only
	char	*path;				// relative file path
	enum mle_loc	location;           // where the mission is
} mle;

typedef char mission_display_name_buf[MISSION_NAME_LEN + 6];

static int num_missions = -1;

Mission *Current_mission = NULL; // currently loaded mission
static char Current_mission_hog[PATH_MAX];
#ifdef DXX_XL_RUNTIME
static char *Current_mission_descriptor;
const char *mission_descriptor_snapshot(void)
{
	return Current_mission_descriptor;
}
#endif
static unsigned Mission_ignored_extra_secret_origins;

int mission_compatibility_warning_count(void)
{
	return Mission_ignored_extra_secret_origins != 0;
}

int mission_compatibility_warning_text(int warning_index, char *buffer, unsigned buffer_size)
{
	if (!buffer || !buffer_size || warning_index || !Mission_ignored_extra_secret_origins)
		return 0;
	snprintf(buffer, buffer_size, "Used first secret origin; ignored %u extra origin%s",
	         Mission_ignored_extra_secret_origins,
	         Mission_ignored_extra_secret_origins == 1 ? "" : "s");
	return 1;
}

// Allocate the Level_names, Secret_level_names and Secret_level_table arrays
static int allocate_levels(void)
{
	MALLOC(Level_names, d_fname, Last_level);
	if (!Level_names)
		return 0;
	
	if (Last_secret_level)
	{
		N_secret_levels = -Last_secret_level;

		MALLOC(Secret_level_names, d_fname, N_secret_levels);
		if (!Secret_level_names)
			return 0;
		
		MALLOC(Secret_level_table, ubyte, N_secret_levels);
		if (!Secret_level_table)
			return 0;
	}
	
	return 1;
}

//
//  Special versions of mission routines for d1 builtins
//

int load_mission_d1(void)
{
	int i;

	switch (PHYSFSX_fsize("descent.hog"))
	{
		case D1_SHAREWARE_MISSION_HOGSIZE:
		case D1_SHAREWARE_10_MISSION_HOGSIZE:
			N_secret_levels = 0;
	
			Last_level = 7;
			Last_secret_level = 0;
			
			if (!allocate_levels())
			{
				free_mission();
				return 0;
			}
	
			//build level names
			for (i=0;i<Last_level;i++)
				sprintf(Level_names[i], "level%02d.sdl", i+1);

			strcpy(Briefing_text_filename,BIMD1_BRIEFING_FILE);
			strcpy(Ending_text_filename,BIMD1_ENDING_FILE_SHARE);
	
			break;
		case D1_TEST_FLIGHT_MISSION_HOGSIZE:
		case D1_MAC_SHARE_MISSION_HOGSIZE:
		case D1_MAC_SHARE_MISSION_HOGSIZE2:
			N_secret_levels = 0;
	
			Last_level = 3;
			Last_secret_level = 0;
	
			if (!allocate_levels())
			{
				free_mission();
				return 0;
			}
			
			//build level names
			for (i=0;i<Last_level;i++)
				sprintf(Level_names[i], "level%02d.sdl", i+1);

			strcpy(Briefing_text_filename,BIMD1_BRIEFING_FILE);
			strcpy(Ending_text_filename,BIMD1_ENDING_FILE_SHARE);

			break;
		case D1_OEM_MISSION_HOGSIZE:
		case D1_OEM_10_MISSION_HOGSIZE:
			N_secret_levels = 1;
	
			Last_level = 15;
			Last_secret_level = -1;
	
			if (!allocate_levels())
			{
				free_mission();
				return 0;
			}
			
			//build level names
			for (i=0; i < Last_level - 1; i++)
				sprintf(Level_names[i], "level%02d.rdl", i+1);
			sprintf(Level_names[i], "saturn%02d.rdl", i+1);
			for (i=0; i < -Last_secret_level; i++)
				sprintf(Secret_level_names[i], "levels%1d.rdl", i+1);
	
			Secret_level_table[0] = 10;

			strcpy(Briefing_text_filename,BIMD1_BRIEFING_FILE_OEM);
			strcpy(Ending_text_filename,BIMD1_ENDING_FILE_OEM);

			break;
		default:
			Int3(); // fall through
		case D1_MISSION_HOGSIZE:
		case D1_10_MISSION_HOGSIZE:
		case D1_MAC_MISSION_HOGSIZE:
			N_secret_levels = 3;
	
			Last_level = BIMD1_LAST_LEVEL;
			Last_secret_level = BIMD1_LAST_SECRET_LEVEL;
	
			if (!allocate_levels())
			{
				free_mission();
				return 0;
			}

			//build level names
			for (i=0;i<Last_level;i++)
				sprintf(Level_names[i], "level%02d.rdl", i+1);
			for (i=0;i<-Last_secret_level;i++)
				sprintf(Secret_level_names[i], "levels%1d.rdl", i+1);
	
			Secret_level_table[0] = 10;
			Secret_level_table[1] = 21;
			Secret_level_table[2] = 24;

			strcpy(Briefing_text_filename,BIMD1_BRIEFING_FILE);
			strcpy(Ending_text_filename,BIMD1_ENDING_FILE);

			break;
	}

	return 1;
}


//
//  Special versions of mission routines for shareware
//

int load_mission_shareware(void)
{
    strcpy(Current_mission->mission_name, SHAREWARE_MISSION_NAME);
    Current_mission->descent_version = 2;
    Current_mission->anarchy_only_flag = 0;
    
    switch (Current_mission->builtin_hogsize)
	{
		case MAC_SHARE_MISSION_HOGSIZE:
			N_secret_levels = 1;

			Last_level = 4;
			Last_secret_level = -1;

			if (!allocate_levels())
			{
				free_mission();
				return 0;
			}
			
			// mac demo is using the regular hog and rl2 files
			strcpy(Level_names[0],"d2leva-1.rl2");
			strcpy(Level_names[1],"d2leva-2.rl2");
			strcpy(Level_names[2],"d2leva-3.rl2");
			strcpy(Level_names[3],"d2leva-4.rl2");
			strcpy(Secret_level_names[0],"d2leva-s.rl2");
			break;
		default:
			Int3(); // fall through
		case CD_SHAREWARE_MISSION_HOGSIZE:
		case SHAREWARE_MISSION_HOGSIZE:
			N_secret_levels = 0;

			Last_level = 3;
			Last_secret_level = 0;

			if (!allocate_levels())
			{
				free_mission();
				return 0;
			}
			
			strcpy(Level_names[0],"d2leva-1.sl2");
			strcpy(Level_names[1],"d2leva-2.sl2");
			strcpy(Level_names[2],"d2leva-3.sl2");
	}

	return 1;
}


//
//  Special versions of mission routines for Diamond/S3 version
//

int load_mission_oem(void)
{
    strcpy(Current_mission->mission_name, OEM_MISSION_NAME);
    Current_mission->descent_version = 2;
    Current_mission->anarchy_only_flag = 0;
    
	N_secret_levels = 2;

	Last_level = 8;
	Last_secret_level = -2;

	if (!allocate_levels())
	{
		free_mission();
		return 0;
	}
	
	strcpy(Level_names[0],"d2leva-1.rl2");
	strcpy(Level_names[1],"d2leva-2.rl2");
	strcpy(Level_names[2],"d2leva-3.rl2");
	strcpy(Level_names[3],"d2leva-4.rl2");

	strcpy(Secret_level_names[0],"d2leva-s.rl2");

	strcpy(Level_names[4],"d2levb-1.rl2");
	strcpy(Level_names[5],"d2levb-2.rl2");
	strcpy(Level_names[6],"d2levb-3.rl2");
	strcpy(Level_names[7],"d2levb-4.rl2");

	strcpy(Secret_level_names[1],"d2levb-s.rl2");

	Secret_level_table[0] = 1;
	Secret_level_table[1] = 5;

	return 1;
}


//compare a string for a token. returns true if match
int istok(char *buf,char *tok)
{
	return d_strnicmp(buf,tok,strlen(tok)) == 0;

}

//adds a terminating 0 after a string at the first white space
void add_term(char *s)
{
	while (*s && !isspace((unsigned char)*s)) s++;

	*s = 0;		//terminate!
}

//returns ptr to string after '=' & white space, or NULL if no '='
//adds 0 after parm at first white space
char *get_value(char *buf)
{
	return mission_descriptor_value(buf);
}

static void set_mission_text_filename(const char *value, char *destination)
{
	char candidate[FILENAME_LEN];
	if (!mission_descriptor_make_text_filename(value, ".tex", candidate, sizeof(candidate))) return;
	if (!PHYSFSX_exists(candidate, 1)) {
		if (!mission_descriptor_make_text_filename(value, ".txb", candidate, sizeof(candidate)) ||
		    !PHYSFSX_exists(candidate, 1))
			return;
	}
	snprintf(destination, FILENAME_LEN, "%s", candidate);
}

//reads a line, returns ptr to value of passed parm.  returns NULL if none
char *get_parm_value(char *parm,PHYSFS_file *f)
{
	static char buf[80];

	if (!PHYSFSX_fgets(buf,80,f))
		return NULL;

	if (istok(buf,parm))
		return get_value(buf);
	else
		return NULL;
}

int ml_sort_func(mle *e0,mle *e1)
{
	return d_stricmp(e0->mission_name,e1->mission_name);

}

//returns 1 if file read ok, else 0
int read_mission_file(mle *mission, char *filename, int location)
{
	char filename2[PATH_MAX];
	PHYSFS_file *mfile;

	switch (location) {
		case ML_MISSIONDIR:
			strcpy(filename2,MISSION_DIR);
			break;

		default:
			Int3();		//fall through

		case ML_CURDIR:
			strcpy(filename2,"");
			break;
	}
	if (strlen(filename2) + strlen(filename) >= sizeof(filename2)) return 0;
	strcat(filename2,filename);

#ifdef __ANDROID__
	if (!android_mission_assets_discover(filename2)) return 0;
#endif

	mfile = PHYSFSX_openReadBuffered(filename2);

	if (mfile) {
		char *p;
		char temp[PATH_MAX], *ext;

		strcpy(temp,filename);
		p = strrchr(temp, '/');	// get the filename at the end of the path
		if (!p)
			p = temp;
		else p++;
		
		if ((ext = strchr(p, '.')) == NULL)
			return 0;	//missing extension
		// look if it's .mn2 or .msn
		mission->descent_version = (ext[3] == '2') ? 2 : 1;
		*ext = 0;			//kill extension

		mission->path = d_strdup(temp);
		mission->anarchy_only_flag = 0;
		mission->requires_xl = 0;
		mission->filename = mission->path + (p - temp);
		mission->location = location;

		p = get_parm_value("name",mfile);

		if (!p) {		//try enhanced mission
			PHYSFSX_fseek(mfile,0,SEEK_SET);
			p = get_parm_value("xname",mfile);
		}

		if (!p) {       //try super-enhanced mission!
			PHYSFSX_fseek(mfile,0,SEEK_SET);
			p = get_parm_value("zname",mfile);
		}

		if (!p) {       //try extensible-enhanced mission!
			PHYSFSX_fseek(mfile,0,SEEK_SET);
			p = get_parm_value("!name",mfile);
		}

		if (!p && mission->descent_version == 2) {
			char line[PATH_MAX];
			PHYSFSX_fseek(mfile,0,SEEK_SET);
			if (PHYSFSX_fgets(line,sizeof(line),mfile) && strlen(line) < sizeof(line)-1 &&
			    mission_descriptor_key_is(line,"d2x-name") &&
			    mission_descriptor_copy_title(get_value(line),temp,MISSION_NAME_LEN+1))
				p = temp;
		}

		if (p) {
			char *t;
			if ((t=strchr(p,';'))!=NULL)
				*t=0;
			t = p + strlen(p);
			while (t > p && isspace((unsigned char)t[-1])) *--t = 0; // remove trailing whitespace
			if (!*p) {
				PHYSFS_close(mfile);
				d_free(mission->path);
				return 0;
			}
			if (strlen(p) > MISSION_NAME_LEN)
				p[MISSION_NAME_LEN] = 0;
			strncpy(mission->mission_name, p, MISSION_NAME_LEN + 1);
		}
		else {
			PHYSFS_close(mfile);
			d_free(mission->path);
			return 0;
		}

		p = get_parm_value("type",mfile);

		//get mission type
		if (p)
			mission->anarchy_only_flag = istok(p,"anarchy");

		PHYSFS_close(mfile);

		if (mission->descent_version == 2) {
			char source_path[PATH_MAX];
#ifdef __ANDROID__
			if (android_mission_assets_descriptor_source(filename2, source_path, sizeof(source_path)))
				mission->requires_xl = xl_mission_requires_extensions(source_path);
			else
#endif
			if (PHYSFSX_getRealPath(filename2, source_path))
				mission->requires_xl = xl_mission_requires_extensions(source_path);
		}

		return 1;
	}

	return 0;
}

void add_d1_builtin_mission_to_list(mle *mission)
{
    int size;
    
	size = PHYSFSX_fsize("descent.hog");
	if (size == -1)
		return;

#ifdef __ANDROID__
	// Android port: D1-in-D2 should only appear when D1 texture/sound assets are available.
	if (!PHYSFSX_exists(D1_PIGFILE, 1))
		return;
#endif

	switch (size) {
	case D1_TEST_FLIGHT_MISSION_HOGSIZE:
	case D1_SHAREWARE_MISSION_HOGSIZE:
	case D1_SHAREWARE_10_MISSION_HOGSIZE:
	case D1_MAC_SHARE_MISSION_HOGSIZE:
	case D1_MAC_SHARE_MISSION_HOGSIZE2:
		mission->filename = d_strdup(D1_MISSION_FILENAME);
		strcpy(mission->mission_name, D1_SHAREWARE_MISSION_NAME);
		mission->anarchy_only_flag = 0;
		break;
	case D1_OEM_MISSION_HOGSIZE:
	case D1_OEM_10_MISSION_HOGSIZE:
		mission->filename = d_strdup(D1_MISSION_FILENAME);
		strcpy(mission->mission_name, D1_OEM_MISSION_NAME);
		mission->anarchy_only_flag = 0;
		break;
	default:
		//Warning("Unknown D1 hogsize %d\n", size);
		con_printf(CON_URGENT, "Unknown D1 hogsize %d\n", size);
		Int3();
		// fall through
	case D1_MISSION_HOGSIZE:
	case D1_MISSION_HOGSIZE2:
	case D1_10_MISSION_HOGSIZE:
	case D1_MAC_MISSION_HOGSIZE:
		mission->filename = d_strdup(D1_MISSION_FILENAME);
		strcpy(mission->mission_name, D1_MISSION_NAME);
		mission->anarchy_only_flag = 0;
		break;
	}

	mission->descent_version = 1;
	mission->requires_xl = 0;
	mission->anarchy_only_flag = 0;
	mission->builtin_hogsize = 0;
	mission->path = mission->filename;
	num_missions++;
}


void add_builtin_mission_to_list(mle *mission, char *name)
{
    int size = PHYSFSX_fsize("descent2.hog");
    
	if (size == -1)
		size = PHYSFSX_fsize("d2demo.hog");

	switch (size) {
	case CD_SHAREWARE_MISSION_HOGSIZE:
	case SHAREWARE_MISSION_HOGSIZE:
	case MAC_SHARE_MISSION_HOGSIZE:
		mission->filename = d_strdup(SHAREWARE_MISSION_FILENAME);
		strcpy(mission->mission_name,SHAREWARE_MISSION_NAME);
		mission->anarchy_only_flag = 0;
		break;
	case OEM_MISSION_HOGSIZE:
		mission->filename = d_strdup(OEM_MISSION_FILENAME);
		strcpy(mission->mission_name,OEM_MISSION_NAME);
		mission->anarchy_only_flag = 0;
		break;
	default:
		Warning("Unknown hogsize %d, trying %s\n", size, FULL_MISSION_FILENAME ".mn2");
		Int3(); //fall through
	case FULL_MISSION_HOGSIZE:
	case FULL_10_MISSION_HOGSIZE:
	case MAC_FULL_MISSION_HOGSIZE:
		if (!read_mission_file(mission, FULL_MISSION_FILENAME ".mn2", ML_CURDIR))
			Error("Could not find required mission file <%s>", FULL_MISSION_FILENAME ".mn2");
	}

	mission->path = mission->filename;
	strcpy(name, mission->filename);
    mission->builtin_hogsize = size;
	mission->descent_version = 2;
	mission->requires_xl = 0;
	mission->anarchy_only_flag = 0;
	num_missions++;
}


void add_missions_to_list(mle *mission_list, char *path, char *rel_path, int anarchy_mode)
{
	char **find, **i, *ext;

	find = PHYSFS_enumerateFiles(path);

	for (i = find; *i != NULL; i++)
	{
		if (strlen(path) + strlen(*i) + 1 >= PATH_MAX)
			continue;	// path is too long

		strcat(rel_path, *i);
		if (PHYSFS_isDirectory(path))
		{
			strcat(rel_path, "/");
			add_missions_to_list(mission_list, path, rel_path, anarchy_mode);
			*(strrchr(path, '/')) = 0;
		}
		else if ((ext = strrchr(*i, '.')) && (!d_strnicmp(ext, ".msn", 4) || !d_strnicmp(ext, ".mn2", 4)))
			if (read_mission_file(&mission_list[num_missions], rel_path, ML_MISSIONDIR))
			{
				if (anarchy_mode || !mission_list[num_missions].anarchy_only_flag)
				{
					mission_list[num_missions].builtin_hogsize = 0;
					num_missions++;
				}
				else
					d_free(mission_list[num_missions].path);
			}
		
		if (num_missions >= MAX_MISSIONS)
		{
			break;
		}

		(strrchr(path, '/'))[1] = 0;	// chop off the entry
	}

	PHYSFS_freeList(find);
}

/* move <mission_name> to <place> on mission list, increment <place> */
void promote (mle *mission_list, char * mission_name, int * top_place)
{
	int i;
	char name[FILENAME_LEN], * t;
	strcpy(name, mission_name);
	if ((t = strchr(name,'.')) != NULL)
		*t = 0; //kill extension
	for (i = *top_place; i < num_missions; i++)
		if (!d_stricmp(mission_list[i].filename, name)) {
			//swap mission positions
			mle temp;

			temp = mission_list[*top_place];
			mission_list[*top_place] = mission_list[i];
			mission_list[i] = temp;
			++(*top_place);
			break;
		}
}

/* Compare the exact mounted path, including current-directory and nested missions */
int mission_owns_resource_archive(const char *source)
{
	return source && Current_mission && Current_mission_hog[0] && !strcmp(source, Current_mission_hog);
}
#ifdef DXX_XL_RUNTIME
const char *mission_resource_archive(void)
{
	return Current_mission ? Current_mission_hog : "";
}
#endif

void free_mission(void)
{
#ifdef DXX_XL_RUNTIME
	d_free(Current_mission_descriptor);
	xl_preview_clear_mission();
	xl_resource_select_mission(0);
	xl_resource_baseline_clear();
#endif
#ifdef __ANDROID__
	if (Current_mission) android_mission_assets_free_begin();
	physfsx_android_unmount_mission_directory();
#endif
    // May become more complex with the editor
    if (Current_mission)
	{
		if (Current_mission_hog[0])
			PHYSFS_removeFromSearchPath(Current_mission_hog);
		Current_mission_hog[0] = '\0';
		input_demo_assets_forget("mission_hog");

		if (Current_mission->path)
			d_free(Current_mission->path);

		if (Level_names)
			d_free(Level_names);
		if(Secret_level_names)
			d_free(Secret_level_names);
		if(Secret_level_table)
			d_free(Secret_level_table);
		if (Current_mission->alternate_ham_file)
			d_free(Current_mission->alternate_ham_file);
		
        d_free(Current_mission);
#ifdef __ANDROID__
		android_mission_assets_free_end();
#endif
    }
}



//fills in the global list of missions.  Returns the number of missions
//in the list.  If anarchy_mode is set, then also add anarchy-only missions.

mle *build_mission_list(int anarchy_mode)
{
	mle *mission_list;
	int top_place;
    char	builtin_mission_filename[FILENAME_LEN] = "";
	char	search_str[PATH_MAX] = MISSION_DIR;

	//now search for levels on disk

//@@Took out this code because after this routine was called once for
//@@a list of single-player missions, a subsequent call for a list of
//@@anarchy missions would not scan again, and thus would not find the
//@@anarchy-only missions.  If we retain the minimum level of install,
//@@we may want to put the code back in, having it always scan for all
//@@missions, and have the code that uses it sort out the ones it wants.
//@@	if (num_missions != -1) {
//@@		if (Current_mission_num != 0)
//@@			load_mission(0);				//set built-in mission as default
//@@		return num_missions;
//@@	}

	MALLOC(mission_list, mle, MAX_MISSIONS);
	num_missions = 0;
	
	if (PHYSFSX_exists("descent2.hog", 1) || PHYSFSX_exists("d2demo.hog", 1))
		add_builtin_mission_to_list(mission_list + num_missions, builtin_mission_filename);  //read built-in first
	add_d1_builtin_mission_to_list(mission_list + num_missions);
	add_missions_to_list(mission_list, search_str, search_str + strlen(search_str), anarchy_mode);
	
	// move original missions (in story-chronological order)
	// to top of mission list
	top_place = 0;
	promote(mission_list, "descent", &top_place); // original descent 1 mission
	promote(mission_list, builtin_mission_filename, &top_place); // d2 or d2demo
	promote(mission_list, "d2x", &top_place); // vertigo

	if (num_missions > top_place)
		qsort(&mission_list[top_place],
		      num_missions - top_place,
		      sizeof(*mission_list),
 				(int (*)( const void *, const void * ))ml_sort_func);


	if (num_missions > top_place)
		qsort(&mission_list[top_place],
		      num_missions - top_place,
		      sizeof(*mission_list),
		      (int (*)( const void *, const void * ))ml_sort_func);

	return mission_list;
}

void free_mission_list(mle *mission_list)
{
	int i;

	for (i = 0; i < num_missions; i++)
		d_free(mission_list[i].path);
	
	d_free(mission_list);
	num_missions = 0;
}

void init_extra_robot_movie(char *filename);

//values for built-in mission

int load_mission_ham()
{
	void bm_read_extra_robots(const char *fname,int type);
	int extra_robots = 0;
#ifdef DXX_XL_RUNTIME
	if (*xl_resource_base_rejection()) return 0;
	xl_resource_baseline_clear();
#else
	if (Current_mission->enhanced == MISSION_ENHANCED_XL) return 0;
#endif
	bm_reset_mission_compatibility_warnings();
	piggy_load_mission_data();

	if (Current_mission->enhanced == 3 && Current_mission->alternate_ham_file) {
		/*
		 * If an alternate HAM is specified, map a HOG of the same name
		 * (if it exists) so that users can reference a HAM within a
		 * HOG.  This is required to let users reference the D2X.HAM
		 * file provided by Descent II: Vertigo.
		 *
		 * Try both plain NAME and missions/NAME, in that order.
		 */
		d_fname *altham = Current_mission->alternate_ham_file;
		unsigned l = strlen(*altham);
		char althog[PATH_MAX];
		snprintf(althog, sizeof(althog), MISSION_DIR "%.*s.hog", l - 4, *altham);
		char *p = althog + sizeof(MISSION_DIR) - 1;
		int exists = PHYSFSX_exists(p,1);
		if (!exists) {
			p = althog;
			exists = PHYSFSX_exists(p,1);
		}
		if (exists)
			PHYSFSX_contfile_init(p, 0);
		bm_read_extra_robots(*Current_mission->alternate_ham_file, 2);
		if (exists)
			PHYSFSX_contfile_close(p);
		extra_robots = 1;
	} else if (Current_mission->enhanced && Current_mission->enhanced != MISSION_ENHANCED_XL) {
		char t[50];
		snprintf(t,sizeof(t),"%s.ham",Current_mission_filename);
		bm_read_extra_robots(t, Current_mission->enhanced);
		extra_robots = 1;
	}
#ifdef DXX_XL_RUNTIME
	xl_resource_baseline_capture();
#endif
	return extra_robots;
}

typedef struct mission_text_reference {
	char name[FILENAME_LEN];
	int ending;
	struct mission_text_reference *next;
} mission_text_reference;

typedef struct mission_descriptor_stage {
	Mission candidate;
	unsigned ignored_origins;
	mission_text_reference *text;
	int ready;
	int has_companion_hog;
#ifdef DXX_XL_RUNTIME
	char companion_hog[PATH_MAX];
	char *descriptor_data;
#endif
} mission_descriptor_stage;

static void release_descriptor_stage(mission_descriptor_stage *stage)
{
#ifdef DXX_XL_RUNTIME
	d_free(stage->descriptor_data);
#endif
	mission_text_reference *text = stage->text;
	while (text) {
		mission_text_reference *next = text->next;
		d_free(text);
		text = next;
	}
	d_free(stage->candidate.level_names);
	d_free(stage->candidate.secret_level_names);
	d_free(stage->candidate.secret_level_table);
	d_free(stage->candidate.alternate_ham_file);
	stage->text = NULL;
	stage->ready = 0;
}

/* Defer archive-dependent text lookup, preserving directive order and fallback */
static int descriptor_text_reference(const char *value, int ending, Mission *candidate, mission_text_reference **deferred)
{
	mission_text_reference *entry;
	char name[FILENAME_LEN];
	if (!deferred) {
		set_mission_text_filename(value, ending ? candidate->ending_text_filename : candidate->briefing_text_filename);
		return 1;
	}
	if (!mission_descriptor_make_text_filename(value, ".tex", name, sizeof(name))) return 1;
	MALLOC(entry, mission_text_reference, 1);
	if (!entry) return 0;
	snprintf(entry->name, sizeof(entry->name), "%s", name);
	entry->ending = ending;
	entry->next = *deferred;
	*deferred = entry;
	return 1;
}

static char *descriptor_line(char *buffer, size_t capacity, PHYSFS_file *file, const char **cursor)
{
	size_t size = 0;
	if (!cursor) return PHYSFSX_fgets(buffer, capacity, file);
	if (!**cursor) return NULL;
	while (**cursor && **cursor != '\n' && size + 1 < capacity) buffer[size++] = *(*cursor)++;
	if (**cursor == '\n') ++*cursor;
	buffer[size] = '\0';
	return buffer;
}

/* Parse into caller-owned fields; never publish or destroy the active mission */
static int parse_mission_descriptor(PHYSFS_file *mfile, Mission *candidate, unsigned *ignored_origins, mission_text_reference **deferred_text, const char **cursor)
{
	char buf[PATH_MAX], *v;
	int parse_error = 0, descriptor_name_seen = 0;
	while (descriptor_line(buf,sizeof(buf),mfile,cursor)) {
		if (candidate->descent_version == 2 && mission_descriptor_key_is(buf,"d2x-name")) {
			char title[MISSION_NAME_LEN+1];
			if (descriptor_name_seen || strlen(buf) == sizeof(buf)-1 ||
			    !mission_descriptor_copy_title(get_value(buf),title,sizeof(title))) {
				parse_error = 1;
				break;
			}
			descriptor_name_seen = 1;
			candidate->enhanced = MISSION_ENHANCED_XL;
			continue;
		}
		char *header = buf;
		while (*header && isspace((unsigned char)*header)) ++header;
		if (istok(header,"name") || istok(header,"xname") || istok(header,"zname") || istok(header,"!name")) {
			if (candidate->enhanced == MISSION_ENHANCED_XL) {
				parse_error = 1;
				break;
			}
			descriptor_name_seen = 1;
		}
		/* Alternate archives and critical directives need separate XL qualification */
		if (candidate->enhanced == MISSION_ENHANCED_XL &&
		    (mission_descriptor_key_is(buf,"hog") || *header == '!')) {
			parse_error = 1;
			break;
		}
		if (istok(buf,"name") && !candidate->enhanced) {
			candidate->enhanced = 0;
			continue;						//already have name, go to next line
		}
		if (istok(buf,"xname") && !candidate->enhanced) {
			candidate->enhanced = 1;
			continue;						//already have name, go to next line
		}
		if (istok(buf,"zname") && !candidate->enhanced) {
			candidate->enhanced = 2;
			continue;						//already have name, go to next line
		}
		if (istok(buf,"!name") && !candidate->enhanced) {
			candidate->enhanced = 3;
			continue;						//already have name, go to next line
		}
		else if (istok(buf,"type"))
			continue;						//already have name, go to next line
		else if (istok(buf,"briefing")) {
			if ((v = get_value(buf)) != NULL && !descriptor_text_reference(v, 0, candidate, deferred_text)) return 0;
		}
		else if (istok(buf,"ending")) {
			if ((v = get_value(buf)) != NULL && !descriptor_text_reference(v, 1, candidate, deferred_text)) return 0;
		}
		else if (istok(buf,"num_levels")) {
			if ((v=get_value(buf))!=NULL) {
				int n_levels,i;
				if (candidate->level_names || !mission_descriptor_parse_count(v, 1, MAX_LEVELS_PER_MISSION, &n_levels)) {
					parse_error = 1;
					break;
				}
				MALLOC(candidate->level_names, d_fname, n_levels);
				if (!candidate->level_names)
				{
					return 0;
				}

				for (i=0;i<n_levels;i++) {
					if (!descriptor_line(buf,sizeof(buf),mfile,cursor) ||
					    !mission_descriptor_copy_level_name(buf, candidate->level_names[i], sizeof(candidate->level_names[i]))) {
						parse_error = 1;
						break;
					}
					candidate->last_level++;
				}
				if (parse_error) break;
			}
			else {
				parse_error = 1;
				break;
			}
		}
		else if (istok(buf,"num_secrets")) {
			if ((v=get_value(buf))!=NULL) {
				int i, n_secret_levels;
				if (candidate->secret_level_names || candidate->secret_level_table ||
				    !mission_descriptor_parse_count(v, 0, MAX_SECRET_LEVELS_PER_MISSION, &n_secret_levels)) {
					parse_error = 1;
					break;
				}
				candidate->n_secret_levels = n_secret_levels;
				if (!n_secret_levels) continue;
				MALLOC(candidate->secret_level_names, d_fname, n_secret_levels);
				if (!candidate->secret_level_names)
				{
					return 0;
				}
				MALLOC(candidate->secret_level_table, ubyte, n_secret_levels);
				if (!candidate->secret_level_table)
				{
					return 0;
				}
				for (i=0;i<n_secret_levels;i++) {
					int origin;
					unsigned extra_origins;
					if (!descriptor_line(buf,sizeof(buf),mfile,cursor) ||
					    !mission_descriptor_parse_secret_ex(buf, candidate->last_level, candidate->secret_level_names[i],
					                                        sizeof(candidate->secret_level_names[i]), &origin,
					                                        &extra_origins)) {
						parse_error = 1;
						break;
					}
					*ignored_origins += extra_origins;
					candidate->secret_level_table[i] = (ubyte)origin;
					candidate->last_secret_level--;
				}
				if (parse_error) break;
			}
			else {
				parse_error = 1;
				break;
			}
		}
		else if (candidate->enhanced == 3 && buf[0] == '!') {
			if (istok(buf+1,"ham")) {
				char alternate[FILENAME_LEN];
				if ((v=get_value(buf))!=NULL &&
				    mission_descriptor_copy_level_name(v, alternate, sizeof(alternate))) {
					unsigned l = strlen(alternate);
					if (l <= 4)
						con_printf(CON_URGENT, "Mission %s has short HAM \"%s\".\n", candidate->path, alternate);
					else {
						if (!candidate->alternate_ham_file)
							MALLOC(candidate->alternate_ham_file, d_fname, 1);
						if (!candidate->alternate_ham_file) {
							parse_error = 1;
							break;
						}
						memcpy(*candidate->alternate_ham_file, alternate, l + 1);
						con_printf(CON_VERBOSE, "Mission %s will use HAM %s.\n", candidate->path, (*candidate->alternate_ham_file));
					}
				}
				else
					con_printf(CON_URGENT, "Mission %s has no HAM.\n", candidate->path);
			}
			else {
				con_printf(CON_URGENT, "Mission %s uses unsupported critical directive \"%s\".\n", candidate->path, buf);
				candidate->last_level = 0;
				break;
			}
		}

	}

	return !parse_error && candidate->last_level > 0;
}

#ifdef DXX_XL_RUNTIME
static char *capture_mission_descriptor(PHYSFS_file *file)
{
	const PHYSFS_sint64 size = PHYSFS_fileLength(file);
	char *data;
	size_t input, output = 0;
	if (size <= 0 || size > 1024 * 1024) return NULL;
	data = d_malloc((size_t)size + 1);
	if (!data) return NULL;
	if (PHYSFS_readBytes(file, data, size) != size || memchr(data, '\0', (size_t)size)) {
		d_free(data);
		return NULL;
	}
	/* Normalize DOS/Mac line endings once for both native parsing and XL checks */
	for (input = 0; input < (size_t)size; ++input) {
		char value = data[input];
		if (value == '\r') {
			if (input + 1 < (size_t)size && data[input + 1] == '\n') ++input;
			value = '\n';
		}
		data[output++] = value;
	}
	data[output] = '\0';
	return data;
}

static void *allocate_mission_identity(size_t size, int allocation)
{
#ifdef DXX_XL_DESCRIPTOR_TEST
	extern int xl_test_fail_identity_allocation(int allocation);
	if (xl_test_fail_identity_allocation(allocation)) return NULL;
#else
	(void)allocation;
#endif
	return d_malloc(size);
}

static void release_mission_identity(Mission *mission)
{
	if (!mission) return;
	d_free(mission->path);
	d_free(mission);
}

static int preflight_mission_archive(char *descriptor_path, mission_descriptor_stage *stage)
{
	char real_path[PATH_MAX];
	strcpy(descriptor_path + strlen(descriptor_path) - 4, ".hog");
	PHYSFSEXT_locateCorrectCase(descriptor_path);
	if (!PHYSFSX_exists(descriptor_path, 1)) return 1; /* Loose-level missions remain valid */
	stage->has_companion_hog = 1;
	if (!PHYSFSX_getRealPath(descriptor_path, real_path)) {
		Warning("Cannot resolve mission archive: %s", descriptor_path);
		return 0;
	}
	strcpy(stage->companion_hog, real_path);
	if (PHYSFS_getMountPoint(real_path)) {
		if (mission_owns_resource_archive(real_path)) return 1;
		Warning("Mission archive belongs to another mount context: %s", descriptor_path);
		return 0;
	}
	if (!PHYSFS_mount(real_path, "__xl_mission_archive_probe", 1)) {
		Warning("Cannot read mission archive: %s", descriptor_path);
		return 0;
	}
	if (!PHYSFS_unmount(real_path)) Error("Cannot release mission archive probe: %s", descriptor_path);
	return 1;
}

static int preflight_mission_descriptor(const mle *mission, mission_descriptor_stage *stage)
{
	char path[PATH_MAX];
	PHYSFS_file *file;
	const char *cursor;
	int valid, length;
	if (mission->builtin_hogsize || mission->descent_version != 2) return 1;
	length = snprintf(path, sizeof(path), "%s%s.mn2", mission->location == ML_MISSIONDIR ? MISSION_DIR : "", mission->path);
	if (length < 0 || (size_t)length >= sizeof(path)) return 0;
	file = PHYSFSX_openReadBuffered(path);
	if (!file) return 0;
	stage->descriptor_data = capture_mission_descriptor(file);
	if (!stage->descriptor_data) {
		PHYSFS_close(file);
		return 0;
	}
	stage->candidate.descent_version = mission->descent_version;
	stage->candidate.path = mission->path;
	cursor = stage->descriptor_data;
	valid = parse_mission_descriptor(NULL, &stage->candidate, &stage->ignored_origins, &stage->text, &cursor);
	PHYSFS_close(file);
	if (valid) valid = preflight_mission_archive(path, stage);
	stage->ready = valid;
	if (!valid) release_descriptor_stage(stage);
	else {
		mission_text_reference *text = stage->text, *ordered = NULL;
		while (text) {
			mission_text_reference *next = text->next;
			text->next = ordered;
			ordered = text;
			text = next;
		}
		stage->text = ordered;
	}
	return valid;
}
#endif

//loads the specfied mission from the mission list.
//build_mission_list() must have been called.
//Returns true if mission loaded ok, else false.
int load_mission(mle *mission)
{
	PHYSFS_file *mfile;
	char buf[PATH_MAX];
	int parse_error = 0;
	mission_descriptor_stage staged = { 0 };
#ifdef DXX_XL_RUNTIME
	Mission *prepared_identity;
	if (!preflight_mission_descriptor(mission, &staged)) return 0;
	/* Reserve identity storage before Android preparation or prior-mission teardown */
	prepared_identity = allocate_mission_identity(sizeof(*prepared_identity), 1);
	if (prepared_identity) {
		memset(prepared_identity, 0, sizeof(*prepared_identity));
		prepared_identity->path = allocate_mission_identity(PATH_MAX, 2);
	}
	if (!prepared_identity || !prepared_identity->path || strlen(mission->path) >= PATH_MAX) {
		release_mission_identity(prepared_identity);
		release_descriptor_stage(&staged);
		con_printf(CON_URGENT, "Cannot prepare mission identity; previous mission retained\n");
		return 0;
	}
#ifdef DXX_XL_DESCRIPTOR_TEST
	{
		extern void xl_test_descriptor_staged(void);
		xl_test_descriptor_staged();
	}
#endif
#endif
#ifdef __ANDROID__
	mle selected = *mission;
	char requested[PATH_MAX], resolved[PATH_MAX];
	const char *extension = mission->descent_version == 2 ? ".mn2" : ".msn";
	int length = snprintf(requested, sizeof(requested), "%s%s%s", mission->location == ML_MISSIONDIR ? MISSION_DIR : "", mission->path, extension);
	if (length < 0 || (size_t)length >= sizeof(requested) ||
	    !android_mission_assets_prepare(requested, resolved, sizeof(resolved))) {
#ifdef DXX_XL_RUNTIME
		release_mission_identity(prepared_identity);
#endif
		release_descriptor_stage(&staged);
		return 0;
	}
	resolved[strlen(resolved) - 4] = 0;
	selected.location = !strncmp(resolved, MISSION_DIR, strlen(MISSION_DIR)) ? ML_MISSIONDIR : ML_CURDIR;
	selected.path = resolved + (selected.location == ML_MISSIONDIR ? strlen(MISSION_DIR) : 0);
	selected.filename = strrchr(selected.path, '/');
	selected.filename = selected.filename ? selected.filename + 1 : selected.path;
	mission = &selected;
#endif

#if defined(DXX_XL_RUNTIME) && !defined(__ANDROID__)
	/* Acquire the native archive before destroying the previous mission */
	if (staged.ready && staged.has_companion_hog) {
		const int owned = mission_owns_resource_archive(staged.companion_hog);
		if ((!owned && PHYSFS_getMountPoint(staged.companion_hog)) ||
		    (!owned && !PHYSFS_mount(staged.companion_hog, NULL, 0))) {
			Warning("Cannot acquire mission archive; previous mission retained: %s", staged.companion_hog);
			release_mission_identity(prepared_identity);
			release_descriptor_stage(&staged);
			return 0;
		}
		/* Reselection transfers the existing mount instead of closing/reopening it */
		if (owned) Current_mission_hog[0] = '\0';
	}
	if (staged.ready && Current_mission_hog[0]) {
		if (!PHYSFS_unmount(Current_mission_hog)) {
			if (staged.has_companion_hog && !PHYSFS_unmount(staged.companion_hog))
				Error("Cannot release candidate mission archive: %s", staged.companion_hog);
			Warning("Cannot release active mission archive; previous mission retained: %s", Current_mission_hog);
			release_mission_identity(prepared_identity);
			release_descriptor_stage(&staged);
			return 0;
		}
		Current_mission_hog[0] = '\0';
	}
#endif
	if (Current_mission)
		free_mission();
#ifdef __ANDROID__
	if (!android_mission_assets_activate()) {
#ifdef DXX_XL_RUNTIME
		release_mission_identity(prepared_identity);
#endif
		release_descriptor_stage(&staged);
		return 0;
	}
#endif
#ifdef DXX_XL_RUNTIME
	Current_mission = prepared_identity;
#ifndef __ANDROID__
	if (staged.ready && staged.has_companion_hog) {
		strcpy(Current_mission_hog, staged.companion_hog);
		input_demo_assets_note_archive("mission_hog", Current_mission_hog);
	}
#endif
#else
	MALLOC(Current_mission, Mission, 1);
	if (!Current_mission) {
		release_descriptor_stage(&staged);
#ifdef __ANDROID__
		android_mission_assets_free_begin();
		android_mission_assets_free_end();
#endif
		return 0;
	}
#endif
	/* List-only presentation flags must not change the runtime mission layout */
	Current_mission->builtin_hogsize = mission->builtin_hogsize;
	memcpy(Current_mission->mission_name, mission->mission_name, sizeof(Current_mission->mission_name));
	Current_mission->descent_version = mission->descent_version;
	Current_mission->anarchy_only_flag = mission->anarchy_only_flag;
#ifdef DXX_XL_RUNTIME
	strcpy(Current_mission->path, mission->path);
#else
	Current_mission->path = d_strdup(mission->path);
#endif
	Current_mission->filename = Current_mission->path + (mission->filename - mission->path);
	Current_mission->n_secret_levels = 0;
	Current_mission->enhanced = 0;
	Current_mission->alternate_ham_file = NULL;
#ifdef DXX_XL_RUNTIME
	/* Browser classification is a hint, not the active resource policy */
	xl_resource_select_mission(0);
#endif

	//init vars
	Mission_ignored_extra_secret_origins = 0;
	Last_level = 0;
	Last_secret_level = 0;
	memset(&Briefing_text_filename, '\0', sizeof(Briefing_text_filename));
	memset(&Ending_text_filename, '\0', sizeof(Ending_text_filename));
	Secret_level_table = NULL;
	Level_names = NULL;
	Secret_level_names = NULL;

	// for Descent 1 missions, load descent.hog
	if (EMULATING_D1) {
		if (!input_demo_assets_mount("d1_hog", "descent.hog", 1))
			Warning("descent.hog not available, this mission may be missing some files required for briefings and exit sequence\n");
		if (!d_stricmp(Current_mission_filename, D1_MISSION_FILENAME))
			return load_mission_d1();
	}

	if (PLAYING_BUILTIN_MISSION) {
		switch (Current_mission->builtin_hogsize) {
		case CD_SHAREWARE_MISSION_HOGSIZE:
		case SHAREWARE_MISSION_HOGSIZE:
		case MAC_SHARE_MISSION_HOGSIZE:
			strcpy(Briefing_text_filename,BIMD2_BRIEFING_FILE_SHARE);
			strcpy(Ending_text_filename,BIMD2_ENDING_FILE_SHARE);
			return load_mission_shareware();
			break;
		case OEM_MISSION_HOGSIZE:
			strcpy(Briefing_text_filename,BIMD2_BRIEFING_FILE_OEM);
			strcpy(Ending_text_filename,BIMD2_ENDING_FILE_OEM);
			return load_mission_oem();
			break;
		default:
			Int3(); // fall through
		case FULL_MISSION_HOGSIZE:
		case FULL_10_MISSION_HOGSIZE:
		case MAC_FULL_MISSION_HOGSIZE:
			strcpy(Briefing_text_filename,BIMD2_BRIEFING_FILE);
			// continue on... (use d2.mn2 from hogfile)
			break;
		}
	}

	//read mission from file

	switch (mission->location) {
	case ML_MISSIONDIR:
		strcpy(buf,MISSION_DIR);
		break;
	default:
		Int3();							//fall through
	case ML_CURDIR:
		strcpy(buf,"");
		break;
	}
	strcat(buf, mission->path);
	if (mission->descent_version == 2)
		strcat(buf,".mn2");
	else
		strcat(buf,".msn");

	PHYSFSEXT_locateCorrectCase(buf);

	mfile = staged.ready ? NULL : PHYSFSX_openReadBuffered(buf);
	if (!staged.ready && mfile == NULL) {
		free_mission();
		return 0;		//error!
	}

#ifdef __ANDROID__
	physfsx_android_mount_mission_directory(buf);
#endif

	//for non-builtin missions, load HOG
	if (!PLAYING_BUILTIN_MISSION)
	{
		strcpy(buf+strlen(buf)-4,".hog");		//change extension
		PHYSFSEXT_locateCorrectCase(buf);
#if defined(DXX_XL_RUNTIME) && !defined(__ANDROID__)
		/* Staged native descriptors already own their archive or its absence */
		if (!staged.ready) {
#endif
		if (PHYSFSX_exists(buf, 1) && PHYSFSX_getRealPath(buf, Current_mission_hog)) {
			if (!PHYSFS_addToSearchPath(Current_mission_hog, 0))
				Current_mission_hog[0] = '\0';
			else
				input_demo_assets_note_archive("mission_hog", Current_mission_hog);
		}
#ifdef DXX_XL_RUNTIME
		if (!Current_mission_hog[0] && (staged.has_companion_hog || PHYSFSX_exists(buf, 1))) {
			Warning("Cannot activate mission archive: %s", buf);
			if (mfile) PHYSFS_close(mfile);
			release_descriptor_stage(&staged);
			free_mission();
			return 0;
		}
#endif
#if defined(DXX_XL_RUNTIME) && !defined(__ANDROID__)
		}
#endif

		snprintf(Briefing_text_filename, sizeof(Briefing_text_filename), "%.8s.tex",Current_mission_filename);
		if (!PHYSFSX_exists(Briefing_text_filename,1))
			snprintf(Briefing_text_filename, sizeof(Briefing_text_filename), "%.8s.txb",Current_mission_filename);
		snprintf(Ending_text_filename, sizeof(Ending_text_filename), "%.8s.tex",Current_mission_filename);
		if (!PHYSFSX_exists(Ending_text_filename,1))
			snprintf(Ending_text_filename, sizeof(Ending_text_filename), "%.8s.txb",Current_mission_filename);
	}

	if (staged.ready) {
		mission_text_reference *text;
#ifdef DXX_XL_RUNTIME
		Current_mission_descriptor = staged.descriptor_data;
		staged.descriptor_data = NULL;
#endif
		Current_mission->enhanced = staged.candidate.enhanced;
		Current_mission->alternate_ham_file = staged.candidate.alternate_ham_file;
		Level_names = staged.candidate.level_names;
		Secret_level_names = staged.candidate.secret_level_names;
		Secret_level_table = staged.candidate.secret_level_table;
		Last_level = staged.candidate.last_level;
		Last_secret_level = staged.candidate.last_secret_level;
		N_secret_levels = staged.candidate.n_secret_levels;
		Mission_ignored_extra_secret_origins = staged.ignored_origins;
		for (text = staged.text; text; text = text->next)
			set_mission_text_filename(text->name, text->ending ? Ending_text_filename : Briefing_text_filename);
		staged.candidate.alternate_ham_file = NULL;
		staged.candidate.level_names = staged.candidate.secret_level_names = NULL;
		staged.candidate.secret_level_table = NULL;
		release_descriptor_stage(&staged);
	} else {
#ifdef DXX_XL_RUNTIME
		if (mission->descent_version == 2) {
			Current_mission_descriptor = capture_mission_descriptor(mfile);
			const char *cursor = Current_mission_descriptor;
			parse_error = !cursor || !parse_mission_descriptor(NULL, Current_mission, &Mission_ignored_extra_secret_origins, NULL, &cursor);
		} else
#endif
		parse_error = !parse_mission_descriptor(mfile, Current_mission, &Mission_ignored_extra_secret_origins, NULL, NULL);
		PHYSFS_close(mfile);
	}

	if (parse_error || Last_level <= 0) {
		free_mission();		//no valid mission loaded
		return 0;
	}

#ifdef DXX_XL_RUNTIME
	xl_resource_commit_mission();
#endif
	// re-read default HAM file, in case this mission brings it's own version of it
	if (d1_in_d2_load_mission_assets())
		init_extra_robot_movie(Current_mission_filename);

	return 1;
}

//loads the named mission if exists.
//Returns true if mission loaded ok, else false.
int load_mission_by_name(char *mission_name)
{
	int i;
	mle *mission_list = build_mission_list(1);
	bool found = 0;

#ifdef __ANDROID__
	int match = -1, ambiguous = 0;
	for (i = 0; i < num_missions; i++)
		if (!d_stricmp(mission_name, mission_list[i].filename) || !d_stricmp(mission_name, mission_list[i].path)) {
			if (match >= 0) ambiguous = 1;
			match = i;
		}
	if (ambiguous) Warning("Mission name '%s' is ambiguous; select its package in the mission menu", mission_name);
	else if (match >= 0) found = load_mission(mission_list + match);
#else
	int match = -1, ambiguous = 0;
	for (i = 0; i < num_missions; i++)
		if (!d_stricmp(mission_name, mission_list[i].filename)) {
			if (match >= 0) ambiguous = 1;
			match = i;
		}
	if (ambiguous) Warning("Mission name '%s' is ambiguous; remove or rename duplicate missions", mission_name);
	else if (match >= 0) found = load_mission(mission_list + match);
#endif

	free_mission_list(mission_list);
	return found;
}

int load_mission_by_name_from_current_dir(char *mission_name)
{
	char descriptor[FILENAME_LEN];
	snprintf(descriptor, sizeof(descriptor), "%s.mn2", mission_name);
	return load_mission_from_current_dir(descriptor);
}

int load_mission_from_current_dir(char *descriptor)
{
	mle mission = { 0 };
	int loaded;

	if (!read_mission_file(&mission, descriptor, ML_CURDIR))
		return 0;
	loaded = load_mission(&mission);
	d_free(mission.path);
	return loaded;
}

typedef struct mission_menu
{
	mle *mission_list;
	int mission_count;
	mission_display_name_buf *display_names;
	int (*when_selected)(void);
} mission_menu;

static void mission_display_name(mle *mission, char *display_name)
{
	if (mission->descent_version == 1)
		snprintf(display_name, MISSION_NAME_LEN + 6, "D1: %s", mission->mission_name);
	else if (mission->requires_xl)
		snprintf(display_name, MISSION_NAME_LEN + 6, "XL: %s", mission->mission_name);
	else
		snprintf(display_name, MISSION_NAME_LEN + 6, "%s", mission->mission_name);
}

int mission_menu_handler(listbox *lb, d_event *event, mission_menu *mm)
{
	char **list = listbox_get_items(lb);
	int citem = listbox_get_citem(lb);

	switch (event->type)
	{
		case EVENT_NEWMENU_SELECTED:
			if (citem >= 0)
			{
				// Chose a mission
				strcpy(GameCfg.LastMission, mm->mission_list[citem].mission_name);
				
				if (!load_mission(mm->mission_list + citem))
				{
					nm_messagebox( NULL, 1, TXT_OK, TXT_MISSION_ERROR);
					return 1;	// stay in listbox so user can select another one
				}
			}
#ifdef __ANDROID__
			{
				int started = (*mm->when_selected)();
				if (!started) free_mission();
				return !started;
			}
#else
			return !(*mm->when_selected)();
#endif
			break;

		case EVENT_WINDOW_CLOSE:
			free_mission_list(mm->mission_list);
			d_free(mm->display_names);
			d_free(list);
			d_free(mm);
			break;
			
		default:
			break;
	}
	
	return 0;
}

int select_mission(int anarchy_mode, char *message, int (*when_selected)(void))
{
    mle *mission_list = build_mission_list(anarchy_mode);
	int new_mission_num;

    if (num_missions <= 1)
	{
        new_mission_num = load_mission(mission_list) ? 0 : -1;
		free_mission_list(mission_list);
#ifdef __ANDROID__
		if (new_mission_num >= 0 && !(*when_selected)()) free_mission();
#else
		(*when_selected)();
#endif
		
		return (new_mission_num >= 0);
    }
	else
	{
		mission_menu *mm;
        int i, default_mission;
        char **m;
		
		MALLOC(m, char *, num_missions);
		if (!m)
		{
			free_mission_list(mission_list);
			return 0;
		}
		
		MALLOC(mm, mission_menu, 1);
		if (!mm)
		{
			d_free(m);
			free_mission_list(mission_list);
			return 0;
		}
		MALLOC(mm->display_names, mission_display_name_buf, num_missions);
		if (!mm->display_names)
		{
			d_free(mm);
			d_free(m);
			free_mission_list(mission_list);
			return 0;
		}

		mm->mission_list = mission_list;
		mm->mission_count = num_missions;
		mm->when_selected = when_selected;
		
        default_mission = 0;
        for (i = 0; i < num_missions; i++) {
            mission_display_name(&mission_list[i], mm->display_names[i]);
            m[i] = mm->display_names[i];
            if ( !d_stricmp( mission_list[i].mission_name, GameCfg.LastMission ) )
                default_mission = i;
        }

        newmenu_listbox1( message, num_missions, m, 1, default_mission, (int (*)(listbox *, d_event *, void *))mission_menu_handler, mm );
    }

    return 1;	// presume success
}

#ifdef EDITOR
void create_new_mission(void)
{
	if (Current_mission)
		free_mission();
	
	CALLOC(Current_mission, Mission, 1);
	if (!Current_mission)
		return;
	
	Current_mission->path = d_strdup("new_mission");
	if (!Current_mission->path)
	{
		free_mission();
		return;
	}

	Current_mission->filename = Current_mission->path;
	
	MALLOC(Level_names, d_fname, 1);
	if (!Level_names)
	{
		free_mission();
		return;
	}

	strcpy(Level_names[0], "GAMESAVE.LVL");
}
#endif
