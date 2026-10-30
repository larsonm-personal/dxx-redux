#ifndef DXX_XL_STATIC_AUDIO_RECEIPT_H
#define DXX_XL_STATIC_AUDIO_RECEIPT_H
#include <array>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace xl_static_audio_receipt
{
// Transport only; both consumers check this table size against their engine
struct Case {
	int low_memory = 0, named_sample = -1;
	std::array<int, 254> sounds{}, alternate{};
	// Index, authored ID/render/clip, loaded render/clip
	std::vector<std::array<int, 6>> objects;
	// Object, physical sound, class, gain, range, flags, loop bounds, decay, external, ambient
	std::vector<std::array<int, 11>> owners;
};
inline void write(std::ostream &out, const Case &value)
{
	out << "XL_STATIC_AUDIO 1\n"
	    << value.low_memory << ' ' << value.named_sample << '\n';
	for (const auto &table : { value.sounds, value.alternate }) {
		for (int number : table) out << number << ' ';
		out << '\n';
	}
	out << value.objects.size() << '\n';
	for (const auto &row : value.objects) {
		for (int number : row) out << number << ' ';
		out << '\n';
	}
	out << value.owners.size() << '\n';
	for (const auto &row : value.owners) {
		for (int number : row) out << number << ' ';
		out << '\n';
	}
}
inline Case read(std::istream &in)
{
	Case value;
	std::string tag;
	unsigned version = 0, count = 0;
	if (!(in >> tag >> version >> value.low_memory >> value.named_sample) || tag != "XL_STATIC_AUDIO" || version != 1 ||
	    value.low_memory < 0 || value.low_memory > 1 || value.named_sample < -1 || value.named_sample > 255)
		throw std::runtime_error("Invalid static audio receipt");
	for (auto *table : { &value.sounds, &value.alternate })
		for (int &number : *table)
			if (!(in >> number) || number < 0 || number > 255) throw std::runtime_error("Invalid static sound table");
	if (!(in >> count) || count > 2000) throw std::runtime_error("Invalid static object count");
	value.objects.resize(count);
	for (auto &row : value.objects)
		for (int &number : row) in >> number;
	if (!(in >> count) || count > 500) throw std::runtime_error("Invalid static owner count");
	value.owners.resize(count);
	for (auto &row : value.owners)
		for (int &number : row) in >> number;
	if (!in || in >> tag) throw std::runtime_error("Incomplete or trailing static audio receipt");
	return value;
}
} // namespace xl_static_audio_receipt
#endif
