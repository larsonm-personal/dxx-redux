#ifndef DXX_XL_PARTICLE_WORLD_RECEIPT_H
#define DXX_XL_PARTICLE_WORLD_RECEIPT_H
#include "../particle_world.h"
#include <istream>
#include <ostream>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_particle_world_receipt
{
struct Query {
	xl_compat::ParticlePoint position{}, normal{};
	int start = -1, located = -1, after = -1;
	bool skybox = false, hit = false;
};
struct Case {
	xl_compat::Inventory source;
	xl_compat::ParticleWorld world;
	std::vector<Query> queries;
};
template <typename Row>
void write_row(std::ostream &out, const Row &row)
{
	for (const auto &value : row) out << value << ' ';
	out << '\n';
}
template <typename Row>
void read_row(std::istream &in, Row &row)
{
	for (auto &value : row) in >> value;
}
inline void write(std::ostream &out, const Case &value)
{
	out << value.source.level_version << ' ' << value.source.vertices.size() << ' ' << value.source.segments.size() << ' ' << value.queries.size() << '\n';
	for (const auto &vertex : value.source.vertices) write_row(out, vertex);
	for (std::size_t i = 0; i < value.source.segments.size(); ++i) {
		const auto &source = value.source.segments[i];
		const auto &compiled = value.world.segments.at(i);
		out << source.function << '\n';
		write_row(out, source.vertices);
		write_row(out, source.children);
		write_row(out, compiled.center);
		for (const auto &bound : compiled.bounds) write_row(out, bound);
		out << compiled.inner_radius << ' ' << compiled.outer_radius << ' ' << compiled.skybox << '\n';
		for (const auto &side : compiled.sides) {
			out << side.faces << ' ' << side.child << ' ' << side.pokes_out << '\n';
			write_row(out, side.anchor);
			for (const auto &normal : side.normals) write_row(out, normal);
		}
	}
	for (const auto &q : value.queries) {
		out << q.start << ' ' << q.skybox << ' ' << q.located << ' ' << q.hit << ' ' << q.after << '\n';
		write_row(out, q.position);
		write_row(out, q.normal);
	}
}
inline Case read(std::istream &in)
{
	Case value;
	unsigned vertices = 0, segments = 0, queries = 0;
	in >> value.source.level_version >> vertices >> segments >> queries;
	if (!in || !vertices || vertices > 65535 || !segments || segments > 32767 || queries > 1000000) throw std::runtime_error("Invalid particle world receipt counts");
	value.source.vertices.resize(vertices);
	value.source.segments.resize(segments);
	value.world.segments.resize(segments);
	value.queries.resize(queries);
	for (auto &vertex : value.source.vertices) read_row(in, vertex);
	for (unsigned i = 0; i < segments; ++i) {
		auto &source = value.source.segments[i];
		auto &compiled = value.world.segments[i];
		in >> source.function;
		read_row(in, source.vertices);
		read_row(in, source.children);
		for (unsigned side = 0; side < 6; ++side) source.sides[side].corners = xl_compat::classic_side_corners[side];
		read_row(in, compiled.center);
		for (auto &bound : compiled.bounds) read_row(in, bound);
		in >> compiled.inner_radius >> compiled.outer_radius >> compiled.skybox;
		for (auto &side : compiled.sides) {
			in >> side.faces >> side.child >> side.pokes_out;
			read_row(in, side.anchor);
			for (auto &normal : side.normals) read_row(in, normal);
		}
	}
	for (auto &q : value.queries) {
		in >> q.start >> q.skybox >> q.located >> q.hit >> q.after;
		read_row(in, q.position);
		read_row(in, q.normal);
	}
	if (!in) throw std::runtime_error("Truncated particle world receipt");
	value.world.build_grids();
	return value;
}
} // namespace xl_particle_world_receipt
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
