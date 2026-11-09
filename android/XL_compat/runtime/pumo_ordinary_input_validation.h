#pragma once

#include <cmath>

namespace pumo_ordinary_input_validation
{
template <typename Json>
bool integer_in_range(const Json &value, int low, int high)
{
	return value.is_number_integer() && value >= low && value <= high;
}

template <typename Json>
bool axis_value_in_range(const Json &value)
{
	if (!value.is_number()) return false;
	const double axis = value.template get<double>();
	return std::isfinite(axis) && axis >= -1 && axis <= 1;
}

template <typename Json>
bool secondary_pressed_value(const Json &step)
{
	const char *key = step.contains("held") ? "held" : "pressed";
	return step.contains(key) && integer_in_range(step.at(key), 0, 1);
}

inline bool selection_released(bool primary_held, bool secondary_held, bool secondary_press_pending, bool secondary_slot)
{
	return !primary_held && !secondary_press_pending && (!secondary_slot || !secondary_held);
}

inline bool route_status_allowed(int status, int idle, int unsupported, bool full_runner)
{
	return status == (full_runner ? idle : unsupported);
}
} // namespace pumo_ordinary_input_validation
