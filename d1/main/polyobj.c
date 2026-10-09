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
 * Hacked-in polygon objects
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef DRIVE
#include "drive.h"
#else
#include "inferno.h"
#endif
#include "polyobj.h"
#include "vecmat.h"
#include "3d.h"
#include "dxxerror.h"
#include "u_mem.h"
#include "args.h"
#ifndef DRIVE
#include "texmap.h"
#include "bm.h"
#include "textures.h"
#include "object.h"
#include "lighting.h"
#include "piggy.h"
#endif
#include "byteswap.h"
#include "render.h"
#include "multi.h"
#ifdef OGL
#include "ogl_init.h"
#endif
#ifdef ANDROID
#include "android_visual_policy.h"
#endif
#include "xmodel.h"
#include "d1_shareware_model.h"
#include "d1_shareware_joints.h"

polymodel Polygon_models[MAX_POLYGON_MODELS];	// = {&bot11,&bot17,&robot_s2,&robot_b2,&bot11,&bot17,&robot_s2,&robot_b2};

int N_polygon_models = 0;

#define MAX_POLYGON_VECS 1000
g3s_point robot_points[MAX_POLYGON_VECS];

#define MODEL_BUF_SIZE 32768

#ifdef DRIVE
#define robot_info void
#else
//set the animation angles for this robot.  Gun fields of robot info must
//be filled in.
void robot_set_angles(robot_info *r,polymodel *pm,vms_angvec angs[N_ANIM_STATES][MAX_SUBMODELS]);
#endif

#ifdef WORDS_NEED_ALIGNMENT
ubyte * old_dest(chunk o) // return where chunk is (in unaligned struct)
{
	return o.old_base + INTEL_SHORT(*((short *)(o.old_base + o.offset)));
}

ubyte * new_dest(chunk o) // return where chunk is (in aligned struct)
{
	return o.new_base + INTEL_SHORT(*((short *)(o.old_base + o.offset))) + o.correction;
}

/*
 * find chunk with smallest address
 */
int get_first_chunks_index(chunk *chunk_list, int no_chunks)
{
	int i, first_index = 0;
	Assert(no_chunks >= 1);
	for (i = 1; i < no_chunks; i++)
		if (old_dest(chunk_list[i]) < old_dest(chunk_list[first_index]))
			first_index = i;
	return first_index;
}
#define SHIFT_SPACE 500 // increase if insufficent

void align_polygon_model_data(polymodel *pm)
{
	int i, chunk_len;
	int total_correction = 0;
	ubyte *cur_old, *cur_new;
	chunk cur_ch;
	chunk ch_list[MAX_CHUNKS];
	int no_chunks = 0;
	int tmp_size = pm->model_data_size + SHIFT_SPACE;
	ubyte *tmp = d_malloc(tmp_size); // where we build the aligned version of pm->model_data

	Assert(tmp != NULL);
	//start with first chunk (is always aligned!)
	cur_old = pm->model_data;
	cur_new = tmp;
	chunk_len = get_chunks(cur_old, cur_new, ch_list, &no_chunks);
	memcpy(cur_new, cur_old, chunk_len);
	while (no_chunks > 0) {
		int first_index = get_first_chunks_index(ch_list, no_chunks);
		cur_ch = ch_list[first_index];
		// remove first chunk from array:
		no_chunks--;
		for (i = first_index; i < no_chunks; i++)
			ch_list[i] = ch_list[i + 1];
		// if (new) address unaligned:
		if ((u_int32_t)new_dest(cur_ch) % 4L != 0) {
			// calculate how much to move to be aligned
			short to_shift = 4 - (u_int32_t)new_dest(cur_ch) % 4L;
			// correct chunks' addresses
			cur_ch.correction += to_shift;
			for (i = 0; i < no_chunks; i++)
				ch_list[i].correction += to_shift;
			total_correction += to_shift;
			Assert((u_int32_t)new_dest(cur_ch) % 4L == 0);
			Assert(total_correction <= SHIFT_SPACE); // if you get this, increase SHIFT_SPACE
		}
		//write (corrected) chunk for current chunk:
		*((short *)(cur_ch.new_base + cur_ch.offset))
		  = INTEL_SHORT(cur_ch.correction
				+ INTEL_SHORT(*((short *)(cur_ch.old_base + cur_ch.offset))));
		//write (correctly aligned) chunk:
		cur_old = old_dest(cur_ch);
		cur_new = new_dest(cur_ch);
		chunk_len = get_chunks(cur_old, cur_new, ch_list, &no_chunks);
		memcpy(cur_new, cur_old, chunk_len);
		//correct submodel_ptr's for pm, too
		for (i = 0; i < MAX_SUBMODELS; i++)
			if (pm->model_data + pm->submodel_ptrs[i] >= cur_old
			    && pm->model_data + pm->submodel_ptrs[i] < cur_old + chunk_len)
				pm->submodel_ptrs[i] += (cur_new - tmp) - (cur_old - pm->model_data);
 	}
	d_free(pm->model_data);
	pm->model_data_size += total_correction;
	pm->model_data = 
	d_malloc(pm->model_data_size);
	Assert(pm->model_data != NULL);
	memcpy(pm->model_data, tmp, pm->model_data_size);
	d_free(tmp);
}
#endif //def WORDS_NEED_ALIGNMENT


/* Source decoding does not touch engine tables; these adapters publish D1 data */
static void read_pof_source(const char *filename, ubyte *buffer, size_t capacity, d1_pof_source *source)
{
	PHYSFS_file *file = PHYSFSX_openReadBuffered(filename);
	PHYSFS_sint64 size;
	const char *error;
	if (!file)
		Error("Can't open file <%s>", filename);
	size = PHYSFS_fileLength(file);
	if (size < 0 || size > (PHYSFS_sint64)capacity || PHYSFS_read(file, buffer, 1, (PHYSFS_uint32)size) != size) {
		PHYSFS_close(file);
		Error("Invalid POF file size or read in <%s>", filename);
	}
	PHYSFS_close(file);
	error = d1_pof_read(buffer, (size_t)size, source);
	if (error)
		Error("%s in <%s>", error, filename);
}

static vms_vector pof_engine_vector(d1_pof_vector source)
{
	vms_vector result;
	result.x = source.x;
	result.y = source.y;
	result.z = source.z;
	return result;
}

//reads a binary file containing a 3d model
polymodel *read_model_file(polymodel *pm,char *filename,robot_info *r)
{
	ubyte model_buf[MODEL_BUF_SIZE];
	d1_pof_source source;
	int i;
	d1_pof_bounds bounds;
	const char *bounds_error;
	read_pof_source(filename, model_buf, sizeof(model_buf), &source);
	bounds_error = d1_pof_find_bounds(&source, &bounds);
	if (bounds_error)
		Error("%s in <%s>", bounds_error, filename);
	pm->mins = pof_engine_vector(bounds.mins);
	pm->maxs = pof_engine_vector(bounds.maxs);
	pm->n_models = source.model_count;
	pm->rad = source.radius;
	for (i = 0; i < source.model_count; ++i) {
		pm->submodel_parents[i] = source.parents[i];
		pm->submodel_norms[i] = pof_engine_vector(source.normals[i]);
		pm->submodel_pnts[i] = pof_engine_vector(source.points[i]);
		pm->submodel_offsets[i] = pof_engine_vector(source.translations[i]);
		pm->submodel_rads[i] = source.radii[i];
		pm->submodel_ptrs[i] = source.offsets[i];
		pm->submodel_mins[i] = pof_engine_vector(bounds.submodel_mins[i]);
		pm->submodel_maxs[i] = pof_engine_vector(bounds.submodel_maxs[i]);
	}
	pm->model_data_size = (int)source.instruction_size;
	pm->model_data = d_malloc(source.instruction_size);
	memcpy(pm->model_data, source.instructions, source.instruction_size);
#ifndef DRIVE
	if (r) {
		r->n_guns = source.gun_count;
		for (i = 0; i < source.gun_count; ++i) {
			r->gun_submodels[i] = source.gun_models[i];
			r->gun_points[i] = pof_engine_vector(source.gun_points[i]);
		}
		if (source.has_animation) {
			vms_angvec angles[N_ANIM_STATES][MAX_SUBMODELS] = { 0 };
			int frame;
			for (frame = 0; frame < N_ANIM_STATES; ++frame)
				for (i = 0; i < source.model_count; ++i) {
					angles[frame][i].p = source.animation[frame][i].p;
					angles[frame][i].b = source.animation[frame][i].b;
					angles[frame][i].h = source.animation[frame][i].h;
				}
			if (!d1_shareware_build_joints(r, pm, angles, Robot_joints, &N_robot_joints, MAX_ROBOT_JOINTS))
				Error("Invalid POF robot animation in <%s>", filename);
		}
	}
#endif
#ifdef WORDS_NEED_ALIGNMENT
	align_polygon_model_data(pm);
#endif
#ifdef WORDS_BIGENDIAN
	swap_polygon_model_data(pm->model_data);
#endif
	return pm;
}

//reads the gun information for a model
//fills in arrays gun_points & gun_dirs, returns the number of guns read
int read_model_guns(char *filename,vms_vector *gun_points, vms_vector *gun_dirs, int *gun_submodels)
{
	ubyte model_buf[MODEL_BUF_SIZE];
	d1_pof_source source;
	int i;
	read_pof_source(filename, model_buf, sizeof(model_buf), &source);
	if (source.version < 7)
		Error("Missing gun directions in file <%s>", filename);
	for (i = 0; i < source.gun_count; ++i) {
		if (gun_submodels)
			gun_submodels[i] = source.gun_models[i];
		else if (source.gun_models[i] != 0)
			Error("Invalid gun submodel in file <%s>", filename);
		gun_points[i] = pof_engine_vector(source.gun_points[i]);
		gun_dirs[i] = pof_engine_vector(source.gun_directions[i]);
	}
	return source.gun_count;
}

//free up a model, getting rid of all its memory
void free_model(polymodel *po)
{
	d_free(po->model_data);
}

grs_bitmap *texture_list[MAX_POLYOBJ_TEXTURES];
bitmap_index texture_list_index[MAX_POLYOBJ_TEXTURES];

int alt_textures_to_ship_color(bitmap_index alt_textures[]) {
	bitmap_index *first_player_texture = &multi_player_textures[0][0];
	bitmap_index *last_player_texture = first_player_texture + MAX_PLAYERS * N_PLAYER_SHIP_TEXTURES;

	if (alt_textures && alt_textures >= first_player_texture && alt_textures < last_player_texture)
		return multi_player_tex_color[(alt_textures - first_player_texture) / N_PLAYER_SHIP_TEXTURES];
	return 0;
}

//draw a polygon model

void draw_polygon_model(vms_vector *pos,vms_matrix *orient,vms_angvec *anim_angles,int model_num,int flags,g3s_lrgb light,fix *glow_values,bitmap_index alt_textures[])
{
	polymodel *po;
	int i;
#ifdef OGL
	int allow_xmodel;
#endif

	if (model_num < 0)
		return;

#ifdef OGL
#ifdef ANDROID
	allow_xmodel = android_visual_replacements_allowed();
#else
	allow_xmodel = !(Game_mode & GM_MULTI) || Netgame.AllowCustomModelsTextures;
#endif
	if (allow_xmodel)
		if (xmodel_show_if_loaded(XM_POLYOBJ, model_num, pos, orient, alt_textures_to_ship_color(alt_textures), &light))
			return;
#endif

	Assert(model_num < N_polygon_models);

	po=&Polygon_models[model_num];

	//check if should use simple model
	if (po->simpler_model )					//must have a simpler model
		if (flags==0)							//can't switch if this is debris
			//!!if (!alt_textures) {				//alternate textures might not match
			//alt textures might not match, but in the one case we're using this
			//for on 11/14/94, they do match.  So we leave it in.
			{
				int cnt=1;
				fix depth;
	
				depth = g3_calc_point_depth(pos);		//gets 3d depth

				while (po->simpler_model && depth > cnt++ * Simple_model_threshhold_scale * po->rad)
					po = &Polygon_models[po->simpler_model-1];
			}

	if (alt_textures)
		for (i=0;i<po->n_textures;i++)	{
			texture_list_index[i] = alt_textures[i];
			texture_list[i] = &GameBitmaps[alt_textures[i].index];
		}
	else
		for (i=0;i<po->n_textures;i++)	{
			texture_list_index[i] = ObjBitmaps[ObjBitmapPtrs[po->first_texture+i]];
			texture_list[i] = &GameBitmaps[ObjBitmaps[ObjBitmapPtrs[po->first_texture+i]].index];
		}

	// Make sure the textures for this object are paged in...
	piggy_page_flushed = 0;
	for (i=0;i<po->n_textures;i++)	
		PIGGY_PAGE_IN( texture_list_index[i] );
	// Hmmm... cache got flushed in the middle of paging all these in,
	// so we need to reread them all in.
	if (piggy_page_flushed)	{
		piggy_page_flushed = 0;
		for (i=0;i<po->n_textures;i++)	
			PIGGY_PAGE_IN( texture_list_index[i] );
	}
	// Make sure that they can all fit in memory.
	Assert( piggy_page_flushed == 0 );

	g3_start_instance_matrix(pos,orient);

	g3_set_interp_points(robot_points);

	if (flags == 0)		//draw entire object

		g3_draw_polygon_model(po->model_data,texture_list,anim_angles,light,glow_values);

	else {
		int i;
	
		for (i=0;flags;flags>>=1,i++)
			if (flags & 1) {
				vms_vector ofs;

				Assert(i < po->n_models);

				//if submodel, rotate around its center point, not pivot point
	
				vm_vec_avg(&ofs,&po->submodel_mins[i],&po->submodel_maxs[i]);
				vm_vec_negate(&ofs);
				g3_start_instance_matrix(&ofs,NULL);
	
				g3_draw_polygon_model(&po->model_data[po->submodel_ptrs[i]],texture_list,anim_angles,light,glow_values);
	
				g3_done_instance();
			}	
	}

	g3_done_instance();

}

void free_polygon_models()
{
	int i;

	for (i=0;i<N_polygon_models;i++) {
		free_model(&Polygon_models[i]);
	}

}

void polyobj_find_min_max(polymodel *pm)
{
	ushort nverts;
	vms_vector *vp;
	ushort *data,type;
	int m;
	vms_vector *big_mn,*big_mx;
	
	big_mn = &pm->mins;
	big_mx = &pm->maxs;

	for (m=0;m<pm->n_models;m++) {
		vms_vector *mn,*mx,*ofs;

		mn = &pm->submodel_mins[m];
		mx = &pm->submodel_maxs[m];
		ofs= &pm->submodel_offsets[m];

		data = (ushort *)&pm->model_data[pm->submodel_ptrs[m]];
	
		type = *data++;
	
		Assert(type == 7 || type == 1);
	
		nverts = *data++;
	
		if (type==7)
			data+=2;		//skip start & pad
	
		vp = (vms_vector *) data;
	
		*mn = *mx = *vp++; nverts--;

		if (m==0)
			*big_mn = *big_mx = *mn;
	
		while (nverts--) {
			if (vp->x > mx->x) mx->x = vp->x;
			if (vp->y > mx->y) mx->y = vp->y;
			if (vp->z > mx->z) mx->z = vp->z;
	
			if (vp->x < mn->x) mn->x = vp->x;
			if (vp->y < mn->y) mn->y = vp->y;
			if (vp->z < mn->z) mn->z = vp->z;
	
			if (vp->x+ofs->x > big_mx->x) big_mx->x = vp->x+ofs->x;
			if (vp->y+ofs->y > big_mx->y) big_mx->y = vp->y+ofs->y;
			if (vp->z+ofs->z > big_mx->z) big_mx->z = vp->z+ofs->z;
	
			if (vp->x+ofs->x < big_mn->x) big_mn->x = vp->x+ofs->x;
			if (vp->y+ofs->y < big_mn->y) big_mn->y = vp->y+ofs->y;
			if (vp->z+ofs->z < big_mn->z) big_mn->z = vp->z+ofs->z;
	
			vp++;
		}
	}
}

char Pof_names[MAX_POLYGON_MODELS][13];

//returns the number of this model
#ifndef DRIVE
int load_polygon_model(char *filename,int n_textures,int first_texture,robot_info *r)
#else
int load_polygon_model(char *filename,int n_textures,grs_bitmap ***textures)
#endif
{
	#ifdef DRIVE
	#define r NULL
	#endif

	Assert(N_polygon_models < MAX_POLYGON_MODELS);
	Assert(n_textures < MAX_POLYOBJ_TEXTURES);

	Assert(strlen(filename) <= 12);
	strcpy(Pof_names[N_polygon_models],filename);

	read_model_file(&Polygon_models[N_polygon_models],filename,r);


	g3_init_polygon_model(Polygon_models[N_polygon_models].model_data);

	if (highest_texture_num+1 != n_textures)
		Error("Model <%s> references %d textures but specifies %d.",filename,highest_texture_num+1,n_textures);

	Polygon_models[N_polygon_models].n_textures = n_textures;
	Polygon_models[N_polygon_models].first_texture = first_texture;
	Polygon_models[N_polygon_models].simpler_model = 0;

//	Assert(polygon_models[N_polygon_models]!=NULL);

	N_polygon_models++;

	return N_polygon_models-1;

}


void init_polygon_models()
{
	N_polygon_models = 0;
}

//compare against this size when figuring how far to place eye for picture
#define BASE_MODEL_SIZE 0x28000

#define DEFAULT_VIEW_DIST 0x60000

//draws the given model in the current canvas.  The distance is set to
//more-or-less fill the canvas.  Note that this routine actually renders
//into an off-screen canvas that it creates, then copies to the current
//canvas.
void draw_model_picture_animated_scene(int mn,vms_angvec *orient_angles,vms_angvec *anim_angles,fix offset_x,fix offset_y,fix distance_offset,fix view_radius,model_picture_extra_drawer draw_extra,void *data)
{
	vms_vector	temp_pos=ZERO_VECTOR;
	vms_matrix	temp_orient = IDENTITY_MATRIX;
	g3s_lrgb	lrgb = { f1_0, f1_0, f1_0 };

	Assert(mn>=0 && mn<N_polygon_models);

	gr_clear_canvas( BM_XRGB(0,0,0) );
	g3_start_frame();
	g3_set_view_matrix(&temp_pos,&temp_orient,0x9000);

	if (view_radius == 0)
		view_radius = Polygon_models[mn].rad;
	if (view_radius != 0)
		temp_pos.z = fixmuldiv(DEFAULT_VIEW_DIST,view_radius,BASE_MODEL_SIZE);
	else
		temp_pos.z = DEFAULT_VIEW_DIST;
	temp_pos.x = offset_x;
	temp_pos.y = offset_y;
	temp_pos.z += distance_offset;

	vm_angles_2_matrix(&temp_orient, orient_angles);
	draw_polygon_model(&temp_pos,&temp_orient,anim_angles,mn,0,lrgb,NULL,NULL);
	if (draw_extra)
		draw_extra(&temp_pos,data);
	g3_end_frame();
}

void draw_model_picture(int mn,vms_angvec *orient_angles)
{
	draw_model_picture_animated_scene(mn,orient_angles,NULL,0,0,0,0,NULL,NULL);
}

/*
 * reads n polymodel structs from a PHYSFS_file
 */
extern int polymodel_read_n(polymodel *pm, int n, PHYSFS_file *fp)
{
	int i, j;

	for (i = 0; i < n; i++) {
		pm[i].n_models = PHYSFSX_readInt(fp);
		pm[i].model_data_size = PHYSFSX_readInt(fp);
		pm[i].model_data = (ubyte *) (size_t)PHYSFSX_readInt(fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			pm[i].submodel_ptrs[j] = PHYSFSX_readInt(fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			PHYSFSX_readVector(&(pm[i].submodel_offsets[j]), fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			PHYSFSX_readVector(&(pm[i].submodel_norms[j]), fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			PHYSFSX_readVector(&(pm[i].submodel_pnts[j]), fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			pm[i].submodel_rads[j] = PHYSFSX_readFix(fp);
		PHYSFS_read(fp, pm[i].submodel_parents, MAX_SUBMODELS, 1);
		for (j = 0; j < MAX_SUBMODELS; j++)
			PHYSFSX_readVector(&(pm[i].submodel_mins[j]), fp);
		for (j = 0; j < MAX_SUBMODELS; j++)
			PHYSFSX_readVector(&(pm[i].submodel_maxs[j]), fp);
		PHYSFSX_readVector(&(pm[i].mins), fp);
		PHYSFSX_readVector(&(pm[i].maxs), fp);
		pm[i].rad = PHYSFSX_readFix(fp);
		pm[i].n_textures = PHYSFSX_readByte(fp);
		pm[i].first_texture = PHYSFSX_readShort(fp);
		pm[i].simpler_model = PHYSFSX_readByte(fp);
	}
	return i;
}


/*
 * routine which allocates, reads, and inits a polymodel's model_data
 */
void polygon_model_data_read(polymodel *pm, PHYSFS_file *fp)
{
	pm->model_data = d_malloc(pm->model_data_size);
	Assert(pm->model_data != NULL);
	PHYSFS_read(fp, pm->model_data, sizeof(ubyte), pm->model_data_size);
#ifdef WORDS_NEED_ALIGNMENT
	align_polygon_model_data(pm);
#endif
#ifdef WORDS_BIGENDIAN
	swap_polygon_model_data(pm->model_data);
#endif
}
