/* Included by D2 piggy.c to reuse its native bitmap header reader and ownership */
#include "pog_resources.h"

struct xl_pog_resources {
	int count;
	ushort *indices;
	DiskBitmapHeader *headers;
	ubyte *source_data;
	ubyte *install_data;
	size_t data_size;
};

void xl_pog_free(struct xl_pog_resources *resources)
{
	if (!resources) return;
	d_free(resources->indices);
	d_free(resources->headers);
	d_free(resources->source_data);
	d_free(resources->install_data);
	d_free(resources);
}

struct xl_pog_resources *xl_pog_prepare(PHYSFS_file *file)
{
	struct xl_pog_resources *resources;
	PHYSFS_sint64 data_size;
	int i, count;
	if (!PHYSFS_seek(file, 8)) return NULL;
	count = PHYSFSX_readInt(file);
	data_size = PHYSFS_fileLength(file) - 12 - (PHYSFS_sint64) count * (2 + sizeof(DiskBitmapHeader));
	if (count < 0 || count > MAX_BITMAP_FILES || data_size < 0 || data_size > 64 * 1024 * 1024) return NULL;
	resources = d_malloc(sizeof(*resources));
	if (!resources) return NULL;
	memset(resources, 0, sizeof(*resources));
	resources->count = count;
	resources->data_size = (size_t) data_size;
	resources->indices = d_malloc((count ? count : 1) * sizeof(*resources->indices));
	resources->headers = d_malloc((count ? count : 1) * sizeof(*resources->headers));
	resources->source_data = d_malloc(data_size ? (size_t) data_size : 1);
	resources->install_data = d_malloc(data_size ? (size_t) data_size : 1);
	if (!resources->indices || !resources->headers || !resources->source_data || !resources->install_data) goto failed;
	for (i = 0; i < count; ++i) {
		resources->indices[i] = PHYSFSX_readShort(file);
		if (resources->indices[i] >= Num_bitmap_files) goto failed;
	}
	for (i = 0; i < count; ++i) {
		DiskBitmapHeader_read(&resources->headers[i], file);
		if (resources->headers[i].offset < 0 || (size_t) resources->headers[i].offset >= resources->data_size) goto failed;
	}
	if (PHYSFS_readBytes(file, resources->source_data, data_size) != data_size) goto failed;
	memcpy(resources->install_data, resources->source_data, resources->data_size);
	return resources;
failed:
	xl_pog_free(resources);
	return NULL;
}

int xl_pog_install(struct xl_pog_resources *resources)
{
	int i;
	if (resources && !resources->install_data) {
		/* Repeated reloads prepare another owned buffer before freeing live pixels */
		resources->install_data = d_malloc(resources->data_size ? resources->data_size : 1);
		if (!resources->install_data) return 0;
		memcpy(resources->install_data, resources->source_data, resources->data_size);
	}
	free_bitmap_replacements();
	if (!resources) return 1;
	Bitmap_replacement_data = resources->install_data;
	resources->install_data = NULL;
#ifdef __ANDROID__
	Bitmap_replacement_data_size = resources->data_size;
#endif
	for (i = 0; i < resources->count; ++i) {
		const DiskBitmapHeader *header = &resources->headers[i];
		grs_bitmap *bitmap = &GameBitmaps[resources->indices[i]];
		const int width = header->width + ((short) (header->wh_extra & 0x0f) << 8);
		gr_set_bitmap_data(bitmap, NULL);
		gr_init_bitmap(bitmap, 0, 0, 0, width, header->height + ((short) (header->wh_extra & 0xf0) << 4), width, NULL);
		bitmap->avg_color = header->avg_color;
		gr_set_bitmap_flags(bitmap, header->flags & BM_FLAGS_TO_COPY);
		gr_set_bitmap_data(bitmap, Bitmap_replacement_data + header->offset);
		GameBitmapOffset[resources->indices[i]] = 0;
	}
	last_palette_loaded_pig[0] = 0;
	texmerge_flush();
	return 1;
}
