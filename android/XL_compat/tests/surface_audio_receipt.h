#ifndef DXX_XL_SURFACE_AUDIO_RECEIPT_H
#define DXX_XL_SURFACE_AUDIO_RECEIPT_H
#include <array>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <vector>
namespace xl_surface_audio_receipt
{
// Transport only; source and native consumers independently produce owners
using Face = std::array<int, 7>;   // flags, base, overlay, child, x, y, z
using Owner = std::array<int, 13>; // sound, segment, side, gain, range, flags, class, decay, loop bounds, xyz
struct Case {
	int custom_volume = 65536;
	std::vector<int> sounds, textures, effects;
	std::vector<std::array<Face, 6>> segments;
	std::vector<Owner> owners;
};
inline void write(std::ostream &out, const Case &value)
{
	out << value.custom_volume << '\n';
	for (const auto *values : { &value.sounds, &value.textures, &value.effects }) {
		out << values->size();
		for (int number : *values) out << ' ' << number;
		out << '\n';
	}
	out << value.segments.size() << '\n';
	for (const auto &segment : value.segments)
		for (const auto &face : segment) {
			for (int number : face) out << number << ' ';
			out << '\n';
		}
	out << value.owners.size() << '\n';
	for (const auto &owner : value.owners) {
		for (int number : owner) out << number << ' ';
		out << '\n';
	}
}
inline Case read(std::istream &in)
{
	Case value;
	in >> value.custom_volume;
	for (auto *values : { &value.sounds, &value.textures, &value.effects }) {
		unsigned count = 0;
		if (!(in >> count) || count > 1200) throw std::runtime_error("Invalid surface table receipt");
		values->resize(count);
		for (auto &number : *values) in >> number;
	}
	unsigned count = 0;
	if (!(in >> count) || !count || count > 9000) throw std::runtime_error("Invalid surface geometry receipt");
	value.segments.resize(count);
	for (auto &segment : value.segments)
		for (auto &face : segment)
			for (int &number : face) in >> number;
	if (!(in >> count) || count > 500) throw std::runtime_error("Invalid surface owner receipt");
	value.owners.resize(count);
	for (auto &owner : value.owners)
		for (int &number : owner) in >> number;
	if (!in) throw std::runtime_error("Incomplete surface receipt");
	return value;
}
} // namespace xl_surface_audio_receipt
#endif
