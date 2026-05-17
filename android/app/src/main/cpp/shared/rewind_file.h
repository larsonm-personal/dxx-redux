#ifndef REWIND_FILE_H
#define REWIND_FILE_H

#include <physfs.h>

#include <stddef.h>
#include <stdint.h>

#include "byteswap.h"
#include "pstypes.h"
#include "vecmat.h"

#if defined(__ANDROID__) || defined(DXX_REWIND_FILE_WRAPPER)
#include <stdlib.h>
#include <string.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rewind_memory_buffer {
	unsigned char *data;
	size_t size;
	size_t capacity;
} rewind_memory_buffer;

typedef enum rewind_file_kind {
	REWIND_FILE_KIND_PHYSFS = 0,
	REWIND_FILE_KIND_MEMORY = 1,
} rewind_file_kind;

#if defined(__ANDROID__) || defined(DXX_REWIND_FILE_WRAPPER)

#define REWIND_FILE_USES_WRAPPER 1

typedef struct rewind_file {
	rewind_file_kind kind;
	PHYSFS_file *physfs;
	rewind_memory_buffer *memory_buffer;
	const unsigned char *memory_read_data;
	size_t memory_read_size;
	size_t position;
} rewind_file;

static inline void rewind_file_init_physfs(rewind_file *file, PHYSFS_file *physfs)
{
	if (!file)
		return;
	memset(file, 0, sizeof(*file));
	file->kind = REWIND_FILE_KIND_PHYSFS;
	file->physfs = physfs;
}

static inline void rewind_file_init_memory_read(rewind_file *file,
	const unsigned char *data,
	size_t size)
{
	if (!file)
		return;
	memset(file, 0, sizeof(*file));
	file->kind = REWIND_FILE_KIND_MEMORY;
	file->memory_read_data = data;
	file->memory_read_size = size;
}

static inline void rewind_file_init_memory_write(rewind_file *file,
	rewind_memory_buffer *buffer)
{
	if (!file)
		return;
	memset(file, 0, sizeof(*file));
	file->kind = REWIND_FILE_KIND_MEMORY;
	file->memory_buffer = buffer;
	if (buffer) {
		file->memory_read_data = buffer->data;
		file->memory_read_size = buffer->size;
	}
}

static inline rewind_file *rewind_file_wrap_physfs(rewind_file *file, PHYSFS_file *physfs)
{
	rewind_file_init_physfs(file, physfs);
	return file;
}

static inline int rewind_file_is_memory(const rewind_file *file)
{
	return file && file->kind == REWIND_FILE_KIND_MEMORY;
}

static inline PHYSFS_file *rewind_file_physfs_handle(rewind_file *file)
{
	if (!file || file->kind != REWIND_FILE_KIND_PHYSFS)
		return NULL;
	return file->physfs;
}

static inline int rewind_file_close(rewind_file *file)
{
	int result = 1;

	if (!file)
		return 0;
	if (file->kind == REWIND_FILE_KIND_PHYSFS && file->physfs)
		result = PHYSFS_close(file->physfs);
	file->physfs = NULL;
	if (file->memory_buffer) {
		file->memory_buffer->data = (unsigned char *) file->memory_read_data;
		file->memory_buffer->size = file->memory_read_size;
		file->memory_buffer->capacity = file->memory_buffer->capacity;
	}
	return result;
}

static inline int rewind_file_memory_reserve(rewind_file *file, size_t required)
{
	unsigned char *next_data;
	size_t next_capacity;

	if (!file || file->kind != REWIND_FILE_KIND_MEMORY || !file->memory_buffer)
		return 0;
	if (required <= file->memory_buffer->capacity)
		return 1;
	next_capacity = file->memory_buffer->capacity ? file->memory_buffer->capacity : 4096;
	while (next_capacity < required) {
		size_t doubled = next_capacity * 2;
		if (doubled <= next_capacity) {
			next_capacity = required;
			break;
		}
		next_capacity = doubled;
	}
	next_data = (unsigned char *) realloc(file->memory_buffer->data, next_capacity);
	if (!next_data)
		return 0;
	file->memory_buffer->data = next_data;
	file->memory_buffer->capacity = next_capacity;
	file->memory_read_data = next_data;
	return 1;
}

static inline PHYSFS_sint64 rewind_file_read(rewind_file *file,
	void *buffer,
	PHYSFS_uint32 obj_size,
	PHYSFS_uint32 obj_count)
{
	PHYSFS_uint64 requested_bytes;

	if (!file || !buffer || obj_size == 0 || obj_count == 0)
		return 0;
	if (file->kind == REWIND_FILE_KIND_PHYSFS)
		return PHYSFS_readBytes(file->physfs, buffer, (PHYSFS_uint64) obj_size * obj_count) / obj_size;
	requested_bytes = (PHYSFS_uint64) obj_size * obj_count;
	if (file->position >= file->memory_read_size)
		return 0;
	if (requested_bytes > file->memory_read_size - file->position)
		requested_bytes = file->memory_read_size - file->position;
	requested_bytes -= requested_bytes % obj_size;
	if (requested_bytes == 0)
		return 0;
	memcpy(buffer, file->memory_read_data + file->position, (size_t) requested_bytes);
	file->position += (size_t) requested_bytes;
	return (PHYSFS_sint64) (requested_bytes / obj_size);
}

static inline PHYSFS_sint64 rewind_file_write(rewind_file *file,
	const void *buffer,
	PHYSFS_uint32 obj_size,
	PHYSFS_uint32 obj_count)
{
	size_t requested_bytes;
	size_t end_position;

	if (!file || !buffer || obj_size == 0 || obj_count == 0)
		return 0;
	if (file->kind == REWIND_FILE_KIND_PHYSFS)
		return PHYSFS_writeBytes(file->physfs, buffer, (PHYSFS_uint64) obj_size * obj_count) / obj_size;
	requested_bytes = (size_t) obj_size * obj_count;
	end_position = file->position + requested_bytes;
	if (!rewind_file_memory_reserve(file, end_position))
		return 0;
	if (file->position > file->memory_read_size)
		memset(file->memory_buffer->data + file->memory_read_size, 0, file->position - file->memory_read_size);
	memcpy(file->memory_buffer->data + file->position, buffer, requested_bytes);
	file->position = end_position;
	if (file->memory_read_size < end_position)
		file->memory_read_size = end_position;
	file->memory_buffer->size = file->memory_read_size;
	file->memory_read_data = file->memory_buffer->data;
	return (PHYSFS_sint64) obj_count;
}

static inline int rewind_file_seek(rewind_file *file, PHYSFS_uint64 offset)
{
	if (!file)
		return 0;
	if (file->kind == REWIND_FILE_KIND_PHYSFS)
		return PHYSFS_seek(file->physfs, offset);
	if (offset > file->memory_read_size)
		return 0;
	file->position = (size_t) offset;
	return 1;
}

static inline PHYSFS_sint64 rewind_file_tell(rewind_file *file)
{
	if (!file)
		return -1;
	if (file->kind == REWIND_FILE_KIND_PHYSFS)
		return PHYSFS_tell(file->physfs);
	return (PHYSFS_sint64) file->position;
}

static inline PHYSFS_sint64 rewind_file_length(rewind_file *file)
{
	if (!file)
		return -1;
	if (file->kind == REWIND_FILE_KIND_PHYSFS)
		return PHYSFS_fileLength(file->physfs);
	return (PHYSFS_sint64) file->memory_read_size;
}

static inline int rewind_file_eof(rewind_file *file)
{
	if (!file)
		return 1;
	if (file->kind == REWIND_FILE_KIND_PHYSFS)
		return PHYSFS_eof(file->physfs);
	return file->position >= file->memory_read_size;
}

static inline int rewind_file_read_sxe16(rewind_file *file, int swap)
{
	PHYSFS_sint16 value = 0;
	rewind_file_read(file, &value, sizeof(value), 1);
	return swap ? SWAPSHORT(value) : value;
}

static inline int rewind_file_read_sxe32(rewind_file *file, int swap)
{
	PHYSFS_sint32 value = 0;
	rewind_file_read(file, &value, sizeof(value), 1);
	return swap ? SWAPINT(value) : value;
}

static inline int rewind_file_read_int(rewind_file *file)
{
	return rewind_file_read_sxe32(file, 0);
}

static inline short rewind_file_read_short(rewind_file *file)
{
	return (short) rewind_file_read_sxe16(file, 0);
}

static inline sbyte rewind_file_read_byte(rewind_file *file)
{
	sbyte value = 0;
	rewind_file_read(file, &value, sizeof(value), 1);
	return value;
}

static inline fix rewind_file_read_fix(rewind_file *file)
{
	return (fix) rewind_file_read_sxe32(file, 0);
}

static inline fixang rewind_file_read_fixang(rewind_file *file)
{
	return (fixang) rewind_file_read_sxe16(file, 0);
}

static inline void rewind_file_read_vector_x(rewind_file *file, vms_vector *vector, int swap)
{
	vector->x = rewind_file_read_sxe32(file, swap);
	vector->y = rewind_file_read_sxe32(file, swap);
	vector->z = rewind_file_read_sxe32(file, swap);
}

static inline void rewind_file_read_anglevec_x(rewind_file *file, vms_angvec *vector, int swap)
{
	vector->p = rewind_file_read_sxe16(file, swap);
	vector->b = rewind_file_read_sxe16(file, swap);
	vector->h = rewind_file_read_sxe16(file, swap);
}

static inline void rewind_file_read_vector(rewind_file *file, vms_vector *vector)
{
	rewind_file_read_vector_x(file, vector, 0);
}

static inline void rewind_file_read_anglevec(rewind_file *file, vms_angvec *vector)
{
	rewind_file_read_anglevec_x(file, vector, 0);
}

static inline int rewind_file_write_sle16(rewind_file *file, PHYSFS_sint16 value)
{
	unsigned char bytes[2];
	uint16_t little_endian = (uint16_t) value;

	bytes[0] = (unsigned char) (little_endian & 0xffu);
	bytes[1] = (unsigned char) ((little_endian >> 8) & 0xffu);
	return rewind_file_write(file, bytes, 1, sizeof(bytes)) == (PHYSFS_sint64) sizeof(bytes);
}

static inline int rewind_file_write_sle32(rewind_file *file, PHYSFS_sint32 value)
{
	unsigned char bytes[4];
	uint32_t little_endian = (uint32_t) value;

	bytes[0] = (unsigned char) (little_endian & 0xffu);
	bytes[1] = (unsigned char) ((little_endian >> 8) & 0xffu);
	bytes[2] = (unsigned char) ((little_endian >> 16) & 0xffu);
	bytes[3] = (unsigned char) ((little_endian >> 24) & 0xffu);
	return rewind_file_write(file, bytes, 1, sizeof(bytes)) == (PHYSFS_sint64) sizeof(bytes);
}

static inline int rewind_file_write_fix(rewind_file *file, fix value)
{
	return rewind_file_write_sle32(file, value);
}

static inline int rewind_file_write_u8(rewind_file *file, uint8_t value)
{
	return rewind_file_write(file, &value, sizeof(value), 1) == 1;
}

static inline int rewind_file_write_vector(rewind_file *file, vms_vector *vector)
{
	if (!rewind_file_write_sle32(file, vector->x) ||
	 !rewind_file_write_sle32(file, vector->y) ||
	 !rewind_file_write_sle32(file, vector->z))
		return 0;
	return 1;
}

#else

#define REWIND_FILE_USES_WRAPPER 0

typedef PHYSFS_file rewind_file;

#endif

#if REWIND_FILE_USES_WRAPPER
#define REWIND_PHYSFS_FILE(name, physfs) \
	rewind_file name##_storage; \
	rewind_file *name = rewind_file_wrap_physfs(&name##_storage, (physfs))
#else
#define REWIND_PHYSFS_FILE(name, physfs) rewind_file *name = (physfs)
#endif

#ifdef __cplusplus
}
#endif

#endif /* REWIND_FILE_H */