#include "online_admission.h"
#include <assert.h>

int main(void)
{
	/* UUID 00010203-0405-0607-0809-0a0b0c0d0e0f, same fixture as the JVM producer */
	unsigned char frame[] = { 68, 88, 79, 65, 2, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 8, 42 };
	unsigned char id[16] = { 0 }, slot = 0;
	assert(online_admission_unwrap(frame, sizeof(frame), 1, 42432, id, &slot) == 21);
	assert(slot == 2 && id[0] == 0 && id[15] == 15);
	assert(online_admission_unwrap(frame, sizeof(frame), 0, 42432, id, &slot) == -1);
	assert(online_admission_unwrap(frame, sizeof(frame), 1, 42433, id, &slot) == -1);
	assert(online_admission_unwrap(frame, 21, 1, 42432, id, &slot) == -1);
	frame[21] = 68;
	assert(online_admission_unwrap(frame, sizeof(frame), 1, 42432, id, &slot) == -1);
	frame[21] = 8;
	memset(frame + 5, 0, 16);
	assert(online_admission_unwrap(frame, sizeof(frame), 1, 42432, id, &slot) == -1);
	frame[0] = 8;
	assert(online_admission_unwrap(frame, sizeof(frame), 1, 42432, id, &slot) == 0);
	return 0;
}
