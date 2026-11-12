/* Guidebot runtime save schema 1: explicit fields, no ABI padding or pointers
 * Changing these field lists requires a new save schema */
#ifndef DXX_GUIDEBOT_METADATA_SAVE_FIELDS_H
#define DXX_GUIDEBOT_METADATA_SAVE_FIELDS_H
#include "guidebot_save_io.h"

#define METADATA_BEGIN(type)                                     \
	static void gb_save_##type(guidebot_save_stream *s, type *v) \
	{
#define METADATA_END()                        }
#define METADATA_FIELD(name, kind)            GB_FIELD(s, v->name, kind);
#define METADATA_LIMIT(name, kind, low, high) GB_LIMIT(s, v->name, kind, low, high);
#define METADATA_TEXT(name)                   GB_TEXT(s, v->name);
#define METADATA_ARRAY(name, count, kind) \
	for (size_t i0 = 0; i0 < count; ++i0) { GB_FIELD(s, v->name[i0], kind); }
#define METADATA_MATRIX(name, rows, columns, kind)                                      \
	for (size_t i0 = 0; i0 < rows; ++i0) {                                              \
		for (size_t i1 = 0; i1 < columns; ++i1) { GB_FIELD(s, v->name[i0][i1], kind); } \
	}
#define METADATA_STRUCT(name, type) gb_save_##type(s, &v->name);
#define METADATA_STRUCT_ARRAY(name, type, count) \
	for (size_t i0 = 0; i0 < count; ++i0) { gb_save_##type(s, &v->name[i0]); }
#include "guidebot_metadata_fields.h"
#undef METADATA_BEGIN
#undef METADATA_END
#undef METADATA_FIELD
#undef METADATA_LIMIT
#undef METADATA_TEXT
#undef METADATA_ARRAY
#undef METADATA_MATRIX
#undef METADATA_STRUCT
#undef METADATA_STRUCT_ARRAY
#endif
