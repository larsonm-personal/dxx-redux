#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include "trigger_state.h"
#include "mission_progress.h"
#include "object_trigger.h"
#include "robot_generator.h"
#include "boss_spawn.h"
#include "boss_routes.h"
#include "equipment.h"
#include "preview.h"
#include "speed_boost.h"
#include "spawn_points.h"
#include "fluids.h"
#include "repair.h"
#include "projectile.h"
#include "changing_light.h"
#include "flicker.h"
#include "level_messages.h"
#include "named_audio.h"
#include "named_audio_state.h"
#include "ambient_objects.h"
#include "particle_state.h"
#include "reactor_countdown.h"
#include "seismic_gameplay.h"
#include "robot_disarm_state.h"
extern "C" {
#include "inferno.h"
#include "switch.h"
#include "segment.h"
#include "object.h"
#include "console.h"
}

#if REWIND_FILE_USES_WRAPPER
#define xl_read  rewind_file_read
#define xl_write rewind_file_write
#define xl_tell  rewind_file_tell
#define xl_seek  rewind_file_seek
#else
#define xl_read  PHYSFS_read
#define xl_write PHYSFS_write
#define xl_tell  PHYSFS_tell
#define xl_seek  PHYSFS_seek
#endif

namespace
{
using nlohmann::json;
constexpr char tag[8] = { 'D', 'X', 'X', 'L', 'T', 'R', '0', '1' };
// Bounded events/lifecycle plus up to 1000 destinations per native boss slot
constexpr std::uint32_t maximum = 8 * 1024 * 1024;
void require(bool result, const char *message)
{
	if (!result) throw std::runtime_error(message);
}
std::uint32_t checksum(const std::string &payload)
{
	std::uint32_t result = 2166136261u;
	for (unsigned char byte : payload) result = (result ^ byte) * 16777619u;
	return result;
}
void write_u32(rewind_file *file, std::uint32_t value)
{
	const unsigned char bytes[] = { static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8), static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24) };
	require(xl_write(file, bytes, 1, 4) == 4, "Cannot write XL trigger state header");
}
std::uint32_t read_u32(rewind_file *file)
{
	unsigned char bytes[4];
	require(xl_read(file, bytes, 1, 4) == 4, "Truncated XL trigger state header");
	return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) | (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}
} // namespace

extern "C" int xl_wall_trigger_write(rewind_file *file)
{
	if (!xl_level_extensions_active()) return 1;
	try {
		require(Num_triggers >= 0 && Num_triggers <= MAX_TRIGGERS, "Invalid XL trigger state count");
		json rows = json::array();
		json actors = json::array();
		for (int index = 0; index < Num_triggers; ++index) {
			xl_trigger_state state;
			require(xl_wall_trigger_state(index, &state), "Cannot capture XL trigger state");
			rows.push_back({ state.elapsed, state.delay_ms, state.object, state.shot, state.operated });
			if (xl_object_trigger_count()) rows.back().push_back(state.destination);
			actors.push_back({ state.attached, state.signature });
		}
		json block = { { "schema", 1 }, { "source", xl_level_trigger_identity() }, { "states", rows } };
		if (xl_object_trigger_count()) {
			block["schema"] = 2;
			block["objects"] = json::array();
			for (int i = 0; i < xl_object_trigger_count(); ++i) {
				xl_object_trigger_state state;
				require(xl_object_trigger_state_get(i, &state), "Cannot capture XL object state");
				block["objects"].push_back({ state.owner, state.signature, state.type, state.flags, state.value, state.operated, state.destination });
			}
		}
		if (xl_boss_spawn_count()) {
			block["schema"] = 3;
			block["producer_implicit"] = xl_robot_generator_ready_mask();
			block["bosses"] = json::array();
			for (int i = 0; i < xl_boss_spawn_count(); ++i) {
				xl_boss_spawn_state state;
				require(xl_boss_spawn_state_get(i, &state), "Cannot capture XL boss spawn state");
				block["bosses"].push_back({ state.elapsed, state.cloak_start, state.cloak_end, state.teleport, state.hit, state.death_elapsed, state.owner, state.signature, state.id, state.interval, state.dying, state.sound });
			}
		}
		if (xl_equipment_count()) {
			block["schema"] = 4;
			block["equipment"] = json::array();
			for (int i = 0; i < xl_equipment_count(); ++i) {
				xl_equipment_state state;
				require(xl_equipment_state_get(i, &state), "Cannot capture XL equipment state");
				block["equipment"].push_back({ state.timer, state.segment, state.enabled, state.pending });
			}
		}
		if (xl_boss_spawn_count()) {
			block["schema"] = 5;
			block["boss_routes"] = json::array();
			for (const auto &route : xl_compat::boss_routes()) block["boss_routes"].push_back({ route.home, route.segments });
		}
		block["schema"] = 27;
		block["mission_progress"] = xl_compat::mission_progress_snapshot();
		block["seismic"] = xl_compat::seismic_gameplay_snapshot();
		require(xl_compat::restore_seismic_gameplay(block["seismic"], false), "Cannot capture XL seismic state");
		block["ambient_objects"] = xl_compat::ambient_objects_snapshot();
		block["particle_objects"] = xl_compat::particle_objects_snapshot();
		require(xl_compat::restore_particle_objects(block["particle_objects"], false), "Cannot capture XL particle object lifetime or enabled state");
		block["neutral_robots"] = xl_compat::disarm_snapshot();
		xl_countdown_state countdown{};
		xl_countdown_get(&countdown);
		require(xl_countdown_restore(&countdown, 0), "Cannot capture XL countdown state");
		block["countdown"] = { countdown.destroyed, countdown.timer, countdown.seconds, countdown.total, countdown.pending, countdown.phase, countdown.fraction };
		block["audio"] = xl_compat::named_audio_snapshot();
		xl_message_state message{};
		xl_message_state_get(&message);
		block["message"] = { xl_message_identity(), message.id, message.duration, message.elapsed_ms };
		block["flickers"] = json::array();
		for (int i = 0; i < xl_flicker_count(); ++i) {
			xl_flicker_state state{};
			require(xl_flicker_get(i, &state), "Cannot capture XL flicker state");
			block["flickers"].push_back({ state.segment, state.side, state.delay, state.timer, state.mask });
		}
		block["lighting"] = json::array();
		for (int segment = 0; segment < xl_light_segment_count(); ++segment) {
			xl_light_segment_state state{};
			require(xl_light_segment_get(segment, &state), "Cannot capture XL changing-light state");
			json row = json::array();
			for (int value : state.light) row.push_back(value);
			row.push_back(state.ambient);
			block["lighting"].push_back(std::move(row));
		}
		block["projectiles"] = json::array();
		std::vector<xl_projectile_state> projectile_states;
		for (int owner = 0; owner <= Highest_object_index; ++owner) {
			xl_projectile_state state{};
			if (!xl_projectile_state_get(owner, &state)) continue;
			projectile_states.push_back(state);
			json hits = json::array();
			for (int i = 0; i < state.count; ++i) hits.push_back(state.victims[i]);
			block["projectiles"].push_back({ state.owner, state.signature, std::move(hits) });
		}
		require(xl_projectile_restore(projectile_states.data(), static_cast<int>(projectile_states.size()), 0), "Cannot capture complete XL projectile histories");
		block["fluid_scrape_elapsed"] = xl_fluid_scrape_elapsed();
		block["repair_elapsed"] = xl_repair_elapsed();
		block["spawns"] = json::array();
		for (int slot = 0; slot < xl_spawn_state_count(); ++slot) {
			xl_spawn_point point;
			require(xl_spawn_state_get(slot, &point), "Cannot capture XL spawn state");
			json row = { point.segment, point.side };
			for (int value : point.position) row.push_back(value);
			for (int value : point.orientation) row.push_back(value);
			block["spawns"].push_back(std::move(row));
		}
		block["boosts"] = { { "speed", xl_speed_boost_last_speed() }, { "states", json::array() } };
		for (int index = 0; index < xl_speed_boost_state_count(); ++index) {
			xl_speed_boost_state state;
			require(xl_speed_boost_state_get(index, &state), "Cannot capture XL boost state");
			json row = { state.owner, state.signature, state.boosted, state.speed, state.steps, state.phase };
			for (const auto *vector : { state.velocity, state.minimum, state.maximum, state.source, state.destination, state.direction })
				for (int axis = 0; axis < 3; ++axis) row.push_back(vector[axis]);
			block["boosts"]["states"].push_back(std::move(row));
		}
		block["actors"] = std::move(actors);
		const auto payload = block.dump();
		require(payload.size() <= maximum && xl_write(file, tag, 1, sizeof(tag)) == sizeof(tag), "Cannot write XL trigger state");
		write_u32(file, static_cast<std::uint32_t>(payload.size()));
		write_u32(file, checksum(payload));
		require(xl_write(file, payload.data(), 1, static_cast<PHYSFS_uint32>(payload.size())) == static_cast<PHYSFS_sint64>(payload.size()), "Cannot finish XL trigger state");
		return 1;
	} catch (const std::exception &error) {
		con_printf(CON_URGENT, "XL trigger save rejected: %s\n", error.what());
		return 0;
	}
}

extern "C" int xl_wall_trigger_read(rewind_file *file)
{
	if (!xl_level_extensions_active()) return 1;
	try {
		const auto position = xl_tell(file);
		char saved_tag[8];
		const auto bytes = xl_read(file, saved_tag, 1, sizeof(saved_tag));
		if (bytes != sizeof(saved_tag) || std::memcmp(saved_tag, tag, sizeof(tag))) {
			require(!xl_level_trigger_state_required(), "Save lacks required XL runtime state");
			require(xl_seek(file, position), "Cannot restore save trailer position");
			xl_wall_trigger_reset();
			return 1;
		}
		const auto size = read_u32(file), expected = read_u32(file);
		require(size > 0 && size <= maximum, "Invalid XL trigger state size");
		std::string payload(size, '\0');
		require(xl_read(file, payload.data(), 1, size) == size && checksum(payload) == expected, "Corrupt XL trigger state payload");
		const auto saved = json::parse(payload);
		const bool objects = xl_object_trigger_count() != 0;
		const bool bosses = xl_boss_spawn_count() != 0;
		const bool equipment = xl_equipment_count() != 0;
		require(saved.is_object() && saved.size() == 19u + unsigned(objects) + 3u * unsigned(bosses) + unsigned(equipment) && saved.at("schema") == 27 &&
		            saved.at("source").is_number_unsigned() && saved.at("source").get<std::uint64_t>() == xl_level_trigger_identity() && saved.at("states").is_array() && saved.at("states").size() == static_cast<unsigned>(Num_triggers) && Num_triggers <= MAX_TRIGGERS,
		        "Invalid XL trigger state layout or authored source identity");
		require(xl_compat::restore_mission_progress(saved.at("mission_progress"), false), "Invalid XL mission progression state");
		require(xl_compat::restore_named_audio(saved.at("audio"), false), "Invalid XL named-audio resource or playback state");
		require(xl_compat::restore_seismic_gameplay(saved.at("seismic"), false), "Invalid XL seismic state");
		require(xl_compat::ambient_audio_matches(saved.at("audio")), "Saved ambient audio differs from the authored object resource");
		require(xl_compat::restore_ambient_objects(saved.at("ambient_objects"), false), "Invalid XL ambient sound object lifetime or enabled state");
		require(xl_compat::restore_particle_objects(saved.at("particle_objects"), false), "Invalid XL particle object lifetime or enabled state");
		require(xl_compat::disarm_restore(saved.at("neutral_robots"), false), "Invalid XL disarmed robot lifetime");
		const auto &countdown = saved.at("countdown");
		require(countdown.is_array() && countdown.size() == 7, "Invalid XL countdown state layout");
		for (const auto &field : countdown)
			require(field.is_number_integer() && (!field.is_number_unsigned() || field.get<std::uint64_t>() <= INT32_MAX) && field.get<std::int64_t>() >= INT32_MIN && field.get<std::int64_t>() <= INT32_MAX, "Invalid XL countdown integer");
		const xl_countdown_state countdown_state{ countdown[0].get<int>(), countdown[1].get<int>(), countdown[2].get<int>(), countdown[3].get<int>(), countdown[4].get<int>(), countdown[5].get<int>(), countdown[6].get<int>() };
		require(xl_countdown_restore(&countdown_state, 0), "Invalid XL countdown state or native timer mismatch");
		const auto &message = saved.at("message");
		require(message.is_array() && message.size() == 4 && message[0].is_string() && message[0].get<std::string>() == xl_message_identity(), "Invalid XL message resource identity");
		for (unsigned i = 1; i < 4; ++i)
			require(message[i].is_number_integer() && (!message[i].is_number_unsigned() || message[i].get<std::uint64_t>() <= INT64_MAX) && message[i].get<std::int64_t>() >= 0 && (i == 3 || message[i].get<std::int64_t>() <= INT32_MAX), "Invalid XL message state integer");
		const xl_message_state message_state{ message[3].get<std::int64_t>(), message[1].get<int>(), message[2].get<int>() };
		require(xl_message_restore(&message_state, 0), "Invalid XL message state or authored duration");
		const auto &flickers = saved.at("flickers");
		require(flickers.is_array() && flickers.size() == static_cast<std::size_t>(xl_flicker_count()), "Invalid XL flicker count");
		std::vector<xl_flicker_state> flicker_states(flickers.size());
		for (std::size_t i = 0; i < flickers.size(); ++i) {
			const auto &row = flickers[i];
			require(row.is_array() && row.size() == 5, "Invalid XL flicker row");
			for (unsigned column = 0; column < 4; ++column)
				require(row[column].is_number_integer() && (!row[column].is_number_unsigned() || row[column].get<std::uint64_t>() <= INT32_MAX) &&
				            row[column].get<std::int64_t>() >= INT32_MIN && row[column].get<std::int64_t>() <= INT32_MAX,
				        "Invalid XL flicker integer");
			require(row[4].is_number_unsigned() && row[4].get<std::uint64_t>() <= UINT32_MAX, "Invalid XL flicker pattern");
			flicker_states[i] = { row[0].get<int>(), row[1].get<int>(), row[2].get<int>(), row[3].get<int>(), row[4].get<std::uint32_t>() };
		}
		require(xl_flicker_restore(flicker_states.data(), static_cast<int>(flicker_states.size()), 0), "Invalid XL flicker phase or source identity");
		const auto &lighting = saved.at("lighting");
		require(lighting.is_array() && lighting.size() == static_cast<std::size_t>(xl_light_segment_count()), "Invalid XL changing-light segment count");
		std::vector<xl_light_segment_state> light_states(lighting.size());
		for (std::size_t segment = 0; segment < lighting.size(); ++segment) {
			const auto &row = lighting[segment];
			require(row.is_array() && row.size() == 25, "Invalid XL changing-light segment row");
			for (unsigned column = 0; column < 25; ++column) {
				const auto &value = row[column];
				require(value.is_number_integer() && (!value.is_number_unsigned() || value.get<std::uint64_t>() <= INT32_MAX) &&
				            value.get<std::int64_t>() >= INT32_MIN && value.get<std::int64_t>() <= INT32_MAX,
				        "Invalid XL changing-light value");
				if (column < 24) light_states[segment].light[column] = value.get<int>();
				else light_states[segment].ambient = value.get<int>();
			}
		}
		require(xl_light_segment_restore(light_states.data(), static_cast<int>(light_states.size()), 0), "Invalid XL changing-light state");
		std::vector<xl_projectile_state> projectiles;
		const auto &projectile_rows = saved.at("projectiles");
		require(projectile_rows.is_array() && projectile_rows.size() <= MAX_OBJECTS, "Invalid XL projectile owner count");
		for (const auto &row : projectile_rows) {
			require(row.is_array() && row.size() == 3 && row[2].is_array() && !row[2].empty() && row[2].size() <= XL_PROJECTILE_HITS, "Invalid XL projectile history row");
			const auto integer = [](const json &value) {
				require(value.is_number_integer() && (!value.is_number_unsigned() || value.get<std::uint64_t>() <= INT32_MAX) && value.get<std::int64_t>() >= 0 && value.get<std::int64_t>() <= INT32_MAX, "Invalid XL projectile identity");
				return value.get<int>();
			};
			xl_projectile_state state{};
			state.owner = integer(row[0]);
			state.signature = integer(row[1]);
			for (const auto &victim : row[2]) state.victims[state.count++] = integer(victim);
			projectiles.push_back(state);
		}
		require(xl_projectile_restore(projectiles.data(), static_cast<int>(projectiles.size()), 0), "Invalid XL projectile owner or hit history");
		const auto &fluid = saved.at("fluid_scrape_elapsed");
		require(fluid.is_number_integer() && (!fluid.is_number_unsigned() || fluid.get<std::uint64_t>() <= INT64_MAX) && xl_fluid_restore(fluid.get<std::int64_t>(), 0), "Invalid XL fluid scrape clock");
		const auto &repair = saved.at("repair_elapsed");
		require(repair.is_number_integer() && (!repair.is_number_unsigned() || repair.get<std::uint64_t>() <= INT64_MAX) && xl_repair_restore(repair.get<std::int64_t>(), 0), "Invalid XL repair cue clock");
		std::vector<xl_spawn_point> spawns;
		const auto &spawn_rows = saved.at("spawns");
		require(spawn_rows.is_array() && (spawn_rows.empty() || spawn_rows.size() == XL_SPAWN_SLOTS), "Invalid XL spawn state count");
		for (const auto &row : spawn_rows) {
			require(row.is_array() && row.size() == 14, "Invalid XL spawn state row");
			for (const auto &field : row)
				require(field.is_number_integer() && (!field.is_number_unsigned() || field.get<std::uint64_t>() <= INT32_MAX) && field.get<std::int64_t>() >= INT32_MIN && field.get<std::int64_t>() <= INT32_MAX, "Invalid XL spawn state field");
			xl_spawn_point point{};
			point.segment = row[0];
			point.side = row[1];
			for (int axis = 0; axis < 3; ++axis) point.position[axis] = row[2 + axis];
			for (int axis = 0; axis < 9; ++axis) point.orientation[axis] = row[5 + axis];
			spawns.push_back(point);
		}
		std::vector<xl_speed_boost_state> boosts;
		const auto &boost_block = saved.at("boosts");
		require(boost_block.is_object() && boost_block.size() == 2 && boost_block.at("speed").is_number_integer() && boost_block.at("speed") >= 0 && boost_block.at("speed") <= 10, "Invalid XL last boost speed");
		const int boost_speed = boost_block.at("speed");
		const auto &boost_rows = boost_block.at("states");
		require(boost_rows.is_array() && boost_rows.size() <= MAX_OBJECTS, "Invalid XL boost state count");
		for (const auto &row : boost_rows) {
			require(row.is_array() && row.size() == 24, "Invalid XL boost state row");
			for (const auto &field : row)
				require(field.is_number_integer() && (!field.is_number_unsigned() || field.get<std::uint64_t>() <= INT32_MAX) && field.get<std::int64_t>() >= INT32_MIN && field.get<std::int64_t>() <= INT32_MAX, "Invalid XL boost state field");
			xl_speed_boost_state state{};
			state.owner = row[0];
			state.signature = row[1];
			state.boosted = row[2];
			state.speed = row[3];
			state.steps = row[4];
			state.phase = row[5];
			int field = 6;
			for (auto *vector : { state.velocity, state.minimum, state.maximum, state.source, state.destination, state.direction })
				for (int axis = 0; axis < 3; ++axis) vector[axis] = row[field++];
			boosts.push_back(state);
		}
		std::vector<xl_compat::BossRoutes> boss_routes;
		if (bosses) {
			const auto &routes = saved.at("boss_routes");
			require(routes.is_array() && routes.size() == static_cast<unsigned>(xl_boss_spawn_count()), "Invalid XL boss destination owner count");
			for (const auto &row : routes) {
				require(row.is_array() && row.size() == 2 && row[0].is_number_integer() && row[0].get<std::int64_t>() >= 0 && row[0].get<std::int64_t>() < Num_segments && row[1].is_array() && row[1].size() <= 1000, "Invalid XL boss destination row");
				xl_compat::BossRoutes route;
				route.home = row[0].get<int>();
				for (const auto &segment : row[1]) {
					require(segment.is_number_integer() && (!segment.is_number_unsigned() || segment.get<std::uint64_t>() < static_cast<std::uint64_t>(Num_segments)) && segment.get<std::int64_t>() >= 0 && segment.get<std::int64_t>() < Num_segments, "Invalid XL boss destination segment");
					route.segments.push_back(segment.get<int>());
				}
				boss_routes.push_back(std::move(route));
			}
		}
		std::uint32_t ready_mask = 0;
		std::vector<xl_equipment_state> equipment_states;
		if (equipment) {
			require(saved.at("equipment").is_array() && saved.at("equipment").size() == static_cast<unsigned>(xl_equipment_count()), "Invalid XL equipment state count");
			for (const auto &row : saved.at("equipment")) {
				require(row.is_array() && row.size() == 4, "Invalid XL equipment state row");
				for (std::size_t field = 0; field < row.size(); ++field) {
					const auto &value = row[field];
					require(value.is_number_integer() && (!value.is_number_unsigned() || value.get<std::uint64_t>() <= std::uint64_t(INT64_MAX)), "Invalid XL equipment state field");
					require(!field || (value.get<std::int64_t>() >= INT32_MIN && value.get<std::int64_t>() <= INT32_MAX), "XL equipment state field overflow");
				}
				equipment_states.push_back({ row[0].get<std::int64_t>(), row[1].get<int>(), row[2].get<int>(), row[3].get<int>() });
			}
		}
		if (bosses) {
			const auto &value = saved.at("producer_implicit");
			require(value.is_number_unsigned() && value.get<std::uint64_t>() <= UINT32_MAX, "Invalid XL producer implicit selection state");
			ready_mask = value.get<std::uint32_t>();
			require(xl_robot_generator_restore_ready(ready_mask, 0), "Invalid XL producer implicit selection mask");
		}
		std::vector<xl_boss_spawn_state> boss_states;
		if (bosses) {
			require(saved.at("bosses").is_array() && saved.at("bosses").size() == static_cast<unsigned>(xl_boss_spawn_count()), "Invalid XL boss spawn state count");
			for (const auto &row : saved.at("bosses")) {
				require(row.is_array() && row.size() == 12, "Invalid XL boss spawn state row");
				for (std::size_t field = 0; field < row.size(); ++field) {
					const auto &value = row[field];
					require(value.is_number_integer() && (!value.is_number_unsigned() || value.get<std::uint64_t>() <= std::uint64_t(INT64_MAX)), "Invalid XL boss spawn state field");
					require(field < 6 || (value.get<std::int64_t>() >= INT32_MIN && value.get<std::int64_t>() <= INT32_MAX), "XL boss spawn state field overflow");
				}
				boss_states.push_back({ row[0].get<std::int64_t>(), row[1].get<std::int64_t>(), row[2].get<std::int64_t>(), row[3].get<std::int64_t>(), row[4].get<std::int64_t>(), row[5].get<std::int64_t>(), row[6].get<int>(), row[7].get<int>(), row[8].get<int>(), row[9].get<int>(), row[10].get<int>(), row[11].get<int>() });
			}
		}
		std::vector<xl_object_trigger_state> object_states;
		if (objects) {
			require(saved.at("objects").is_array() && saved.at("objects").size() == static_cast<unsigned>(xl_object_trigger_count()), "Invalid XL object state count");
			for (const auto &row : saved.at("objects")) {
				require(row.is_array() && row.size() == 7, "Invalid XL object state row");
				for (const auto &value : row) require(value.is_number_integer() && (!value.is_number_unsigned() || value.get<std::uint64_t>() <= std::uint64_t(INT32_MAX)) && value.get<std::int64_t>() >= INT32_MIN && value.get<std::int64_t>() <= INT32_MAX, "Invalid XL object state field");
				object_states.push_back({ row[0].get<int>(), row[1].get<int>(), row[2].get<int>(), row[3].get<int>(), row[4].get<int>(), row[5].get<int>(), row[6].get<int>() });
			}
		}
		std::vector<xl_trigger_state> states;
		const auto &actors = saved.at("actors");
		require(actors.is_array() && actors.size() == static_cast<unsigned>(Num_triggers), "Invalid XL pending actor count");
		for (const auto &row : saved.at("states")) {
			require(row.is_array() && row.size() == (objects ? 6u : 5u), "Invalid XL trigger state row");
			for (std::size_t field = 0; field < row.size(); ++field) {
				const auto &value = row[field];
				require(value.is_number_integer() && (!value.is_number_unsigned() || value.get<std::uint64_t>() <= std::uint64_t(INT64_MAX)), "Invalid XL trigger state field");
				require(!field || (value.get<std::int64_t>() >= INT32_MIN && value.get<std::int64_t>() <= INT32_MAX), "XL trigger state field overflow");
			}
			const auto &actor = actors[states.size()];
			require(actor.is_array() && actor.size() == 2, "Invalid XL pending actor row");
			for (const auto &field : actor)
				require(field.is_number_integer() && (!field.is_number_unsigned() || field.get<std::uint64_t>() <= INT32_MAX) && field.get<std::int64_t>() >= 0 && field.get<std::int64_t>() <= INT32_MAX, "Invalid XL pending actor field");
			states.push_back({ row[0].get<std::int64_t>(), row[1].get<int>(), row[2].get<int>(), row[3].get<int>(), row[4].get<int>(), objects ? row[5].get<int>() : 0, actor[0].get<int>(), actor[1].get<int>() });
		}
		require(xl_robot_generator_validate(), "Invalid XL generator assignment or robot selection in save");
		require(xl_spawn_restore(spawns.data(), static_cast<int>(spawns.size()), 0) && xl_speed_boost_restore(boosts.data(), static_cast<int>(boosts.size()), boost_speed, 0) && xl_wall_trigger_restore_mode(states.data(), static_cast<int>(states.size()), 0) && xl_object_trigger_restore(object_states.data(), static_cast<int>(object_states.size()), 0) && xl_boss_spawn_restore(boss_states.data(), static_cast<int>(boss_states.size()), 0) && xl_compat::restore_boss_routes(boss_routes, 0) && xl_equipment_restore(equipment_states.data(), static_cast<int>(equipment_states.size()), 0), "Invalid XL event state");
		xl_projectile_restore(projectiles.data(), static_cast<int>(projectiles.size()), 1);
		xl_light_segment_restore(light_states.data(), static_cast<int>(light_states.size()), 1);
		xl_flicker_restore(flicker_states.data(), static_cast<int>(flicker_states.size()), 1);
		xl_message_restore(&message_state, 1);
		xl_countdown_restore(&countdown_state, 1);
		xl_compat::disarm_restore(saved.at("neutral_robots"), true);
		xl_compat::restore_named_audio(saved.at("audio"), true);
		xl_compat::restore_seismic_gameplay(saved.at("seismic"), true);
		xl_compat::restore_ambient_objects(saved.at("ambient_objects"), true);
		xl_compat::restore_particle_objects(saved.at("particle_objects"), true);
		xl_compat::restore_mission_progress(saved.at("mission_progress"), true);
		xl_spawn_restore(spawns.data(), static_cast<int>(spawns.size()), 1);
		xl_fluid_restore(fluid.get<std::int64_t>(), 1);
		xl_repair_restore(repair.get<std::int64_t>(), 1);
		xl_speed_boost_restore(boosts.data(), static_cast<int>(boosts.size()), boost_speed, 1);
		xl_wall_trigger_restore(states.data(), static_cast<int>(states.size()));
		xl_object_trigger_restore(object_states.data(), static_cast<int>(object_states.size()), 1);
		xl_boss_spawn_restore(boss_states.data(), static_cast<int>(boss_states.size()), 1);
		xl_compat::restore_boss_routes(boss_routes, 1);
		xl_equipment_restore(equipment_states.data(), static_cast<int>(equipment_states.size()), 1);
		if (bosses) xl_robot_generator_restore_ready(ready_mask, 1);
		return 1;
	} catch (const std::exception &error) {
		con_printf(CON_URGENT, "XL trigger restore rejected: %s\n", error.what());
		return 0;
	}
}
