#include <nlohmann/json.hpp>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#include "trigger_state.h"
#include "object_trigger.h"
#include "robot_generator.h"
#include "boss_spawn.h"
#include "boss_routes.h"
#include "equipment.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "switch.h"
#include "segment.h"
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
		for (int index = 0; index < Num_triggers; ++index) {
			xl_trigger_state state;
			require(xl_wall_trigger_state(index, &state), "Cannot capture XL trigger state");
			rows.push_back({ state.elapsed, state.delay_ms, state.object, state.shot, state.operated });
			if (xl_object_trigger_count()) rows.back().push_back(state.destination);
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
			require(!xl_level_trigger_state_required(), "Save lacks required XL scheduling state");
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
		require(saved.is_object() && saved.size() == 3u + unsigned(objects) + 3u * unsigned(bosses) + unsigned(equipment) && saved.at("schema") == (bosses ? 5 : equipment ? 4
		                                                                                                                                                     : objects     ? 2
		                                                                                                                                                                   : 1) &&
		            saved.at("source").is_number_unsigned() && saved.at("source").get<std::uint64_t>() == xl_level_trigger_identity() && saved.at("states").is_array() && saved.at("states").size() == static_cast<unsigned>(Num_triggers) && Num_triggers <= MAX_TRIGGERS,
		        "Invalid XL trigger state layout or authored source identity");
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
		for (const auto &row : saved.at("states")) {
			require(row.is_array() && row.size() == (objects ? 6u : 5u), "Invalid XL trigger state row");
			for (std::size_t field = 0; field < row.size(); ++field) {
				const auto &value = row[field];
				require(value.is_number_integer() && (!value.is_number_unsigned() || value.get<std::uint64_t>() <= std::uint64_t(INT64_MAX)), "Invalid XL trigger state field");
				require(!field || (value.get<std::int64_t>() >= INT32_MIN && value.get<std::int64_t>() <= INT32_MAX), "XL trigger state field overflow");
			}
			states.push_back({ row[0].get<std::int64_t>(), row[1].get<int>(), row[2].get<int>(), row[3].get<int>(), row[4].get<int>(), objects ? row[5].get<int>() : 0 });
		}
		require(xl_robot_generator_validate(), "Invalid XL generator assignment or robot selection in save");
		require(xl_wall_trigger_restore_mode(states.data(), static_cast<int>(states.size()), 0) && xl_object_trigger_restore(object_states.data(), static_cast<int>(object_states.size()), 0) && xl_boss_spawn_restore(boss_states.data(), static_cast<int>(boss_states.size()), 0) && xl_compat::restore_boss_routes(boss_routes, 0) && xl_equipment_restore(equipment_states.data(), static_cast<int>(equipment_states.size()), 0), "Invalid XL event state");
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
