/* Optional Guide-Bot source preparation
 * Reads an explicit D2 package without publishing or borrowing live game data
 * Record decoding uses the engine's readers after checking each disk span */

#include <string.h>
#include "d1_in_d2_assets.h"
#include "d1_pig_validation.h"
#include "gauges.h"
#include "interp.h"
#include "args.h"
#include "d1_in_d2.h"
#include "laser.h"
#include "makesig.h"
#include "sounds.h"
#include "textures.h"
#include "u_mem.h"
#include "console.h"

enum source_kind {
	SOURCE_ROBOT, SOURCE_MODEL, SOURCE_JOINT, SOURCE_WEAPON, SOURCE_POWERUP,
	SOURCE_VCLIP, SOURCE_EFFECT, SOURCE_TEXTURE, SOURCE_OBJPTR, SOURCE_OBJBMP,
	SOURCE_BITMAP, SOURCE_SOUND, SOURCE_SAMPLE, SOURCE_KINDS
};

void d1_in_d2_prepare_available_guidebot(d1_asset_generation *base)
{
	/* Optional registered D2 package; never capture whichever live tables a
	 * previous mission left behind. Prepare after D1 custom definitions */
	const char *error = NULL;
	const char *sound = PHYSFSX_exists("descent2.s22", 1) ? "descent2.s22" : "descent2.s11";
	const d1_guidebot_source source = {
		"descent2.ham", "groupa.pig", "groupa.256", sound,
		!strcmp(sound, "descent2.s22") ? SAMPLE_RATE_22K : SAMPLE_RATE_11K
	};
	if (!PHYSFSX_exists(source.ham_path, 1) || !PHYSFSX_exists(source.pig_path, 1) ||
	    !PHYSFSX_exists(source.palette_path, 1) || !PHYSFSX_exists(source.sound_path, 1))
		return;
	if (!d1_in_d2_prepare_guidebot_extension(base, &source, &error))
		con_printf(CON_URGENT, "Optional D1 Guide-Bot unavailable: %s\n", error ? error : "invalid D2 assets");
}

struct d1_guidebot_assets {
	int count[SOURCE_KINDS];
	/* 0 absent, 1 pending, 2 visited; source IDs stay distinct until remapping */
	ubyte selected[SOURCE_KINDS][MAX_BITMAP_FILES];
	robot_info robots[MAX_ROBOT_TYPES];
	polymodel models[MAX_POLYGON_MODELS];
	PHYSFS_sint64 model_offsets[MAX_POLYGON_MODELS];
	int dying[MAX_POLYGON_MODELS], dead[MAX_POLYGON_MODELS];
	jointpos joints[MAX_ROBOT_JOINTS];
	weapon_info weapons[MAX_WEAPON_TYPES];
	powerup_type_info powerups[MAX_POWERUP_TYPES];
	vclip vclips[VCLIP_MAXNUM];
	eclip effects[MAX_EFFECTS];
	bitmap_index textures[MAX_TEXTURES], obj_bitmaps[MAX_OBJ_BITMAPS];
	tmap_info texture_info[MAX_TEXTURES];
	ushort obj_ptrs[MAX_OBJ_BITMAPS];
	ubyte sound_map[2][MAX_SOUNDS];
	d1_bitmap_generation *images;
	d1_sound_generation samples;
	d1_guidebot_asset_stats stats;
	int runtime_map[SOURCE_KINDS][MAX_BITMAP_FILES];
	int runtime_base[SOURCE_KINDS], runtime_end[SOURCE_KINDS];
	ubyte identity[32];
	int prepared;
};

/* Borrowed lookup, cleared by generation retirement; never a second owner */
static d1_guidebot_assets *Published_guidebot;

static int has_bytes(PHYSFS_file *file, PHYSFS_sint64 size)
{
	return d1_pig_validate_span(PHYSFS_fileLength(file), PHYSFS_tell(file), size);
}

static int read_count(PHYSFS_file *file, int capacity, int record_size, int *count)
{
	if (!has_bytes(file, 4))
		return 0;
	*count = PHYSFSX_readInt(file);
	return *count >= 0 && *count <= capacity && has_bytes(file, (PHYSFS_sint64)*count * record_size);
}

static int skip_records(PHYSFS_file *file, int capacity, int record_size)
{
	int count;
	return read_count(file, capacity, record_size, &count) &&
		PHYSFS_seek(file, PHYSFS_tell(file) + (PHYSFS_sint64)count * record_size);
}

/* This private reader deliberately supports the registered v3 package first
 * An unsupported optional source leaves the caller's native D1 data untouched */
static int read_definitions(d1_guidebot_assets *assets, PHYSFS_file *file, const char **error)
{
	int i, n;
	*error = "optional HAM header/version";
	if (PHYSFS_fileLength(file) > 0x7fffffff || !has_bytes(file, 8) ||
	    PHYSFSX_readInt(file) != MAKE_SIG('!','M','A','H') || PHYSFSX_readInt(file) != 3)
		return 0;
	*error = "optional texture table";
	if (!read_count(file, MAX_TEXTURES, 22, &assets->count[SOURCE_TEXTURE]))
		return 0;
	bitmap_index_read_n(assets->textures, assets->count[SOURCE_TEXTURE], file);
	tmap_info_read_n(assets->texture_info, assets->count[SOURCE_TEXTURE], file);
	*error = "optional sound maps";
	if (!read_count(file, MAX_SOUNDS, 2, &assets->count[SOURCE_SOUND]))
		return 0;
	n = assets->count[SOURCE_SOUND];
	if (PHYSFS_read(file, assets->sound_map[0], 1, n) != n || PHYSFS_read(file, assets->sound_map[1], 1, n) != n)
		return 0;
	*error = "optional vclip table";
	if (!read_count(file, VCLIP_MAXNUM, 82, &assets->count[SOURCE_VCLIP]))
		return 0;
	vclip_read_n(assets->vclips, assets->count[SOURCE_VCLIP], file);
	*error = "optional effect table";
	if (!read_count(file, MAX_EFFECTS, 130, &assets->count[SOURCE_EFFECT]))
		return 0;
	eclip_read_n(assets->effects, assets->count[SOURCE_EFFECT], file);
	*error = "optional wall animation table";
	if (!skip_records(file, MAX_WALL_ANIMS, 126))
		return 0;
	*error = "optional robot table";
	if (!read_count(file, MAX_ROBOT_TYPES, 480, &assets->count[SOURCE_ROBOT]))
		return 0;
	robot_info_read_n(assets->robots, assets->count[SOURCE_ROBOT], file);
	*error = "optional joint table";
	if (!read_count(file, MAX_ROBOT_JOINTS, 8, &assets->count[SOURCE_JOINT]))
		return 0;
	jointpos_read_n(assets->joints, assets->count[SOURCE_JOINT], file);
	*error = "optional weapon table";
	if (!read_count(file, MAX_WEAPON_TYPES, 125, &assets->count[SOURCE_WEAPON]))
		return 0;
	weapon_info_read_n(assets->weapons, assets->count[SOURCE_WEAPON], file, 3);
	*error = "optional powerup table";
	if (!read_count(file, MAX_POWERUP_TYPES, 16, &assets->count[SOURCE_POWERUP]))
		return 0;
	powerup_type_info_read_n(assets->powerups, assets->count[SOURCE_POWERUP], file);
	*error = "optional model table";
	if (!read_count(file, MAX_POLYGON_MODELS, 734, &assets->count[SOURCE_MODEL]))
		return 0;
	for (i = 0; i < assets->count[SOURCE_MODEL]; i++) {
		polymodel_read(&assets->models[i], file);
		/* The serialized pointer word is never owned memory */
		assets->models[i].model_data = NULL;
	}
	*error = "optional model data spans";
	for (i = 0; i < assets->count[SOURCE_MODEL]; i++) {
		int size = assets->models[i].model_data_size;
		assets->model_offsets[i] = PHYSFS_tell(file);
		if (size <= 0 || !has_bytes(file, size) || !PHYSFS_seek(file, PHYSFS_tell(file) + size))
			return 0;
	}
	*error = "optional destroyed model table";
	if (!has_bytes(file, assets->count[SOURCE_MODEL] * 8))
		return 0;
	for (i = 0; i < assets->count[SOURCE_MODEL]; i++)
		assets->dying[i] = PHYSFSX_readInt(file);
	for (i = 0; i < assets->count[SOURCE_MODEL]; i++)
		assets->dead[i] = PHYSFSX_readInt(file);
	*error = "optional gauge table";
	if (!skip_records(file, MAX_GAUGE_BMS, 4))
		return 0;
	*error = "optional object bitmap table";
	if (!read_count(file, MAX_OBJ_BITMAPS, 4, &assets->count[SOURCE_OBJBMP]))
		return 0;
	n = assets->count[SOURCE_OBJPTR] = assets->count[SOURCE_OBJBMP];
	bitmap_index_read_n(assets->obj_bitmaps, n, file);
	for (i = 0; i < n; i++)
		assets->obj_ptrs[i] = PHYSFSX_readShort(file);
	/* Remaining HAM fields describe the base ship, cockpit and reactors
	 * They have no authority over the native D1 generation */
	assets->count[SOURCE_BITMAP] = MAX_BITMAP_FILES;
	assets->count[SOURCE_SAMPLE] = MAX_SOUND_FILES;
	return 1;
}

static int select_reference(d1_guidebot_assets *assets, enum source_kind kind, int id)
{
	if (id < 0 || id >= assets->count[kind])
		return 0;
	if (!assets->selected[kind][id])
		assets->selected[kind][id] = 1;
	return 1;
}

static int select_optional(d1_guidebot_assets *assets, enum source_kind kind, int id)
{
	return id == -1 || select_reference(assets, kind, id);
}

static int select_sound(d1_guidebot_assets *assets, int id)
{
	return id == -1 || id == 255 || select_reference(assets, SOURCE_SOUND, id);
}

static int select_bitmap(d1_guidebot_assets *assets, int id)
{
	/* Zero is the ordinary engine fallback, not a PIG directory entry */
	return id == 0 || select_reference(assets, SOURCE_BITMAP, id);
}

static int select_clip(d1_guidebot_assets *assets, const vclip *clip)
{
	int i;
	if (!d1_pig_validate_timed_clip(clip->num_frames, VCLIP_MAX_FRAMES, clip->play_time, clip->frame_time) ||
	    !select_sound(assets, clip->sound_num))
		return 0;
	for (i = 0; i < clip->num_frames; i++)
		if (!clip->frames[i].index || !select_bitmap(assets, clip->frames[i].index))
			return 0;
	return 1;
}

static int select_robot(d1_guidebot_assets *assets, const robot_info *robot)
{
	int gun, state, i;
	if (!select_reference(assets, SOURCE_MODEL, robot->model_num) || robot->n_guns < 0 || robot->n_guns > MAX_GUNS ||
	    !select_optional(assets, SOURCE_VCLIP, robot->exp1_vclip_num) || !select_sound(assets, robot->exp1_sound_num) ||
	    !select_optional(assets, SOURCE_VCLIP, robot->exp2_vclip_num) || !select_sound(assets, robot->exp2_sound_num) ||
	    !select_optional(assets, SOURCE_WEAPON, robot->weapon_type) || !select_optional(assets, SOURCE_WEAPON, robot->weapon_type2) ||
	    !select_sound(assets, robot->see_sound) || !select_sound(assets, robot->attack_sound) ||
	    !select_sound(assets, robot->claw_sound) || !select_sound(assets, robot->taunt_sound) ||
	    !select_sound(assets, robot->deathroll_sound))
		return 0;
	if (robot->contains_count < 0 || robot->contains_prob < 0 || robot->contains_prob > 16)
		return 0;
	if (robot->contains_count && robot->contains_prob &&
	    ((robot->contains_type != OBJ_ROBOT && robot->contains_type != OBJ_POWERUP) ||
	     !select_reference(assets, robot->contains_type == OBJ_ROBOT ? SOURCE_ROBOT : SOURCE_POWERUP, robot->contains_id)))
		return 0;
	if ((robot->smart_blobs || robot->energy_blobs) && !select_reference(assets, SOURCE_WEAPON, ROBOT_SMART_HOMING_ID))
		return 0;
	for (gun = 0; gun < robot->n_guns; gun++)
		if (robot->gun_submodels[gun] >= assets->models[robot->model_num].n_models)
			return 0;
	for (gun = 0; gun <= MAX_GUNS; gun++)
		for (state = 0; state < N_ANIM_STATES; state++) {
			const jointlist *list = &robot->anim_states[gun][state];
			if (list->n_joints < 0 || list->offset < 0 || list->offset > assets->count[SOURCE_JOINT] ||
			    list->n_joints > assets->count[SOURCE_JOINT] - list->offset)
				return 0;
			for (i = 0; i < list->n_joints; i++) {
				int joint = list->offset + i;
				if (assets->joints[joint].jointnum <= 0 || assets->joints[joint].jointnum >= assets->models[robot->model_num].n_models ||
				    !select_reference(assets, SOURCE_JOINT, joint))
					return 0;
			}
		}
	return 1;
}

static int select_model(d1_guidebot_assets *assets, int id)
{
	const polymodel *model = &assets->models[id];
	int i, next = id, steps = 0;
	if (model->n_models <= 0 || model->n_models > MAX_SUBMODELS ||
	    model->first_texture < 0 || model->first_texture > assets->count[SOURCE_OBJPTR] ||
	    model->n_textures > assets->count[SOURCE_OBJPTR] - model->first_texture ||
	    !select_optional(assets, SOURCE_MODEL, assets->dying[id]) || !select_optional(assets, SOURCE_MODEL, assets->dead[id]))
		return 0;
	/* The renderer follows simpler_model repeatedly; cycles must not reach it */
	while (assets->models[next].simpler_model) {
		next = assets->models[next].simpler_model - 1;
		if (!select_reference(assets, SOURCE_MODEL, next) || ++steps > assets->count[SOURCE_MODEL])
			return 0;
	}
	for (i = 0; i < model->n_models; i++) {
		int parent = i, depth = 0;
		if (model->submodel_ptrs[i] < 0 || model->submodel_ptrs[i] >= model->model_data_size)
			return 0;
		while (parent) {
			parent = model->submodel_parents[parent];
			if (parent >= model->n_models || ++depth >= model->n_models)
				return 0;
		}
	}
	for (i = 0; i < model->n_textures; i++)
		if (!select_reference(assets, SOURCE_OBJPTR, model->first_texture + i))
			return 0;
	return 1;
}

static int select_weapon_dependencies(d1_guidebot_assets *assets, int id)
{
	const weapon_info *weapon = &assets->weapons[id];
	int next = id, steps = 0;
	if (weapon->render_type < WEAPON_RENDER_NONE || weapon->render_type > WEAPON_RENDER_VCLIP ||
	    !select_optional(assets, SOURCE_VCLIP, weapon->flash_vclip) ||
	    !select_optional(assets, SOURCE_VCLIP, weapon->robot_hit_vclip) ||
	    !select_optional(assets, SOURCE_VCLIP, weapon->wall_hit_vclip) ||
	    !select_sound(assets, weapon->flash_sound) || !select_sound(assets, weapon->robot_hit_sound) ||
	    !select_sound(assets, weapon->wall_hit_sound) ||
	    !select_bitmap(assets, weapon->picture.index) || !select_bitmap(assets, weapon->hires_picture.index))
		return 0;
	if (weapon->render_type == WEAPON_RENDER_POLYMODEL &&
	    (!select_reference(assets, SOURCE_MODEL, weapon->model_num) || !select_optional(assets, SOURCE_MODEL, weapon->model_num_inner)))
		return 0;
	if (weapon->render_type == WEAPON_RENDER_VCLIP && !select_reference(assets, SOURCE_VCLIP, weapon->weapon_vclip))
		return 0;
	if ((weapon->render_type == WEAPON_RENDER_BLOB || weapon->render_type == WEAPON_RENDER_LASER) && !select_bitmap(assets, weapon->bitmap.index))
		return 0;
	if (weapon->afterburner_size && !select_reference(assets, SOURCE_VCLIP, VCLIP_AFTERBURNER_BLOB))
		return 0;
	while (assets->weapons[next].children != -1) {
		next = assets->weapons[next].children;
		if (!select_reference(assets, SOURCE_WEAPON, next) || ++steps > assets->count[SOURCE_WEAPON])
			return 0;
	}
	return 1;
}

static int select_dependencies(d1_guidebot_assets *assets, enum source_kind kind, int id)
{
	int i;
	switch (kind) {
	case SOURCE_ROBOT:
		return select_robot(assets, &assets->robots[id]);
	case SOURCE_MODEL:
		return select_model(assets, id);
	case SOURCE_WEAPON:
		return select_weapon_dependencies(assets, id);
	case SOURCE_POWERUP:
		return select_reference(assets, SOURCE_VCLIP, assets->powerups[id].vclip_num) && select_sound(assets, assets->powerups[id].hit_sound);
	case SOURCE_VCLIP:
		return select_clip(assets, &assets->vclips[id]);
	case SOURCE_EFFECT: {
		const eclip *effect = &assets->effects[id];
		return select_clip(assets, &effect->vc) &&
			select_optional(assets, SOURCE_TEXTURE, effect->changing_wall_texture) &&
			select_optional(assets, SOURCE_OBJBMP, effect->changing_object_texture) &&
			select_optional(assets, SOURCE_EFFECT, effect->crit_clip) &&
			select_optional(assets, SOURCE_TEXTURE, effect->dest_bm_num) &&
			select_optional(assets, SOURCE_VCLIP, effect->dest_vclip) &&
			select_optional(assets, SOURCE_EFFECT, effect->dest_eclip) && select_sound(assets, effect->sound_num);
	}
	case SOURCE_TEXTURE:
		return select_bitmap(assets, assets->textures[id].index) &&
			select_optional(assets, SOURCE_EFFECT, assets->texture_info[id].eclip_num) &&
			select_optional(assets, SOURCE_TEXTURE, assets->texture_info[id].destroyed);
	case SOURCE_OBJPTR:
		return select_reference(assets, SOURCE_OBJBMP, assets->obj_ptrs[id]);
	case SOURCE_OBJBMP:
		if (!select_bitmap(assets, assets->obj_bitmaps[id].index))
			return 0;
		for (i = 0; i < assets->count[SOURCE_EFFECT]; i++)
			if (assets->effects[i].changing_object_texture == id && !select_reference(assets, SOURCE_EFFECT, i))
				return 0;
		return 1;
	case SOURCE_SOUND:
		return (assets->sound_map[0][id] == 255 || select_reference(assets, SOURCE_SAMPLE, assets->sound_map[0][id])) &&
			select_sound(assets, assets->sound_map[1][id]);
	default:
		return 1;
	}
}

static int collect_dependencies(d1_guidebot_assets *assets, const char **error)
{
	static const char *errors[SOURCE_KINDS] = {
		"optional robot references", "optional model references", "optional joint references",
		"optional weapon references", "optional powerup references", "optional vclip references",
		"optional effect references", "optional texture references", "optional object pointer references",
		"optional object bitmap references", "optional bitmap references", "optional sound references", "optional sample references"
	};
	int i, kind, pending;
	assets->stats.source_robot = -1;
	*error = "optional companion definition";
	for (i = 0; i < assets->count[SOURCE_ROBOT]; i++)
		if (assets->robots[i].companion) {
			if (assets->stats.source_robot != -1)
				return 0;
			assets->stats.source_robot = i;
		}
	if (!select_reference(assets, SOURCE_ROBOT, assets->stats.source_robot))
		return 0;
	/* These roots are emitted by companion/spawn behavior, not robot records */
	*error = "optional companion behavior resources";
	if (!select_reference(assets, SOURCE_WEAPON, FLARE_ID) ||
	    !select_reference(assets, SOURCE_VCLIP, VCLIP_MORPHING_ROBOT) ||
	    !select_reference(assets, SOURCE_SOUND, SOUND_BUDDY_MET_GOAL))
		return 0;
	do {
		pending = 0;
		for (kind = 0; kind < SOURCE_KINDS; kind++)
			for (i = 0; i < assets->count[kind]; i++)
				if (assets->selected[kind][i] == 1) {
					assets->selected[kind][i] = 2;
					*error = errors[kind];
					if (!select_dependencies(assets, (enum source_kind)kind, i))
						return 0;
					pending = 1;
				}
	} while (pending);
	return 1;
}

static int read_models(d1_guidebot_assets *assets, PHYSFS_file *file)
{
	int i, sub;
	for (i = 0; i < assets->count[SOURCE_MODEL]; i++) {
		polymodel *model = &assets->models[i];
		if (!assets->selected[SOURCE_MODEL][i])
			continue;
		model->model_data = d_malloc(model->model_data_size);
		if (!model->model_data || !PHYSFS_seek(file, assets->model_offsets[i]) ||
		    PHYSFS_read(file, model->model_data, 1, model->model_data_size) != model->model_data_size ||
		    !d1_pig_validate_model_textures(model->model_data, model->model_data_size, 0, model->n_textures))
			return 0;
		for (sub = 0; sub < model->n_models; sub++)
			if (!d1_pig_validate_model_textures(model->model_data, model->model_data_size, model->submodel_ptrs[sub], model->n_textures))
				return 0;
		assets->stats.model_bytes += model->model_data_size;
	}
	return 1;
}

static int read_samples(d1_guidebot_assets *assets, const d1_guidebot_source *source)
{
	PHYSFS_file *file = PHYSFSX_openReadBuffered(source->sound_path);
	d1_sound_generation *samples = &assets->samples;
	PHYSFS_sint64 offsets[MAX_SOUND_FILES], data_start, bytes = 0;
	int i, valid = 0;
	if (!file)
		return 0;
	if (!has_bytes(file, 8) || PHYSFSX_readInt(file) != MAKE_SIG('D','N','S','D') || PHYSFSX_readInt(file) != 1 ||
	    !read_count(file, MAX_SOUND_FILES, 20, &samples->count))
		goto done;
	data_start = 12 + (PHYSFS_sint64)samples->count * 20;
	for (i = samples->count; i < MAX_SOUND_FILES; i++)
		if (assets->selected[SOURCE_SAMPLE][i])
			goto done;
	for (i = 0; i < samples->count; i++) {
		int length, data_length, offset;
		if (PHYSFS_read(file, samples->names[i], 8, 1) != 1)
			goto done;
		length = PHYSFSX_readInt(file);
		data_length = PHYSFSX_readInt(file);
		offset = PHYSFSX_readInt(file);
		if (!assets->selected[SOURCE_SAMPLE][i])
			continue;
		if (!samples->names[i][0] || length <= 0 || data_length != length || offset < 0 ||
		    !d1_pig_validate_span(PHYSFS_fileLength(file), data_start + offset, length) || bytes > 0x7fffffff - length)
			goto done;
		offsets[i] = data_start + offset;
		samples->samples[i].length = length;
		samples->samples[i].bits = 8;
		samples->samples[i].freq = source->sample_rate;
		bytes += length;
	}
	samples->data = d_malloc((size_t)bytes + 16);
	if (!samples->data)
		goto done;
	samples->bytes = (size_t)bytes;
	bytes = 0;
	for (i = 0; i < samples->count; i++) {
		digi_sound *sample = &samples->samples[i];
		if (!assets->selected[SOURCE_SAMPLE][i])
			continue;
		sample->data = samples->data + bytes;
		if (!PHYSFS_seek(file, offsets[i]) || PHYSFS_read(file, sample->data, 1, sample->length) != sample->length)
			goto done;
		bytes += sample->length;
	}
	assets->count[SOURCE_SAMPLE] = samples->count;
	assets->stats.sound_bytes = samples->bytes;
	valid = 1;
done:
	PHYSFS_close(file);
	return valid;
}

static int selected_count(const d1_guidebot_assets *assets, enum source_kind kind)
{
	int i, count = 0;
	for (i = 0; i < assets->count[kind]; i++)
		count += assets->selected[kind][i] != 0;
	return count;
}

void d1_in_d2_guidebot_source_stats(const d1_guidebot_assets *assets, d1_guidebot_asset_stats *stats)
{
	if (!stats)
		return;
	memset(stats, 0, sizeof(*stats));
	stats->source_robot = -1;
	if (!assets)
		return;
	*stats = assets->stats;
	stats->robots = selected_count(assets, SOURCE_ROBOT);
	stats->models = selected_count(assets, SOURCE_MODEL);
	stats->joints = selected_count(assets, SOURCE_JOINT);
	stats->weapons = selected_count(assets, SOURCE_WEAPON);
	stats->powerups = selected_count(assets, SOURCE_POWERUP);
	stats->vclips = selected_count(assets, SOURCE_VCLIP);
	stats->effects = selected_count(assets, SOURCE_EFFECT);
	stats->textures = selected_count(assets, SOURCE_TEXTURE);
	stats->bitmaps = selected_count(assets, SOURCE_BITMAP);
	stats->sounds = selected_count(assets, SOURCE_SAMPLE);
	stats->logical_sounds = selected_count(assets, SOURCE_SOUND);
}

void d1_in_d2_free_guidebot_source(d1_guidebot_assets *assets)
{
	int i;
	if (!assets)
		return;
	if (Published_guidebot == assets) {
		Published_guidebot = NULL;
		for (i = 1; i < assets->count[SOURCE_BITMAP]; i++)
			if (assets->selected[SOURCE_BITMAP][i]) {
				int slot = assets->runtime_map[SOURCE_BITMAP][i];
				if (GameBitmaps[slot].bm_data == assets->images->bitmaps[i].bm_data)
					gr_set_bitmap_data(&GameBitmaps[slot], NULL);
			}
		for (i = 0; i < assets->count[SOURCE_SAMPLE]; i++)
			if (assets->selected[SOURCE_SAMPLE][i]) {
				int slot = assets->runtime_map[SOURCE_SAMPLE][i];
				if (GameSounds[slot].data == assets->samples.samples[i].data)
					GameSounds[slot].data = NULL;
			}
	}
	for (i = 0; i < MAX_POLYGON_MODELS; i++)
		if (assets->models[i].model_data)
			d_free(assets->models[i].model_data);
	d1_in_d2_free_bitmaps(assets->images);
	if (assets->samples.data)
		d_free(assets->samples.data);
	d_free(assets);
}

d1_guidebot_assets *d1_in_d2_read_guidebot_source(const d1_guidebot_source *source, const char **error)
{
	d1_guidebot_assets *assets = NULL;
	PHYSFS_file *file = NULL;
	const char *stage = "optional source paths/sample rate";
	if (!source || !source->ham_path || !source->pig_path || !source->palette_path || !source->sound_path ||
	    (source->sample_rate != SAMPLE_RATE_11K && source->sample_rate != SAMPLE_RATE_22K))
		goto failed;
	stage = "optional source HAM";
	file = PHYSFSX_openReadBuffered(source->ham_path);
	if (!file)
		goto failed;
	assets = d_malloc(sizeof(*assets));
	if (!assets)
		goto failed;
	memset(assets, 0, sizeof(*assets));
	memset(assets->sound_map, 255, sizeof(assets->sound_map));
	if (!read_definitions(assets, file, &stage) || !collect_dependencies(assets, &stage))
		goto failed;
	stage = "optional model streams";
	if (!read_models(assets, file))
		goto failed;
	stage = "optional bitmap source";
	assets->images = d1_in_d2_read_feature_bitmaps(source->pig_path, source->palette_path,
		assets->selected[SOURCE_BITMAP], MAX_BITMAP_FILES);
	if (!assets->images)
		goto failed;
	assets->count[SOURCE_BITMAP] = assets->images->bitmap_count + 1;
	stage = "optional sample source";
	if (!read_samples(assets, source))
		goto failed;
	PHYSFS_close(file);
	if (error)
		*error = NULL;
	return assets;
failed:
	if (file)
		PHYSFS_close(file);
	d1_in_d2_free_guidebot_source(assets);
	if (error)
		*error = stage;
	return NULL;
}

static void extension_bases(const d1_asset_generation *base, int *counts)
{
	counts[SOURCE_ROBOT] = base->num_robot_types;
	counts[SOURCE_MODEL] = base->num_polygon_models;
	counts[SOURCE_JOINT] = base->num_robot_joints;
	counts[SOURCE_WEAPON] = base->num_weapon_types;
	counts[SOURCE_POWERUP] = base->num_powerups;
	counts[SOURCE_VCLIP] = base->num_vclips;
	counts[SOURCE_EFFECT] = base->num_effects;
	counts[SOURCE_TEXTURE] = base->num_textures;
	counts[SOURCE_OBJPTR] = counts[SOURCE_OBJBMP] = D1_MAX_OBJ_BITMAPS;
	counts[SOURCE_BITMAP] = base->bitmap_data->bitmap_count + 1;
	/* Preserve even undefined original logical slots and their silent meaning */
	counts[SOURCE_SOUND] = 256;
	counts[SOURCE_SAMPLE] = base->sound_bank.count;
}

int d1_in_d2_prepare_guidebot_extension(d1_asset_generation *base, const d1_guidebot_source *source, const char **error)
{
	static const int capacity[SOURCE_KINDS] = {
		MAX_ROBOT_TYPES, MAX_POLYGON_MODELS, MAX_ROBOT_JOINTS, MAX_WEAPON_TYPES,
		MAX_POWERUP_TYPES, VCLIP_MAXNUM, MAX_EFFECTS, MAX_TEXTURES, MAX_OBJ_BITMAPS,
		MAX_OBJ_BITMAPS, MAX_BITMAP_FILES, 256 + MAX_SOUNDS, MAX_SOUND_FILES
	};
	d1_guidebot_assets *assets = NULL;
	const char *stage = "optional extension base";
	int kind, i, j;
	if (!base || base->guidebot || !base->bitmap_data || !d1_in_d2_validate_asset_references(base, &stage))
		goto failed;
	assets = d1_in_d2_read_guidebot_source(source, &stage);
	if (!assets)
		goto failed;
	extension_bases(base, assets->runtime_base);
	stage = "optional extension capacities";
	for (kind = 0; kind < SOURCE_KINDS; kind++) {
		int next = assets->runtime_base[kind];
		if (next < 0 || next > capacity[kind])
			goto failed;
		for (i = 0; i < MAX_BITMAP_FILES; i++) {
			assets->runtime_map[kind][i] = -1;
			if (assets->selected[kind][i]) {
				if (next >= capacity[kind])
					goto failed;
				assets->runtime_map[kind][i] = next++;
			}
		}
		assets->runtime_end[kind] = next;
	}
	assets->runtime_map[SOURCE_BITMAP][0] = 0;
	/* Registration aliases cannot replace a native hash entry even when source
	 * files contain the same bitmap/sample names in their separate namespaces */
	stage = "optional extension registration names";
	for (kind = SOURCE_BITMAP; kind <= SOURCE_SAMPLE; kind++) {
		if (kind == SOURCE_SOUND)
			continue;
		for (i = 0; i < assets->count[kind]; i++)
			if (assets->selected[kind][i]) {
				char alias[9];
				snprintf(alias, sizeof(alias), kind == SOURCE_BITMAP ? "~gb%04d" : "~gs%04d", i);
				for (j = 0; j < assets->runtime_base[kind]; j++) {
					const char *name = kind == SOURCE_BITMAP ? base->bitmap_data->names[j] : base->sound_bank.names[j];
					if (!d_stricmp(alias, name))
						goto failed;
				}
			}
	}
	stage = "optional extension palette conversion";
	if (!d1_in_d2_remap_feature_bitmaps(assets->images, base->bitmap_data->palette))
		goto failed;
	stage = "optional source identity";
	if (!d1_in_d2_hash_guidebot_source(source, assets->runtime_map,
		SOURCE_KINDS, assets->identity))
		goto failed;
	assets->prepared = 1;
	base->guidebot = assets;
	if (error)
		*error = NULL;
	return 1;
failed:
	d1_in_d2_free_guidebot_source(assets);
	if (error)
		*error = stage;
	return 0;
}

int d1_in_d2_validate_guidebot_extension(const d1_asset_generation *base)
{
	int counts[SOURCE_KINDS];
	if (!base->guidebot)
		return 1;
	if (!base->guidebot->prepared || base->guidebot == Published_guidebot || !base->bitmap_data)
		return 0;
	extension_bases(base, counts);
	return memcmp(counts, base->guidebot->runtime_base, sizeof(counts)) == 0;
}

int d1_in_d2_prepare_guidebot_output(d1_guidebot_assets *assets, int target_rate)
{
	return !assets || d1_in_d2_prepare_sound_output(&assets->samples, target_rate);
}

static int mapped(const d1_guidebot_assets *assets, enum source_kind kind, int id)
{
	if (id == -1 || (kind == SOURCE_SOUND && id == 255))
		return id;
	return assets->runtime_map[kind][id];
}

static void map_clip(const d1_guidebot_assets *assets, vclip *clip)
{
	int i;
	clip->sound_num = mapped(assets, SOURCE_SOUND, clip->sound_num);
	for (i = 0; i < clip->num_frames; i++)
		clip->frames[i].index = mapped(assets, SOURCE_BITMAP, clip->frames[i].index);
}

void d1_in_d2_publish_guidebot_extension(d1_asset_generation *base)
{
	d1_guidebot_assets *assets = base->guidebot;
	int i, gun, state;
	extern int SoundOffset[MAX_SOUND_FILES];
	extern int extra_bitmap_num;
	if (!assets)
		return;
	for (i = 1; i < assets->count[SOURCE_BITMAP]; i++)
		if (assets->selected[SOURCE_BITMAP][i]) {
			int slot = mapped(assets, SOURCE_BITMAP, i);
			char alias[9];
			snprintf(alias, sizeof(alias), "~gb%04d", i);
			GameBitmaps[slot] = assets->images->bitmaps[i];
			piggy_bitmap_set_file_state(slot, 0, GameBitmaps[slot].bm_flags);
			piggy_register_bitmap(&GameBitmaps[slot], alias, 1);
			compute_average_rgb(&GameBitmaps[slot], GameBitmaps[slot].avg_color_rgb);
		}
	extra_bitmap_num = Num_bitmap_files;
	for (i = 0; i < assets->count[SOURCE_SAMPLE]; i++)
		if (assets->selected[SOURCE_SAMPLE][i]) {
			int slot = mapped(assets, SOURCE_SAMPLE, i);
			char alias[9];
			snprintf(alias, sizeof(alias), "~gs%04d", i);
			SoundOffset[slot] = -1;
			piggy_register_sound(&assets->samples.samples[i], alias, 1);
		}
	for (i = 0; i < assets->count[SOURCE_MODEL]; i++)
		if (assets->selected[SOURCE_MODEL][i]) {
			int slot = mapped(assets, SOURCE_MODEL, i);
			polymodel *model = &Polygon_models[slot];
			*model = assets->models[i];
			assets->models[i].model_data = NULL;
			model->first_texture = model->n_textures ? mapped(assets, SOURCE_OBJPTR, model->first_texture) : 0;
			model->simpler_model = model->simpler_model ? mapped(assets, SOURCE_MODEL, model->simpler_model - 1) + 1 : 0;
			Dying_modelnums[slot] = mapped(assets, SOURCE_MODEL, assets->dying[i]);
			Dead_modelnums[slot] = mapped(assets, SOURCE_MODEL, assets->dead[i]);
#ifdef WORDS_NEED_ALIGNMENT
			align_polygon_model_data(model);
#endif
#ifdef WORDS_BIGENDIAN
			swap_polygon_model_data(model->model_data);
#endif
			g3_init_polygon_model(model->model_data);
		}
	for (i = 0; i < assets->count[SOURCE_OBJPTR]; i++)
		if (assets->selected[SOURCE_OBJPTR][i])
			ObjBitmapPtrs[mapped(assets, SOURCE_OBJPTR, i)] = mapped(assets, SOURCE_OBJBMP, assets->obj_ptrs[i]);
	for (i = 0; i < assets->count[SOURCE_OBJBMP]; i++)
		if (assets->selected[SOURCE_OBJBMP][i])
			ObjBitmaps[mapped(assets, SOURCE_OBJBMP, i)].index = mapped(assets, SOURCE_BITMAP, assets->obj_bitmaps[i].index);
	for (i = 0; i < assets->count[SOURCE_JOINT]; i++)
		if (assets->selected[SOURCE_JOINT][i])
			Robot_joints[mapped(assets, SOURCE_JOINT, i)] = assets->joints[i];
	for (i = 0; i < assets->count[SOURCE_ROBOT]; i++)
		if (assets->selected[SOURCE_ROBOT][i]) {
			robot_info *robot = &Robot_info[mapped(assets, SOURCE_ROBOT, i)];
			*robot = assets->robots[i];
			robot->model_num = mapped(assets, SOURCE_MODEL, robot->model_num);
			robot->weapon_type = mapped(assets, SOURCE_WEAPON, robot->weapon_type);
			robot->weapon_type2 = mapped(assets, SOURCE_WEAPON, robot->weapon_type2);
			robot->exp1_vclip_num = mapped(assets, SOURCE_VCLIP, robot->exp1_vclip_num);
			robot->exp2_vclip_num = mapped(assets, SOURCE_VCLIP, robot->exp2_vclip_num);
			robot->exp1_sound_num = mapped(assets, SOURCE_SOUND, robot->exp1_sound_num);
			robot->exp2_sound_num = mapped(assets, SOURCE_SOUND, robot->exp2_sound_num);
			robot->see_sound = mapped(assets, SOURCE_SOUND, robot->see_sound);
			robot->attack_sound = mapped(assets, SOURCE_SOUND, robot->attack_sound);
			robot->claw_sound = mapped(assets, SOURCE_SOUND, robot->claw_sound);
			robot->taunt_sound = mapped(assets, SOURCE_SOUND, robot->taunt_sound);
			robot->deathroll_sound = mapped(assets, SOURCE_SOUND, robot->deathroll_sound);
			if (robot->contains_count && robot->contains_prob)
				robot->contains_id = mapped(assets, robot->contains_type == OBJ_ROBOT ? SOURCE_ROBOT : SOURCE_POWERUP, robot->contains_id);
			for (gun = 0; gun <= MAX_GUNS; gun++)
				for (state = 0; state < N_ANIM_STATES; state++) {
					jointlist *list = &robot->anim_states[gun][state];
					list->offset = list->n_joints ? mapped(assets, SOURCE_JOINT, list->offset) : 0;
				}
		}
	for (i = 0; i < assets->count[SOURCE_WEAPON]; i++)
		if (assets->selected[SOURCE_WEAPON][i]) {
			weapon_info *weapon = &Weapon_info[mapped(assets, SOURCE_WEAPON, i)];
			*weapon = assets->weapons[i];
			weapon->model_num = weapon->render_type == WEAPON_RENDER_POLYMODEL ? mapped(assets, SOURCE_MODEL, weapon->model_num) : -1;
			weapon->model_num_inner = weapon->render_type == WEAPON_RENDER_POLYMODEL ? mapped(assets, SOURCE_MODEL, weapon->model_num_inner) : -1;
			weapon->flash_vclip = mapped(assets, SOURCE_VCLIP, weapon->flash_vclip);
			weapon->robot_hit_vclip = mapped(assets, SOURCE_VCLIP, weapon->robot_hit_vclip);
			weapon->wall_hit_vclip = mapped(assets, SOURCE_VCLIP, weapon->wall_hit_vclip);
			weapon->weapon_vclip = weapon->render_type == WEAPON_RENDER_VCLIP ? mapped(assets, SOURCE_VCLIP, weapon->weapon_vclip) : -1;
			weapon->bitmap.index = (weapon->render_type == WEAPON_RENDER_BLOB || weapon->render_type == WEAPON_RENDER_LASER) ?
				mapped(assets, SOURCE_BITMAP, weapon->bitmap.index) : 0;
			weapon->picture.index = mapped(assets, SOURCE_BITMAP, weapon->picture.index);
			weapon->hires_picture.index = mapped(assets, SOURCE_BITMAP, weapon->hires_picture.index);
			weapon->children = mapped(assets, SOURCE_WEAPON, weapon->children);
			weapon->flash_sound = mapped(assets, SOURCE_SOUND, weapon->flash_sound);
			weapon->robot_hit_sound = mapped(assets, SOURCE_SOUND, weapon->robot_hit_sound);
			weapon->wall_hit_sound = mapped(assets, SOURCE_SOUND, weapon->wall_hit_sound);
		}
	for (i = 0; i < assets->count[SOURCE_POWERUP]; i++)
		if (assets->selected[SOURCE_POWERUP][i]) {
			powerup_type_info *powerup = &Powerup_info[mapped(assets, SOURCE_POWERUP, i)];
			*powerup = assets->powerups[i];
			powerup->vclip_num = mapped(assets, SOURCE_VCLIP, powerup->vclip_num);
			powerup->hit_sound = mapped(assets, SOURCE_SOUND, powerup->hit_sound);
		}
	for (i = 0; i < assets->count[SOURCE_VCLIP]; i++)
		if (assets->selected[SOURCE_VCLIP][i]) {
			vclip *clip = &Vclip[mapped(assets, SOURCE_VCLIP, i)];
			*clip = assets->vclips[i];
			map_clip(assets, clip);
		}
	for (i = 0; i < assets->count[SOURCE_EFFECT]; i++)
		if (assets->selected[SOURCE_EFFECT][i]) {
			eclip *effect = &Effects[mapped(assets, SOURCE_EFFECT, i)];
			*effect = assets->effects[i];
			map_clip(assets, &effect->vc);
			effect->changing_wall_texture = mapped(assets, SOURCE_TEXTURE, effect->changing_wall_texture);
			effect->changing_object_texture = mapped(assets, SOURCE_OBJBMP, effect->changing_object_texture);
			effect->crit_clip = mapped(assets, SOURCE_EFFECT, effect->crit_clip);
			effect->dest_bm_num = mapped(assets, SOURCE_TEXTURE, effect->dest_bm_num);
			effect->dest_vclip = mapped(assets, SOURCE_VCLIP, effect->dest_vclip);
			effect->dest_eclip = mapped(assets, SOURCE_EFFECT, effect->dest_eclip);
			effect->sound_num = mapped(assets, SOURCE_SOUND, effect->sound_num);
			effect->segnum = -1;
			effect->sidenum = -1;
		}
	for (i = 0; i < assets->count[SOURCE_TEXTURE]; i++)
		if (assets->selected[SOURCE_TEXTURE][i]) {
			int slot = mapped(assets, SOURCE_TEXTURE, i);
			Textures[slot].index = mapped(assets, SOURCE_BITMAP, assets->textures[i].index);
			TmapInfo[slot] = assets->texture_info[i];
			TmapInfo[slot].eclip_num = mapped(assets, SOURCE_EFFECT, TmapInfo[slot].eclip_num);
			TmapInfo[slot].destroyed = mapped(assets, SOURCE_TEXTURE, TmapInfo[slot].destroyed);
		}
	N_robot_types = assets->runtime_end[SOURCE_ROBOT];
	N_polygon_models = assets->runtime_end[SOURCE_MODEL];
	N_robot_joints = assets->runtime_end[SOURCE_JOINT];
	N_weapon_types = assets->runtime_end[SOURCE_WEAPON];
	N_powerup_types = assets->runtime_end[SOURCE_POWERUP];
	Num_vclips = assets->runtime_end[SOURCE_VCLIP];
	Num_effects = assets->runtime_end[SOURCE_EFFECT];
	NumTextures = assets->runtime_end[SOURCE_TEXTURE];
	N_ObjBitmaps = max(assets->runtime_end[SOURCE_OBJPTR], assets->runtime_end[SOURCE_OBJBMP]);
	Published_guidebot = assets;
}

int d1_in_d2_translate_extended_sound(int soundno)
{
	const d1_guidebot_assets *assets = Published_guidebot;
	int i, sample;
	if (!assets || soundno < assets->runtime_base[SOURCE_SOUND] || soundno >= assets->runtime_end[SOURCE_SOUND])
		return -1;
	for (i = 0; i < assets->count[SOURCE_SOUND]; i++)
		if (assets->runtime_map[SOURCE_SOUND][i] == soundno)
			break;
	if (i == assets->count[SOURCE_SOUND])
		return -1;
	if (GameArg.SysLowMem) {
		i = assets->sound_map[1][i];
		if (i == 255)
			return -1;
	}
	sample = assets->sound_map[0][i];
	return sample == 255 ? -1 : mapped(assets, SOURCE_SAMPLE, sample);
}

int d1_in_d2_untranslate_extended_sound(int sample)
{
	const d1_guidebot_assets *assets = Published_guidebot;
	int logical;
	if (!assets || sample < assets->runtime_base[SOURCE_SAMPLE] || sample >= assets->runtime_end[SOURCE_SAMPLE])
		return -1;
	for (logical = assets->runtime_base[SOURCE_SOUND]; logical < assets->runtime_end[SOURCE_SOUND]; logical++)
		if (d1_in_d2_translate_extended_sound(logical) == sample)
			return logical;
	return -1;
}

int d1_in_d2_guidebot_owns_model(int model_num)
{
	return Published_guidebot && model_num >= Published_guidebot->runtime_base[SOURCE_MODEL] &&
		model_num < Published_guidebot->runtime_end[SOURCE_MODEL];
}

const ubyte *d1_in_d2_guidebot_identity(void)
{
	return Published_guidebot ? Published_guidebot->identity : NULL;
}
