#ifndef DXX_XL_OOF_METADATA_CONTROLS_H
#define DXX_XL_OOF_METADATA_CONTROLS_H
#include "oof_fixture.h"
#include "../oof_mesh.h"
#include "../oof_geometry.h"
namespace xl_oof_test
{
inline void patch32(Bytes &b, std::size_t at, uint32_t n)
{
	for (int i = 0; i < 4; ++i) b.at(at + i) = uint8_t(n >> (8 * i));
}
inline bool same_mesh(const xl_compat::OofMesh &a, const xl_compat::OofMesh &b)
{
	if (a.low != b.low || a.high != b.high || a.offsets != b.offsets || a.spinning_parts != b.spinning_parts || a.faces.size() != b.faces.size()) return false;
	for (std::size_t i = 0; i < a.faces.size(); ++i) {
		const auto &x = a.faces[i];
		const auto &y = b.faces[i];
		if (x.submodel != y.submodel || x.texture != y.texture || x.collision != y.collision || x.normal != y.normal || x.triangles.size() != y.triangles.size()) return false;
		for (std::size_t j = 0; j < x.triangles.size(); ++j) {
			const auto &p = x.triangles[j];
			const auto &q = y.triangles[j];
			if (p.position != q.position || p.normal != q.normal || p.uv != q.uv || p.color != q.color) return false;
		}
	}
	return true;
}
inline void metadata_controls()
{
	const auto require = [](bool ok, const char *why) {if(!ok)throw std::runtime_error(why); };
	const auto base = fixture(-1, 2, 0, "", 1, 1);
	const auto original = xl_compat::compile_oof_mesh(xl_compat::read_oof(base), false);
	const auto rejects = [&](const Bytes &b) {bool failed=false;try{(void)xl_compat::compile_oof_mesh(xl_compat::read_oof(b),false);}catch(const std::runtime_error&){failed=true;}require(failed,"Malformed or unsupported OOF metadata admitted"); };
	for (int variant : { 0, 1 }) {
		const auto bytes = metadata_fixture(variant);
		const auto parsed = xl_compat::read_oof(bytes);
		require(parsed.special_points.size() == 1 && parsed.special_points[0].name == "$dummy" && parsed.special_points[0].properties.empty(), "Padded metadata strings lost source meaning");
		require(parsed.additional_chunks[0].bytes == special_points() && parsed.additional_chunks[1].bytes == rotation_tracks(1, variant), "Raw metadata bytes changed");
		require(parsed.rotation_tracks.size() == 1 && parsed.rotation_tracks[0].frames.size() == 2, "Timed rotation rows lost");
		require(parsed.rotation_tracks[0].frames[0].axis == xl_compat::OofVector{ variant ? 3.0f : -1.0f, variant ? 4.0f : 0.0f, 0.0f }, "Raw rotation axis changed");
		const auto mesh = xl_compat::compile_oof_mesh(parsed, false);
		require(same_mesh(mesh, original) && xl_compat::oof_native_geometry(mesh) == xl_compat::oof_native_geometry(original), "Metadata changed existing static render/collision output");
	}
	for (const std::string tag : { "SPCL", "RANI" }) {
		const auto payload = tag == "SPCL" ? special_points() : rotation_tracks();
		for (std::size_t end = 0; end < payload.size(); ++end) {
			auto b = base;
			chunk(b, tag.c_str(), Bytes(payload.begin(), payload.begin() + end));
			rejects(b);
		}
		auto b = base;
		chunk(b, tag.c_str(), payload);
		chunk(b, tag.c_str(), payload);
		rejects(b);
		auto extra = payload;
		extra.push_back(0);
		b = base;
		chunk(b, tag.c_str(), extra);
		rejects(b);
	}
	const auto bad_special = [&](std::size_t at, uint32_t value) {auto p=special_points();patch32(p,at,value);auto b=base;chunk(b,"SPCL",p);rejects(b); };
	bad_special(0, 0xffffffff);
	bad_special(0, 4097);
	bad_special(4, 4097);
	bad_special(16, 4097);
	bad_special(23, 0x7fc00000);
	bad_special(35, 0x7f800000);
	bad_special(35, 0xbf800000);
	{
		auto p = special_points();
		p[15] = 'x';
		auto b = base;
		chunk(b, "SPCL", p);
		rejects(b);
	}
	const auto bad_rotation = [&](std::size_t at, uint32_t value) {auto p=rotation_tracks();patch32(p,at,value);auto b=base;chunk(b,"RANI",p);rejects(b); };
	bad_rotation(0, 0xffffffff);
	bad_rotation(0, 257);
	bad_rotation(8, 0xffffffff);
	bad_rotation(8, 1048576);
	bad_rotation(4, 0x80000000);
	bad_rotation(12, 2);
	bad_rotation(32, 0xffffffff);
	bad_rotation(16, 0x7fc00000);
	bad_rotation(16, 0x7f7fffff);
	{
		auto p = rotation_tracks();
		for (std::size_t at : { 16, 20, 24 }) patch32(p, at, 0);
		auto b = base;
		chunk(b, "RANI", p);
		require(xl_compat::read_oof(b).rotation_tracks[0].frames[0].normalized_axis == xl_compat::OofVector{}, "Zero axis was invented");
	}
	for (const char *tag : { "PANI", "ANIM", "ATCH", "WBAT", "NATH" }) {
		auto b = base;
		chunk(b, tag, Bytes{ 0, 0, 0, 0 });
		rejects(b);
	}
	for (const char *property : { "$rotate=1", "$turret=1", "$facing" }) {
		auto b = fixture(-1, 2, 0, property, 1, 1);
		chunk(b, "RANI", rotation_tracks());
		rejects(b);
	}
	{
		auto b = fixture(-1, 2, 0, "", 0.5f, 1);
		chunk(b, "SPCL", special_points());
		rejects(b);
	}
	{
		auto m = xl_compat::read_oof(metadata_fixture());
		m.submodels[0].facing_lists.push_back(0);
		bool failed = false;
		try {
			(void) xl_compat::compile_oof_mesh(m, false);
		} catch (const std::runtime_error &) {
			failed = true;
		}
		require(failed, "Facing list bypassed metadata guard");
	}
	{
		Bytes b = base; // Resource strings still reject embedded/padded NULs
		const std::string name = "owned-skin";
		const auto at = std::search(b.begin(), b.end(), name.begin(), name.end());
		require(at != b.end(), "Missing resource string control");
		*at = 0;
		rejects(b);
	}
}
} // namespace xl_oof_test
#endif
