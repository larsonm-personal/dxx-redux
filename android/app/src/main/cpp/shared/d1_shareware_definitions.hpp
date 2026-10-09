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
/* Shared table semantics. Engine adapters bind destinations and resource operations */
#ifndef DXX_D1_SHAREWARE_DEFINITIONS_HPP
#define DXX_D1_SHAREWARE_DEFINITIONS_HPP
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include "d1_shareware_table.h"

#define BM_NONE       -1
#define BM_COCKPIT    0
#define BM_TEXTURES   2
#define BM_UNUSED     3
#define BM_VCLIP      4
#define BM_EFFECTS    5
#define BM_ECLIP      6
#define BM_WEAPON     7
#define BM_DEMO       8
#define BM_ROBOTEX    9
#define BM_WALL_ANIMS 12
#define BM_WCLIP      13
#define BM_ROBOT      14
#define BM_GAUGES     20

#define MAX_BITMAPS_PER_BRUSH 30

template <typename T, size_t Capacity>
class D1TableArray
{
	T *data = nullptr;
	size_t count = Capacity;

  public:
	D1TableArray &operator=(T *value)
	{
		data = value;
		return *this;
	}
	void bind(T *value, size_t size)
	{
		data = value;
		count = size;
	}
	T &operator[](int index)
	{
		if (!data || index < 0 || static_cast<size_t>(index) >= count)
			throw std::runtime_error("D1 table destination index out of bounds");
		return data[index];
	}
};

class D1TableParser
{
  public:
	virtual ~D1TableParser() = default;
	using Name = char[16];
	player_ship *Player_ship = nullptr;
	D1TableArray<bitmap_index, 800> Textures;
	D1TableArray<bitmap_index, 85> Gauges;
	D1TableArray<bitmap_index, 4> cockpit_bitmap;
	D1TableArray<bitmap_index, 210> ObjBitmaps;
	D1TableArray<tmap_info, 800> TmapInfo;
	D1TableArray<vclip, 70> Vclip;
	D1TableArray<eclip, 60> Effects;
	D1TableArray<wclip, 30> WallAnims;
	D1TableArray<robot_info, 30> Robot_info;
	D1TableArray<weapon_info, 30> Weapon_info;
	D1TableArray<powerup_type_info, 29> Powerup_info;
	D1TableArray<polymodel, 85> Polygon_models;
	D1TableArray<reactor, 1> Reactors;
	D1TableArray<grs_bitmap, 1630> GameBitmaps;
	D1TableArray<ubyte, 250> Sounds;
	D1TableArray<ubyte, 250> AltSounds;
	D1TableArray<ushort, 210> ObjBitmapPtrs;
	D1TableArray<sbyte, 100> ObjType;
	D1TableArray<sbyte, 100> ObjId;
	D1TableArray<fix, 100> ObjStrength;
	D1TableArray<int, 85> Dying_modelnums;
	D1TableArray<int, 85> Dead_modelnums;
	D1TableArray<int, 800> TmapList;
	D1TableArray<int, 1> Hostage_vclip_num;
	D1TableArray<Name, 30> Robot_names;
	D1TableArray<Name, 29> Powerup_names;
	int NumTextures = 0, Num_tmaps = 0, Num_effects = 0, Num_vclips = 0, Num_wall_anims = 0;
	int N_robot_types = 0, N_weapon_types = 0, N_powerup_types = 0, N_hostage_types = 0;
	int Num_cockpits = 0, Num_total_object_types = 0, First_multi_bitmap_num = -1;
	int exit_modelnum = -1, destroyed_exit_modelnum = -1;
	int gauge_limit = 80;
	bool excluded_effects[60] = {};

  protected:
	virtual bitmap_index bm_load_sub(int skip, char *filename) = 0;
	virtual void ab_load(int skip, char *filename, bitmap_index *bitmaps, int *frames) = 0;
	virtual int ds_load(int skip, char *filename) = 0;
	virtual int load_polygon_model(char *filename, int textures, int first_texture, robot_info *robot) = 0;
	virtual int read_model_guns(char *filename, vms_vector *points, vms_vector *directions, int *submodels) = 0;
	virtual char *texture_name(int index) = 0;
	int SuperX = -1;

  private:
	int N_ObjBitmaps = 0, N_ObjBitmapPtrs = 0, Num_robot_ais = 0;
	char *arg = nullptr, *tokens = nullptr;
	int tmap_count = 0, texture_count = 0, clip_count = 0, clip_num = 0, sound_num = 0, frames = 0;
	float play_time = 0, vlighting = 0;
	int hit_sound = -1, bm_flag = BM_NONE, abm_flag = 0, rod_flag = 0;
	int wall_open_sound = 0, wall_close_sound = 0, wall_explodes = 0, wall_blastable = 0, wall_hidden = 0;
	int obj_eclip = 0, dest_vclip = 0, dest_eclip = 0, crit_clip = 0, crit_flag = 0, tmap1_flag = 0, num_sounds = 0;
	fix dest_size = 0;
	char *dest_bm = nullptr;
	unsigned linenum = 0;
	const char *space = " \t", *equal_space = " \t=";

	[[noreturn]] void Error(const char *format, ...)
	{
		char message[512];
		va_list args;
		va_start(args, format);
		vsnprintf(message, sizeof(message), format, args);
		va_end(args);
		throw std::runtime_error(std::string("D1 table line ") + std::to_string(linenum) + ": " + message);
	}
	int checked_frames(int count)
	{
		check(count > 0 && count <= 30);
		return count;
	}
	void check(bool condition)
	{
		if (!condition) Error("Invalid definition or index");
	}
	char *strtok(char *text, const char *delimiters)
	{
		if (text) tokens = text;
		if (!tokens) return nullptr;
		tokens += strspn(tokens, delimiters);
		if (!*tokens) return nullptr;
		char *start = tokens;
		tokens += strcspn(tokens, delimiters);
		if (*tokens) *tokens++ = 0;
		return start;
	}
	int atoi(const char *text)
	{
		if (!text) Error("Missing integer");
		return std::atoi(text);
	}
	double atof(const char *text)
	{
		if (!text) Error("Missing number");
		return std::atof(text);
	}
	void remove_char(char *text, char value)
	{
		char *p = strchr(text, value);
		if (p) *p = 0;
	}

  public:
#define IFTOK(str) if (!strcmp(arg, str))
	int read(d1_shareware_table_reader &table_reader, int pc_shareware)
	{
		char inputline[600];
		int i, table_result;
		ObjType[0] = 5;
		ObjId[0] = 0;
		Num_total_object_types = 1;

		for (i = 0; i < 250; i++) {
			Sounds[i] = 255;
			AltSounds[i] = 255;
		}

		for (i = 0; i < 800; i++) {
			TmapInfo[i].eclip_num = -1;
			TmapInfo[i].flags = 0;
		}

		Num_effects = 0;
		for (i = 0; i < 60; i++) {
			// Effects[i].bm_ptr = (grs_bitmap **) -1;
			Effects[i].changing_wall_texture = -1;
			Effects[i].changing_object_texture = -1;
			Effects[i].segnum = -1;
			Effects[i].vc.num_frames = -1; // another mark of being unused
		}

		for (i = 0; i < 85; i++)
			Dying_modelnums[i] = Dead_modelnums[i] = -1;

		Num_vclips = 0;
		for (i = 0; i < 70; i++) {
			Vclip[i].num_frames = -1;
			Vclip[i].flags = 0;
		}

		for (i = 0; i < 30; i++)
			WallAnims[i].num_frames = -1;
		Num_wall_anims = 0;

		while ((table_result = d1_shareware_table_next(&table_reader, inputline, sizeof(inputline))) > 0) {
			char *temp_ptr;
			int skip;

			linenum = table_reader.line;

			SuperX = -1;

			if ((temp_ptr = strstr(inputline, "superx="))) {
				SuperX = atoi(&temp_ptr[7]);
			}

			arg = strtok(inputline, space);
			if (arg && arg[0] == '@') {
				arg++;
				skip = pc_shareware;
			} else
				skip = 0;

			while (arg != NULL) {
				// Check all possible flags and defines.
				if (*arg == '$') bm_flag = BM_NONE; // reset to no flags as default.

				IFTOK("$COCKPIT")
				bm_flag = BM_COCKPIT;
				else IFTOK("$GAUGES")
				{
					bm_flag = BM_GAUGES;
					clip_count = 0;
				}
				else IFTOK("$SOUND") bm_read_sound(skip, pc_shareware);
				else IFTOK("$DOOR_ANIMS") bm_flag = BM_WALL_ANIMS;
				else IFTOK("$WALL_ANIMS") bm_flag = BM_WALL_ANIMS;
				else IFTOK("$TEXTURES") bm_flag = BM_TEXTURES;
				else IFTOK("$VCLIP")
				{
					bm_flag = BM_VCLIP;
					vlighting = 0;
					clip_count = 0;
				}
				else IFTOK("$ECLIP")
				{
					bm_flag = BM_ECLIP;
					vlighting = 0;
					clip_count = 0;
					obj_eclip = 0;
					dest_bm = NULL;
					dest_vclip = -1;
					dest_eclip = -1;
					dest_size = -1;
					crit_clip = -1;
					crit_flag = 0;
					sound_num = -1;
				}
				else IFTOK("$WCLIP")
				{
					bm_flag = BM_WCLIP;
					vlighting = 0;
					clip_count = 0;
					wall_explodes = wall_blastable = 0;
					wall_open_sound = wall_close_sound = -1;
					tmap1_flag = 0;
					wall_hidden = 0;
				}

				else IFTOK("$EFFECTS")
				{
					bm_flag = BM_EFFECTS;
					clip_num = 0;
				}

#ifdef EDITOR
				else IFTOK("!METALS_FLAG") TextureMetals = texture_count;
				else IFTOK("!LIGHTS_FLAG") TextureLights = texture_count;
				else IFTOK("!EFFECTS_FLAG") TextureEffects = texture_count;
#else
				else IFTOK("!METALS_FLAG");
				else IFTOK("!LIGHTS_FLAG");
				else IFTOK("!EFFECTS_FLAG");
#endif

				else IFTOK("lighting") TmapInfo[texture_count - 1].lighting = fl2f(get_float());
				else IFTOK("damage") TmapInfo[texture_count - 1].damage = fl2f(get_float());
				else IFTOK("volatile") TmapInfo[texture_count - 1].flags |= TMI_VOLATILE;
				// else IFTOK("Num_effects")		Num_effects = get_int();
				else IFTOK("Num_wall_anims") Num_wall_anims = get_int();
				else IFTOK("clip_num") clip_num = get_int();
				else IFTOK("dest_bm") dest_bm = strtok(NULL, space);
				else IFTOK("dest_vclip") dest_vclip = get_int();
				else IFTOK("dest_eclip") dest_eclip = get_int();
				else IFTOK("dest_size") dest_size = fl2f(get_float());
				else IFTOK("crit_clip") crit_clip = get_int();
				else IFTOK("crit_flag") crit_flag = get_int();
				else IFTOK("sound_num") sound_num = get_int();
				else IFTOK("frames") frames = get_int();
				else IFTOK("time") play_time = get_float();
				else IFTOK("obj_eclip") obj_eclip = get_int();
				else IFTOK("hit_sound") hit_sound = get_int();
				else IFTOK("abm_flag") abm_flag = get_int();
				else IFTOK("tmap1_flag") tmap1_flag = get_int();
				else IFTOK("vlighting") vlighting = get_float();
				else IFTOK("rod_flag") rod_flag = get_int();
				else IFTOK("superx") get_int();
				else IFTOK("open_sound") wall_open_sound = get_int();
				else IFTOK("close_sound") wall_close_sound = get_int();
				else IFTOK("explodes") wall_explodes = get_int();
				else IFTOK("blastable") wall_blastable = get_int();
				else IFTOK("hidden") wall_hidden = get_int();
				else IFTOK("$ROBOT_AI") bm_read_robot_ai(skip);

				else IFTOK("$POWERUP")
				{
					bm_read_powerup(0);
					continue;
				}
				else IFTOK("$POWERUP_UNUSED")
				{
					bm_read_powerup(1);
					continue;
				}
				else IFTOK("$HOSTAGE")
				{
					bm_read_hostage();
					continue;
				}
				else IFTOK("$ROBOT")
				{
					bm_read_robot(skip);
					continue;
				}
				else IFTOK("$WEAPON")
				{
					bm_read_weapon(skip, 0);
					continue;
				}
				else IFTOK("$WEAPON_UNUSED")
				{
					bm_read_weapon(skip, 1);
					continue;
				}
				else IFTOK("$OBJECT")
				{
					bm_read_object(skip);
					continue;
				}
				else IFTOK("$PLAYER_SHIP")
				{
					bm_read_player_ship(skip);
					continue;
				}

				else
				{ // not a special token, must be a bitmap!

					// Remove any illegal/unwanted spaces and tabs at this point.
					while ((*arg == '\t') || (*arg == ' ')) arg++;
					if (*arg == '\0') { break; }

					// Otherwise, 'arg' is apparently a bitmap filename.
					// Load bitmap and process it below:
					bm_read_some_file(skip);
				}

				arg = strtok(NULL, equal_space);
				continue;
			}
		}

		NumTextures = texture_count;
		Num_tmaps = tmap_count;
		if (table_result < 0)
			Error("BITMAPS.TBL/BIN line %u: %s", table_reader.line + 1, table_reader.error);

		check(N_robot_types == Num_robot_ais); // should be one ai info per robot

		verify_textures();

		// check for refereced but unused clip count
		for (i = 0; i < 60; i++)
			if ((
			        (Effects[i].changing_wall_texture != -1) ||
			        (Effects[i].changing_object_texture != -1)) &&
			    (Effects[i].vc.num_frames == -1))
				Error("EClip %d referenced (by polygon object?), but not defined", i);

#ifndef NDEBUG
		{
			int used;
			for (i = used = 0; i < num_sounds; i++)
				if (Sounds[i] != 255)
					used++;
		}
#endif

#ifdef EDITOR
		//	piggy_dump_all();	// causes problems - we're not going to bother
#endif

		return 0;
	}
	void verify_textures()
	{
		grs_bitmap *bmp;
		int i, j;
		j = 0;
		for (i = 0; i < Num_tmaps; i++) {
			bmp = &GameBitmaps[Textures[i].index];
			if ((bmp->bm_w != 64) || (bmp->bm_h != 64) || (bmp->bm_rowsize != 64)) {
				j++;
			}
		}
		if (j) Error("There are game textures that are not 64x64");
	}

	void set_lighting_flag(sbyte *bp)
	{
		if (vlighting < 0)
			*bp |= BM_FLAG_NO_LIGHTING;
		else
			*bp &= (0xff ^ BM_FLAG_NO_LIGHTING);
	}

	void set_texture_name(char *name)
	{
		strcpy(texture_name(texture_count), name);
		REMOVE_DOTS(texture_name(texture_count));
	}

	void bm_read_eclip(int skip)
	{
		bitmap_index bitmap;

		check(clip_num >= 0 && clip_num < 60);

		if (clip_num + 1 > Num_effects)
			Num_effects = clip_num + 1;

		excluded_effects[clip_num] = skip != 0;
		Effects[clip_num].flags = 0;

		if (!abm_flag) {
			bitmap = bm_load_sub(skip, arg);

			Effects[clip_num].vc.play_time = fl2f(play_time);
			Effects[clip_num].vc.num_frames = frames;
			Effects[clip_num].vc.frame_time = fl2f(play_time) / checked_frames(frames);

			check(clip_count < frames);
			Effects[clip_num].vc.frames[clip_count] = bitmap;
			set_lighting_flag(&GameBitmaps[bitmap.index].bm_flags);

			check(!obj_eclip); // obj eclips for non-abm files not supported!
			check(crit_flag == 0);

			if (clip_count == 0) {
				Effects[clip_num].changing_wall_texture = texture_count;
				check(tmap_count < 800);
				TmapList[tmap_count++] = texture_count;
				Textures[texture_count] = bitmap;
				set_texture_name(arg);
				check(texture_count < 800);
				texture_count++;
				TmapInfo[texture_count].eclip_num = clip_num;
				NumTextures = texture_count;
			}

			clip_count++;

		} else {
			bitmap_index bm[MAX_BITMAPS_PER_BRUSH];
			abm_flag = 0;

			ab_load(skip, arg, bm, &Effects[clip_num].vc.num_frames);

			Effects[clip_num].vc.play_time = fl2f(play_time);
			Effects[clip_num].vc.frame_time = Effects[clip_num].vc.play_time / checked_frames(Effects[clip_num].vc.num_frames);

			clip_count = 0;
			set_lighting_flag(&GameBitmaps[bm[clip_count].index].bm_flags);
			Effects[clip_num].vc.frames[clip_count] = bm[clip_count];

			if (!obj_eclip && !crit_flag) {
				Effects[clip_num].changing_wall_texture = texture_count;
				check(tmap_count < 800);
				TmapList[tmap_count++] = texture_count;
				Textures[texture_count] = bm[clip_count];
				set_texture_name(arg);
				check(texture_count < 800);
				TmapInfo[texture_count].eclip_num = clip_num;
				texture_count++;
				NumTextures = texture_count;
			}

			if (obj_eclip) {

				if (Effects[clip_num].changing_object_texture == -1) {        // first time referenced
					Effects[clip_num].changing_object_texture = N_ObjBitmaps; // XChange ObjectBitmaps
					N_ObjBitmaps++;
				}

				ObjBitmaps[Effects[clip_num].changing_object_texture] = Effects[clip_num].vc.frames[0];
			}

			// if for an object, Effects_bm_ptrs set in object load

			for (clip_count = 1; clip_count < Effects[clip_num].vc.num_frames; clip_count++) {
				set_lighting_flag(&GameBitmaps[bm[clip_count].index].bm_flags);
				Effects[clip_num].vc.frames[clip_count] = bm[clip_count];
			}
		}

		Effects[clip_num].crit_clip = crit_clip;
		Effects[clip_num].sound_num = sound_num;

		if (dest_bm) { // deal with bitmap for blown up clip
			char short_name[13];
			int i;
			strcpy(short_name, dest_bm);
			REMOVE_DOTS(short_name);
			for (i = 0; i < texture_count; i++)
				if (!d_stricmp(texture_name(i), short_name))
					break;
			if (i == texture_count) {
				Textures[texture_count] = bm_load_sub(skip, dest_bm);
				strcpy(texture_name(texture_count), short_name);
				texture_count++;
				check(texture_count < 800);
				NumTextures = texture_count;
			}
			Effects[clip_num].dest_bm_num = i;

			if (dest_vclip == -1)
				Error("Desctuction vclip missing on line %d", linenum);
			if (dest_size == -1)
				Error("Desctuction vclip missing on line %d", linenum);

			Effects[clip_num].dest_vclip = dest_vclip;
			Effects[clip_num].dest_size = dest_size;

			Effects[clip_num].dest_eclip = dest_eclip;
		} else {
			Effects[clip_num].dest_bm_num = -1;
			Effects[clip_num].dest_eclip = -1;
		}

		if (crit_flag)
			Effects[clip_num].flags |= EF_CRITICAL;
	}

	void bm_read_gauges(int skip)
	{
		bitmap_index bitmap;
		int i, num_abm_frames;

		if (!abm_flag) {
			bitmap = bm_load_sub(skip, arg);
			check(clip_count < gauge_limit);
			Gauges[clip_count] = bitmap;
			clip_count++;
		} else {
			bitmap_index bm[MAX_BITMAPS_PER_BRUSH];
			abm_flag = 0;
			ab_load(skip, arg, bm, &num_abm_frames);
			for (i = clip_count; i < clip_count + num_abm_frames; i++) {
				check(i < gauge_limit);
				Gauges[i] = bm[i - clip_count];
			}
			clip_count += num_abm_frames;
		}
	}

	void bm_read_wclip(int skip)
	{
		bitmap_index bitmap;
		check(clip_num >= 0 && clip_num < 30);

		WallAnims[clip_num].flags = 0;

		if (wall_explodes) WallAnims[clip_num].flags |= WCF_EXPLODES;
		if (wall_blastable) WallAnims[clip_num].flags |= WCF_BLASTABLE;
		if (wall_hidden) WallAnims[clip_num].flags |= WCF_HIDDEN;
		if (tmap1_flag) WallAnims[clip_num].flags |= WCF_TMAP1;

		if (!abm_flag) {
			bitmap = bm_load_sub(skip, arg);
			if ((WallAnims[clip_num].num_frames > -1) && (clip_count == 0))
				Error("Wall Clip %d is already used!", clip_num);
			WallAnims[clip_num].play_time = fl2f(play_time);
			WallAnims[clip_num].num_frames = frames;
			// WallAnims[clip_num].frame_time = fl2f(play_time)/checked_frames(frames);
			check(clip_count < frames);
			WallAnims[clip_num].frames[clip_count++] = texture_count;
			WallAnims[clip_num].open_sound = wall_open_sound;
			WallAnims[clip_num].close_sound = wall_close_sound;
			Textures[texture_count] = bitmap;
			set_lighting_flag(&GameBitmaps[bitmap.index].bm_flags);
			set_texture_name(arg);
			check(texture_count < 800);
			texture_count++;
			NumTextures = texture_count;
			if (clip_num >= Num_wall_anims) Num_wall_anims = clip_num + 1;
		} else {
			bitmap_index bm[MAX_BITMAPS_PER_BRUSH];
			int nframes;
			if ((WallAnims[clip_num].num_frames > -1))
				Error("AB_Wall clip %d is already used!", clip_num);
			abm_flag = 0;
			ab_load(skip, arg, bm, &nframes);
			WallAnims[clip_num].num_frames = nframes;
			WallAnims[clip_num].play_time = fl2f(play_time);
			// WallAnims[clip_num].frame_time = fl2f(play_time)/nframes;
			WallAnims[clip_num].open_sound = wall_open_sound;
			WallAnims[clip_num].close_sound = wall_close_sound;

			WallAnims[clip_num].close_sound = wall_close_sound;
			strcpy(WallAnims[clip_num].filename, arg);
			REMOVE_DOTS(WallAnims[clip_num].filename);

			if (clip_num >= Num_wall_anims) Num_wall_anims = clip_num + 1;

			set_lighting_flag(&GameBitmaps[bm[0].index].bm_flags);

			for (clip_count = 0; clip_count < WallAnims[clip_num].num_frames; clip_count++) {
				Textures[texture_count] = bm[clip_count];
				set_lighting_flag(&GameBitmaps[bm[clip_count].index].bm_flags);
				WallAnims[clip_num].frames[clip_count] = texture_count;
				REMOVE_DOTS(arg);
				sprintf(texture_name(texture_count), "%s#%d", arg, clip_count);
				check(texture_count < 800);
				texture_count++;
				NumTextures = texture_count;
			}
		}
	}

	void bm_read_vclip(int skip)
	{
		bitmap_index bi;
		check(clip_num >= 0 && clip_num < 70);

		if (!abm_flag) {
			if ((Vclip[clip_num].num_frames > -1) && (clip_count == 0))
				Error("Vclip %d is already used!", clip_num);
			bi = bm_load_sub(skip, arg);
			Vclip[clip_num].play_time = fl2f(play_time);
			Vclip[clip_num].num_frames = frames;
			Vclip[clip_num].frame_time = fl2f(play_time) / checked_frames(frames);
			Vclip[clip_num].light_value = fl2f(vlighting);
			Vclip[clip_num].sound_num = sound_num;
			set_lighting_flag(&GameBitmaps[bi.index].bm_flags);
			check(clip_count < frames);
			Vclip[clip_num].frames[clip_count++] = bi;
			if (rod_flag) {
				rod_flag = 0;
				Vclip[clip_num].flags |= VF_ROD;
			}

		} else {
			bitmap_index bm[MAX_BITMAPS_PER_BRUSH];
			abm_flag = 0;
			if ((Vclip[clip_num].num_frames > -1))
				Error("AB_Vclip %d is already used!", clip_num);
			ab_load(skip, arg, bm, &Vclip[clip_num].num_frames);

			if (rod_flag) {
				// int i;
				rod_flag = 0;
				Vclip[clip_num].flags |= VF_ROD;
			}
			Vclip[clip_num].play_time = fl2f(play_time);
			Vclip[clip_num].frame_time = fl2f(play_time) / Vclip[clip_num].num_frames;
			Vclip[clip_num].light_value = fl2f(vlighting);
			Vclip[clip_num].sound_num = sound_num;
			set_lighting_flag(&GameBitmaps[bm[0].index].bm_flags);

			for (clip_count = 0; clip_count < Vclip[clip_num].num_frames; clip_count++) {
				set_lighting_flag(&GameBitmaps[bm[clip_count].index].bm_flags);
				Vclip[clip_num].frames[clip_count] = bm[clip_count];
			}
		}
	}

	// ------------------------------------------------------------------------------
	void get4fix(fix *fixp)
	{
		char *curtext;
		int i;

		for (i = 0; i < NDL; i++) {
			curtext = strtok(NULL, space);
			fixp[i] = fl2f(atof(curtext));
		}
	}

	// ------------------------------------------------------------------------------
	void get4byte(sbyte *bytep)
	{
		char *curtext;
		int i;

		for (i = 0; i < NDL; i++) {
			curtext = strtok(NULL, space);
			bytep[i] = atoi(curtext);
		}
	}

	// ------------------------------------------------------------------------------
	//	Convert field of view from an angle in 0..360 to cosine.
	void adjust_field_of_view(fix *fovp)
	{
		int i;
		fixang tt;
		float ff;
		fix temp;

		for (i = 0; i < NDL; i++) {
			ff = -f2fl(fovp[i]);
			if (ff > 179) {
				ff = 179;
			}
			ff = ff / 360;
			tt = fl2f(ff);
			fix_sincos(tt, &temp, &fovp[i]);
		}
	}

	void clear_to_end_of_line(void)
	{
		arg = strtok(NULL, space);
		while (arg != NULL)
			arg = strtok(NULL, space);
	}

	void bm_read_sound(int skip, int pc_shareware)
	{
		int sound_num;
		int alt_sound_num;

		sound_num = get_int();
		alt_sound_num = pc_shareware ? sound_num : get_int();

		if (sound_num >= 250)
			Error("Too many sound files.\n");

		if (sound_num >= num_sounds)
			num_sounds = sound_num + 1;

		arg = strtok(NULL, space);

		Sounds[sound_num] = ds_load(skip, arg);

		if (alt_sound_num == 0)
			AltSounds[sound_num] = sound_num;
		else if (alt_sound_num < 0)
			AltSounds[sound_num] = 255;
		else
			AltSounds[sound_num] = alt_sound_num;

		if (Sounds[sound_num] == 255)
			Error("Can't load soundfile <%s>", arg);
	}

	// ------------------------------------------------------------------------------
	void bm_read_robot_ai(int skip)
	{
		char *robotnum_text;
		int robotnum;
		robot_info *robptr;

		robotnum_text = strtok(NULL, space);
		robotnum = atoi(robotnum_text);
		check(robotnum < 30);
		robptr = &Robot_info[robotnum];

		check(robotnum == Num_robot_ais); // make sure valid number

		if (skip) {
			Num_robot_ais++;
			clear_to_end_of_line();
			return;
		}

		Num_robot_ais++;

		get4fix(robptr->field_of_view);
		get4fix(robptr->firing_wait);
		get4byte(robptr->rapidfire_count);
		get4fix(robptr->turn_time);
		fix fire_power[NDL]; //	damage done by a hit from this robot
		fix shield[NDL];     //	shield strength of this robot
		get4fix(fire_power);
		get4fix(shield);
		get4fix(robptr->max_speed);
		get4fix(robptr->circle_distance);
		get4byte(robptr->evade_speed);

		robptr->always_0xabcd = 0xabcd;

		adjust_field_of_view(robptr->field_of_view);
	}

	//	----------------------------------------------------------------------------------------------
	// this will load a bitmap for a polygon models.  it puts the bitmap into
	// the array ObjBitmaps[], and also deals with animating bitmaps
	// returns a pointer to the bitmap
	grs_bitmap *load_polymodel_bitmap(int skip, char *name)
	{
		check(N_ObjBitmaps < 210);

		//	check( N_ObjBitmaps == N_ObjBitmapPtrs );

		if (name[0] == '%') { // an animating bitmap!
			int eclip_num;

			eclip_num = atoi(name + 1);

			if (Effects[eclip_num].changing_object_texture == -1) { // first time referenced
				Effects[eclip_num].changing_object_texture = N_ObjBitmaps;
				ObjBitmapPtrs[N_ObjBitmapPtrs++] = N_ObjBitmaps;
				N_ObjBitmaps++;
			} else {
				ObjBitmapPtrs[N_ObjBitmapPtrs++] = Effects[eclip_num].changing_object_texture;
			}
			return NULL;
		} else {
			ObjBitmaps[N_ObjBitmaps] = bm_load_sub(skip, name);
			ObjBitmapPtrs[N_ObjBitmapPtrs++] = N_ObjBitmaps;
			N_ObjBitmaps++;
			return &GameBitmaps[ObjBitmaps[N_ObjBitmaps - 1].index];
		}
	}

#define MAX_MODEL_VARIANTS 4

	// ------------------------------------------------------------------------------
	void bm_read_robot(int skip)
	{
		char *model_name[MAX_MODEL_VARIANTS];
		int n_models, i;
		int first_bitmap_num[MAX_MODEL_VARIANTS + 1];
		char *equal_ptr;
		int exp1_vclip_num = -1;
		int exp1_sound_num = -1;
		int exp2_vclip_num = -1;
		int exp2_sound_num = -1;
		fix lighting = F1_0 / 2;  // Default
		fix strength = F1_0 * 10; // Default strength
		fix mass = f1_0 * 4;
		fix drag = f1_0 / 2;
		short weapon_type = 0;
		int g, s;
		char name[16];
		int contains_count = 0, contains_id = 0, contains_prob = 0, contains_type = 0;
		int score_value = 1000;
		int cloak_type = 0;  //	Default = this robot does not cloak
		int attack_type = 0; //	Default = this robot attacks by firing (1=lunge)
		int boss_flag = 0;   //	Default = robot is not a boss.
		int see_sound = 170;
		int attack_sound = 171;
		int claw_sound = 190;

		check(N_robot_types < 30);

		if (skip) {
			Robot_info[N_robot_types].model_num = -1;
			N_robot_types++;
			Num_total_object_types++;
			clear_to_end_of_line();
			return;
		}

		model_name[0] = strtok(NULL, space);
		first_bitmap_num[0] = N_ObjBitmapPtrs;
		n_models = 1;

		// Process bitmaps
		bm_flag = BM_ROBOT;
		arg = strtok(NULL, space);
		while (arg != NULL) {
			equal_ptr = strchr(arg, '=');
			if (equal_ptr) {
				*equal_ptr = '\0';
				equal_ptr++;
				// if we have john=cool, arg is 'john' and equal_ptr is 'cool'
				if (!d_stricmp(arg, "exp1_vclip")) {
					exp1_vclip_num = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "exp2_vclip")) {
					exp2_vclip_num = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "exp1_sound")) {
					exp1_sound_num = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "exp2_sound")) {
					exp2_sound_num = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "lighting")) {
					lighting = fl2f(atof(equal_ptr));
					if ((lighting < 0) || (lighting > F1_0)) {
						Error("In bitmaps.tbl, lighting value of %.2f is out of range 0..1.\n", f2fl(lighting));
					}
				} else if (!d_stricmp(arg, "weapon_type")) {
					weapon_type = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "strength")) {
					strength = i2f(atoi(equal_ptr));
				} else if (!d_stricmp(arg, "mass")) {
					mass = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "drag")) {
					drag = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "contains_id")) {
					contains_id = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "contains_type")) {
					contains_type = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "contains_count")) {
					contains_count = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "contains_prob")) {
					contains_prob = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "cloak_type")) {
					cloak_type = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "attack_type")) {
					attack_type = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "boss")) {
					boss_flag = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "score_value")) {
					score_value = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "see_sound")) {
					see_sound = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "attack_sound")) {
					attack_sound = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "claw_sound")) {
					claw_sound = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "name")) {
					check(strlen(equal_ptr) < 16); //	Oops, name too long.
					strcpy(name, &equal_ptr[1]);
					name[strlen(name) - 1] = 0;
				} else if (!d_stricmp(arg, "simple_model")) {
					check(n_models < MAX_MODEL_VARIANTS);
					model_name[n_models] = equal_ptr;
					first_bitmap_num[n_models] = N_ObjBitmapPtrs;
					n_models++;
				}
			} else { // Must be a texture specification...
				load_polymodel_bitmap(skip, arg);
			}
			arg = strtok(NULL, space);
		}

		// clear out anim info
		for (g = 0; g < MAX_GUNS + 1; g++)
			for (s = 0; s < N_ANIM_STATES; s++)
				Robot_info[N_robot_types].anim_states[g][s].n_joints = 0; // inialize to zero

		first_bitmap_num[n_models] = N_ObjBitmapPtrs;

		for (i = 0; i < n_models; i++) {
			int n_textures;
			int model_num, last_model_num = 0;

			n_textures = first_bitmap_num[i + 1] - first_bitmap_num[i];

			model_num = load_polygon_model(model_name[i], n_textures, first_bitmap_num[i], (i == 0) ? &Robot_info[N_robot_types] : NULL);

			if (i == 0)
				Robot_info[N_robot_types].model_num = model_num;
			else
				Polygon_models[last_model_num].simpler_model = model_num + 1;

			last_model_num = model_num;
		}

		ObjType[Num_total_object_types] = 1;
		ObjId[Num_total_object_types] = N_robot_types;

		Robot_info[N_robot_types].exp1_vclip_num = exp1_vclip_num;
		Robot_info[N_robot_types].exp2_vclip_num = exp2_vclip_num;
		Robot_info[N_robot_types].exp1_sound_num = exp1_sound_num;
		Robot_info[N_robot_types].exp2_sound_num = exp2_sound_num;
		Robot_info[N_robot_types].lighting = lighting;
		Robot_info[N_robot_types].weapon_type = weapon_type;
		Robot_info[N_robot_types].strength = strength;
		Robot_info[N_robot_types].mass = mass;
		Robot_info[N_robot_types].drag = drag;
		Robot_info[N_robot_types].cloak_type = cloak_type;
		Robot_info[N_robot_types].attack_type = attack_type;
		Robot_info[N_robot_types].boss_flag = boss_flag;

		Robot_info[N_robot_types].contains_id = contains_id;
		Robot_info[N_robot_types].contains_count = contains_count;
		Robot_info[N_robot_types].contains_prob = contains_prob;
		Robot_info[N_robot_types].score_value = score_value;
		Robot_info[N_robot_types].see_sound = see_sound;
		Robot_info[N_robot_types].attack_sound = attack_sound;
		Robot_info[N_robot_types].claw_sound = claw_sound;

		if (contains_type)
			Robot_info[N_robot_types].contains_type = OBJ_ROBOT;
		else
			Robot_info[N_robot_types].contains_type = OBJ_POWERUP;

		strcpy(Robot_names[N_robot_types], name);

		N_robot_types++;
		Num_total_object_types++;
	}

	// read a polygon object of some sort
	void bm_read_object(int skip)
	{
		char *model_name, *model_name_dead = NULL;
		int first_bitmap_num, first_bitmap_num_dead = 0, n_normal_bitmaps;
		char *equal_ptr;
		short model_num;
		fix lighting = F1_0 / 2; // Default
		int type = -1;
		fix strength = 0;

		model_name = strtok(NULL, space);

		// Process bitmaps
		bm_flag = BM_NONE;
		arg = strtok(NULL, space);
		first_bitmap_num = N_ObjBitmapPtrs;

		while (arg != NULL) {

			equal_ptr = strchr(arg, '=');

			if (equal_ptr) {
				*equal_ptr = '\0';
				equal_ptr++;

				// if we have john=cool, arg is 'john' and equal_ptr is 'cool'

				if (!d_stricmp(arg, "type")) {
					if (!d_stricmp(equal_ptr, "controlcen"))
						type = 4;
					else if (!d_stricmp(equal_ptr, "clutter"))
						type = 6;
					else if (!d_stricmp(equal_ptr, "exit"))
						type = 7;
				} else if (!d_stricmp(arg, "dead_pof")) {
					model_name_dead = equal_ptr;
					first_bitmap_num_dead = N_ObjBitmapPtrs;
				} else if (!d_stricmp(arg, "lighting")) {
					lighting = fl2f(atof(equal_ptr));
					if ((lighting < 0) || (lighting > F1_0)) {
						Error("In bitmaps.tbl, lighting value of %.2f is out of range 0..1.\n", f2fl(lighting));
					}
				} else if (!d_stricmp(arg, "strength")) {
					strength = fl2f(atof(equal_ptr));
				}
			} else { // Must be a texture specification...
				load_polymodel_bitmap(skip, arg);
			}
			arg = strtok(NULL, space);
		}

		if (model_name_dead)
			n_normal_bitmaps = first_bitmap_num_dead - first_bitmap_num;
		else
			n_normal_bitmaps = N_ObjBitmapPtrs - first_bitmap_num;

		model_num = load_polygon_model(model_name, n_normal_bitmaps, first_bitmap_num, NULL);

		if (type == 4)
			Reactors[0].n_guns = read_model_guns(model_name, Reactors[0].gun_points, Reactors[0].gun_dirs, NULL);

		if (model_name_dead)
			Dead_modelnums[model_num] = load_polygon_model(model_name_dead, N_ObjBitmapPtrs - first_bitmap_num_dead, first_bitmap_num_dead, NULL);
		else
			Dead_modelnums[model_num] = -1;

		if (type == -1)
			Error("No object type specfied for object in BITMAPS.TBL on line %d\n", linenum);

		ObjType[Num_total_object_types] = type;
		ObjId[Num_total_object_types] = model_num;
		ObjStrength[Num_total_object_types] = strength;

		Num_total_object_types++;

		if (type == 7) {
			exit_modelnum = model_num;
			destroyed_exit_modelnum = Dead_modelnums[model_num];
		}
	}

	void bm_read_player_ship(int skip)
	{
		char *model_name_dying = NULL;
		char *model_name[MAX_MODEL_VARIANTS];
		int n_models = 0, i;
		int first_bitmap_num[MAX_MODEL_VARIANTS + 1];
		char *equal_ptr;
		robot_info ri;
		int last_multi_bitmap_num = -1;

		// Process bitmaps
		bm_flag = BM_NONE;

		arg = strtok(NULL, space);

		Player_ship->mass = Player_ship->drag = 0; // stupid defaults
		Player_ship->expl_vclip_num = -1;

		while (arg != NULL) {

			equal_ptr = strchr(arg, '=');

			if (equal_ptr) {

				*equal_ptr = '\0';
				equal_ptr++;

				// if we have john=cool, arg is 'john' and equal_ptr is 'cool'

				if (!d_stricmp(arg, "model")) {
					check(n_models == 0);
					model_name[0] = equal_ptr;
					first_bitmap_num[0] = N_ObjBitmapPtrs;
					n_models = 1;
				} else if (!d_stricmp(arg, "simple_model")) {
					check(n_models < MAX_MODEL_VARIANTS);
					model_name[n_models] = equal_ptr;
					first_bitmap_num[n_models] = N_ObjBitmapPtrs;
					n_models++;

					if (First_multi_bitmap_num != -1 && last_multi_bitmap_num == -1)
						last_multi_bitmap_num = N_ObjBitmapPtrs;
				} else if (!d_stricmp(arg, "mass"))
					Player_ship->mass = fl2f(atof(equal_ptr));
				else if (!d_stricmp(arg, "drag"))
					Player_ship->drag = fl2f(atof(equal_ptr));
				//			else if (!d_stricmp( arg, "low_thrust" ))
				//				Player_ship->low_thrust = fl2f(atof(equal_ptr));
				else if (!d_stricmp(arg, "max_thrust"))
					Player_ship->max_thrust = fl2f(atof(equal_ptr));
				else if (!d_stricmp(arg, "reverse_thrust"))
					Player_ship->reverse_thrust = fl2f(atof(equal_ptr));
				else if (!d_stricmp(arg, "brakes"))
					Player_ship->brakes = fl2f(atof(equal_ptr));
				else if (!d_stricmp(arg, "wiggle"))
					Player_ship->wiggle = fl2f(atof(equal_ptr));
				else if (!d_stricmp(arg, "max_rotthrust"))
					Player_ship->max_rotthrust = fl2f(atof(equal_ptr));
				else if (!d_stricmp(arg, "dying_pof"))
					model_name_dying = equal_ptr;
				else if (!d_stricmp(arg, "expl_vclip_num"))
					Player_ship->expl_vclip_num = atoi(equal_ptr);
			} else if (!d_stricmp(arg, "multi_textures")) {

				First_multi_bitmap_num = N_ObjBitmapPtrs;
				first_bitmap_num[n_models] = N_ObjBitmapPtrs;

			} else // Must be a texture specification...

				load_polymodel_bitmap(skip, arg);

			arg = strtok(NULL, space);
		}

		check(n_models > 0 && model_name[0] != NULL);

		if (First_multi_bitmap_num != -1 && last_multi_bitmap_num == -1)
			last_multi_bitmap_num = N_ObjBitmapPtrs;

		if (First_multi_bitmap_num == -1)
			first_bitmap_num[n_models] = N_ObjBitmapPtrs;

#ifdef NETWORK
		check(last_multi_bitmap_num - First_multi_bitmap_num == (MAX_PLAYERS - 1) * 2);
#endif

		for (i = 0; i < n_models; i++) {
			int n_textures;
			int model_num, last_model_num = 0;

			n_textures = first_bitmap_num[i + 1] - first_bitmap_num[i];

			model_num = load_polygon_model(model_name[i], n_textures, first_bitmap_num[i], (i == 0) ? &ri : NULL);

			if (i == 0)
				Player_ship->model_num = model_num;
			else
				Polygon_models[last_model_num].simpler_model = model_num + 1;

			last_model_num = model_num;
		}

		if (model_name_dying) {
			check(n_models);
			Dying_modelnums[Player_ship->model_num] = load_polygon_model(model_name_dying, first_bitmap_num[1] - first_bitmap_num[0], first_bitmap_num[0], NULL);
		}

		check(ri.n_guns == N_PLAYER_GUNS);

		// calc player gun positions

		{
			polymodel *pm;
			robot_info *r;
			vms_vector pnt;
			int mn; // submodel number
			int gun_num;

			r = &ri;
			pm = &Polygon_models[Player_ship->model_num];

			for (gun_num = 0; gun_num < r->n_guns; gun_num++) {

				pnt = r->gun_points[gun_num];
				mn = r->gun_submodels[gun_num];

				// instance up the tree for this gun
				while (mn != 0) {
					vm_vec_add2(&pnt, &pm->submodel_offsets[mn]);
					mn = pm->submodel_parents[mn];
				}

				Player_ship->gun_points[gun_num] = pnt;
			}
		}
	}

	void bm_read_some_file(int skip)
	{

		switch (bm_flag) {
			case BM_COCKPIT: {
				bitmap_index bitmap;
				bitmap = bm_load_sub(skip, arg);
				check(Num_cockpits < 4);
				cockpit_bitmap[Num_cockpits++] = bitmap;

				// bm_flag = BM_NONE;
			} break;
			case BM_GAUGES:
				bm_read_gauges(skip);
				break;
			case BM_WEAPON:
				bm_read_weapon(skip, 0);
				break;
			case BM_VCLIP:
				bm_read_vclip(skip);
				break;
			case BM_ECLIP:
				bm_read_eclip(skip);
				break;
			case BM_TEXTURES: {
				bitmap_index bitmap;
				bitmap = bm_load_sub(skip, arg);
				check(tmap_count < 800);
				TmapList[tmap_count++] = texture_count;
				Textures[texture_count] = bitmap;
				set_texture_name(arg);
				check(texture_count < 800);
				texture_count++;
				NumTextures = texture_count;
			} break;
			case BM_WCLIP:
				bm_read_wclip(skip);
				break;
			default:
				break;
		}
	}

	// ------------------------------------------------------------------------------
	//	If unused_flag is set, then this is just a placeholder.  Don't actually reference vclips or load bbms.
	void bm_read_weapon(int skip, int unused_flag)
	{
		int i, n;
		int n_models = 0;
		char *equal_ptr;
		char *pof_file_inner = NULL;
		char *model_name[MAX_MODEL_VARIANTS];
		int first_bitmap_num[MAX_MODEL_VARIANTS + 1];
		int lighted; // flag for whether is a texture is lighted

		check(N_weapon_types < 30);

		n = N_weapon_types;
		N_weapon_types++;

		if (unused_flag) {
			clear_to_end_of_line();
			return;
		}

		if (skip) {
			clear_to_end_of_line();
			return;
		}

		// Initialize weapon array
		Weapon_info[n].render_type = WEAPON_RENDER_NONE; // 0=laser, 1=blob, 2=object
		Weapon_info[n].bitmap.index = 0;
		Weapon_info[n].model_num = -1;
		Weapon_info[n].model_num_inner = -1;
		Weapon_info[n].blob_size = 0x1000; // size of blob
		Weapon_info[n].flash_vclip = -1;
		Weapon_info[n].flash_sound = SOUND_LASER_FIRED;
		Weapon_info[n].flash_size = 0;
		Weapon_info[n].robot_hit_vclip = -1;
		Weapon_info[n].robot_hit_sound = -1;
		Weapon_info[n].wall_hit_vclip = -1;
		Weapon_info[n].wall_hit_sound = -1;
		Weapon_info[n].impact_size = 0;
		for (i = 0; i < NDL; i++) {
			Weapon_info[n].strength[i] = F1_0;
			Weapon_info[n].speed[i] = F1_0 * 10;
		}
		Weapon_info[n].mass = F1_0;
		Weapon_info[n].thrust = 0;
		Weapon_info[n].drag = 0;
		Weapon_info[n].persistent = 0;

		Weapon_info[n].energy_usage = 0;     //	How much fuel is consumed to fire this weapon.
		Weapon_info[n].ammo_usage = 0;       //	How many units of ammunition it uses.
		Weapon_info[n].fire_wait = F1_0 / 4; //	Time until this weapon can be fired again.
		Weapon_info[n].fire_count = 1;       //	Number of bursts fired from EACH GUN per firing.  For weapons which fire from both sides, 3*fire_count shots will be fired.
		Weapon_info[n].damage_radius = 0;    //	Radius of damage for missiles, not lasers.  Does damage to objects within this radius of hit point.
		                                     //--01/19/95, mk--	Weapon_info[n].damage_force = 0;					//	Force (movement) due to explosion
		Weapon_info[n].destroyable = 1;      //	Weapons default to destroyable
		Weapon_info[n].matter = 0;           //	Weapons default to not being constructed of matter (they are energy!)
		Weapon_info[n].bounce = 0;           //	Weapons default to not bouncing off walls

		Weapon_info[n].lifetime = WEAPON_DEFAULT_LIFETIME; //	Number of bursts fired from EACH GUN per firing.  For weapons which fire from both sides, 3*fire_count shots will be fired.

		Weapon_info[n].po_len_to_width_ratio = F1_0 * 10;

		Weapon_info[n].picture.index = 0;
		Weapon_info[n].homing_flag = 0;

		// Process arguments
		arg = strtok(NULL, space);

		lighted = 1; // assume first texture is lighted

		while (arg != NULL) {
			equal_ptr = strchr(arg, '=');
			if (equal_ptr) {
				*equal_ptr = '\0';
				equal_ptr++;
				// if we have john=cool, arg is 'john' and equal_ptr is 'cool'
				if (!d_stricmp(arg, "laser_bmp")) {
					// Load bitmap with name equal_ptr

					Weapon_info[n].bitmap = bm_load_sub(skip, equal_ptr); // load_polymodel_bitmap(equal_ptr);
					Weapon_info[n].render_type = WEAPON_RENDER_LASER;

				} else if (!d_stricmp(arg, "blob_bmp")) {
					// Load bitmap with name equal_ptr

					Weapon_info[n].bitmap = bm_load_sub(skip, equal_ptr); // load_polymodel_bitmap(equal_ptr);
					Weapon_info[n].render_type = WEAPON_RENDER_BLOB;

				} else if (!d_stricmp(arg, "weapon_vclip")) {
					// Set vclip to play for this weapon.
					Weapon_info[n].bitmap.index = 0;
					Weapon_info[n].render_type = WEAPON_RENDER_VCLIP;
					Weapon_info[n].weapon_vclip = atoi(equal_ptr);

				} else if (!d_stricmp(arg, "none_bmp")) {
					Weapon_info[n].bitmap = bm_load_sub(skip, equal_ptr);
					Weapon_info[n].render_type = WEAPON_RENDER_NONE;

				} else if (!d_stricmp(arg, "weapon_pof")) {
					// Load pof file
					check(n_models == 0);
					model_name[0] = equal_ptr;
					first_bitmap_num[0] = N_ObjBitmapPtrs;
					n_models = 1;
				} else if (!d_stricmp(arg, "simple_model")) {
					check(n_models < MAX_MODEL_VARIANTS);
					model_name[n_models] = equal_ptr;
					first_bitmap_num[n_models] = N_ObjBitmapPtrs;
					n_models++;
				} else if (!d_stricmp(arg, "weapon_pof_inner")) {
					// Load pof file
					pof_file_inner = equal_ptr;
				} else if (!d_stricmp(arg, "strength")) {
					for (i = 0; i < NDL - 1; i++) {
						Weapon_info[n].strength[i] = i2f(atoi(equal_ptr));
						equal_ptr = strtok(NULL, space);
					}
					Weapon_info[n].strength[i] = i2f(atoi(equal_ptr));
				} else if (!d_stricmp(arg, "mass")) {
					Weapon_info[n].mass = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "drag")) {
					Weapon_info[n].drag = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "thrust")) {
					Weapon_info[n].thrust = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "matter")) {
					Weapon_info[n].matter = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "bounce")) {
					Weapon_info[n].bounce = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "speed")) {
					for (i = 0; i < NDL - 1; i++) {
						Weapon_info[n].speed[i] = i2f(atoi(equal_ptr));
						equal_ptr = strtok(NULL, space);
					}
					Weapon_info[n].speed[i] = i2f(atoi(equal_ptr));
				} else if (!d_stricmp(arg, "flash_vclip")) {
					Weapon_info[n].flash_vclip = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "flash_sound")) {
					Weapon_info[n].flash_sound = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "flash_size")) {
					Weapon_info[n].flash_size = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "blob_size")) {
					Weapon_info[n].blob_size = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "robot_hit_vclip")) {
					Weapon_info[n].robot_hit_vclip = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "robot_hit_sound")) {
					Weapon_info[n].robot_hit_sound = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "wall_hit_vclip")) {
					Weapon_info[n].wall_hit_vclip = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "wall_hit_sound")) {
					Weapon_info[n].wall_hit_sound = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "impact_size")) {
					Weapon_info[n].impact_size = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "lighted")) {
					lighted = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "lw_ratio")) {
					Weapon_info[n].po_len_to_width_ratio = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "lightcast")) {
					Weapon_info[n].light = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "persistent")) {
					Weapon_info[n].persistent = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "energy_usage")) {
					Weapon_info[n].energy_usage = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "ammo_usage")) {
					Weapon_info[n].ammo_usage = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "fire_wait")) {
					Weapon_info[n].fire_wait = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "fire_count")) {
					Weapon_info[n].fire_count = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "damage_radius")) {
					Weapon_info[n].damage_radius = fl2f(atof(equal_ptr));
					//--01/19/95, mk--			} else if (!d_stricmp(arg, "damage_force" )) {
					//--01/19/95, mk--				Weapon_info[n].damage_force = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "lifetime")) {
					Weapon_info[n].lifetime = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "destroyable")) {
					Weapon_info[n].destroyable = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "picture")) {
					Weapon_info[n].picture = bm_load_sub(skip, equal_ptr);
				} else if (!d_stricmp(arg, "homing")) {
					Weapon_info[n].homing_flag = !!atoi(equal_ptr);
				}
			} else { // Must be a texture specification...
				grs_bitmap *bm;

				bm = load_polymodel_bitmap(skip, arg);
				if (bm && !lighted)
					bm->bm_flags |= BM_FLAG_NO_LIGHTING;

				lighted = 1; // default for next bitmap is lighted
			}
			arg = strtok(NULL, space);
		}

		first_bitmap_num[n_models] = N_ObjBitmapPtrs;

		for (i = 0; i < n_models; i++) {
			int n_textures;
			int model_num, last_model_num = 0;

			n_textures = first_bitmap_num[i + 1] - first_bitmap_num[i];

			model_num = load_polygon_model(model_name[i], n_textures, first_bitmap_num[i], NULL);

			if (i == 0) {
				Weapon_info[n].render_type = WEAPON_RENDER_POLYMODEL;
				Weapon_info[n].model_num = model_num;
			} else
				Polygon_models[last_model_num].simpler_model = model_num + 1;

			last_model_num = model_num;
		}

		if (pof_file_inner) {
			check(n_models);
			Weapon_info[n].model_num_inner = load_polygon_model(pof_file_inner, first_bitmap_num[1] - first_bitmap_num[0], first_bitmap_num[0], NULL);
		}
	}

// ------------------------------------------------------------------------------
#define DEFAULT_POWERUP_SIZE i2f(3)

	void bm_read_powerup(int unused_flag)
	{
		int n;
		char *equal_ptr;

		check(N_powerup_types < 29);

		n = N_powerup_types;
		N_powerup_types++;

		if (unused_flag) {
			clear_to_end_of_line();
			return;
		}

		// Initialize powerup array
		Powerup_info[n].light = F1_0 / 3; //	Default lighting value.
		Powerup_info[n].vclip_num = -1;
		Powerup_info[n].hit_sound = -1;
		Powerup_info[n].size = DEFAULT_POWERUP_SIZE;
		Powerup_names[n][0] = 0;

		// Process arguments
		arg = strtok(NULL, space);

		while (arg != NULL) {
			equal_ptr = strchr(arg, '=');
			if (equal_ptr) {
				*equal_ptr = '\0';
				equal_ptr++;
				// if we have john=cool, arg is 'john' and equal_ptr is 'cool'
				if (!d_stricmp(arg, "vclip_num")) {
					Powerup_info[n].vclip_num = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "light")) {
					Powerup_info[n].light = fl2f(atof(equal_ptr));
				} else if (!d_stricmp(arg, "hit_sound")) {
					Powerup_info[n].hit_sound = atoi(equal_ptr);
				} else if (!d_stricmp(arg, "name")) {
					check(strlen(equal_ptr) < 16); //	Oops, name too long.
					strcpy(Powerup_names[n], &equal_ptr[1]);
					Powerup_names[n][strlen(Powerup_names[n]) - 1] = 0;
				} else if (!d_stricmp(arg, "size")) {
					Powerup_info[n].size = fl2f(atof(equal_ptr));
				}
			}
			arg = strtok(NULL, space);
		}

		ObjType[Num_total_object_types] = 3;
		ObjId[Num_total_object_types] = n;
		Num_total_object_types++;
	}

	void bm_read_hostage()
	{
		int n;
		char *equal_ptr;

		check(N_hostage_types < 1);

		n = N_hostage_types;
		N_hostage_types++;

		// Process arguments
		arg = strtok(NULL, space);

		while (arg != NULL) {
			equal_ptr = strchr(arg, '=');
			if (equal_ptr) {
				*equal_ptr = '\0';
				equal_ptr++;

				if (!d_stricmp(arg, "vclip_num"))
					Hostage_vclip_num[n] = atoi(equal_ptr);
			}

			arg = strtok(NULL, space);
		}

		ObjType[Num_total_object_types] = 2;
		ObjId[Num_total_object_types] = n;
		Num_total_object_types++;
	}

	float get_float()
	{
		char *xarg;

		xarg = strtok(NULL, space);
		return atof(xarg);
	}

	// parse an int
	int get_int()
	{
		char *xarg;

		xarg = strtok(NULL, space);
		return atoi(xarg);
	}
};
#undef IFTOK
#endif
