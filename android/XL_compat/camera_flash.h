#ifndef DXX_XL_CAMERA_FLASH_H
#define DXX_XL_CAMERA_FLASH_H
#include "camera_splash.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <optional>

namespace xl_compat
{
inline int camera_flash_byte(int value)
{
	const auto bits = static_cast<std::uint8_t>(value);
	std::int8_t result;
	std::memcpy(&result, &bits, sizeof(result));
	return result;
}
template <class Random>
bool camera_flash_robot(int &skip, FixedLightPoint &thrust, const FixedLightPoint &force,
                        int flash, bool boss, int explosion_skip, int frame, Random random)
{
	if (!flash || boss) return false;
	if (flash < 0 || frame < 0 || skip < -128 || skip > 127) throw std::runtime_error("Invalid camera flash robot state");
	const int magnitude = splash_fixed(double(geometry_default_magnitude(force)) * flash);
	const int count = splash_divide(magnitude, frame) / 128 / 65536 + 2;
	if (std::int64_t(explosion_skip) * frame >= 65536) {
		skip = camera_flash_byte(skip - 1);
		return false;
	}
	skip = camera_flash_byte(skip + count);
	for (auto &axis : thrust) axis = splash_fixed(double(std::int64_t(16383 - random()) * count / 16));
	return true;
}
struct CameraFlashPulse {
	int delay = 0, intensity = 0;
};
inline CameraFlashPulse camera_flash_pulse(int force, int flash, bool self)
{
	if (force < 0 || flash < 0) throw std::runtime_error("Invalid camera flash player force");
	if (!flash) return {};
	int delay = (std::min) (4 * 65536, splash_fixed(double(force) * flash) / 32);
	if (self) {
		delay /= 2;
		force /= 2;
	}
	return force > 65536 ? CameraFlashPulse{ delay, 1 + splash_fixed(double(8) * force) / 65536 } : CameraFlashPulse{};
}
struct CameraFlashFrame {
	std::optional<int> sound;
	std::optional<std::array<int, 3>> demo;
};
// Source palette timing, with the existing renderer consuming quantized RGB values
struct CameraFlashPalette {
	std::array<float, 3> color{};
	int remaining = 0, duration = 0, delay = 0;
	std::int64_t last_sound = 0;
	float fade() const
	{
		return duration ? float(remaining) / float(duration) : 0.0f;
	}
	void start(const std::array<float, 3> &value)
	{
		bool visible = false;
		for (unsigned i = 0; i < 3; ++i) {
			color[i] = std::clamp(value[i], 0.0f, 1.0f);
			visible |= color[i] > 0;
		}
		remaining = duration = visible ? 32768 : 0;
	}
	void bump(const std::array<int, 3> &value)
	{
		std::array<float, 3> next{};
		const float scale = std::clamp(fade(), 0.0f, 1.0f);
		for (unsigned i = 0; i < 3; ++i) {
			const float previous = color[i] * scale;
			next[i] = (std::max) (std::clamp(previous + float(value[i]) / 64.0f, 0.0f, 0.46875f), previous);
		}
		start(next);
	}
	std::array<int, 3> saved_color() const
	{
		std::array<int, 3> result{};
		for (unsigned i = 0; i < 3; ++i) result[i] = int(color[i] * 64.0f * fade());
		return result;
	}
	template <class Random>
	CameraFlashFrame frame(int elapsed, std::int64_t time, Random random)
	{
		if (elapsed < 0) throw std::runtime_error("Invalid camera flash frame time");
		CameraFlashFrame result;
		bool record = false;
		if (delay) {
			const bool force = last_sound != 0;
			if (last_sound + 8192 < time || last_sound > time) {
				result.sound = delay / 4;
				last_sound = time;
			}
			delay = (std::max) (0, delay - elapsed);
			// The source also consumes this draw when normal gameplay is not recording a demo
			record = force || random() > 4096;
		} else {
			remaining = (std::max) (0, remaining - elapsed);
			record = true;
		}
		if (record && color != std::array<float, 3>{})
			result.demo = std::array<int, 3>{ int(color[0] * 64), int(color[1] * 64), int(color[2] * 64) };
		if (!remaining) start({});
		return result;
	}
};
} // namespace xl_compat
#endif
