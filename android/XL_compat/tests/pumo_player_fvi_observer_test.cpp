#include "../runtime/pumo_player_fvi_observer.h"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>

struct Sample {
	int level, player_object, player_signature, start_segment;
};

int main()
{
	using Observer = pumo_player_fvi_observer::PlanBound<Sample>;
	Observer observer;
	const int segments[] = { 12, 13 };
	const int duplicate[] = { 12, 12 };
	assert(!observer.configure(7, 4, 99, 2, duplicate, 2, 2, 2));
	assert(!observer.complete());
	assert(observer.configure(7, 4, 99, 2, segments, 2, 2, 2));
	assert(!observer.complete());       // An unstarted receipt cannot be complete
	observer.observe({ 7, 4, 99, 12 }); // Default callback state is inactive
	assert(observer.count() == 0);
	observer.begin_frame(1, 7, 2);
	observer.observe({ 7, 5, 99, 12 });  // Wrong player slot
	observer.observe({ 7, 4, 100, 12 }); // Wrong player signature
	assert(observer.count() == 0);
	observer.observe({ 7, 4, 99, 12 });
	observer.end_frame();
	observer.observe({ 7, 4, 99, 12 }); // Outside the frame bracket
	assert(observer.count() == 1 && observer.event(0).frame == 1 && observer.event(0).load_generation == 2);
	observer.begin_frame(2, 7, 2);
	observer.observe({ 7, 4, 99, 13 });
	observer.end_frame();
	assert(observer.complete() && observer.count() == 2);
	observer.begin_frame(2, 7, 2); // Duplicate frame invalidates the binding
	assert(observer.truncated() && !observer.complete());
	observer.reset();
	assert(observer.configure(7, 4, 99, 2, segments, 2, 2, 2));
	observer.begin_frame(1, 7, 2);
	observer.observe({ 7, 4, 99, 12 });
	observer.observe({ 7, 4, 99, 13 });
	observer.observe({ 7, 4, 99, 12 });
	assert(observer.truncated() && !observer.complete() && observer.matched_count() == 3 && observer.count() == 2);
	observer.reset(); // Consecutive runs cannot inherit samples or truncation
	assert(observer.count() == 0 && !observer.complete());
	assert(observer.configure(7, 4, 99, 3, segments, 2, 1, 2));
	observer.begin_frame(1, 7, 3);
	observer.observe({ 7, 4, 99, 14 }); // Wrong bound-player segment fails closed
	assert(observer.truncated() && !observer.complete());
	observer.reset();
	assert(observer.configure(7, 4, 99, 3, segments, 2, 1, 2));
	observer.begin_frame(1, 7, 3);
	observer.observe({ 8, 4, 99, 12 }); // Wrong bound-player level fails closed
	assert(observer.truncated() && !observer.complete());
	observer.reset();
	assert(observer.configure(7, 4, 99, 3, segments, 2, 1, 2));
	observer.begin_frame(1, 7, 3);
	observer.end_frame(); // A frame without the bound player's FVI is incomplete
	assert(observer.truncated() && !observer.complete());
	observer.reset();
	assert(observer.configure(7, 4, 99, 3, segments, 2, 1, 2));
	observer.begin_frame(1, 7, 2); // A stale generation is invalid
	assert(observer.truncated() && !observer.complete());
	observer.reset();
	assert(observer.configure(7, 4, 99, 3, segments, 2, 1, 2));
	observer.begin_frame(1, 8, 3); // A different level is invalid
	assert(observer.truncated() && !observer.complete());
	observer.reset();
	assert(observer.configure(7, 4, 99, 3, segments, 2, 1, 2));
	observer.begin_frame(1, 7, 3);
	observer.observe({ 7, 4, 99, 12 });
	observer.end_frame();
	observer.begin_frame(2, 7, 3); // Frame budget is strict
	assert(observer.truncated() && !observer.complete());
	observer.reset();
	assert(observer.configure(7, 4, 99, 3, segments, 2, 1, 2));
	observer.begin_frame(1, 7, 3);
	observer.observe({ 7, 4, 99, 12 });
	observer.end_frame();
	assert(observer.complete());
	observer.invalidate(); // Load or level change must fail closed
	assert(observer.truncated() && !observer.complete());
}
