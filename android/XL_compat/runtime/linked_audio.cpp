#include "linked_audio.h"
#include "shared_audio.h"
#include "shared_audio_internal.h"
#include "audio_geometry.h"
#include "preview.h"
#include <algorithm>
#include <map>
#include <memory>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "config.h"
}
namespace
{
using namespace xl_compat;
using namespace xl_compat::audio_runtime;
std::unique_ptr<ClassicAudioSpatial> scene;
bool deferred = false;
FixedLightPoint point(const vms_vector &value)
{
	return { value.x, value.y, value.z };
}
bool valid_segment(int index)
{
	return index >= 0 && index <= Highest_segment_index;
}
bool valid_object(int index)
{
	return index >= 0 && index <= Highest_object_index && Objects[index].type != OBJ_NONE;
}
AudioSpatialOutput spatial(const FixedLightPoint &position, int segment, int volume, int range, int decay)
{
	if (!Viewer || !valid_segment(Viewer->segnum)) throw std::runtime_error("XL linked sound has no valid listener");
	if (!scene) scene = std::make_unique<ClassicAudioSpatial>();
	return scene->output(Viewer->segnum, segment, point(Viewer->pos), position,
	                     point(Viewer->orient.rvec), point(Viewer->orient.uvec), volume, range, decay, GameCfg.ReverseStereo != 0);
}
AudioOwnerUpdate update(const AudioOwner &entry)
{
	auto position = entry.position;
	int segment = entry.segment;
	if (!(entry.flags & sound_link_position)) {
		if (!valid_object(entry.object) || Objects[entry.object].signature != entry.signature) return { false, true, 0, 0 };
		const auto &object = Objects[entry.object];
		position = point(object.pos);
		segment = object.segnum;
	}
	const auto output = spatial(position, segment, entry.max_volume, entry.max_distance, entry.decay);
	return { true, true, output.volume, output.pan };
}
std::int64_t install(AudioOwner entry)
{
	if (deferred) entry.flags |= sound_permanent;
	else {
		const auto output = update(entry);
		entry.volume = output.volume;
		entry.pan = output.pan;
	}
	// Geometry above can traverse the mine. Hold SDL's lock only for registry and
	// channel mutations, keeping the callback independent of routing costs
	AudioLock lock;
	owners.reconcile(mixer);
	const int index = owners.add(entry);
	if (index < 0) return -1;
	const auto serial = owners.entries[unsigned(index)].serial;
	owners.start(unsigned(index), priority_volume, deferred, mixer, backend_ready);
	return std::int64_t(serial);
}
} // namespace

void xl_compat::audio_runtime::clear_linked_scene()
{
	scene.reset();
	deferred = false;
}

extern "C" std::int64_t xl_audio_link_object(int sample, int sound_class, int object_index, int forever, int volume, int range, int loop_start, int loop_end, int decay, int custom)
{
	try {
		const int mapped = classic_sample(sample);
		if (mapped < 0 || !valid_object(object_index) || sound_class < 0 || sound_class > 6 || volume < 0 || range <= 0 || decay < 0 || decay > 6) return -1;
		const auto &object = Objects[object_index];
		if (!valid_segment(object.segnum)) return -1;
		for (unsigned i = 0; i < owners.count; ++i) {
			const auto &entry = owners.entries[i];
			if ((entry.flags & sound_link_object) && entry.sound == sample && entry.channel >= 0 && entry.object == object_index && entry.signature == object.signature)
				return std::int64_t(entry.serial);
		}
		if (!forever) {
			const auto output = spatial(point(object.pos), object.segnum, volume, range, decay);
			if (output.volume > 0) xl_audio_start_sample(sample, sound_class, output.volume, output.pan, 0, -1, -1);
			return -1;
		}
		AudioOwner entry;
		entry.flags = sound_used | sound_link_object | sound_forever;
		entry.sound = sample;
		entry.sample = unsigned(mapped);
		entry.sound_class = sound_class;
		entry.object = object_index;
		entry.signature = object.signature;
		entry.max_volume = volume;
		entry.max_distance = range;
		entry.loop_start = loop_start;
		entry.loop_end = loop_end;
		// The pinned CreateObjectSound uses class as decay for persistent owners
		entry.decay = sound_class;
		entry.ambient = sound_class == 1;
		entry.custom = custom != 0;
		return install(entry);
	} catch (const std::exception &error) {
		failure = error.what();
		return -1;
	}
}

extern "C" std::int64_t xl_audio_link_position(int sample, int segment, int side, const vms_vector *position, int forever, int volume, int range)
{
	try {
		const int mapped = classic_sample(sample);
		if (mapped < 0 || !position || !valid_segment(segment) || side < -1 || side >= 6 || volume < 0 || range <= 0) return -1;
		const auto location = point(*position);
		for (unsigned i = 0; i < owners.count; ++i) {
			const auto &entry = owners.entries[i];
			if ((entry.flags & sound_link_position) && entry.sound == sample && entry.channel >= 0 && entry.segment == segment && entry.side == side && audio_point_distance(entry.position, location) <= 32768)
				return std::int64_t(entry.serial);
		}
		if (!forever) {
			const auto output = spatial(location, segment, volume, range, 0);
			if (output.volume > 0) xl_audio_start_sample(sample, 0, output.volume, output.pan, 0, -1, -1);
			return -1;
		}
		AudioOwner entry;
		entry.flags = sound_used | sound_link_position | sound_forever;
		entry.sound = sample;
		entry.sample = unsigned(mapped);
		entry.segment = segment;
		entry.side = side;
		entry.position = location;
		entry.max_volume = volume;
		entry.max_distance = range;
		return install(entry);
	} catch (const std::exception &error) {
		failure = error.what();
		return -1;
	}
}

extern "C" void xl_audio_defer_objects(int value)
{
	deferred = xl_level_extensions_active() && value != 0;
}

extern "C" void xl_audio_sync_objects(void)
{
	if (!xl_level_extensions_active() || !owners.count) return;
	try {
		// Only the game thread mutates owners; callback completions change voices
		// only. Stable serials still identify prepared outputs after reconciliation
		std::map<std::uint64_t, AudioOwnerUpdate> prepared;
		for (unsigned i = 0; i < owners.count; ++i) prepared.emplace(owners.entries[i].serial, update(owners.entries[i]));
		AudioLock lock;
		owners.sync([&](const AudioOwner &entry) { return prepared.at(entry.serial); }, mixer, { priority_volume, priority_volume }, deferred, backend_ready);
	} catch (const std::exception &error) {
		failure = error.what();
	}
}

extern "C" int xl_audio_destroy_object(int object_index, int sample)
{
	if (!xl_level_extensions_active()) return 0;
	AudioLock lock;
	if (object_index < 0)
		return owners.destroy_missing([](int index, int signature) { return valid_object(index) && Objects[index].signature == signature; }, mixer);
	return owners.destroy_object(object_index, sample, mixer);
}
extern "C" int xl_audio_destroy_position(int segment, int side, int sample)
{
	if (!xl_level_extensions_active()) return 0;
	AudioLock lock;
	return owners.destroy_segment(segment, side, sample, mixer);
}
