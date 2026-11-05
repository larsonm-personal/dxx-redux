#ifdef _MSC_VER
// The native engine leaves packed legacy structures active at include sites
#pragma pack(push, 1)
#endif
#include "../oof.h"
#include "../oof_mesh.h"
#ifdef _MSC_VER
struct PackingRestored {
	char byte;
	int integer;
};
static_assert(sizeof(PackingRestored) == 5, "OOF headers must restore the caller packing");
#pragma pack(pop)
#endif
static_assert(offsetof(xl_compat::OofRenderFace, triangles) == 24, "OOF mesh layout must not depend on legacy header order");
#include "../oof_geometry.h"
#include "../model_geometry.h"
#include <fstream>
#include <iostream>
#include <iterator>

using Bytes = std::vector<uint8_t>;
static void check(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
#include "oof_fixture.h"
#include "oof_metadata_controls.h"
using namespace xl_oof_test;
static bool rejected(const Bytes &bytes)
{
	try {
		(void) xl_compat::read_oof(bytes);
		return false;
	} catch (const std::runtime_error &) {
		return true;
	}
}
int main(int argc, char **argv)
{
	try {
		metadata_controls();
		const auto bytes = fixture();
		const auto model = xl_compat::read_oof(bytes);
		const auto &part = model.submodels.at(0);
		check(model.textures.at(0) == "owned-skin" && part.offset == xl_compat::OofVector{ 2, 3, 4 } &&
		          part.properties == "$facing" && part.alpha.at(1) == 0.5f && part.faces.at(0).corners.at(2).v == 1,
		      "OOF decoder lost authored geometry/material data");
		for (std::size_t size = 0; size < bytes.size(); ++size)
			check(rejected(Bytes(bytes.begin(), bytes.begin() + size)), "Truncated OOF was accepted");
		check(rejected(fixture(1)) && rejected(fixture(-1, 3)) && rejected(fixture(-1, 2, 1)), "Invalid OOF references accepted");
		auto static_model = model;
		static_model.submodels.at(0).properties.clear();
		const auto mesh = xl_compat::compile_oof_mesh(static_model, true);
		check(mesh.low == xl_compat::OofVector{ 2, 3, 4 } && mesh.high == xl_compat::OofVector{ 3, 4, 4 } &&
		          mesh.faces.at(0).triangles.at(1).color[3] == 0.5f && mesh.faces.at(0).triangles.at(2).uv[1] == -1,
		      "OOF mesh lost transformed bounds, alpha or UV direction");
		const auto geometry = xl_compat::oof_native_geometry(mesh);
		const auto bounds = xl_compat::ModelGeometry(geometry.data(), geometry.size(), 1).hitbox_bounds();
		check(bounds[0].low == std::array<int32_t, 3>{ 2 * 65536, 3 * 65536, 4 * 65536 } &&
		          bounds[0].high == std::array<int32_t, 3>{ 3 * 65536, 4 * 65536, 4 * 65536 },
		      "Native OOF geometry lost authored bounds");
		{
			xl_compat::OofMesh large;
			for (int i = 0; i < 400; ++i) {
				auto face = mesh.faces.at(0);
				for (auto &vertex : face.triangles) vertex.position[0] += float(i * 2);
				large.faces.push_back(std::move(face));
			}
			const auto payload = xl_compat::oof_native_geometry(large);
			const auto word = [&](std::size_t at) { return unsigned(payload.at(at)) | unsigned(payload.at(at + 1)) << 8; };
			const auto integer_at = [&](std::size_t at) { return int32_t(word(at) | uint32_t(word(at + 2)) << 16); };
			std::vector<std::array<int32_t, 3>> bank;
			unsigned banks = 0, triangles = 0;
			std::size_t at = 0;
			while (word(at)) {
				if (word(at) == 1) {
					bank.resize(word(at + 2));
					check(!bank.empty() && bank.size() <= 1000, "Native geometry batch exceeds scratch bank");
					at += 4;
					for (auto &point : bank)
						for (auto &axis : point) {
							axis = integer_at(at);
							at += 4;
						}
					++banks;
				} else {
					check(word(at) == 2 && word(at + 2) == 3, "Unexpected native geometry opcode");
					for (unsigned corner = 0; corner < 3; ++corner)
						for (unsigned axis = 0; axis < 3; ++axis)
							check(bank.at(word(at + 30 + corner * 2))[axis] == int32_t(large.faces.at(triangles).triangles.at(corner).position[axis] * 65536),
							      "Native geometry batching changed authored triangle vertices");
					++triangles;
					at += 36;
				}
			}
			check(banks > 1 && triangles == large.faces.size() && at + 2 == payload.size(), "Native geometry batching lost faces or termination");
			check(xl_compat::ModelGeometry(payload.data(), payload.size(), 1).custom_radius() > 0, "Batched geometry has no native radius");
		}
		auto extended = bytes;
		chunk(extended, "PANI", Bytes{ 1, 2, 3 });
		check(xl_compat::read_oof(extended).additional_chunks.at(0).bytes == Bytes({ 1, 2, 3 }), "OOF extension silently discarded");
		for (int i = 1; i < argc; ++i) {
			std::ifstream file(argv[i], std::ios::binary);
			check(bool(file), "Cannot open original OOF");
			const auto original = xl_compat::read_oof(Bytes(std::istreambuf_iterator<char>(file), {}));
			const auto original_mesh = xl_compat::compile_oof_mesh(original, false);
			check(!original_mesh.faces.empty(), "Original OOF has no compiled faces");
			const auto payload = xl_compat::oof_native_geometry(original_mesh);
			check(xl_compat::ModelGeometry(payload.data(), payload.size(), 1).custom_radius() > 0, "Original OOF has no native collision radius");
			std::cout << argv[i] << ": " << original.submodels.size() << " submodels, " << original.textures.size() << " textures\n";
			for (const auto &entry : original.submodels) std::cout << "  " << entry.first << " parent=" << entry.second.parent << " properties=" << entry.second.properties << '\n';
		}
		std::cout << "OOF decoder controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
