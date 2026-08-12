#include <stddef.h>

#pragma pack(push, 1)
#include "input_demo_direct_command_policy.h"
struct input_demo_policy_outer_pack1_probe {
	char prefix;
	void *pointer;
};
typedef char input_demo_policy_outer_pack1_preserved[
	offsetof(struct input_demo_policy_outer_pack1_probe, pointer) == 1 ? 1 : -1];
#pragma pack(pop)
