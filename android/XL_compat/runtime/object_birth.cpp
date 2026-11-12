#include "object_birth.h"
#include "preview.h"
#include "camera_collision.h"
#include <array>
#include <algorithm>
#include <set>
#include <vector>
#include <nlohmann/json.hpp>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "dxxerror.h"
}
namespace
{
struct Birth {
	int signature = -1;
	int64_t time = 0;
	uint64_t order = 0;
};
std::array<Birth, MAX_OBJECTS> births{};
uint64_t next_order = 0;
} // namespace
extern "C" void xl_object_birth_clear(void)
{
	births.fill({});
	next_order = 0;
	xl_camera_hit_state_clear();
}
extern "C" void xl_object_birth_created(int owner)
{
	if (owner < 0 || owner >= object_capacity()) return;
	xl_camera_hit_state_created(owner);
	if (xl_level_extensions_active() && next_order == UINT64_MAX - 1) Error("XL object creation order exhausted");
	births[owner] = xl_level_extensions_active() ? Birth{ Objects[owner].signature, GameTime64, ++next_order } : Birth{};
}
extern "C" int64_t xl_object_birth_time(int owner)
{
	if (!xl_level_extensions_active() || owner < 0 || owner > Highest_object_index || Objects[owner].type == OBJ_NONE) return 0;
	const auto &birth = births[owner];
	// Authored objects retain zero; forward saves preserve earned runtime births
	return birth.signature == Objects[owner].signature ? birth.time : 0;
}

extern "C" uint64_t xl_object_birth_order(int owner)
{
	if (!xl_level_extensions_active() || owner < 0 || owner > Highest_object_index || Objects[owner].type == OBJ_NONE) return 0;
	const auto &birth = births[owner];
	// Authored objects precede runtime objects; forward saves retain append order
	return birth.signature == Objects[owner].signature ? birth.order : 0;
}

nlohmann::json xl_object_birth_snapshot()
{
	nlohmann::json rows = nlohmann::json::array();
	if (xl_level_extensions_active())
		for (int owner = 0; owner <= Highest_object_index; ++owner) {
			if (Objects[owner].type == OBJ_NONE) continue;
			rows.push_back({ owner, int(Objects[owner].signature), xl_object_birth_time(owner), xl_object_birth_order(owner) });
		}
	return { { "version", 1 }, { "next", next_order }, { "objects", std::move(rows) } };
}

bool xl_object_birth_restore(const nlohmann::json &saved, bool apply)
{
	try {
		const auto nonnegative = [](const nlohmann::json &field, uint64_t maximum) {
			return field.is_number_integer() && (field.is_number_unsigned() ? field.get<uint64_t>() <= maximum
			                                                                : field.get<int64_t>() >= 0 && uint64_t(field.get<int64_t>()) <= maximum);
		};
		if (!xl_level_extensions_active() || !saved.is_object() || saved.size() != 3 || saved.at("version") != 1 ||
		    !saved.at("version").is_number_integer() || !nonnegative(saved.at("next"), UINT64_MAX - 1) ||
		    GameTime64 < 0 || Highest_object_index >= object_capacity()) return false;
		const auto next = saved.at("next").get<uint64_t>();
		const auto &rows = saved.at("objects");
		if (!rows.is_array() || rows.size() > unsigned(object_capacity())) return false;
		// Keep the expanded object pool off the Windows stack; publish only after validation
		std::vector<Birth> prepared(object_capacity());
		std::set<uint64_t> orders;
		std::size_t index = 0;
		for (int owner = 0; owner <= Highest_object_index; ++owner) {
			if (Objects[owner].type == OBJ_NONE) continue;
			if (index == rows.size()) return false;
			const auto &row = rows[index++];
			if (!row.is_array() || row.size() != 4) return false;
			if (!nonnegative(row[0], uint64_t(object_capacity() - 1)) || !nonnegative(row[1], INT32_MAX) ||
			    !nonnegative(row[2], uint64_t(GameTime64)) || !nonnegative(row[3], next)) return false;
			if (row[0].get<uint64_t>() != uint64_t(owner) || row[1].get<uint64_t>() != uint64_t(Objects[owner].signature) ||
			    row[2].get<uint64_t>() > uint64_t(GameTime64) || row[3].get<uint64_t>() > next) return false;
			const auto time = row[2].get<int64_t>();
			const auto order = row[3].get<uint64_t>();
			if ((!order && time) || (order && !orders.insert(order).second)) return false;
			prepared[owner] = { Objects[owner].signature, time, order };
		}
		if (index != rows.size()) return false;
		if (apply) {
			std::copy(prepared.begin(), prepared.end(), births.begin());
			next_order = next;
		}
		return true;
	} catch (const std::exception &) {
		return false;
	}
}
