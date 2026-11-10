#include "../XL_compat/runtime/pumo_ordinary_input_validation.h"
#include <nlohmann/json.hpp>
#include <cassert>
#include <limits>

using Json = nlohmann::json;
using namespace pumo_ordinary_input_validation;

int main()
{
	for (int axis = 0; axis <= 4; ++axis) assert(integer_in_range(Json(axis), 0, 4));
	for (int slot = 0; slot <= 4; ++slot) assert(integer_in_range(Json(slot), 0, 4));
	for (int button : { 100, 101 }) assert(integer_in_range(Json(button), 100, 101));
	for (const Json &invalid : { Json(-1), Json(5), Json(10000000000LL), Json("2"), Json(2.0), Json(true), Json(nullptr) })
		assert(!integer_in_range(invalid, 0, 4));
	for (const Json &invalid : { Json(99), Json(102), Json("101"), Json(101.0), Json(true), Json(nullptr) })
		assert(!integer_in_range(invalid, 100, 101));
	for (double axis : { -1.0, 0.0, 0.25, 1.0 }) assert(axis_value_in_range(Json(axis)));
	for (const Json &invalid : { Json(-1.001), Json(1.001), Json("0.5"), Json(true), Json(nullptr),
	                             Json(std::numeric_limits<double>::infinity()), Json(std::numeric_limits<double>::quiet_NaN()) })
		assert(!axis_value_in_range(invalid));
	for (const Json &valid : { Json{ { "pressed", 0 } }, Json{ { "pressed", 1 } }, Json{ { "held", 0 } }, Json{ { "held", 1 } } })
		assert(secondary_pressed_value(valid));
	for (const Json &invalid : { Json::object(), Json{ { "held", 2 } }, Json{ { "pressed", -1 } }, Json{ { "held", true } },
	                             Json{ { "held", "1" } }, Json{ { "held", 1.0 } }, Json{ { "held", 2 }, { "pressed", 1 } } })
		assert(!secondary_pressed_value(invalid));
	// A press then release before any frame leaves a queued edge. Neither selector
	// may run until that edge has been consumed by an ordinary frame.
	bool secondary_held = false, pending = false;
	secondary_held = true;
	pending = true;
	secondary_held = false;
	assert(!selection_released(false, secondary_held, pending, true));
	assert(!selection_released(false, secondary_held, pending, false));
	pending = false;
	assert(selection_released(false, secondary_held, pending, true));
	assert(!selection_released(false, true, pending, true));
	assert(!selection_released(true, false, pending, true));
	// The real route runner must be idle; the metadata stub must be exactly
	// unsupported. Neither branch admits running, terminal, or unknown states.
	assert(route_status_allowed(0, 0, 6, true));
	assert(!route_status_allowed(1, 0, 6, true));
	assert(!route_status_allowed(2, 0, 6, true));
	assert(!route_status_allowed(6, 0, 6, true));
	assert(route_status_allowed(6, 0, 6, false));
	assert(!route_status_allowed(0, 0, 6, false));
	assert(!route_status_allowed(1, 0, 6, false));
	assert(!route_status_allowed(2, 0, 6, false));
	assert(!route_status_allowed(99, 0, 6, false));
}
