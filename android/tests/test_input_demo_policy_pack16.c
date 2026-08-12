#include <stddef.h>

#pragma pack(push, 16)
#include "input_demo_direct_command_policy.h"
struct input_demo_policy_outer_pack16_probe {
	char prefix;
	void *pointer;
};
typedef char input_demo_policy_outer_pack16_preserved[
	offsetof(struct input_demo_policy_outer_pack16_probe, pointer) == sizeof(void *) ? 1 : -1];
#pragma pack(pop)
