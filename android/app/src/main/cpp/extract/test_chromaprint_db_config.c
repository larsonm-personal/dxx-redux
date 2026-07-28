#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "chromaprint_db.h"

int main(void)
{
	static const char empty_db[] = "[]";

	assert(chromaprint_db_load(empty_db, 2) == -1);
	assert(chromaprint_db_set_threshold(NAN) == 0);
	assert(chromaprint_db_load(empty_db, 2) == -1);
	assert(chromaprint_db_set_threshold(INFINITY) == 0);
	assert(chromaprint_db_set_threshold(0.0f) == 0);
	assert(chromaprint_db_set_threshold(1.01f) == 0);

	assert(chromaprint_db_set_threshold(0.40f) == 1);
	assert(chromaprint_db_load(empty_db, 2) == 0);
	assert(chromaprint_db_set_threshold(0.65f) == 1);
	assert(chromaprint_db_load(empty_db, 2) == 0);

	assert(chromaprint_db_set_threshold(NAN) == 0);
	assert(chromaprint_db_load(empty_db, 2) == -1);
	chromaprint_db_free();
	printf("chromaprint DB configuration tests passed\n");
	return 0;
}
