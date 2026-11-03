#ifndef DXX_XL_MODEL_NAMES_H
#define DXX_XL_MODEL_NAMES_H
#include <string>
#include <vector>
namespace xl_compat
{
// Literal polygon slots from pinned XL models/hiresmodels.cpp replacementModels
// Virtual powerup/hostage IDs (source capacity500) are not native polygon slots
struct ModelName {
	int id;
	const char *name;
	int shared;
	bool flip_v;
};
inline ModelName model_name(int id)
{
	static const ModelName names[] = {
		{ 108, "pyrogl", -1, 0 },
		{ 109, "pyrogl", -1, 0 },
		{ 110, "pyrogl", -1, 0 },
		{ 0, "smallhulk", -1, 0 },
		{ 2, "mediumlifter", -1, 0 },
		{ 4, "spider", -1, 0 },
		{ 6, "class1drone", -1, 0 },
		{ 8, "class2drone", -1, 0 },
		{ 10, "class1driller-cloaked", -1, 0 },
		{ 14, "class2supervisor", -1, 0 },
		{ 15, "secondarylifter", -1, 0 },
		{ 17, "class1heavydriller", -1, 0 },
		{ 19, "class3gopher", -1, 0 },
		{ 20, "class1platform", -1, 0 },
		{ 21, "class2platform", -1, 0 },
		{ 23, "smallspider", -1, 0 },
		{ 25, "fusionhulk", -1, 0 },
		{ 26, "superhulk", -1, 0 },
		{ 28, "d1boss1", -1, 0 },
		{ 50, "", 28, 0 },
		{ 29, "cloakedlifter", -1, 0 },
		{ 31, "class1driller", -1, 0 },
		{ 33, "mediumhulk", -1, 0 },
		{ 35, "advancedlifter", -1, 0 },
		{ 37, "ptmcdefense", -1, 0 },
		{ 39, "d1boss2", -1, 0 },
		{ 40, "bper", -1, 0 },
		{ 41, "smelter", -1, 0 },
		{ 43, "icespider", -1, 0 },
		{ 44, "bulkdestroyer", -1, 0 },
		{ 45, "trnracer", -1, 0 },
		{ 46, "foxattackbot", -1, 0 },
		{ 47, "sidarm", -1, 0 },
		{ 49, "redfattyboss", -1, 0 },
		{ 51, "guidebot", -1, 0 },
		{ 54, "eviltwin", -1, 0 },
		{ 55, "itsc", -1, 0 },
		{ 56, "itdroid", -1, 0 },
		{ 58, "pest", -1, 0 },
		{ 60, "pig", -1, 0 },
		{ 62, "d-claw", -1, 0 },
		{ 64, "redhornet", -1, 0 },
		{ 65, "thief", -1, 0 },
		{ 67, "seeker", -1, 0 },
		{ 68, "e-bandit", -1, 0 },
		{ 69, "fireboss", -1, 0 },
		{ 70, "waterboss", -1, 0 },
		{ 71, "boarshead", -1, 0 },
		{ 72, "greenspider", -1, 0 },
		{ 73, "omegabot", -1, 0 },
		{ 75, "louguard", -1, 0 },
		{ 76, "alienboss1", -1, 0 },
		{ 77, "redfattyjunior", -1, 0 },
		{ 78, "d-claw-cloaked", -1, 0 },
		{ 79, "", 78, 0 },
		{ 80, "smelter-cloaked", -1, 0 },
		{ 81, "smelterclone-cloaked", -1, 0 },
		{ 82, "omegaspawn", -1, 0 },
		{ 83, "smelterclone", -1, 0 },
		{ 85, "omegaclone", -1, 0 },
		{ 86, "bperclone", -1, 0 },
		{ 87, "spiderclone", -1, 0 },
		{ 88, "spiderspawn", -1, 0 },
		{ 89, "iceboss", -1, 0 },
		{ 90, "spiderspawnclone", -1, 0 },
		{ 91, "alienboss2", -1, 0 },
		{ 166, "compactlifter", -1, 0 },
		{ 167, "fervid99", -1, 0 },
		{ 168, "fiddler", -1, 0 },
		{ 169, "class2heavydriller", -1, 0 },
		{ 170, "smelter2", -1, 0 },
		{ 171, "max", -1, 0 },
		{ 172, "sniperng", -1, 0 },
		{ 173, "logikill", -1, 0 },
		{ 174, "canary", -1, 0 },
		{ 175, "vertigoboss", -1, 0 },
		{ 176, "redguard", -1, 0 },
		{ 177, "spike", -1, 0 },
		{ 93, "reactorD1", -1, 0 },
		{ 94, "reactorD1-destr", -1, 0 },
		{ 95, "reactorD2-1", -1, 0 },
		{ 96, "reactorD2-1-destr", -1, 0 },
		{ 97, "reactorD2-2", -1, 0 },
		{ 98, "reactorD2-2-destr", -1, 0 },
		{ 99, "reactorD2-3", -1, 0 },
		{ 100, "reactorD2-3-destr", -1, 0 },
		{ 101, "reactorD2-4", -1, 0 },
		{ 102, "reactorD2-4-destr", -1, 0 },
		{ 103, "reactorD2-5", -1, 0 },
		{ 104, "reactorD2-5-destr", -1, 0 },
		{ 105, "reactorD2-6", -1, 0 },
		{ 106, "reactorD2-6-destr", -1, 0 },
		{ 114, "laser1-inner", -1, 0 },
		{ 111, "laser1-outer", -1, 0 },
		{ 118, "laser2-inner", -1, 0 },
		{ 115, "laser2-outer", -1, 0 },
		{ 122, "laser3-inner", -1, 0 },
		{ 119, "laser3-outer", -1, 0 },
		{ 126, "laser4-inner", -1, 0 },
		{ 123, "laser4-outer", -1, 0 },
		{ 146, "laser5-inner", -1, 0 },
		{ 143, "laser5-outer", -1, 0 },
		{ 150, "laser6-inner", -1, 0 },
		{ 147, "laser6-outer", -1, 0 },
		{ 139, "botlas1-inner", -1, 0 },
		{ 138, "botlas1-outer", -1, 0 },
		{ 130, "botlas2-inner", -1, 0 },
		{ 129, "botlas2-outer", -1, 0 },
		{ 141, "botlas4-inner", -1, 0 },
		{ 140, "botlas4-outer", -1, 0 },
		{ 157, "botlas6-inner", -1, 0 },
		{ 156, "botlas6-outer", -1, 0 },
		{ 155, "lightvulcan", -1, 0 },
		{ 131, "fusion-outer", -1, 0 },
		{ 132, "fusion-inner", -1, 0 },
		{ 128, "flare", -1, 0 },
		{ 107, "marker", -1, 0 },
		{ 159, "smallmine", -1, 0 },
		{ 127, "concussion", -1, 1 },
		{ 137, "", 127, 1 },
		{ 133, "homer", -1, 1 },
		{ 136, "", 133, 1 },
		{ 134, "smartmsl", -1, 1 },
		{ 162, "", 134, 1 },
		{ 135, "mega", -1, 1 },
		{ 142, "", 135, 1 },
		{ 151, "flashmsl", -1, 1 },
		{ 158, "", 151, 1 },
		{ 165, "", 151, 1 },
		{ 152, "guided", -1, 1 },
		{ 153, "mercury", -1, 1 },
		{ 161, "", 153, 1 },
		{ 154, "eshaker", -1, 1 },
		{ 163, "", 154, 1 },
		{ 160, "shakrsub", -1, 1 },
	};
	for (const auto &entry : names)
		if (entry.id == id) return entry;
	return { id, "", -1, false };
}
inline std::vector<std::string> model_candidates(int id)
{
	const auto entry = model_name(id);
	std::vector<std::string> result;
	for (const char *extension : { ".ase", ".oof" }) {
		if (*entry.name) result.push_back(std::string(entry.name) + extension);
		result.push_back("model" + std::to_string(id) + extension);
	}
	return result;
}
} // namespace xl_compat
#endif
