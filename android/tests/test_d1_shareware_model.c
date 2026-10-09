#include <stdio.h>
#include <stdlib.h>
#include "d1_shareware_model.h"

#define CHECK(x)                                                      \
	do {                                                              \
		if (!(x)) {                                                   \
			fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #x); \
			return 1;                                                 \
		}                                                             \
	} while (0)

static void word(uint8_t *p, uint32_t value)
{
	p[0] = (uint8_t) value;
	p[1] = (uint8_t) (value >> 8);
	p[2] = (uint8_t) (value >> 16);
	p[3] = (uint8_t) (value >> 24);
}

static int test_hog(const char *path)
{
	FILE *file = fopen(path, "rb");
	uint8_t header[17];
	int models = 0, submodels = 0, guns = 0, animations = 0;
	CHECK(file != NULL);
	CHECK(fread(header, 1, 3, file) == 3 && !memcmp(header, "DHF", 3));
	while (fread(header, 1, sizeof(header), file) == sizeof(header)) {
		uint32_t size = d1_pof_u32(header + 13);
		char name[14];
		memcpy(name, header, 13);
		name[13] = 0;
		if (strstr(name, ".pof")) {
			uint8_t *bytes;
			d1_pof_source source;
			d1_pof_bounds bounds;
			const char *error;
			CHECK(size <= 32768);
			bytes = (uint8_t *) malloc(size);
			CHECK(bytes != NULL && fread(bytes, 1, size, file) == size);
			error = d1_pof_read(bytes, size, &source);
			if (error)
				fprintf(stderr, "%s: %s\n", name, error);
			CHECK(error == NULL);
			CHECK(d1_pof_find_bounds(&source, &bounds) == NULL);
			++models;
			submodels += source.model_count;
			guns += source.gun_count;
			animations += source.has_animation;
			/* Truncated chunks must fail before a caller can publish model data */
			CHECK(d1_pof_read(bytes, size - 1, &source) != NULL);
			free(bytes);
		} else {
			CHECK(fseek(file, size, SEEK_CUR) == 0);
		}
	}
	fclose(file);
	printf("Fixture models=%d submodels=%d guns=%d animations=%d\n", models, submodels, guns, animations);
	CHECK(models == 45 && submodels == 96 && guns == 52 && animations == 16);
	return 0;
}

static int test_bounds(void)
{
	/* The source is deliberately unaligned, with signed little-endian vertices */
	uint8_t bytes[33] = { 0 };
	d1_pof_source source = { 0 };
	d1_pof_bounds bounds;
	source.model_count = 1;
	source.instructions = bytes + 1;
	source.instruction_size = 32;
	source.translations[0].x = 7;
	bytes[1] = 7;
	bytes[3] = 2;
	word(bytes + 9, (uint32_t) -100);
	word(bytes + 13, 200);
	word(bytes + 17, 300);
	word(bytes + 21, 400);
	word(bytes + 25, (uint32_t) -500);
	word(bytes + 29, 600);
	CHECK(d1_pof_find_bounds(&source, &bounds) == NULL);
	CHECK(bounds.submodel_mins[0].x == -100 && bounds.submodel_maxs[0].x == 400);
	CHECK(bounds.mins.x == -100 && bounds.maxs.x == 407);
	CHECK(bounds.mins.y == -500 && bounds.maxs.y == 200);
	CHECK(bounds.mins.z == 300 && bounds.maxs.z == 600);
	source.instruction_size--;
	CHECK(d1_pof_find_bounds(&source, &bounds) != NULL);
	source.instruction_size++;
	source.translations[0].x = INT32_MAX;
	CHECK(d1_pof_find_bounds(&source, &bounds) != NULL);
	source.translations[0].x = 0;
	bytes[3] = 0;
	CHECK(d1_pof_find_bounds(&source, &bounds) != NULL);
	bytes[3] = 2;
	source.offsets[0] = -1;
	CHECK(d1_pof_find_bounds(&source, &bounds) != NULL);
	return 0;
}

int main(int argc, char **argv)
{
	uint8_t bytes[112] = { 0 };
	d1_pof_source source;
	CHECK(test_bounds() == 0);
	memcpy(bytes, "PSPO", 4);
	bytes[4] = 6;
	memcpy(bytes + 6, "OHDR", 4);
	word(bytes + 10, 32);
	word(bytes + 14, 1);
	memcpy(bytes + 46, "SOBJ", 4);
	word(bytes + 50, 48);
	bytes[56] = bytes[57] = 0xff;
	memcpy(bytes + 102, "IDTA", 4);
	word(bytes + 106, 2);
	CHECK(d1_pof_read(bytes, sizeof(bytes), &source) == NULL);
	CHECK(source.model_count == 1 && source.instruction_size == 2);
	CHECK(d1_pof_read(bytes, 5, &source) != NULL);
	word(bytes + 10, 0xffffffffu);
	CHECK(d1_pof_read(bytes, sizeof(bytes), &source) != NULL);
	word(bytes + 10, 32);
	word(bytes + 98, 2);
	CHECK(d1_pof_read(bytes, sizeof(bytes), &source) != NULL);
	word(bytes + 98, 0);
	word(bytes + 14, D1_POF_SUBMODELS + 1);
	CHECK(d1_pof_read(bytes, sizeof(bytes), &source) != NULL);
	if (argc == 2)
		CHECK(test_hog(argv[1]) == 0);
	puts("PASS: shared D1 POF source decoding");
	return 0;
}
