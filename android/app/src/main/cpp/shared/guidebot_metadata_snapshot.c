/* Use the memory stream on host tests as well as Android */
#define DXX_REWIND_FILE_CORE_ONLY
#define DXX_REWIND_FILE_WRAPPER
#include "guidebot_metadata_snapshot.h"
#include "guidebot_metadata_save_fields.h"

static void snapshot_fields(guidebot_save_stream *s, guidebot_metadata_snapshot *snapshot)
{
#define METADATA_STRUCT(type, name, serializer) serializer(s, &snapshot->name);
#define METADATA_VALUE(type, name, kind)        GB_FIELD(s, snapshot->name, kind);
#include "guidebot_metadata_save_members.h"
#undef METADATA_STRUCT
#undef METADATA_VALUE
}

size_t guidebot_metadata_snapshot_encoded_size(void)
{
	static guidebot_metadata_snapshot empty;
	guidebot_save_stream stream = { 0 };
	stream.writing = 2;
	stream.ok = 1;
	snapshot_fields(&stream, &empty);
	return stream.ok ? stream.bytes : 0;
}

int guidebot_metadata_snapshot_encode(const guidebot_metadata_snapshot *snapshot,
                                      void *data, size_t size, int64_t epoch)
{
	rewind_memory_buffer buffer = { 0 };
	rewind_file file;
	guidebot_save_stream stream = { 0 };
	if (!snapshot || !data || size != guidebot_metadata_snapshot_encoded_size()) return 0;
	buffer.data = data;
	buffer.capacity = size;
	rewind_file_init_memory_write(&file, &buffer);
	stream.file = &file;
	stream.writing = stream.ok = 1;
	stream.epoch = epoch;
	/* The shared writer does not modify fields when writing is nonzero */
	snapshot_fields(&stream, (guidebot_metadata_snapshot *) snapshot);
	return rewind_file_close(&file) && stream.ok && buffer.size == size;
}

int guidebot_metadata_snapshot_decode(guidebot_metadata_snapshot *snapshot,
                                      const void *data, size_t size, int64_t epoch)
{
	guidebot_metadata_snapshot *decoded;
	rewind_file file;
	guidebot_save_stream stream = { 0 };
	int ok;
	if (!snapshot || !data || size != guidebot_metadata_snapshot_encoded_size()) return 0;
	decoded = calloc(1, sizeof(*decoded));
	if (!decoded) return 0;
	rewind_file_init_memory_read(&file, data, size);
	stream.file = &file;
	stream.apply = stream.ok = 1;
	stream.epoch = epoch;
	snapshot_fields(&stream, decoded);
	ok = stream.ok && stream.bytes == size;
	if (ok) memcpy(snapshot, decoded, sizeof(*snapshot));
	free(decoded);
	return ok;
}
