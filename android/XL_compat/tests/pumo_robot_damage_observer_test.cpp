#include "../runtime/pumo_robot_damage_observer.h"
#include <climits>
#include <stdexcept>

enum { OBJ_PLAYER = 4 };
struct Sample {
	int level, weapon, weapon_signature, weapon_id, weapon_valid;
	int parent_type, parent_object, parent_signature, parent_actual_type, parent_actual_signature, parent_valid;
	int target, target_signature, target_id, target_valid;
	int requested_raw, before_raw, after_raw;
	long long accepted_delta_raw, game_time_raw;
};
using Observer = pumo_robot_damage_observer::PlanBound<Sample>;
static void check(bool condition)
{
	if (!condition) throw std::runtime_error("robot damage observer contract failure");
}
static Observer &configured(unsigned limit = 4)
{
	static Observer observer;
	check(observer.configure(2, 0, 13, OBJ_PLAYER, 12, 45, 7, 3, 2, limit));
	observer.begin_frame(1, 2, 0, 13, 3);
	return observer;
}
static Sample sample(int before = 100, int after = 90)
{
	return { 2, 21, 33, 2, 1, OBJ_PLAYER, 0, 13, OBJ_PLAYER, 13, 1,
		12, 45, 7, 1, 10, before, after,
		static_cast<long long>(before) - static_cast<long long>(after), 100 };
}
int main()
{
	{
		auto &observer = configured();
		observer.observe(sample());
		observer.observe(sample(90, 90)); // A rejected or zero-damage call is not accepted damage.
		observer.end_frame();
		check(observer.complete() && observer.count() == 1);
		check(observer.event(0).sample.accepted_delta_raw == 10);
	}
	{
		auto &observer = configured();
		auto wrong = sample();
		wrong.parent_object = 1;
		observer.observe(wrong);
		wrong = sample();
		wrong.target = 11;
		observer.observe(wrong);
		observer.end_frame();
		check(observer.complete() && observer.count() == 0);
	}
	{
		auto &observer = configured();
		auto wrong = sample();
		wrong.parent_actual_signature = 99;
		observer.observe(wrong);
		check(!observer.complete());
	}
	{
		auto &observer = configured();
		auto replaced = sample();
		replaced.parent_valid = 0;
		observer.observe(replaced);
		check(!observer.complete());
	}
	{
		auto &observer = configured();
		auto rejected = sample();
		rejected.requested_raw = 0;
		observer.observe(rejected);
		check(!observer.complete());
	}
	{
		auto &observer = configured();
		observer.observe(sample(90, 100));
		check(!observer.complete()); // A health increase is not accepted damage.
	}
	{
		auto &observer = configured();
		auto reused = sample();
		reused.target_valid = 0;
		observer.observe(reused);
		check(!observer.complete());
	}
	{
		auto &observer = configured();
		auto reused = sample();
		reused.weapon_valid = 0;
		observer.observe(reused);
		check(!observer.complete());
	}
	{
		auto &observer = configured();
		auto reused = sample();
		reused.target_signature = 46;
		observer.observe(reused);
		check(!observer.complete());
	}
	{
		auto &observer = configured();
		observer.end_frame();
		observer.begin_frame(2, 2, 0, 13, 4);
		check(!observer.complete()); // A load cannot reuse a prior binding.
	}
	{
		auto &observer = configured(1);
		observer.observe(sample());
		observer.observe(sample());
		check(observer.truncated() && !observer.complete());
	}
	{
		auto &observer = configured();
		observer.observe(sample(INT_MAX, INT_MIN));
		observer.end_frame();
		check(observer.complete() && observer.event(0).sample.accepted_delta_raw == 4294967295LL);
	}
	{
		auto &observer = configured();
		observer.end_frame();
		check(observer.complete() && observer.count() == 0); // The disabled callback is a null path.
	}
}
