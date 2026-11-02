#ifndef DXX_XL_STATIC_CONTACT_H
#define DXX_XL_STATIC_CONTACT_H
#include "model_hitbox_query.h"
#include <optional>
namespace xl_compat
{
struct StaticContactResult {
	FixedLightPoint velocity{}, rotation_force{};
	bool apply = false;
};
// Source BumpTwoObjects with a stationary static first actor and default hit angles
inline StaticContactResult static_contact(const FixedLightPoint &position, int mass,
                                          const FixedLightPoint &actor_position, FixedLightPoint velocity,
                                          int actor_mass, bool player, bool persistent, bool boosted,
                                          std::optional<FixedLightPoint> normal = {})
{
	StaticContactResult result{ velocity, {}, !persistent && !boosted };
	if (!result.apply) return result;
	if (mass < 0 || actor_mass < 0) throw std::runtime_error("Static contact mass is negative");
	const int sum = hitbox_fixed(double(mass) + actor_mass);
	const int difference = hitbox_fixed(double(actor_mass) - mass);
	const auto scale = [](const FixedLightPoint &v, int amount) {
		FixedLightPoint out{};
		for (unsigned i = 0; i < 3; ++i) out[i] = hitbox_fixed(double(amount) / 65536.0 * v[i]);
		return out;
	};
	FixedLightPoint force{}, rotation{};
	if (velocity != FixedLightPoint{}) {
		const auto direction = normal ? *normal : geometry_default_direction(hitbox_difference(position, actor_position));
		const int magnitude = geometry_default_magnitude(velocity);
		const auto movement = geometry_default_direction(velocity);
		const int dot = hitbox_dot(movement, direction);
		if (dot >= 0) {
			const int limited = (std::min) (65536, dot);
			force = scale(direction, hitbox_fixed(double(limited) / 65536.0 * magnitude));
			rotation = scale(movement, hitbox_fixed(double(65536 - limited) / 65536.0 * magnitude));
			velocity = hitbox_difference(velocity, force);
		}
	}
	auto response = scale(force, difference);
	rotation = scale(rotation, difference);
	for (auto &axis : response) axis = hitbox_fixed(sum ? double(axis) / sum * 65536.0 : 1);
	if (player) {
		response = scale(response, 65536 / 4);
		rotation = scale(rotation, 65536 / 4);
	}
	for (unsigned i = 0; i < 3; ++i) result.velocity[i] = hitbox_fixed(double(velocity[i]) + response[i]);
	result.rotation_force = rotation;
	return result;
}
} // namespace xl_compat
#endif
