#ifdef _MSC_VER
/* Exercise callers that include the byte-packed engine headers first */
#pragma pack(push, 1)
#endif
#include "secret_area_scan.h"
#ifdef _MSC_VER
#pragma pack(pop)
#endif

#include <stdio.h>
#include <string.h>

#define N 70
#define CHECK(x)                                            \
	do {                                                    \
		if (!(x)) {                                         \
			fprintf(stderr, "line %d: %s\n", __LINE__, #x); \
			return 1;                                       \
		}                                                   \
	} while (0)
enum { DOOR = 1,
	   ILLUSION = 2,
	   OPEN = 3,
	   BLASTABLE = 4 };
enum { ROBOT = 1,
	   HOSTAGE = 2,
	   POWERUP = 3,
	   REACTOR = 4 };
typedef struct fixture {
	int count;
	int optional_switch;
	int child[N][6], wall[N][6], opaque[N][6], trigger[N][6], blocked[N][6];
	int special[N], object[N], id[N];
} fixture;
static secret_area_state state;

static int child(void *u, int s, int side)
{
	return ((fixture *) u)->child[s][side];
}
static int reverse(void *u, int s, int c)
{
	int side;
	for (side = 0; side < 6; ++side)
		if (child(u, c, side) == s) return side;
	return -1;
}
static int wall(void *u, int s, int side)
{
	return ((fixture *) u)->wall[s][side] ? s * 6 + side : -1;
}
static int wall_type(void *u, int w)
{
	return ((fixture *) u)->wall[w / 6][w % 6];
}
static int zero(void *u, int n)
{
	(void) u;
	(void) n;
	return 0;
}
static int clip(void *u, int w)
{
	return wall_type(u, w) == DOOR;
}
static int special(void *u, int s)
{
	return ((fixture *) u)->special[s];
}
static int count(void *u)
{
	return ((fixture *) u)->count;
}
static int obj_segment(void *u, int n)
{
	(void) u;
	return n;
}
static int obj_type(void *u, int n)
{
	return ((fixture *) u)->object[n];
}
static int obj_id(void *u, int n)
{
	return ((fixture *) u)->id[n];
}
static int opaque(void *u, int s, int side)
{
	return ((fixture *) u)->opaque[s][side];
}
static int trigger(void *u, int s, int side)
{
	return ((fixture *) u)->trigger[s][side];
}
static int passable(void *u, int s, int side)
{
	fixture *f = (fixture *) u;
	return !f->blocked[s][side] && f->wall[s][side] != DOOR;
}
static int optional_open(void *u, int s, int side, const int *outside, int segments)
{
	fixture *f = (fixture *) u;
	return f->optional_switch && s == 1 && side == 2 && segments == f->count && outside[0] >= 0 && outside[1] < 0;
}
static void connect(fixture *f, int a, int side, int b, int back, int type, int concealed)
{
	f->child[a][side] = b;
	f->child[b][back] = a;
	f->wall[a][side] = f->wall[b][back] = type;
	f->opaque[a][side] = f->opaque[b][back] = concealed;
}
static secret_area_scan_view view(fixture *f)
{
	secret_area_scan_view v;
	memset(&v, 0, sizeof(v));
	v.user = f;
	v.num_segments = f->count;
	v.num_walls = N * 6;
	v.max_generated = SECRET_AREA_MAX_GENERATED;
	v.wall_type_door = DOOR;
	v.wall_type_illusion = ILLUSION;
	v.wall_type_open = OPEN;
	v.wall_type_blastable = BLASTABLE;
	v.wall_clip_hidden = 1;
	v.wall_flag_illusion_off = 2;
	v.wall_flag_door_locked = 4;
	v.obj_type_robot = ROBOT;
	v.obj_type_hostage = HOSTAGE;
	v.obj_type_powerup = POWERUP;
	v.obj_type_control_center = REACTOR;
	v.powerup_key_blue = 10;
	v.powerup_key_red = 11;
	v.powerup_key_gold = 12;
	v.segment_special_control_center = 1;
	v.segment_special_robotmaker = 2;
	v.segment_child = child;
	v.reverse_side = reverse;
	v.wall_num = wall;
	v.wall_type = wall_type;
	v.wall_flags = zero;
	v.wall_keys = zero;
	v.wall_clip_flags = clip;
	v.segment_special = special;
	v.object_count = count;
	v.object_segment = obj_segment;
	v.object_type = obj_type;
	v.object_id = obj_id;
	v.side_is_concealed_liquid = opaque;
	v.side_has_trigger = trigger;
	v.side_is_physically_passable = passable;
	v.side_has_optional_open_trigger = optional_open;
	return v;
}
static void init(fixture *f, int count)
{
	int s, side;
	memset(f, 0, sizeof(*f));
	f->count = count;
	for (s = 0; s < N; ++s)
		for (side = 0; side < 6; ++side) f->child[s][side] = -1;
	connect(f, 0, 0, 1, 0, ILLUSION, 1);
	f->object[1] = POWERUP;
	f->id[1] = 100;
}
static int scan(fixture *f)
{
	secret_area_scan_view v = view(f);
	return secret_area_scan_level(&v, &state);
}
static int classification(void)
{
	fixture f;
	int type;
	init(&f, 2);
	CHECK(scan(&f) == 1);
	CHECK(state.secrets[0].liquid_only && state.secrets[0].segment_count == 1);
	CHECK(state.segments[0] == 1 && state.secrets[0].item_count == 1);
	CHECK(state.secrets[0].entrance_count == 1);
	f.opaque[1][0] = 0;
	CHECK(scan(&f) == 0); /* Transparent/unknown reverse face */
	init(&f, 2);
	f.object[1] = 0;
	CHECK(scan(&f) == 0); /* Empty scenery */
	for (type = 0; type < 4; ++type) {
		init(&f, 3);
		connect(&f, 1, 1, 2, 0, 0, 0);
		f.object[2] = type == 0 ? POWERUP : type == 1 ? HOSTAGE
		                                : type == 2   ? REACTOR
		                                              : 0;
		f.id[2] = 10;
		f.special[2] = type == 3 ? 2 : 0;
		CHECK(scan(&f) == 0);
	}
	init(&f, 2);
	f.trigger[1][2] = 1;
	CHECK(scan(&f) == 0);
	f.optional_switch = 1;
	CHECK(scan(&f) == 1); /* Proven optional source does not make loot progression */
	f.trigger[0][0] = 1;
	CHECK(scan(&f) == 0); /* Entrance controls are never exempt */
	init(&f, 2);
	f.trigger[0][0] = 1;
	CHECK(scan(&f) == 0);
	init(&f, 2);
	f.blocked[1][0] = 1;
	CHECK(scan(&f) == 0);
	init(&f, 2);
	f.child[1][0] = -1;
	CHECK(scan(&f) == 0);
	init(&f, 3);
	connect(&f, 1, 1, 2, 0, DOOR, 0);
	CHECK(scan(&f) == 0);
	init(&f, 3);
	connect(&f, 0, 1, 2, 0, 0, 0);
	connect(&f, 2, 1, 1, 1, 0, 0);
	CHECK(scan(&f) == 0); /* Ordinary alternate entrance */
	init(&f, 3);
	connect(&f, 1, 1, 2, 0, ILLUSION, 1);
	f.object[2] = POWERUP;
	f.id[2] = 101;
	CHECK(scan(&f) == 1 && state.secrets[0].segment_count == 2);
	CHECK(state.secrets[0].item_count == 2 && state.secrets[0].entrance_count == 1);
	init(&f, 3);
	connect(&f, 0, 1, 2, 0, 0, 0);
	connect(&f, 2, 1, 1, 1, ILLUSION, 1);
	CHECK(scan(&f) == 1 && state.secrets[0].entrance_count == 2);
	/* Existing hidden-door region owns its liquid interior */
	init(&f, 3);
	f.wall[0][0] = f.wall[1][0] = DOOR;
	f.opaque[0][0] = f.opaque[1][0] = 0;
	connect(&f, 1, 1, 2, 0, ILLUSION, 1);
	f.object[2] = POWERUP;
	f.id[2] = 101;
	CHECK(scan(&f) == 1 && !state.secrets[0].liquid_only && state.secrets[0].segment_count == 2);
	return 0;
}
static int persistence(void)
{
	fixture f;
	secret_area_saved_state saved, unchanged;
	unsigned char bytes[SECRET_AREA_IDENTITY_SAVE_SIZE], visited[N];
	unsigned long long identity;
	init(&f, 3);
	CHECK(scan(&f) == 1);
	identity = state.secrets[0].identity;
	CHECK(secret_area_mark_segment_entered(&state, 1) == 1);
	secret_area_encode_saved_state(&state, 0x0123456789abcdefULL, bytes);
	CHECK(bytes[0] == 1 && bytes[4] == 0xef && bytes[11] == 0x01);
	CHECK(secret_area_decode_saved_state(bytes, sizeof(bytes), &saved));
	CHECK(saved.level_identity == 0x0123456789abcdefULL && saved.identities[0] == identity && saved.found[0]);
	/* Add an earlier entrance without changing the old pocket's membership */
	connect(&f, 0, 1, 1, 0, ILLUSION, 1);
	connect(&f, 0, 0, 2, 0, ILLUSION, 1);
	f.object[2] = POWERUP;
	f.id[2] = 101;
	CHECK(scan(&f) == 2 && state.secrets[1].identity == identity);
	CHECK(secret_area_restore_identities(&state, saved.count, saved.identities, saved.found));
	CHECK(!state.found[0] && state.found[1] && state.found_count == 1);
	memset(visited, 1, sizeof(visited));
	secret_area_restore_found_from_visited(&state, visited, N);
	CHECK(state.found_count == 0); /* Seen liquid pocket is not necessarily entered */
	memcpy(&unchanged, &saved, sizeof(saved));
	CHECK(!secret_area_decode_saved_state(bytes, sizeof(bytes) - 1, &saved));
	CHECK(!memcmp(&saved, &unchanged, sizeof(saved)));
	bytes[20] = 2;
	CHECK(!secret_area_decode_saved_state(bytes, sizeof(bytes), &saved));
	bytes[20] = 1;
	bytes[0] = 31;
	CHECK(!secret_area_decode_saved_state(bytes, sizeof(bytes), &saved));
	bytes[0] = 2;
	memcpy(bytes + 21, bytes + 12, 9);
	CHECK(!secret_area_decode_saved_state(bytes, sizeof(bytes), &saved)); /* Duplicate identity */
	bytes[0] = 1;
	CHECK(!secret_area_decode_saved_state(bytes, sizeof(bytes), &saved)); /* Nonzero unused slot */
	CHECK(!memcmp(&saved, &unchanged, sizeof(saved)));
	/* Splitting membership must not transfer discovery by overlap */
	init(&f, 3);
	connect(&f, 1, 1, 2, 0, 0, 0);
	CHECK(scan(&f) == 1);
	CHECK(state.secrets[0].identity != identity);
	CHECK(secret_area_restore_identities(&state, unchanged.count, unchanged.identities, unchanged.found));
	CHECK(!state.found_count);
	return 0;
}
static int budget(void)
{
	fixture f;
	int i;
	init(&f, 64);
	for (i = 0; i < 32; ++i) {
		if (i < 31) connect(&f, i, 1, i + 1, 2, 0, 0);
		connect(&f, i, 0, 32 + i, 0, ILLUSION, 1);
		f.object[i] = 0;
		f.object[32 + i] = POWERUP;
		f.id[32 + i] = 100;
	}
	CHECK(scan(&f) == 0 && !state.enabled);
	CHECK(state.disabled_reason == SECRET_AREA_DISABLED_TOO_MANY_CANDIDATES);
	return 0;
}
int main(void)
{
	if (classification() || persistence() || budget()) return 1;
	puts("secret liquid classification and identity serialization passed");
	return 0;
}
