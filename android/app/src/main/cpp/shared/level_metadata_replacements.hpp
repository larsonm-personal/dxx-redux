#ifndef DXX_LEVEL_METADATA_REPLACEMENTS_HPP
#define DXX_LEVEL_METADATA_REPLACEMENTS_HPP

#include <cstdio>
#include <cstring>
#include <string>

#include <nlohmann/json.hpp>

#ifdef DXX_BUILD_DESCENT_II
struct level_metadata_model_signature {
	int radius;
	int data_size;
	unsigned long long data_hash;
};

struct level_metadata_base_definitions {
	player_ship ship;
	weapon_info weapons[MAX_WEAPON_TYPES];
	robot_info robots[MAX_ROBOT_TYPES];
	level_metadata_model_signature models[MAX_POLYGON_MODELS];
	char sound_sources[2][PATH_MAX];
	int weapon_count;
	int robot_count;
	int model_count;
	int captured;
};

static unsigned long long level_metadata_replacement_hash(
    const unsigned char *data, size_t size)
{
	unsigned long long hash = 1469598103934665603ULL;

	for (size_t index = 0; index < size; ++index) {
		hash ^= data[index];
		hash *= 1099511628211ULL;
	}
	return hash;
}

static level_metadata_model_signature level_metadata_model_state(
    const polymodel &model)
{
	level_metadata_model_signature signature = {
		model.rad,
		model.model_data_size,
		model.model_data && model.model_data_size > 0
		    ? level_metadata_replacement_hash(
		          model.model_data, (size_t) model.model_data_size)
		    : 0,
	};
	return signature;
}

static void level_metadata_capture_base_definitions(
    level_metadata_base_definitions &base)
{
	base.ship = *Player_ship;
	base.weapon_count = N_weapon_types;
	base.robot_count = N_robot_types;
	base.model_count = N_polygon_models;
	memcpy(base.weapons, Weapon_info, sizeof(base.weapons));
	memcpy(base.robots, Robot_info, sizeof(base.robots));
	for (int index = 0; index < N_polygon_models; ++index)
		base.models[index] = level_metadata_model_state(Polygon_models[index]);
	const char *sound_files[] = { "descent2.s11", "descent2.s22" };
	for (int index = 0; index < 2; ++index) {
		const char *source = PHYSFS_getRealDir(sound_files[index]);
		snprintf(base.sound_sources[index], sizeof(base.sound_sources[index]), "%s", source ? source : "");
	}
	base.captured = 1;
}

template <typename Json>
static void level_metadata_add_changed_field(
    Json &fields, const char *kind, const char *label,
    int base_value, int mod_value, const char *format = "integer")
{
	if (base_value == mod_value)
		return;
	fields.push_back({
	    { "kind", kind },
	    { "label", label },
	    { "base_game", base_value },
	    { "mod", mod_value },
	    { "format", format },
	});
}

template <typename Json>
static void level_metadata_add_group(
    Json &groups, const char *kind, const char *label, Json items)
{
	if (items.empty())
		return;
	groups.push_back({
	    { "kind", kind },
	    { "label", label },
	    { "summary", std::to_string(items.size()) + (items.size() == 1 ? " change" : " changes") },
	    { "items", items },
	});
}

static int level_metadata_pog_replacement_count(const char *level_file)
{
	char pog_file[FILENAME_LEN];
	char extension[] = ".POG";
	PHYSFS_file *file;

	if (!level_file || !level_file[0])
		return 0;
	change_filename_extension(pog_file, level_file, extension);
	file = PHYSFSX_openReadBuffered(pog_file);
	if (!file)
		return 0;
	const int id = PHYSFSX_readInt(file);
	const int version = PHYSFSX_readInt(file);
	const int count = PHYSFSX_readInt(file);
	PHYSFS_close(file);
	return id == MAKE_SIG('G', 'O', 'P', 'D') && version == 1 && count > 0 ? count : 0;
}

template <typename Json>
static Json level_metadata_serialize_replacement_groups(
    const level_metadata_base_definitions &base, const char *level_file)
{
	Json groups = Json::array();
	Json ship_items = Json::array();
	Json weapon_items = Json::array();
	Json robot_items = Json::array();
	Json asset_items = Json::array();
	Json fields = Json::array();
	char label[48];

	if (!base.captured)
		return groups;
	level_metadata_add_changed_field(
	    fields, "player_ship_size", "Size",
	    base.models[base.ship.model_num].radius,
	    Polygon_models[Player_ship->model_num].rad, "fixed");
	level_metadata_add_changed_field(fields, "ship_mass", "Mass", base.ship.mass, Player_ship->mass, "fixed");
	level_metadata_add_changed_field(fields, "ship_drag", "Drag", base.ship.drag, Player_ship->drag, "fixed");
	level_metadata_add_changed_field(fields, "ship_forward_thrust", "Forward thrust", base.ship.max_thrust, Player_ship->max_thrust, "fixed");
	level_metadata_add_changed_field(fields, "ship_reverse_thrust", "Reverse thrust", base.ship.reverse_thrust, Player_ship->reverse_thrust, "fixed");
	level_metadata_add_changed_field(fields, "ship_brakes", "Braking thrust", base.ship.brakes, Player_ship->brakes, "fixed");
	level_metadata_add_changed_field(fields, "ship_rotation_thrust", "Rotation thrust", base.ship.max_rotthrust, Player_ship->max_rotthrust, "fixed");
	level_metadata_add_changed_field(fields, "ship_wiggle", "Wiggle", base.ship.wiggle, Player_ship->wiggle, "fixed");
	if (!fields.empty())
		ship_items.push_back({ { "kind", "player_ship" }, { "label", "Player ship" }, { "fields", fields } });

	for (int index = 0; index < N_weapon_types; ++index) {
		fields = Json::array();
		if (index < base.weapon_count) {
			const weapon_info &before = base.weapons[index];
			const weapon_info &after = Weapon_info[index];
			level_metadata_add_changed_field(fields, "damage", "Damage (Hotshot)", before.strength[2], after.strength[2], "fixed");
			level_metadata_add_changed_field(fields, "speed", "Projectile speed (Hotshot)", before.speed[2], after.speed[2], "fixed");
			level_metadata_add_changed_field(fields, "fire_wait", "Fire interval", before.fire_wait, after.fire_wait, "fixed");
			level_metadata_add_changed_field(fields, "energy_usage", "Energy per shot", before.energy_usage, after.energy_usage, "fixed");
			level_metadata_add_changed_field(fields, "ammo_usage", "Ammo per shot", before.ammo_usage, after.ammo_usage);
			level_metadata_add_changed_field(fields, "fire_count", "Projectiles per burst", before.fire_count, after.fire_count);
			level_metadata_add_changed_field(fields, "damage_radius", "Blast radius", before.damage_radius, after.damage_radius, "fixed");
			if (fields.empty() && memcmp(&before, &after, sizeof(before)))
				fields.push_back({ { "kind", "other" }, { "label", "Other definition data" }, { "base_game_text", "Base game" }, { "mod_text", "Changed" } });
		} else {
			fields.push_back({ { "kind", "added" }, { "label", "Definition" }, { "base_game_text", "Not present" }, { "mod_text", "Added" } });
		}
		if (!fields.empty()) {
			snprintf(label, sizeof(label), "Weapon %d", index);
			weapon_items.push_back({ { "kind", "weapon" }, { "label", label }, { "fields", fields } });
		}
	}

	for (int index = 0; index < N_robot_types; ++index) {
		fields = Json::array();
		if (index < base.robot_count) {
			const robot_info &before = base.robots[index];
			const robot_info &after = Robot_info[index];
			level_metadata_add_changed_field(fields, "shields", "Shields", before.strength, after.strength, "fixed");
			level_metadata_add_changed_field(fields, "speed", "Maximum speed (Hotshot)", before.max_speed[2], after.max_speed[2], "fixed");
			level_metadata_add_changed_field(fields, "fire_wait", "Fire interval (Hotshot)", before.firing_wait[2], after.firing_wait[2], "fixed");
			level_metadata_add_changed_field(fields, "weapon", "Primary weapon ID", before.weapon_type, after.weapon_type);
			level_metadata_add_changed_field(fields, "score", "Score", before.score_value, after.score_value);
			level_metadata_add_changed_field(fields, "aim", "Aim", before.aim, after.aim);
			level_metadata_add_changed_field(fields, "mass", "Mass", before.mass, after.mass, "fixed");
			level_metadata_add_changed_field(fields, "drag", "Drag", before.drag, after.drag, "fixed");
			if (fields.empty() && memcmp(&before, &after, sizeof(before)))
				fields.push_back({ { "kind", "other" }, { "label", "Other definition data" }, { "base_game_text", "Base game" }, { "mod_text", "Changed" } });
		} else {
			fields.push_back({ { "kind", "added" }, { "label", "Definition" }, { "base_game_text", "Not present" }, { "mod_text", "Added" } });
		}
		if (!fields.empty()) {
			snprintf(label, sizeof(label), "Robot %d", index);
			robot_items.push_back({ { "kind", "robot" }, { "number", index }, { "label", label }, { "fields", fields } });
		}
	}

	int changed_models = 0;
	for (int index = 0; index < N_polygon_models; ++index) {
		if (index >= base.model_count) {
			++changed_models;
			continue;
		}
		const level_metadata_model_signature current = level_metadata_model_state(Polygon_models[index]);
		if (current.radius != base.models[index].radius ||
		    current.data_size != base.models[index].data_size ||
		    current.data_hash != base.models[index].data_hash)
			++changed_models;
	}
	if (changed_models)
		asset_items.push_back({ { "kind", "models" }, { "label", "Polygon models" }, { "summary", std::to_string(changed_models) + " changed" }, { "fields", Json::array() } });
	const int changed_textures = level_metadata_pog_replacement_count(level_file);
	if (changed_textures)
		asset_items.push_back({ { "kind", "textures" }, { "label", "Textures" }, { "summary", std::to_string(changed_textures) + " replaced" }, { "fields", Json::array() } });
	const char *sound_files[] = { "descent2.s11", "descent2.s22" };
	const char *sound_labels[] = { "11 kHz sound bank", "22 kHz sound bank" };
	for (int index = 0; index < 2; ++index) {
		const char *source = PHYSFS_getRealDir(sound_files[index]);
		if (source && strcmp(base.sound_sources[index], source))
			asset_items.push_back({ { "kind", "sounds" }, { "label", sound_labels[index] }, { "summary", "Replaced" }, { "fields", Json::array() } });
	}

	const int total_changes = (int) (ship_items.size() + weapon_items.size() + robot_items.size() + asset_items.size());
	if (!total_changes)
		return groups;
	level_metadata_add_group(groups, "ship_stats", "Ship stats", ship_items);
	level_metadata_add_group(groups, "weapon_balance", "Weapon balance", weapon_items);
	level_metadata_add_group(groups, "robot_changes", "Robot changes", robot_items);
	level_metadata_add_group(groups, "asset_replacements", "Texture/model/sound replacements", asset_items);
	return groups;
}
#endif

#endif
