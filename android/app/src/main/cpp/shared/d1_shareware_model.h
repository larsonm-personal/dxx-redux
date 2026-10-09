/* Bounded POF source decoding shared by native D1 and D1-in-D2 adapters */
#ifndef DXX_D1_SHAREWARE_MODEL_H
#define DXX_D1_SHAREWARE_MODEL_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* D1 POF limits, independent of the receiving engine's structure layout */
#define D1_POF_SUBMODELS   10
#define D1_POF_GUNS        8
#define D1_POF_ANIM_STATES 5

typedef struct d1_pof_vector {
	int32_t x, y, z;
} d1_pof_vector;
typedef struct d1_pof_angles {
	int16_t p, b, h;
} d1_pof_angles;
typedef struct d1_pof_source {
	int version, model_count, gun_count, has_animation;
	int32_t radius;
	int32_t offsets[D1_POF_SUBMODELS], radii[D1_POF_SUBMODELS];
	uint8_t parents[D1_POF_SUBMODELS], gun_models[D1_POF_GUNS];
	d1_pof_vector normals[D1_POF_SUBMODELS], points[D1_POF_SUBMODELS], translations[D1_POF_SUBMODELS];
	d1_pof_vector gun_points[D1_POF_GUNS], gun_directions[D1_POF_GUNS];
	d1_pof_angles animation[D1_POF_ANIM_STATES][D1_POF_SUBMODELS];
	/* Borrowed span; the adapter copies it into its owned model allocation */
	const uint8_t *instructions;
	size_t instruction_size;
} d1_pof_source;

static inline uint16_t d1_pof_u16(const uint8_t *p)
{
	return (uint16_t) (p[0] | ((uint16_t) p[1] << 8));
}

static inline uint32_t d1_pof_u32(const uint8_t *p)
{
	return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24);
}

static inline d1_pof_vector d1_pof_vec(const uint8_t *p)
{
	d1_pof_vector result;
	result.x = (int32_t) d1_pof_u32(p);
	result.y = (int32_t) d1_pof_u32(p + 4);
	result.z = (int32_t) d1_pof_u32(p + 8);
	return result;
}

typedef struct d1_pof_bounds {
	d1_pof_vector mins, maxs;
	d1_pof_vector submodel_mins[D1_POF_SUBMODELS], submodel_maxs[D1_POF_SUBMODELS];
} d1_pof_bounds;

/* Read source byte order without unaligned engine-vector casts */
static inline const char *d1_pof_find_bounds(const d1_pof_source *source, d1_pof_bounds *bounds)
{
	int m;
	memset(bounds, 0, sizeof(*bounds));
	if (source->model_count < 1 || source->model_count > D1_POF_SUBMODELS || !source->instructions)
		return "Invalid POF bounds source";
	for (m = 0; m < source->model_count; ++m) {
		size_t offset, header;
		unsigned opcode, count, vertex;
		const uint8_t *data;
		d1_pof_vector *mn = &bounds->submodel_mins[m], *mx = &bounds->submodel_maxs[m];
		if (source->offsets[m] < 0 || (size_t) source->offsets[m] > source->instruction_size)
			return "Invalid POF vertex offset";
		offset = (size_t) source->offsets[m];
		if (source->instruction_size - offset < 4)
			return "Missing POF vertices";
		data = source->instructions + offset;
		opcode = d1_pof_u16(data);
		count = d1_pof_u16(data + 2);
		header = opcode == 7 ? 8 : 4;
		if ((opcode != 1 && opcode != 7) || !count || source->instruction_size - offset < header ||
		    count > (source->instruction_size - offset - header) / 12)
			return "Invalid POF vertex span";
		data += header;
		*mn = *mx = d1_pof_vec(data);
		if (!m)
			bounds->mins = bounds->maxs = *mn;
		/* Preserve native D1's first-vertex and submodel-offset conventions */
		for (vertex = 1; vertex < count; ++vertex) {
			d1_pof_vector p = d1_pof_vec(data + vertex * 12);
			int axis;
			int32_t coordinates[3] = { p.x, p.y, p.z };
			int32_t translations[3] = { source->translations[m].x, source->translations[m].y, source->translations[m].z };
			int32_t *local_min[3] = { &mn->x, &mn->y, &mn->z }, *local_max[3] = { &mx->x, &mx->y, &mx->z };
			int32_t *global_min[3] = { &bounds->mins.x, &bounds->mins.y, &bounds->mins.z };
			int32_t *global_max[3] = { &bounds->maxs.x, &bounds->maxs.y, &bounds->maxs.z };
			for (axis = 0; axis < 3; ++axis) {
				int64_t translated = (int64_t) coordinates[axis] + translations[axis];
				if (translated < INT32_MIN || translated > INT32_MAX)
					return "POF bounds overflow";
				if (coordinates[axis] < *local_min[axis]) *local_min[axis] = coordinates[axis];
				if (coordinates[axis] > *local_max[axis]) *local_max[axis] = coordinates[axis];
				if (translated < *global_min[axis]) *global_min[axis] = (int32_t) translated;
				if (translated > *global_max[axis]) *global_max[axis] = (int32_t) translated;
			}
		}
	}
	return NULL;
}

/* No engine globals or allocations are touched, including on failure */
static inline const char *d1_pof_read(const uint8_t *bytes, size_t size, d1_pof_source *result)
{
	size_t position = 6;
	unsigned submodels = 0;
	int header = 0, guns = 0;
	int i;
	memset(result, 0, sizeof(*result));
	if (size < 6 || memcmp(bytes, "PSPO", 4))
		return "Invalid POF signature";
	result->version = d1_pof_u16(bytes + 4);
	if (result->version < 6 || result->version > 8)
		return "Unsupported POF version";
	while (position < size) {
		const uint8_t *chunk;
		uint32_t tag, length;
		if (size - position < 8)
			return "Truncated POF chunk header";
		tag = d1_pof_u32(bytes + position);
		length = d1_pof_u32(bytes + position + 4);
		position += 8;
		if (length > size - position)
			return "Truncated POF chunk";
		chunk = bytes + position;
		position += length;
		switch (tag) {
			case 0x5244484f: /* OHDR */
				if (header++ || length < 32)
					return "Invalid POF object header";
				result->model_count = (int32_t) d1_pof_u32(chunk);
				result->radius = (int32_t) d1_pof_u32(chunk + 4);
				if (result->model_count < 1 || result->model_count > D1_POF_SUBMODELS)
					return "Invalid POF submodel count";
				break;
			case 0x4a424f53: { /* SOBJ */
				unsigned index, parent;
				if (length < 48)
					return "Truncated POF submodel";
				index = d1_pof_u16(chunk);
				parent = d1_pof_u16(chunk + 2);
				if (index >= D1_POF_SUBMODELS || (submodels & (1u << index)) ||
				    (parent >= D1_POF_SUBMODELS && parent != 0xffff && parent != 0xff))
					return "Invalid POF submodel index";
				submodels |= 1u << index;
				result->parents[index] = (uint8_t) parent;
				result->normals[index] = d1_pof_vec(chunk + 4);
				result->points[index] = d1_pof_vec(chunk + 16);
				result->translations[index] = d1_pof_vec(chunk + 28);
				result->radii[index] = (int32_t) d1_pof_u32(chunk + 40);
				result->offsets[index] = (int32_t) d1_pof_u32(chunk + 44);
				break;
			}
			case 0x534e5547: { /* GUNS */
				unsigned seen = 0;
				size_t stride = result->version >= 7 ? 28 : 16;
				if (guns++ || length < 4)
					return "Invalid POF gun header";
				result->gun_count = (int32_t) d1_pof_u32(chunk);
				if (result->gun_count < 0 || result->gun_count > D1_POF_GUNS ||
				    (size_t) result->gun_count > (length - 4) / stride)
					return "Invalid POF gun count";
				for (i = 0; i < result->gun_count; ++i) {
					const uint8_t *gun = chunk + 4 + i * stride;
					unsigned index = d1_pof_u16(gun), model = d1_pof_u16(gun + 2);
					if (index >= (unsigned) result->gun_count || (seen & (1u << index)) || model >= D1_POF_SUBMODELS)
						return "Invalid POF gun index";
					seen |= 1u << index;
					result->gun_models[index] = (uint8_t) model;
					result->gun_points[index] = d1_pof_vec(gun + 4);
					if (result->version >= 7)
						result->gun_directions[index] = d1_pof_vec(gun + 16);
				}
				break;
			}
			case 0x4d494e41: { /* ANIM */
				int model, frame;
				if (!header || result->has_animation || length < 2 ||
				    d1_pof_u16(chunk) != D1_POF_ANIM_STATES ||
				    length - 2 < (unsigned) result->model_count * D1_POF_ANIM_STATES * 6)
					return "Invalid POF animation";
				result->has_animation = 1;
				for (model = 0; model < result->model_count; ++model)
					for (frame = 0; frame < D1_POF_ANIM_STATES; ++frame) {
						const uint8_t *angles = chunk + 2 + (model * D1_POF_ANIM_STATES + frame) * 6;
						result->animation[frame][model].p = (int16_t) d1_pof_u16(angles);
						result->animation[frame][model].b = (int16_t) d1_pof_u16(angles + 2);
						result->animation[frame][model].h = (int16_t) d1_pof_u16(angles + 4);
					}
				break;
			}
			case 0x41544449: /* IDTA */
				if (result->instructions || length < 2)
					return "Invalid POF instruction block";
				result->instructions = chunk;
				result->instruction_size = length;
				break;
			default:
				break;
		}
	}
	if (!header || !result->instructions || submodels != (1u << result->model_count) - 1)
		return "Incomplete POF model";
	for (i = 0; i < result->model_count; ++i) {
		int parent = i, depth = 0;
		if (result->offsets[i] < 0 || (size_t) result->offsets[i] > result->instruction_size - 2)
			return "Invalid POF instruction offset";
		/* The root is submodel zero; its historical parent sentinel varies */
		while (parent != 0) {
			parent = result->parents[parent];
			if (parent >= result->model_count || ++depth >= result->model_count)
				return "Invalid POF parent tree";
		}
	}
	for (i = 0; i < result->gun_count; ++i)
		if (result->gun_models[i] >= result->model_count)
			return "Invalid POF gun submodel";
	return NULL;
}

#endif
