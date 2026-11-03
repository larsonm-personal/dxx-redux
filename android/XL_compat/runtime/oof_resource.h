#ifndef DXX_XL_OOF_RESOURCE_H
#define DXX_XL_OOF_RESOURCE_H
#include "../oof_mesh.h"
#include "../ase_mesh.h"
#include "../pog.h"
#include <memory>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct OofResource {
	std::string path;
	OofModel model;
	OofMesh mesh;
	std::vector<std::shared_ptr<const PogBitmap>> skins;
	std::map<int, OofVector> gunpoints;
	std::vector<AseThruster> thrusters;
};
struct OwnedModelSet {
	std::map<int, std::shared_ptr<OofResource>> models;
	std::map<std::string, std::string> sources;
};
} // namespace xl_compat
// Pure preparation: never publishes models, native geometry, GPU handles or save identity
xl_compat::OwnedModelSet xl_prepare_owned_models(int ordinal);
// CPU resources are owned by the current mission; callers must not retain across selection
std::shared_ptr<const xl_compat::OofResource> xl_owned_oof_model(int id);
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
