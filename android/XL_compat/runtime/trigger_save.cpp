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
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "switch.h"
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
constexpr std::uint32_t maximum = 64 * 1024;
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
		require(saved.is_object() && saved.size() == (objects ? 4u : 3u) && saved.at("schema") == (objects ? 2 : 1) && saved.at("source").is_number_unsigned() && saved.at("source").get<std::uint64_t>() == xl_level_trigger_identity() && saved.at("states").is_array() && saved.at("states").size() == static_cast<unsigned>(Num_triggers) && Num_triggers <= MAX_TRIGGERS, "Invalid XL trigger state layout or authored source identity");
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
			require(row.is_array() && row.size() == 5, "Invalid XL trigger state row");
			for (int field = 0; field < 5; ++field) {
				const auto &value = row[field];
				require(value.is_number_integer() && (!value.is_number_unsigned() || value.get<std::uint64_t>() <= std::uint64_t(INT64_MAX)), "Invalid XL trigger state field");
				require(!field || (value.get<std::int64_t>() >= INT32_MIN && value.get<std::int64_t>() <= INT32_MAX), "XL trigger state field overflow");
			}
			states.push_back({ row[0].get<std::int64_t>(), row[1].get<int>(), row[2].get<int>(), row[3].get<int>(), row[4].get<int>() });
		}
		require(xl_wall_trigger_restore_mode(states.data(), static_cast<int>(states.size()), 0) && xl_object_trigger_restore(object_states.data(), static_cast<int>(object_states.size()), 0), "Invalid XL event state");
		xl_wall_trigger_restore(states.data(), static_cast<int>(states.size()));
		xl_object_trigger_restore(object_states.data(), static_cast<int>(object_states.size()), 1);
		return 1;
	} catch (const std::exception &error) {
		con_printf(CON_URGENT, "XL trigger restore rejected: %s\n", error.what());
		return 0;
	}
}
