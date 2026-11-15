#ifndef DXX_XL_NAMED_ACTION_RECEIPT_H
#define DXX_XL_NAMED_ACTION_RECEIPT_H
#include <array>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace xl_named_action_receipt
{
// Transport only: repeat count, disabled, game time, prior operated/cooldown/playing,
// accepted, resulting operated/cooldown/playing, actual failed output requests
struct Case {
	std::array<int, 11> values{};
	std::string filename;
};
inline void write(std::ostream &out, const std::vector<Case> &cases)
{
	out << "XL_NAMED_ACTION 1 " << cases.size() << '\n';
	for (const auto &row : cases) {
		for (int value : row.values) out << value << ' ';
		out << std::quoted(row.filename) << '\n';
	}
}
inline std::vector<Case> read(std::istream &in)
{
	std::string tag;
	unsigned version = 0, count = 0;
	if (!(in >> tag >> version >> count) || tag != "XL_NAMED_ACTION" || version != 1 || !count || count > 128)
		throw std::runtime_error("Invalid named action receipt");
	std::vector<Case> cases(count);
	for (auto &row : cases) {
		for (int &value : row.values) in >> value;
		in >> std::quoted(row.filename);
	}
	if (!in || in >> tag) throw std::runtime_error("Incomplete or trailing named action receipt");
	return cases;
}
} // namespace xl_named_action_receipt
#endif
