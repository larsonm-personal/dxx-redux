#include <stdio.h>
#include <physfs.h>

int main(void)
{
	PHYSFS_Version compiled;
	PHYSFS_Version linked;
	PHYSFS_VERSION(&compiled);
	PHYSFS_getLinkedVersion(&linked);
	printf("{\"compiled\":\"%u.%u.%u\",\"linked\":\"%u.%u.%u\"}\n",
	       compiled.major, compiled.minor, compiled.patch,
	       linked.major, linked.minor, linked.patch);
	return compiled.major != linked.major || compiled.minor != linked.minor || compiled.patch != linked.patch;
}
