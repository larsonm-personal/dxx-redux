#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "coop/coop_save_format.h"

static void write_save(FILE *file, unsigned version, int corrupt)
{
	unsigned char payload[5000] = { 0 };
	uint32_t tag = COOP_SAVE_META_TAG;
	uint16_t ver = (uint16_t) version;
	coop_save_footer footer = { 0 };
	memcpy(payload, &tag, sizeof(tag));
	memcpy(payload + sizeof(tag), &ver, sizeof(ver));
	footer.tag = COOP_SAVE_FOOTER_TAG;
	footer.version = ver;
	footer.payload_size = sizeof(payload);
	footer.checksum = coop_save_checksum(payload, sizeof(payload), 2166136261u);
	if (corrupt) payload[4097] ^= 1;
	rewind(file);
	assert(fwrite(payload, sizeof(payload), 1, file) == 1);
	assert(fwrite(&footer, sizeof(footer), 1, file) == 1);
	assert(fflush(file) == 0);
}

int main(void)
{
	FILE *file = tmpfile();
	long end;
	assert(file);
	write_save(file, COOP_SAVE_META_VER, 0);
	end = ftell(file);
	assert(coop_save_format_supported(file, end));
	assert(!coop_save_format_supported(file, end - 1));
	assert(!coop_save_format_supported(file, 0));
	write_save(file, COOP_SAVE_META_VER - 1, 0);
	assert(!coop_save_format_supported(file, end));
	write_save(file, COOP_SAVE_META_VER, 1);
	assert(!coop_save_format_supported(file, end));
	write_save(file, COOP_SAVE_META_VER, 0);
	assert(fseek(file, end, SEEK_SET) == 0);
	assert(fwrite("optional Android trailer", 24, 1, file) == 1);
	assert(coop_save_format_supported(file, end));
	assert(!coop_save_format_supported(file, end + 24));
	fclose(file);
	return 0;
}
