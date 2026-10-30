#ifndef DXX_XL_AMBIENT_AUDIO_RECEIPT_H
#define DXX_XL_AMBIENT_AUDIO_RECEIPT_H
#include <array>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace xl_ambient_audio_receipt
{
// Transport only: decoded inputs and independently captured runtime owners
struct Object {
	int index = 0, id = 2, volume = 0, enabled = 0;
	std::string filename;
};
// object, sound, class, gain, range, flags, loop bounds, decay, external, ambient
using Owner = std::pair<std::array<int, 11>, std::string>;
struct Case {
	std::vector<Object> objects;
	std::vector<Owner> owners;
};
inline void write(std::ostream &out, const Case &value)
{
	out << "XL_AMBIENT_AUDIO 1\n"
	    << value.objects.size() << '\n';
	for (const auto &object : value.objects)
		out << object.index << ' ' << object.id << ' ' << object.volume << ' ' << object.enabled << ' ' << std::quoted(object.filename) << '\n';
	out << value.owners.size() << '\n';
	for (const auto &owner : value.owners) {
		for (int number : owner.first) out << number << ' ';
		out << std::quoted(owner.second) << '\n';
	}
}
inline Case read(std::istream &in)
{
	std::string tag;
	unsigned version = 0, count = 0;
	if (!(in >> tag >> version >> count) || tag != "XL_AMBIENT_AUDIO" || version != 1 || count > 2000)
		throw std::runtime_error("Invalid ambient audio receipt");
	Case value;
	value.objects.resize(count);
	for (auto &object : value.objects)
		in >> object.index >> object.id >> object.volume >> object.enabled >> std::quoted(object.filename);
	if (!(in >> count) || count > 500) throw std::runtime_error("Invalid ambient owner receipt");
	value.owners.resize(count);
	for (auto &owner : value.owners) {
		for (int &number : owner.first) in >> number;
		in >> std::quoted(owner.second);
	}
	if (!in || in >> tag) throw std::runtime_error("Incomplete or trailing ambient receipt");
	return value;
}
} // namespace xl_ambient_audio_receipt
#endif
