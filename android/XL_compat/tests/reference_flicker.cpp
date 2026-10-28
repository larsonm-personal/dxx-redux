#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
using fix = int32_t;
constexpr int WID_VISIBLE_FLAG = 1;
struct CVariableLight {
	int16_t m_nSegment = 0, m_nSide = 0;
	uint32_t m_mask = 0;
	fix m_timer = 0, m_delay = 0;
};
struct CSide {
	int m_nBaseTex = 0, m_nOvlTex = 1;
};
int visible, action;
struct CSegment {
	CSide m_sides[6];
	int IsPassable(int, void *) const
	{
		return visible ? WID_VISIBLE_FLAG : 0;
	}
} segment;
CSegment *SEGMENT(int)
{
	return &segment;
}
struct Array {
	std::vector<CVariableLight> data;
	CVariableLight *Buffer()
	{
		return data.empty() ? nullptr : data.data();
	}
	int Length() const
	{
		return static_cast<int>(data.size());
	}
};
struct {
	struct {
		struct {
			Array flicker;
		} lights;
	} renderData;
	struct {
		fix xFrame = 0;
	} timeData;
	struct {
		struct {
			int brightness[2]{};
		} tex;
	} pigData;
} gameData;
struct Options {
	struct {
		bool bEpilepticFriendly = false;
	} app;
} options;
Options *gameOpts = &options;
int AddLight(int, int)
{
	action = 1;
	return 1;
}
int SubtractLight(int, int)
{
	action = 0;
	return 1;
}
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include "xl_reference_flicker.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected production flicker sample path");
		std::ifstream in(argv[1]);
		if (!in) throw std::runtime_error("Cannot open flicker samples");
		int timer, delay, elapsed, emitting, expected_timer, expected_action, count = 0;
		uint32_t mask, expected_mask;
		while (in >> timer >> mask >> delay >> elapsed >> visible >> emitting >> expected_timer >> expected_mask >> expected_action) {
			gameData.renderData.lights.flicker.data.assign(1, { 0, 0, mask, timer, delay });
			gameData.timeData.xFrame = elapsed;
			gameData.pigData.tex.brightness[0] = emitting;
			action = -1;
			FlickerLights();
			const auto &result = gameData.renderData.lights.flicker.data[0];
			if (result.m_timer != expected_timer || result.m_mask != expected_mask || action != expected_action)
				throw std::runtime_error("Source flicker phase/action differs");
			++count;
		}
		if (!in.eof() || count < 1000) throw std::runtime_error("Missing or malformed flicker coverage");
		gameData.renderData.lights.flicker.data.assign(2, { 0, 0, 2, 123, 65536 });
		DisableVariableLight(0, 0);
		if (gameData.renderData.lights.flicker.data[0].m_timer != INT32_MIN || gameData.renderData.lights.flicker.data[1].m_timer != 123)
			throw std::runtime_error("Source disable owner differs");
		EnableVariableLight(0, 0);
		if (gameData.renderData.lights.flicker.data[0].m_timer != 0 || gameData.renderData.lights.flicker.data[0].m_mask != 2 || gameData.renderData.lights.flicker.data[1].m_timer != 123)
			throw std::runtime_error("Source enable phase differs");
		std::cout << count << " source flicker phases/actions and first-duplicate controls matched\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
