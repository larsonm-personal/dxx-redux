#include "afterburner_animation.h"
#include "preview.h"
#include "../xl_vclip_frame.h"
#include "../afterburner_emission.h"
#include "resource_validation.h"
#include <array>
#include <algorithm>
#include <vector>
#include <nlohmann/json.hpp>
extern "C" {
#include "object.h"
#include "vclip.h"
#include "dxxerror.h"
#include "fireball.h"
#include "gameseg.h"
}
namespace
{
struct Duration {
	int signature = -1;
	int total = 0;
};
std::array<Duration, MAX_OBJECTS> durations{};
bool blob(int owner)
{
	return owner >= 0 && owner <= Highest_object_index && owner < object_capacity() &&
	       Objects[owner].type == OBJ_FIREBALL && Objects[owner].id == VCLIP_AFTERBURNER_BLOB;
}
int total(int owner)
{
	const auto &duration = durations[owner];
	if (duration.signature == Objects[owner].signature) return duration.total;
	if (Objects[owner].rtype.vclip_info.frametime) Error("XL afterburner animation duration is missing");
	return 0;
}
} // namespace
extern "C" int xl_drop_afterburner_blobs(const object *emitter, int size, int lifetime)
{
	if (!xl_level_extensions_active()) return 0;
	try {
		if (!emitter) throw std::runtime_error("XL afterburner emitter is missing");
		const auto model = xl_prepared_afterburner_model(emitter->rtype.pobj_info.model_num, emitter->type == OBJ_PLAYER);
		const auto point = [](const vms_vector &v) { return xl_compat::EmissionPoint{ v.x, v.y, v.z }; };
		// The supported XL model profile enables owned high-resolution replacements
		const auto emission = xl_compat::afterburner_emission(point(emitter->pos), point(emitter->orient.fvec), point(emitter->orient.rvec),
		                                                      emitter->size, emitter->type == OBJ_PLAYER, true, model.ase, model.low_z, model.high_z);
		for (unsigned i = 0; i < emission.count; ++i) {
			const auto &p = emission.positions[i];
			vms_vector position{ p[0], p[1], p[2] };
			const int segment = find_point_seg(&position, emitter->segnum);
			if (segment < 0) continue;
			if (auto *created = object_create_explosion(segment, &position, size, VCLIP_AFTERBURNER_BLOB))
				xl_afterburner_lifetime(int(created - Objects), lifetime);
		}
		return 1;
	} catch (const std::exception &error) {
		Error("XL afterburner emission: %s", error.what());
	}
	return 1;
}
extern "C" void xl_afterburner_animation_clear(void)
{
	durations.fill({});
}
extern "C" void xl_afterburner_animation_created(int owner)
{
	if (owner >= 0 && owner < object_capacity()) durations[owner] = {};
}
extern "C" int xl_afterburner_lifetime(int owner, int lifetime)
{
	if (!xl_level_extensions_active()) return 0;
	if (!blob(owner)) Error("XL afterburner lifetime owner is invalid");
	if (lifetime == -1) return 1;
	auto &object = Objects[owner];
	const auto &clip = Vclip[VCLIP_AFTERBURNER_BLOB];
	if (lifetime < 0 || object.lifeleft <= 0 || clip.frame_time <= 0)
		Error("XL afterburner animation duration is invalid");
	const int64_t period = int64_t(clip.frame_time) * lifetime / object.lifeleft;
	if (period > INT32_MAX) Error("XL afterburner animation period exceeds native storage");
	durations[owner] = { object.signature, lifetime };
	object.rtype.vclip_info.frametime = int(period);
	object.lifeleft = lifetime;
	return 1;
}
extern "C" int xl_afterburner_frame(int owner, int clip, int timeleft, int frames, int *frame)
{
	if (!frame || !xl_level_extensions_active() || clip != VCLIP_AFTERBURNER_BLOB || !blob(owner)) return 0;
	*frame = dxx_xl_vclip_object_frame(Vclip[clip].play_time, Vclip[clip].frame_time, frames, timeleft,
	                                   total(owner), Objects[owner].rtype.vclip_info.frametime);
	return 1;
}
nlohmann::json xl_afterburner_animation_snapshot()
{
	nlohmann::json rows = nlohmann::json::array();
	if (xl_level_extensions_active())
		for (int owner = 0; owner <= Highest_object_index; ++owner)
			if (blob(owner)) rows.push_back({ owner, int(Objects[owner].signature), total(owner), int(Objects[owner].rtype.vclip_info.frametime) });
	return { { "version", 1 }, { "objects", std::move(rows) } };
}
bool xl_afterburner_animation_restore(const nlohmann::json &saved, bool apply)
{
	try {
		if (!xl_level_extensions_active() || !saved.is_object() || saved.size() != 2 ||
		    !saved.at("version").is_number_integer() || saved.at("version") != 1 ||
		    Highest_object_index >= object_capacity()) return false;
		const auto &rows = saved.at("objects");
		if (!rows.is_array() || rows.size() > unsigned(object_capacity())) return false;
		std::vector<Duration> prepared(object_capacity());
		std::size_t index = 0;
		for (int owner = 0; owner <= Highest_object_index; ++owner) {
			if (!blob(owner)) continue;
			if (index == rows.size()) return false;
			const auto &row = rows[index++];
			if (!row.is_array() || row.size() != 4) return false;
			for (const auto &field : row)
				if (!field.is_number_integer() || (field.is_number_unsigned() ? field.get<uint64_t>() > INT32_MAX : field.get<int64_t>() < 0 || field.get<int64_t>() > INT32_MAX)) return false;
			const int duration = row[2].get<int>(), period = row[3].get<int>();
			if (row[0].get<int>() != owner || row[1].get<int>() != Objects[owner].signature ||
			    period != Objects[owner].rtype.vclip_info.frametime || (!duration && period) ||
			    (duration && Objects[owner].lifeleft > duration)) return false;
			const auto &clip = Vclip[VCLIP_AFTERBURNER_BLOB];
			if (duration && (clip.play_time <= 0 || clip.frame_time <= 0 ||
			                 int64_t(clip.frame_time) * duration / clip.play_time != period)) return false;
			prepared[owner] = { Objects[owner].signature, duration };
		}
		if (index != rows.size()) return false;
		if (apply) std::copy(prepared.begin(), prepared.end(), durations.begin());
		return true;
	} catch (const std::exception &) {
		return false;
	}
}
