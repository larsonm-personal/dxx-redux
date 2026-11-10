#include "../slow_motion.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using Event = xl_compat::SlowMotionEvent;
using State = xl_compat::SlowMotionState;
using Input = xl_compat::SlowMotionInput;
namespace reference
{
#define I2X(x)                  ((x) * 65536)
#define PLAYER_FLAGS_SLOWMOTION 32768
#define PLAYER_FLAGS_BULLETTIME 65536
#define TXT_SLOWING_DOWN        1
#define TXT_SPEEDING_UP         2
#define SLOWDOWN_SECS           2
#define SLOWDOWN_FPS            40
struct Clock {
	float fSpeed = 1;
	int nState = 0;
	std::int64_t tUpdate = 0;
	int bActive = 0;
};
struct {
	struct {
		Clock slowmo[2];
	} gameplay;
	struct {
		struct {
			bool bSpeed = false;
		} cheats;
		int nDifficultyLevel = 0;
		std::int64_t nSDLTicks[1]{};
		bool bNostalgia = false;
	} app;
} gameStates;
struct {
	struct {
		bool bDestroyed = false;
	} reactorData;
	struct {
		int xFrame = 0;
	} timeData;
} gameData;
struct Options {
	struct {
		bool bUseSDLMixer = false;
	} sound;
	struct {
		int nSlowMotionSpeedup = 6;
	} gameplay;
} options, *gameOpts = &options;
struct {
	int slowMotionCount = 0, bulletTimeCount = 0;
} controls[1];
std::vector<Event> events;
std::int32_t delta = 0;
struct Player {
	std::uint32_t flags = 0;
	int energy = 0;
	int Energy()
	{
		return energy;
	}
	void UpdateEnergy(int change)
	{
		delta += change;
		energy = std::clamp(energy + change, -1, 200 * 65536);
	}
} player;
#define LOCALPLAYER player
bool IsMultiGame = false, available = true;
int nSlowMotionChannel = -1;
struct {
	void StopSound(int)
	{
		events.push_back(Event::stop_transition_sound);
	}
	int PlayWAV(const char *name)
	{
		events.push_back(name[0] == 's' && name[1] == 'l' ? Event::play_slowdown : Event::play_speedup);
		return available ? 42 : -1;
	}
} audio;
void HUDInitMessage(int kind)
{
	events.push_back(kind == 1 ? Event::slowing_message : Event::speeding_message);
}
void SpeedupSound()
{
	events.push_back(Event::normal_audio);
}
void SlowdownSound()
{
	events.push_back(Event::slow_audio);
}
#include "xl_reference_slow_motion.h"
} // namespace reference
namespace
{
void require(bool good, const char *reason)
{
	if (!good) throw std::runtime_error(reason);
}
bool equal(const State &a, const State &b)
{
	if (a.transition_sound != b.transition_sound) return false;
	for (unsigned i = 0; i < 2; ++i) {
		const auto &x = a.clocks[i], &y = b.clocks[i];
		if (x.speed != y.speed || x.direction != y.direction || x.updated_ms != y.updated_ms || x.active != y.active) return false;
	}
	return true;
}
std::size_t comparisons = 0;
xl_compat::SlowMotionOutput compare(State &state, const Input &input, bool sound_available = true)
{
	using namespace reference;
	for (unsigned i = 0; i < 2; ++i) {
		const auto &c = state.clocks[i];
		gameStates.gameplay.slowmo[i] = { c.speed, c.direction, c.updated_ms, c.active };
	}
	gameStates.app.cheats.bSpeed = input.cheat;
	gameStates.app.nDifficultyLevel = input.difficulty;
	gameStates.app.nSDLTicks[0] = input.now_ms;
	gameStates.app.bNostalgia = input.nostalgia || !input.xl;
	gameData.reactorData.bDestroyed = input.reactor_destroyed;
	gameData.timeData.xFrame = input.frame;
	options.sound.bUseSDLMixer = input.mixer;
	player = { input.equipment, input.energy };
	controls[0] = { input.slow_pressed, input.bullet_pressed };
	IsMultiGame = input.multiplayer;
	available = sound_available;
	nSlowMotionChannel = state.transition_sound ? 42 : -1;
	events.clear();
	delta = 0;
	if (input.force_off && input.xl && !input.multiplayer && !input.nostalgia) SlowMotionOff();
	else DoSlowMotionFrame();
	std::vector<Event> delivered;
	auto actual = xl_compat::slow_motion_frame(state, input, [&](Event event) { delivered.push_back(event); return sound_available; });
	require(delivered == actual.events, "Sink delivery did not follow recorded event order");
	State expected;
	for (unsigned i = 0; i < 2; ++i) {
		const auto &c = gameStates.gameplay.slowmo[i];
		expected.clocks[i] = { c.fSpeed, c.nState, c.tUpdate, c.bActive != 0 };
	}
	expected.transition_sound = nSlowMotionChannel >= 0;
	require(equal(state, expected), "World/player state differs from pinned source");
	require(actual.events == events, "Audio/message order differs from pinned source");
	require(actual.energy_delta == delta, "Energy debit differs from pinned source");
	require(actual.consumed_controls == (input.xl && !input.multiplayer && !input.nostalgia && !input.reactor_destroyed && !input.force_off), "Control-consumption contract differs");
	require(controls[0].slowMotionCount == (actual.consumed_controls ? 0 : input.slow_pressed) &&
	            controls[0].bulletTimeCount == (actual.consumed_controls ? 0 : input.bullet_pressed),
	        "Reference controls were not preserved/consumed");
	++comparisons;
	return actual;
}
void exhaustive()
{
	for (float world : { 1.0f, 1.025f, 2.0f, 2.99f, 3.0f })
		for (float player : { 1.0f, 1.025f, 2.0f, 2.99f, 3.0f })
			for (int wd : { -1, 0, 1 })
				for (int pd : { -1, 0, 1 })
					for (unsigned flags : { 0u, 32768u, 65536u, 98304u })
						for (int energy : { -1, 0, 655359, 655360, 655361, 6553600 })
							for (int requests = 0; requests < 4; ++requests)
								for (int elapsed : { 0, 25, 26, 1000 }) {
									State state;
									state.clocks = { { { world, wd, 100, false }, { player, pd, 100, true } } };
									state.transition_sound = requests % 2;
									Input input;
									input.xl = true;
									input.now_ms = 100 + elapsed;
									input.equipment = flags;
									input.energy = energy;
									input.frame = 1092;
									input.difficulty = requests;
									input.slow_pressed = requests & 1;
									input.bullet_pressed = requests & 2;
									input.mixer = true;
									compare(state, input, requests != 3);
								}
}
void trajectories()
{
	for (int mode = 0; mode < 12; ++mode) {
		State state;
		Input input;
		input.xl = true;
		input.equipment = xl_compat::slow_motion_equipment;
		input.energy = 100 * 65536;
		input.frame = mode % 2 ? 65536 : 1092;
		input.difficulty = mode % 5;
		input.mixer = mode % 2;
		input.cheat = mode == 2;
		for (int step = 0; step < 1000; ++step) {
			input.now_ms += step % 3 ? 26 : 25;
			input.slow_pressed = step == 0 || step == 400 || step == 900;
			input.bullet_pressed = step == 200 || step == 600;
			input.reactor_destroyed = mode == 3 && step > 25;
			input.force_off = mode == 3 && step == 25;
			input.multiplayer = mode == 4 && step >= 20 && step <= 300;
			input.nostalgia = mode == 5 && step >= 20 && step <= 300;
			input.xl = !(mode == 6 && step >= 20 && step <= 300);
			const bool sound_available = !(mode == 7 && step > 100);
			if (mode == 8 && step == 123) input.equipment = 0;
			if (mode == 9 && step == 123) input.energy = 10 * 65536;
			if (mode == 10) input.frame = 0;
			if (mode == 11) {
				input.now_ms += 10000;
				input.frame = 3 * 65536;
			}
			auto saved = state;
			const auto result = compare(state, input, sound_available);
			const auto replay = xl_compat::slow_motion_frame(saved, input, [=](Event) { return sound_available; });
			require(equal(saved, state) && replay.events == result.events && replay.energy_delta == result.energy_delta, "Copied state did not replay identically");
			input.energy = std::clamp(input.energy + result.energy_delta, -1, 200 * 65536);
		}
	}
}
void no_sink()
{
	State state;
	Input input;
	input.xl = input.mixer = input.slow_pressed = true;
	input.equipment = xl_compat::slow_motion_equipment;
	input.energy = 100 * 65536;
	const auto result = xl_compat::slow_motion_frame(state, input);
	require(!state.transition_sound && result.events == std::vector<Event>{ Event::play_slowdown, Event::slowing_message }, "Absent sink claimed successful playback");
}
void negatives()
{
	for (int mode = 0; mode < 14; ++mode) {
		State state;
		Input input;
		input.xl = true;
		input.now_ms = 100;
		switch (mode) {
			case 0: state.clocks[0].speed = std::numeric_limits<float>::quiet_NaN(); break;
			case 1: state.clocks[1].speed = std::numeric_limits<float>::infinity(); break;
			case 2: state.clocks[0].speed = 0.99f; break;
			case 3: state.clocks[1].speed = 3.01f; break;
			case 4: state.clocks[0].direction = 2; break;
			case 5: state.clocks[1].updated_ms = -1; break;
			case 6: state.clocks[1].updated_ms = 101; break;
			case 7: input.now_ms = -1; break;
			case 8: input.frame = -1; break;
			case 9: input.frame = (std::numeric_limits<std::int32_t>::max)() / 16 + 1; break;
			case 10: input.difficulty = -1; break;
			case 11: input.difficulty = 5; break;
			case 12: input.energy = -2; break;
			case 13: input.energy = 200 * 65536 + 1; break;
		}
		const auto before = state;
		bool rejected = false;
		try {
			(void) xl_compat::slow_motion_frame(state, input, [](Event) -> bool { throw std::runtime_error("Rejected input reached effect sink"); });
		} catch (const std::invalid_argument &) {
			rejected = true;
		}
		require(rejected, "Malformed clock/input accepted");
		// NaN cannot compare equal to itself; compare the bytes of that field separately
		if (mode == 0) {
			require(std::isnan(state.clocks[0].speed), "NaN input mutated");
			state.clocks[0].speed = 1;
		}
		auto expected = before;
		if (mode == 0) expected.clocks[0].speed = 1;
		require(equal(state, expected), "Rejected input mutated state");
		for (int disabled = 0; disabled < 3; ++disabled) {
			input.xl = disabled != 0;
			input.multiplayer = disabled == 1;
			input.nostalgia = disabled == 2;
			const auto output = xl_compat::slow_motion_frame(state, input, [](Event) -> bool { throw std::runtime_error("Disabled path reached effect sink"); });
			require(equal(state, expected) && output.events.empty() && !output.energy_delta && !output.consumed_controls, "Disabled path changed state");
		}
	}
}
} // namespace
int main()
{
	try {
		exhaustive();
		trajectories();
		no_sink();
		negatives();
		std::cout << comparisons << " pinned slow-motion frames matched; malformed and disabled controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
