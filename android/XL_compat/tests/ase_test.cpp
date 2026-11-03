#include "../ase.h"
#include "../ase_model.h"
#include "../ase_mesh.h"
#include "../oof_geometry.h"
#include "../model_spin.h"
#include "../model_names.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

static void check(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
static bool rejected(const std::string &text)
{
	try {
		xl_compat::read_ase_records(text);
		return false;
	} catch (const std::runtime_error &) {
		return true;
	}
}
static void model_controls()
{
	const std::string source = R"ASE(*3DSMAX_ASCIIEXPORT 200
*MATERIAL_LIST {
*MATERIAL_COUNT 1
*MATERIAL 0 {
*MATERIAL_NAME "original material"
*MATERIAL_DIFFUSE 0.25 0.5 0.75
*MATERIAL_TRANSPARENCY 0.125
*MAP_DIFFUSE {
*BITMAP "owned.tga"
}
}
}
*GEOMOBJECT {
*NODE_NAME "original part"
*MATERIAL_REF 0
*MESH {
*MESH_NUMVERTEX 3
*MESH_NUMFACES 1
*MESH_NUMTVERTEX 3
*MESH_NUMTVFACES 1
*MESH_VERTEX_LIST {
*MESH_VERTEX 0 1 2 3
*MESH_VERTEX 1 4 5 6
*MESH_VERTEX 2 7 8 9
}
*MESH_FACE_LIST {
*MESH_FACE 0: A: 0 B: 1 C: 2 *MESH_MTLID 0
}
*MESH_TVERTLIST {
*MESH_TVERT 0 0.25 0.75 0
*MESH_TVERT 1 0.5 0.5 0
*MESH_TVERT 2 1 0 0
}
*MESH_TFACELIST {
*MESH_TFACE 0 0 1 2
}
*MESH_NORMALS {
*MESH_FACENORMAL 0 0 1 0
*MESH_VERTEXNORMAL 0 0 1 0
*MESH_VERTEXNORMAL 1 0 1 0
*MESH_VERTEXNORMAL 2 0 1 0
}
}
}
)ASE";
	const auto model = xl_compat::decode_ase_model(xl_compat::read_ase_records(source));
	const auto &part = model.submodels.at(0);
	const auto mesh = xl_compat::compile_ase_mesh(model);
	auto marked = model;
	auto marker = marked.submodels.front();
	marker.name = "$gunpnt-2";
	for (auto &v : marker.vertices)
		for (float &axis : v) axis += 100;
	marked.submodels.push_back(marker);
	const auto marked_mesh = xl_compat::compile_ase_mesh(marked);
	check(marked_mesh.geometry.faces.size() == mesh.geometry.faces.size() &&
	          marked_mesh.geometry.low == mesh.geometry.low && marked_mesh.geometry.high == mesh.geometry.high &&
	          marked_mesh.gunpoints.at(2) == xl_compat::OofVector{ 104, 106, 95 },
	      "ASE gunpoint marker changed visible/collision geometry or lost authored center");
	marked.submodels.push_back(marker);
	bool duplicate_marker = false;
	try {
		xl_compat::compile_ase_mesh(marked);
	} catch (const std::runtime_error &) {
		duplicate_marker = true;
	}
	check(duplicate_marker, "Duplicate ASE gunpoint accepted");
	for (const auto &name : { "$thruster", "$thruster-RXX-00", "$thruster-RLT-00" }) {
		auto thrust_model = model;
		marker.name = name;
		thrust_model.submodels.push_back(marker);
		const auto thrust_mesh = xl_compat::compile_ase_mesh(thrust_model);
		const bool visible = std::string(name) == "$thruster-RXX-00";
		check(thrust_mesh.thrusters.size() == 1 && thrust_mesh.geometry.faces.size() == mesh.geometry.faces.size() + unsigned(visible) &&
		          thrust_mesh.geometry.spinning_parts.size() == unsigned(visible) &&
		          thrust_mesh.geometry.low == mesh.geometry.low && thrust_mesh.geometry.high == mesh.geometry.high &&
		          xl_compat::oof_native_geometry(thrust_mesh.geometry) == xl_compat::oof_native_geometry(mesh.geometry),
		      "ASE thruster changed collision body or source visibility");
	}
	xl_compat::OofRenderVertex spinning;
	spinning.position = { 3, 3, 4 };
	spinning.normal = { 1, 0, 0 };
	xl_compat::spin_model_vertex(spinning, { 2, 3, 4 }, 8);
	check(std::abs(spinning.position[0] - 2) < 0.00001f && std::abs(spinning.position[1] - 2) < 0.00001f &&
	          std::abs(spinning.normal[0]) < 0.00001f && std::abs(spinning.normal[1] + 1) < 0.00001f,
	      "ASE thruster spin differs from source direction, center or frame phase");
	check(mesh.geometry.faces.at(0).triangles.at(0).position == xl_compat::OofVector{ 1, 3, -2 } &&
	          mesh.geometry.faces.at(0).triangles.at(0).color[3] == 0.875f && mesh.textures.at(0) == "owned.tga",
	      "ASE mesh lost geometry or material data");
	check(part.vertices.at(0) == xl_compat::AseVector{ 1, 3, -2 } &&
	          part.faces.at(0).normal == xl_compat::AseVector{ 0, 0, -1 } &&
	          part.texcoords.at(0) == std::array<float, 2>{ 0.25f, -0.75f } &&
	          part.faces.at(0).vertices == std::array<int, 3>{ 0, 1, 2 } &&
	          model.materials.at(0).texture == "owned.tga" && model.materials.at(0).transparency == 0.125f,
	      "ASE decoding changed coordinate, normal, UV, face or material data");
	for (const auto &mutation : std::vector<std::pair<std::string, std::string>>{
	         { "C: 2", "C: 3" }, { "*MESH_VERTEX 2", "*MESH_VERTEX 1" }, { "1 2 3", "nan 2 3" }, { "*MESH_TFACE 0 0 1 2", "*MESH_TFACE 0 0 1 3" }, { "*MATERIAL_REF 0", "*MATERIAL_REF 1" }, { "*NODE_NAME \"original part\"", "*NODE_NAME \"original part\"\n*NODE_PARENT \"original part\"" } }) {
		auto changed = source;
		const auto at = changed.find(mutation.first);
		check(at != std::string::npos, "Missing ASE mutation target");
		changed.replace(at, mutation.first.size(), mutation.second);
		bool invalid = false;
		try {
			xl_compat::decode_ase_model(xl_compat::read_ase_records(changed));
		} catch (const std::exception &) {
			invalid = true;
		}
		check(invalid, "Invalid ASE geometry or hierarchy accepted");
	}
}
int main(int argc, char **argv)
{
	try {
		model_controls();
		check(xl_compat::model_candidates(108) == std::vector<std::string>{ "pyrogl.ase", "model108.ase", "pyrogl.oof", "model108.oof" } &&
		          xl_compat::model_name(137).shared == 127 && xl_compat::model_name(137).flip_v &&
		          xl_compat::model_candidates(199) == std::vector<std::string>{ "model199.ase", "model199.oof" },
		      "XL named model precedence or shared slots differ from source");
		const std::string fixture = "*3DSMAX_ASCIIEXPORT 200\n*GEOMOBJECT {\n*NODE_NAME \"authored {name}\"\n"
		                            "*MESH {\n*MESH_FACE 0: A: 0 B: 1 C: 2 *MESH_MTLID 3\n}\n*BITMAP \"C:\\models\\skin.tga\"\n}\n";
		const auto parsed = xl_compat::read_ase_records(fixture);
		const auto &object = parsed.at(1);
		check(object.block && object.children.at(0).values.at(0) == "authored {name}" &&
		          object.children.at(2).values.at(0) == "C:\\models\\skin.tga" &&
		          object.children.at(1).children.at(0).values.back() == "3",
		      "ASE parser lost authored records");
		check(rejected(fixture.substr(0, fixture.size() - 2)) && rejected(fixture + "}\n") &&
		          rejected("*3DSMAX_ASCIIEXPORT 200\n*NODE_NAME \"broken\n") &&
		          rejected("*3DSMAX_ASCIIEXPORT 999\n"),
		      "Malformed ASE accepted");
		if (argc > 1) {
			const bool inventory = argc > 2 && std::string(argv[2]) == "--inventory";
			unsigned files = 0, meshes = 0, unsupported = 0;
			for (const auto &entry : std::filesystem::recursive_directory_iterator(argv[1])) {
				if (!inventory && entry.path().parent_path() != std::filesystem::path(argv[1])) continue;
				if (entry.path().extension() != ".ase") continue;
				std::ifstream input(entry.path(), std::ios::binary);
				const std::string text{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
				const auto records = xl_compat::read_ase_records(text);
				try {
					const auto model = xl_compat::decode_ase_model(records);
					if (inventory || entry.path().filename().string().find("model") == 0) (void) xl_compat::compile_ase_mesh(model);
				} catch (const std::exception &error) {
					const auto failure = entry.path().lexically_relative(argv[1]).generic_string() + ": " + error.what();
					if (!inventory) throw std::runtime_error(failure);
					std::cout << "UNQUALIFIED " << failure << '\n';
					++unsupported;
				}
				unsigned objects = 0;
				for (const auto &record : records)
					if (record.name == "*GEOMOBJECT") ++objects;
				check(objects > 0, "Original ASE lacks geometry records");
				meshes += objects;
				++files;
			}
			check(files > 0, "No original ASE fixtures found");
			std::cout << "Original ASE documents=" << files << " geometry objects=" << meshes << '\n';
			if (unsupported) {
				std::cout << "Unqualified mesh documents=" << unsupported << '\n';
				return 2;
			}
		}
		std::cout << "ASE record and model decoding controls passed; rendered integration is separate\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
