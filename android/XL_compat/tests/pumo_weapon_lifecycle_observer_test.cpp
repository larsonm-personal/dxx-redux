#include "../runtime/pumo_weapon_lifecycle_observer.h"
#include <stdexcept>

struct Sample {
	int kind, level, object, signature, parent_type, parent_object, parent_signature;
	long long game_time_raw;
};
using Observer = pumo_weapon_lifecycle_observer::PlanBound<Sample>;
static void check(bool value)
{
	if (!value) throw std::runtime_error("weapon lifecycle contract failure");
}
static Observer &configured(unsigned limit = 16)
{
	static Observer observer;
	check(observer.configure(7, 3, 21, 4, 2, limit));
	return observer;
}
static void begin(Observer &observer)
{
	observer.begin_frame(1, 7, 3, 21, 4, 100);
}
static void world(Observer &observer)
{
	observer.set_phase(Observer::game_time_advance, 100);
	observer.set_phase(Observer::world_update, 101);
}
static Sample sample(int kind, int object, int signature, long long clock = 101)
{
	return { kind, 7, object, signature, 0, 0, 0, clock };
}
int main()
{
	{
		auto &observer = configured();
		check(observer.seed(1, 9)); // Pre-existing save weapon has no invented birth.
		begin(observer);
		observer.observe(sample(1, 2, 10, 100));
		observer.set_phase(Observer::game_time_advance, 100);
		observer.set_phase(Observer::world_update, 101);
		observer.observe(sample(2, 2, 10)); // Standard parent promotion.
		observer.observe(sample(3, 3, 11)); // No birth is invalid even for Omega.
		check(!observer.valid());
	}
	{
		auto &observer = configured();
		check(observer.seed(1, 9));
		begin(observer);
		observer.observe(sample(1, 2, 10, 100));
		world(observer);
		observer.observe(sample(2, 2, 10));
		observer.observe(sample(6, 2, 10)); // Born and retired inside one frame.
		observer.observe(sample(6, 1, 9));
		observer.end_frame(101);
		check(observer.valid() && observer.count() == 4);
		check(observer.event(0).phase == Observer::weapon_input);
		check(observer.event(1).phase == Observer::world_update);
	}
	{
		auto &observer = configured();
		begin(observer);
		world(observer);
		observer.observe(sample(1, 2, 10));
		observer.observe(sample(3, 2, 10)); // Omega direct parent.
		observer.observe(sample(4, 2, 10)); // Buddy rewrite.
		observer.observe(sample(6, 2, 10));
		observer.observe(sample(1, 2, 11)); // Reused slot requires a new signature.
		observer.observe(sample(5, 2, 11)); // Reflection preserves unknown native parent.
		observer.end_frame(101);
		check(observer.valid() && observer.count() == 6);
	}
	{
		auto &observer = configured();
		begin(observer);
		world(observer);
		observer.observe(sample(1, 2, 10));
		observer.observe(sample(1, 2, 10));
		check(!observer.valid()); // Duplicate birth.
	}
	{
		auto &observer = configured(1);
		begin(observer);
		world(observer);
		observer.observe(sample(1, 2, 10));
		observer.observe(sample(6, 2, 10));
		check(observer.truncated() && !observer.valid());
	}
	{
		auto &observer = configured();
		observer.begin_frame(1, 7, 3, 21, 5, 100);
		check(!observer.valid()); // Wrong load generation.
	}
	{
		auto &observer = configured();
		begin(observer);
		world(observer);
		observer.observe(sample(1, 2, 10, 100));
		check(!observer.valid()); // Wrong phase clock.
	}
	{
		auto &observer = configured();
		begin(observer);
		world(observer);
		auto birth = sample(1, 2, 10);
		birth.parent_type = 1;
		observer.observe(birth);
		check(!observer.valid()); // Guessed provisional owner.
	}
	{
		auto &observer = configured();
		begin(observer);
		world(observer);
		observer.observe(sample(1, 2, 10));
		observer.end_frame(101);
		observer.begin_frame(2, 8, 3, 21, 4, 101);
		check(!observer.valid()); // Level transition invalidates binding.
	}
	{
		auto &observer = configured();
		begin(observer);
		world(observer);
		observer.observe(sample(6, 2, 10));
		check(!observer.valid()); // Retirement needs a PRE seed or observed birth.
	}
	{
		auto &observer = configured();
		begin(observer);
		world(observer);
		observer.end_frame(101);
		check(observer.valid() && observer.count() == 0);
		check(observer.configure(7, 3, 21, 5, 2, 16));
		observer.begin_frame(1, 7, 3, 21, 5, 201);
		observer.set_phase(Observer::game_time_advance, 201);
		observer.set_phase(Observer::world_update, 202);
		observer.end_frame(202);
		check(observer.valid() && observer.count() == 0); // No callback state leaks into a second run.
	}
}
