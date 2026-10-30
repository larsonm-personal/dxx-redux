#ifndef DXX_XL_AMBIENT_ACTIVATION_RECEIPT_H
#define DXX_XL_AMBIENT_ACTIVATION_RECEIPT_H
#include <array>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <vector>
namespace xl_ambient_activation_receipt
{
// Transport only; source and native consumers independently change these states
struct Case {
	int wall = -1;
	std::array<int, 3> action{}; // type, flags, value
	std::array<int, 4> result{}; // type, flags, value, operated
	std::vector<std::array<int, 2>> links;
	std::vector<std::array<int, 3>> walls, before_triggers, after_triggers;
	std::vector<std::array<int, 4>> before_objects, after_objects; // index, type, id, enabled
};
template <std::size_t N>
void write_rows(std::ostream &out, const std::vector<std::array<int, N>> &rows)
{
	out << rows.size() << '\n';
	for (const auto &row : rows) {
		for (int value : row) out << value << ' ';
		out << '\n';
	}
}
template <std::size_t N>
void read_rows(std::istream &in, std::vector<std::array<int, N>> &rows, unsigned limit)
{
	unsigned count = 0;
	if (!(in >> count) || count > limit) throw std::runtime_error("Invalid ambient activation receipt count");
	rows.resize(count);
	for (auto &row : rows)
		for (auto &value : row) in >> value;
}
inline void write(std::ostream &out, const Case &value)
{
	out << value.wall << '\n';
	for (int field : value.action) out << field << ' ';
	out << '\n';
	for (int field : value.result) out << field << ' ';
	out << '\n';
	write_rows(out, value.links);
	write_rows(out, value.walls);
	write_rows(out, value.before_triggers);
	write_rows(out, value.after_triggers);
	write_rows(out, value.before_objects);
	write_rows(out, value.after_objects);
}
inline Case read(std::istream &in)
{
	Case value;
	in >> value.wall;
	for (int &field : value.action) in >> field;
	for (int &field : value.result) in >> field;
	read_rows(in, value.links, 10);
	read_rows(in, value.walls, 10);
	read_rows(in, value.before_triggers, 254);
	read_rows(in, value.after_triggers, 254);
	read_rows(in, value.before_objects, 2000);
	read_rows(in, value.after_objects, 2000);
	if (!in) throw std::runtime_error("Truncated ambient activation receipt");
	return value;
}
} // namespace xl_ambient_activation_receipt
#endif
