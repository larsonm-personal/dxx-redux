#ifndef DXX_XL_OOF_FIXTURE_H
#define DXX_XL_OOF_FIXTURE_H
#include "../oof.h"
namespace xl_oof_test
{
using Bytes = std::vector<uint8_t>;
static void integer(Bytes &out, uint32_t value)
{
	for (int i = 0; i < 4; ++i) out.push_back(uint8_t(value >> (i * 8)));
}
static void real(Bytes &out, float value)
{
	uint32_t bits;
	std::memcpy(&bits, &value, 4);
	integer(out, bits);
}
static void vector(Bytes &out, float x = 0, float y = 0, float z = 0)
{
	real(out, x);
	real(out, y);
	real(out, z);
}
static void string(Bytes &out, const std::string &value)
{
	integer(out, uint32_t(value.size()));
	out.insert(out.end(), value.begin(), value.end());
}
static void chunk(Bytes &out, const char *type, const Bytes &payload)
{
	out.insert(out.end(), type, type + 4);
	integer(out, uint32_t(payload.size()));
	out.insert(out.end(), payload.begin(), payload.end());
}
static Bytes fixture(int parent = -1, int vertex = 2, int texture = 0, const std::string &properties = "$facing", float middle_alpha = 0.5f, float last_alpha = 0.0f)
{
	Bytes result{ 'P', 'S', 'P', 'O' };
	integer(result, 2300);
	Bytes textures;
	integer(textures, 1);
	string(textures, "owned-skin");
	chunk(result, "TXTR", textures);
	Bytes header;
	integer(header, 1);
	real(header, 2);
	vector(header);
	vector(header, 1, 1, 0);
	integer(header, 0);
	chunk(result, "OHDR", header);
	Bytes object;
	integer(object, 0);
	integer(object, uint32_t(parent));
	vector(object, 0, 0, 1);
	real(object, 0);
	vector(object);
	vector(object, 2, 3, 4);
	real(object, 2);
	integer(object, 0);
	integer(object, 0);
	vector(object);
	string(object, "root");
	string(object, properties);
	integer(object, 0);
	integer(object, 0);
	integer(object, 0);
	integer(object, 3);
	vector(object);
	vector(object, 1, 0, 0);
	vector(object, 0, 1, 0);
	for (int i = 0; i < 3; ++i) vector(object, 0, 0, 1);
	real(object, 1);
	real(object, middle_alpha);
	real(object, last_alpha);
	integer(object, 1);
	vector(object, 0, 0, 1);
	integer(object, 3);
	integer(object, 1);
	integer(object, uint32_t(texture));
	for (int i : { 0, 1, vertex }) {
		integer(object, uint32_t(i));
		real(object, float(i));
		real(object, float(i) / 2);
	}
	real(object, 1);
	real(object, 1);
	chunk(result, "SOBJ", object);
	return result;
}
inline Bytes special_points()
{
	Bytes b;
	integer(b, 1);
	string(b, std::string("$dummy\0\0", 8));
	string(b, std::string(3, '\0'));
	vector(b);
	real(b, 0.8660253882408142f);
	return b;
}
inline Bytes rotation_tracks(unsigned parts = 1, int variant = 0)
{
	Bytes b;
	for (unsigned part = 0; part < parts; ++part) {
		integer(b, 2);
		integer(b, variant ? -20 : 0);
		integer(b, variant ? 40 : 1);
		integer(b, variant ? -20 : 0);
		vector(b, variant ? 3 : -1, variant ? 4 : 0, 0);
		integer(b, variant ? -2147483647 : 0);
		integer(b, variant ? 40 : 1);
		vector(b, 0, 0, -1);
		integer(b, variant ? 2147483647 : 65536);
	}
	return b;
}
inline Bytes metadata_fixture(int variant = 0)
{
	auto b = fixture(-1, 2, 0, "", 1.0f, 1.0f);
	chunk(b, "SPCL", special_points());
	chunk(b, "RANI", rotation_tracks(1, variant));
	return b;
}
} // namespace xl_oof_test
#endif
