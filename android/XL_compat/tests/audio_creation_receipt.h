// Text transport only; native and source consumers produce their own state maps
#ifndef DXX_XL_AUDIO_CREATION_RECEIPT_H
#define DXX_XL_AUDIO_CREATION_RECEIPT_H
#include <cstdint>
#include <istream>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
namespace xl_audio_creation_receipt
{
using Fields = std::map<std::string, std::int64_t>;
inline void write(std::ostream &stream, const Fields &fields)
{
	stream << fields.size() << '\n';
	for (const auto &[name, value] : fields) stream << name << ' ' << value << '\n';
}
inline Fields read(std::istream &stream)
{
	unsigned count = 0;
	if (!(stream >> count) || count > 128) throw std::runtime_error("Invalid creation receipt field count");
	Fields fields;
	for (unsigned i = 0; i < count; ++i) {
		std::string name;
		std::int64_t value;
		if (!(stream >> name >> value) || !fields.emplace(name, value).second) throw std::runtime_error("Invalid creation receipt field");
	}
	return fields;
}
} // namespace xl_audio_creation_receipt
#endif
