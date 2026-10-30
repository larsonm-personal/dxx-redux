#include "surface_audio_receipt.h"
#include "ambient_audio_receipt.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace surface_reference
{
#define ENTER(a, b)
#define RETURN                    return;
#define RETVAL(value)             return (value);
#define I2X(value)                ((value) * 65536)
#define IS_CHILD(value)           ((value) >= 0)
#define OBJ_LIST_TYPE             1
#define FORALL_EFFECT_OBJS(value) for ((value) = effect_list.head; (value); (value) = (value)->m_links[1].next)
#define FORALL_STATIC_OBJS(value) for ((value) = nullptr; (value); (value) = nullptr)
constexpr int SEGMENT_SIDE_COUNT = 6, FILENAME_LEN = 256, SOUND_ID = 2, SOUNDCLASS_AMBIENT = 1, OBJ_EXPLOSION = 0, RT_POWERUP = 0;
struct CObject;
struct tObjListRef {
	CObject *head = nullptr, *tail = nullptr;
	int nObjects = 0;
} effect_list;
struct CObjListLink {
	CObject *prev = nullptr, *next = nullptr;
	tObjListRef *list = nullptr;
};
struct CObject {
	int index = 0;
	std::array<CObjListLink, 2> m_links{};
	void Link(tObjListRef &, int32_t);
	struct {
		int nId = 0, nType = 0, renderType = 0;
	} info;
	struct {
		struct {
			char szFilename[40]{};
			int nVolume = 0, bEnabled = 0;
		} soundInfo;
		struct {
			int nClipIndex = 0;
		} animationInfo;
	} rType;
	int Index() const
	{
		return index;
	}
};
std::vector<CObject> effect_objects;
struct Side {
	int m_nOvlTex = 0, m_nBaseTex = 0, flags = 2;
	std::array<int, 3> center{};
};
struct CSegment {
	int index = 0;
	std::array<int, 6> m_children{ -1, -1, -1, -1, -1, -1 };
	std::array<Side, 6> m_sides{};
	int IsPassable(int side, void *) const
	{
		return m_sides.at(unsigned(side)).flags;
	}
	int ConnectedSide(const CSegment *other) const
	{
		for (int side = 0; side < 6; ++side)
			if (other->m_children[unsigned(side)] == index) return side;
		throw std::runtime_error("Surface reference has no connected side");
	}
	const std::array<int, 3> &SideCenter(int side) const
	{
		return m_sides.at(unsigned(side)).center;
	}
};
struct SegmentStore {
	std::vector<CSegment> items;
	CSegment *Buffer()
	{
		return items.data();
	}
} SEGMENTS;
CSegment *SEGMENT(int index)
{
	return &SEGMENTS.items.at(unsigned(index));
}
struct Texture {
	int nEffectClip = -1;
};
struct Effect {
	int nSound = -1;
};
struct {
	struct {
		struct {
			std::vector<Texture> pTexMapInfo;
		} tex;
	} pigData;
	struct {
		std::vector<Effect> pEffect;
	} effectData;
	struct {
		int nLastSegment = -1;
	} segData;
} gameData;
struct {
	struct {
		int bDontStartObjects = 0;
	} sound;
} gameStates;
struct {
	struct {
		int xCustomSoundVolume = 65536;
	} sound;
} options;
auto *gameOpts = &options;
int FixMul(int a, int b)
{
	return int(std::int64_t(a) * b / 65536);
}
int Max(int a, int b)
{
	return a > b ? a : b;
}
void SetD1Sound() {}
struct Call {
	int sound, segment, side, loop, volume, range;
	std::array<int, 3> position;
	bool deferred;
};
struct Audio {
	std::vector<Call> calls;
	std::vector<xl_ambient_audio_receipt::Owner> object_calls;
	unsigned resets = 0;
	void InitSounds()
	{
		calls.clear();
		object_calls.clear();
		++resets;
	}
	void CreateSegmentSound(int sound, int segment, int side, const std::array<int, 3> &position, int loop, int volume, int range = 256 * 65536)
	{
		calls.push_back({ sound, segment, side, loop, volume, range, position, gameStates.sound.bDontStartObjects != 0 });
	}
	int GetSoundByName(const char *)
	{
		return -1;
	}
	void CreateObjectSound(int sound, int sound_class, int object, int loop, int volume = 65536, int range = 256 * 65536, int loop_start = -1, int loop_end = -1, const char *filename = nullptr)
	{
		if (!filename) throw std::runtime_error("Ambient source control entered unqualified static-object dispatch");
		// Capture initializer arguments; the separate creation source suite qualifies
		// their mapping to owner flags, external identity and class-derived decay
		object_calls.push_back({ { object, sound, sound_class, volume, range,
		                           1 | 4 | (loop ? 16 : 0) | (gameStates.sound.bDontStartObjects ? 32 : 0),
		                           loop_start, loop_end, 1, 1, 1 },
		                         filename });
	}
} audio;
#include "xl_reference_surface_audio.h"
void reset(unsigned segments)
{
	SEGMENTS.items.assign(segments, {});
	for (unsigned i = 0; i < segments; ++i) SEGMENTS.items[i].index = int(i);
	gameData.segData.nLastSegment = int(segments) - 1;
	gameData.pigData.tex.pTexMapInfo.assign(16, {});
	gameData.effectData.pEffect.assign(8, {});
	gameStates.sound.bDontStartObjects = 0;
	audio.calls.clear();
	audio.object_calls.clear();
	audio.resets = 0;
	effect_objects.clear();
	effect_list = {};
}
void install_effects(const std::vector<xl_ambient_audio_receipt::Object> &objects)
{
	effect_objects.resize(objects.size());
	for (unsigned i = 0; i < objects.size(); ++i) {
		const auto &input = objects[i];
		auto &object = effect_objects[i];
		if (input.filename.size() >= sizeof(object.rType.soundInfo.szFilename)) throw std::runtime_error("Ambient source filename exceeds loaded field");
		object.index = input.index;
		object.info.nType = 20;
		object.info.nId = input.id;
		object.rType.soundInfo.nVolume = input.volume;
		object.rType.soundInfo.bEnabled = input.enabled;
		std::strcpy(object.rType.soundInfo.szFilename, input.filename.c_str());
		object.Link(effect_list, 1);
	}
}
} // namespace surface_reference
void surface_audio_source_controls()
{
	using namespace surface_reference;
	unsigned cases = 0;
	for (int flags = 0; flags < 8; ++flags)
		for (int base = 0; base < 4; ++base)
			for (int overlay = 0; overlay < 4; ++overlay)
				for (int child : { -1, 0, 2 })
					for (int same = 0; same < 2; ++same) {
						reset(3);
						const int base_sounds[] = { -1, 7, -1, 0 };
						gameData.pigData.tex.pTexMapInfo[1].nEffectClip = base ? base : -1;
						for (int i = 1; i < 4; ++i) gameData.effectData.pEffect[unsigned(i)].nSound = base_sounds[i];
						gameData.pigData.tex.pTexMapInfo[3].nEffectClip = 4;
						gameData.pigData.tex.pTexMapInfo[4].nEffectClip = 5;
						gameData.effectData.pEffect[4].nSound = 8;
						auto &room = *SEGMENT(1);
						const int overlay_texture = overlay ? overlay + 1 : 0;
						room.m_sides[0] = { overlay_texture, 1, flags, {} };
						room.m_children[0] = child;
						if (child >= 0) {
							SEGMENT(child)->m_children[0] = 1;
							SEGMENT(child)->m_sides[0].m_nOvlTex = same ? overlay_texture : 15;
						}
						int expected = overlay == 2 ? 8 : overlay == 3 ? -1
						                                               : base_sounds[base];
						if (!(flags & 2) || (child == 0 && same && (flags & 5))) expected = -1;
						if (SideIsSoundSource(1, 0) != expected) throw std::runtime_error("Actual surface selector changed visibility, precedence or portal suppression");
						++cases;
					}
	for (int count = 0; count <= 6; ++count) {
		reset(1);
		gameData.pigData.tex.pTexMapInfo[1].nEffectClip = 0;
		gameData.effectData.pEffect[0].nSound = 7;
		for (int side = 0; side < count; ++side) SEGMENT(0)->m_sides[unsigned(side)].m_nBaseTex = 1;
		SetSoundSources();
		if (audio.resets != 1 || audio.calls.size() != unsigned(count) || gameStates.sound.bDontStartObjects) throw std::runtime_error("Actual surface setup changed ownership lifecycle");
		for (int side = 0; side < count; ++side) {
			const auto &call = audio.calls[unsigned(side)];
			if (call.side != side || call.sound != 7 || call.segment || !call.loop || !call.deferred || call.volume != 65536 / (2 * count) || call.range != 256 * 65536)
				throw std::runtime_error("Actual surface setup changed per-segment volume or deferred ownership");
		}
		++cases;
	}
	std::cout << "PASS: " << cases << " actual source surface selector/setup controls\n";
}
void ambient_audio_source_controls()
{
	using namespace surface_reference;
	unsigned cases = 0;
	for (int enabled : { 0, 1 })
		for (int volume : { 0, 1, 19661, 65536, 131072 }) {
			reset(0);
			install_effects({ { 9, 2, volume, enabled, "first" }, { 3, 0, volume, enabled, "particle" }, { 17, 1, volume, enabled, "lightning" }, { 1, 2, volume, !enabled, "last.wav" } });
			SetSoundSources();
			const std::vector<xl_ambient_audio_receipt::Owner> expected{
				{ { 9, -1, 1, volume, 256 * 65536, 53, 0, 0, 1, 1, 1 }, "first.wav" },
				{ { 1, -1, 1, volume, 256 * 65536, 53, 0, 0, 1, 1, 1 }, "last.wav.wav" }
			};
			if (audio.object_calls != expected || audio.resets != 1 || gameStates.sound.bDontStartObjects || effect_list.nObjects != 4)
				throw std::runtime_error("Actual ambient initializer changed disabled ownership, effect filtering, list order or resource arguments");
			++cases;
		}
	std::cout << "PASS: " << cases << " actual ambient initializer controls, including disabled ownership and real source list insertion\n";
}
void ambient_audio_runtime_controls(const char *path)
{
	using namespace surface_reference;
	std::ifstream input(path);
	const auto receipt = xl_ambient_audio_receipt::read(input);
	if (receipt.objects.size() != 3 || receipt.owners.size() != 3) throw std::runtime_error("Loaded ambient initializer coverage incomplete");
	reset(0);
	install_effects(receipt.objects);
	SetSoundSources();
	if (audio.object_calls != receipt.owners || audio.resets != 1 || gameStates.sound.bDontStartObjects)
		throw std::runtime_error("Native ambient initializer differs from actual source ownership or resource arguments");
	std::cout << "PASS: 3 loaded ambient owners match actual source initialization, order, resource, volume and permanent flags\n";
}
void surface_audio_runtime_controls(const char *path)
{
	using namespace surface_reference;
	std::ifstream input(path);
	std::string tag;
	unsigned version = 0;
	if (!(input >> tag >> version) || tag != "XL_SURFACE_AUDIO" || version != 1) throw std::runtime_error("Invalid surface audio receipt");
	unsigned cases = 0, compared = 0;
	unsigned ordinal = 0;
	while (input >> tag >> ordinal) {
		if (tag == "end") break;
		if (tag != "case" || ordinal != cases) throw std::runtime_error("Surface cases missing or reordered");
		const auto receipt = xl_surface_audio_receipt::read(input);
		const auto &scene = receipt.segments;
		reset(unsigned(scene.size()));
		gameData.pigData.tex.pTexMapInfo.clear();
		for (int effect : receipt.textures) gameData.pigData.tex.pTexMapInfo.push_back({ effect });
		gameData.effectData.pEffect.clear();
		for (int sound : receipt.effects) gameData.effectData.pEffect.push_back({ sound });
		options.sound.xCustomSoundVolume = receipt.custom_volume;
		for (unsigned segment = 0; segment < scene.size(); ++segment)
			for (unsigned side = 0; side < 6; ++side) {
				const auto &row = scene[segment][side];
				SEGMENT(int(segment))->m_sides[side] = { row[2], row[1], row[0], { row[4], row[5], row[6] } };
				SEGMENT(int(segment))->m_children[side] = row[3];
			}
		SetSoundSources();
		std::vector<xl_surface_audio_receipt::Owner> expected;
		for (const auto &call : audio.calls) {
			if (call.sound < 0 || unsigned(call.sound) >= receipt.sounds.size()) continue;
			const int sound = receipt.sounds[unsigned(call.sound)];
			if (sound == 255) continue;
			if (expected.size() == 500) break;
			expected.push_back({ sound, call.segment, call.side, SurfaceScaledVolume(call.sound, call.volume), call.range,
			                     1 | 8 | (call.loop ? 16 : 0) | (call.deferred ? 32 : 0), 0, 0, -1, -1, call.position[0], call.position[1], call.position[2] });
		}
		if (expected != receipt.owners) throw std::runtime_error("Native surface ownership differs from actual source in case " + std::to_string(cases));
		compared += unsigned(expected.size());
		++cases;
	}
	if (tag != "end" || ordinal != cases || cases != 16 || compared < 30 || input >> tag) throw std::runtime_error("Surface receipt coverage incomplete");
	std::cout << "PASS: " << cases << " loaded-mine surface cases and " << compared << " owners match actual source selection, volume, position and lifetime flags\n";
}
