#ifndef DXX_XL_COMPAT_AMBIENT_SOUND_H
#define DXX_XL_COMPAT_AMBIENT_SOUND_H

#include "inventory.h"
#include <limits>
#include <stdexcept>

namespace xl_compat
{
struct AmbientSound {
	std::string filename;
	int volume;
	bool enabled;
};

inline bool ambient_sound_shape(const Object &object)
{
	return object.type == 20 && object.reader_id == 2 && object.render == 13 && object.reader_render == 13 &&
	       object.control == 0 && object.movement == 0 && object.multiplayer == 0 && object.render_data.size() == 42;
}

inline int ambient_sound_volume(int serialized)
{
	// CObject::Read performs float arithmetic, then FRound, then integer conversion
	const float scaled = float(serialized) * 65536.0f / 10.0f;
	const float rounded = scaled < 0.0f ? scaled - 0.5f : scaled + 0.5f;
	if (double(rounded) < std::numeric_limits<int>::min() || double(rounded) > std::numeric_limits<int>::max())
		throw std::runtime_error("XL ambient sound volume exceeds the fixed-point runtime range");
	return int(rounded);
}

inline AmbientSound decode_ambient_sound(const Object &object)
{
	if (object.render != 13 || object.render_data.size() != 42)
		throw std::runtime_error("XL ambient sound object has an invalid payload layout");
	AmbientSound result{ {}, ambient_sound_volume(object.render_data[40]), object.render_data[41] != 0 };
	// The source forcibly terminates byte 39 before lowercasing the filename
	for (unsigned i = 0; i < 39 && object.render_data[i]; ++i) {
		const int byte = object.render_data[i];
		if (byte < 0 || byte > 255) throw std::runtime_error("XL ambient filename contains a non-byte value");
		result.filename.push_back(char(byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte));
	}
	return result;
}
} // namespace xl_compat
#endif
