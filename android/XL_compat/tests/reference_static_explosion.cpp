#include "static_explosion.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
namespace static_explosion_reference
{
using fix = std::int32_t;
#define DBG            0
#define I2X(value)     ((value) * 65536)
#define OBJ_IDX(value) ((value)->index)
#include "xl_reference_static_constants.h"
struct tBitmapIndex {
	int index = 0;
};
struct tAnimationInfo {
	fix xFrameTime = 0;
	int nFrames = 0, nSound = -1;
	tBitmapIndex frames[127]{};
};
struct tAnimationState {
	fix xFrameTime = 0;
	int nCurFrame = 0, nClipIndex = 0, xTotalTime = 0;
};
struct CObject {
	int index = 0;
	struct {
		int nType = OBJ_EXPLOSION, nId = 0, renderType = RT_POWERUP, nSegment = 0;
		fix xLifeLeft = 0x3fffffff;
		struct {
			std::array<int, 3> vPos{};
		} position;
	} info;
	struct {
		tAnimationState animationInfo;
	} rType;
	void DoPowerupFrame();
};
std::array<CObject, 64> objects;
CObject *OBJECT(int index)
{
	return &objects.at(unsigned(index));
}
struct AddonBitmap {
	int FrameCount() const
	{
		throw std::runtime_error("Unqualified addon animation path");
	}
};
struct {
	struct {
		fix xTime = 0;
	} physicsData;
	struct {
		tAnimationInfo animations[2][MAX_ANIMATIONS_D2];
	} effectData;
	struct {
		struct {
			AddonBitmap addonBitmaps[MAX_ADDON_BITMAP_FILES];
		} tex;
	} pigData;
} gameData;
struct {
	struct {
		struct {
			float fSpeed = 1;
		} slowmo[2];
	} gameplay;
	struct {
		int nSDLTicks[2]{};
	} app;
} gameStates;
int SetupHiresVClip(tAnimationInfo *info, tAnimationState *)
{
	return info->nFrames;
}
int nDbgMinFrame = 0;
struct Draw {
	int object, base, bitmap, frame;
	float alpha;
};
std::vector<Draw> draws;
void DrawObjectBitmap(CObject *object, int base, int bitmap, int frame, void *, float alpha)
{
	draws.push_back({ object->index, base, bitmap, frame, alpha });
}
void DrawMonsterball(CObject *, float, float, float, float)
{
	throw std::runtime_error("Unqualified monsterball path");
}
void RenderMslLockIndicator(CObject *)
{
	throw std::runtime_error("Unqualified lock indicator path");
}
int disappearances = 0, disappearance_sounds = 0;
void CreateExplosion(CObject *, int, const std::array<int, 3> &, const std::array<int, 3> &, int size, int clip)
{
	if (size != I2X(7) / 2 || clip != ANIM_POWERUP_DISAPPEARANCE) throw std::runtime_error("Unexpected disappearance arguments");
	++disappearances;
}
struct Audio {
	void CreateObjectSound(int sound, int sound_class, int object)
	{
		if (sound != 17 || sound_class != SOUNDCLASS_GENERIC || object != 1) throw std::runtime_error("Unexpected disappearance sound");
		++disappearance_sounds;
	}
} audio;
#include "xl_reference_static_explosion.h"
} // namespace static_explosion_reference

void static_explosion_source_controls()
{
	using namespace static_explosion_reference;
	unsigned cases = 0;
	for (int index = 0; index < 4; ++index)
		for (int frames : { 1, 2, 7, 30, 110 })
			for (int duration : { 1, 1092, 4096, 65536 })
				for (int initial : { -duration, -1, 0, duration / 2, duration }) {
					auto &object = objects[unsigned(index)];
					object = {};
					object.index = index;
					object.rType.animationInfo = { initial, frames - 1, 80, 0 };
					auto &clip = gameData.effectData.animations[0][80];
					clip.xFrameTime = duration;
					clip.nFrames = frames;
					xl_compat::StaticExplosionFrame actual{ initial, frames - 1 };
					for (int elapsed : { 0, 1, duration - 1, duration, duration + 1, duration * frames * 3 + 17, 1092, 2184 }) {
						gameData.physicsData.xTime = elapsed;
						object.DoPowerupFrame();
						actual = xl_compat::advance_static_explosion(actual, frames, duration, elapsed);
						if (actual.remaining != object.rType.animationInfo.xFrameTime || actual.frame != object.rType.animationInfo.nCurFrame)
							throw std::runtime_error("Static explosion animation differs: slot=" + std::to_string(index) + " frames=" + std::to_string(frames) + " duration=" + std::to_string(duration) + " elapsed=" + std::to_string(elapsed) + " initial=" + std::to_string(initial) + " actual=" + std::to_string(actual.remaining) + "/" + std::to_string(actual.frame) + " source=" + std::to_string(object.rType.animationInfo.xFrameTime) + "/" + std::to_string(object.rType.animationInfo.nCurFrame));
						++cases;
					}
				}
	// DrawPowerup selects the current bitmap from the animation, not the powerup ID
	for (int id : { 0, 47, 48, 80, 109 }) {
		auto &object = objects[1];
		object = {};
		object.index = 1;
		object.info.nId = id;
		object.rType.animationInfo = { 0, 3, id, 0 };
		auto &clip = gameData.effectData.animations[0][id];
		clip.frames[0].index = 100 + id;
		clip.frames[3].index = 200 + id;
		draws.clear();
		DrawPowerup(&object);
		if (draws.size() != 1 || draws[0].object != 1 || draws[0].base != 100 + id || draws[0].bitmap != 200 + id || draws[0].frame != 3 || draws[0].alpha != 0)
			throw std::runtime_error("Static explosion draw selection differs");
		++cases;
	}
	if (disappearances || disappearance_sounds) throw std::runtime_error("Immortal static explosion expired");
	auto &object = objects[1];
	object.info.xLifeLeft = 0;
	gameData.effectData.animations[0][ANIM_POWERUP_DISAPPEARANCE].nSound = 17;
	object.DoPowerupFrame();
	if (disappearances != 1 || disappearance_sounds != 1) throw std::runtime_error("Static explosion expiration differs");
	std::cout << "PASS: " << cases << " pinned static explosion animation/draw comparisons and expiration control\n";
}

void static_explosion_runtime_controls(const char *path)
{
	using namespace static_explosion_reference;
	std::ifstream input(path);
	std::string tag;
	int version = 0;
	if (!(input >> tag >> version) || tag != "XL_STATIC_ANIMATION" || version != 1)
		throw std::runtime_error("Invalid static animation receipt");
	unsigned count = 0;
	while (input >> tag && tag != "end") {
		const int index = std::stoi(tag);
		int frames, duration, remaining, frame, elapsed, expected_remaining, expected_frame;
		if (!(input >> frames >> duration >> remaining >> frame >> elapsed >> expected_remaining >> expected_frame) ||
		    (index != 49 && index != 50) || frames <= 0 || frames > 127 || duration <= 0 || elapsed < 0 || frame < 0 || frame >= frames || count >= 48)
			throw std::runtime_error("Invalid loaded static animation row");
		auto &object = objects.at(unsigned(index));
		object = {};
		object.index = index;
		object.rType.animationInfo = { remaining, frame, 0, 0 };
		auto &clip = gameData.effectData.animations[0][0];
		clip.nFrames = frames;
		clip.xFrameTime = duration;
		gameData.physicsData.xTime = elapsed;
		object.DoPowerupFrame();
		if (object.rType.animationInfo.xFrameTime != expected_remaining || object.rType.animationInfo.nCurFrame != expected_frame)
			throw std::runtime_error("Loaded static animation differs from source at row " + std::to_string(count));
		++count;
	}
	unsigned total = 0;
	if (tag != "end" || !(input >> total) || total != count || count != 48 || input >> tag)
		throw std::runtime_error("Incomplete loaded static animation coverage");
	std::cout << "PASS: 48 loaded static explosion animation steps match actual source\n";
}

namespace static_sprite_reference
{
#include "xl_reference_static_constants.h"
#include "xl_reference_static_blend_constants.h"
struct {
	struct {
		struct {
			int bUseHires[1]{};
		} textures;
		struct {
			int bTransparent = 1;
		} effects;
		int nImageQuality = 1;
	} render;
} options;
auto *gameOpts = &options;
bool IsEnergyPowerup(int)
{
	throw std::runtime_error("Static sprite entered powerup policy");
}
std::pair<float, int> policy(int hires, int transparent, int quality)
{
	options.render.textures.bUseHires[0] = hires;
	options.render.effects.bTransparent = transparent;
	options.render.nImageQuality = quality;
	const int nType = OBJ_EXPLOSION, nId = 0;
	struct {
		struct {
			int nId = 0;
		} info;
	} object;
	const auto *pObj = &object;
	float fAlpha = 0;
	int bAdditive = 0, bEnergy = 0;
#include "xl_reference_static_sprite.h"
	return { fAlpha, bAdditive };
}
// Record the GL API arguments of the unchanged source overload
constexpr int GL_ZERO = 0, GL_ONE = 1, GL_SRC_COLOR = 0x300, GL_ONE_MINUS_SRC_COLOR = 0x301,
              GL_SRC_ALPHA = 0x302, GL_ONE_MINUS_SRC_ALPHA = 0x303, GL_DST_COLOR = 0x306;
struct Blend {
	int source = -1, destination = -1;
	void SetBlendMode(int a, int b)
	{
		source = a;
		destination = b;
	}
#include "xl_reference_static_blend.h"
};
} // namespace static_sprite_reference
void static_explosion_render_source_controls()
{
	using namespace static_sprite_reference;
	unsigned cases = 0;
	for (int hires : { 0, 1 })
		for (int transparent : { 0, 1 })
			for (int quality : { 0, 1, 4 }) {
				const auto selected = policy(hires, transparent, quality);
				Blend blend;
				blend.SetBlendMode(selected.second);
				const bool additive = (hires || transparent) && quality;
				if (selected.first != 1 || selected.second != int(additive) || blend.source != (additive ? GL_ONE : GL_SRC_ALPHA) ||
				    blend.destination != (additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA))
					throw std::runtime_error("Pinned static sprite opacity/blend profile differs");
				++cases;
			}
	std::cout << "PASS: " << cases << " actual static sprite profiles and GL blend arguments; full opacity, additive ONE/ONE\n";
}
