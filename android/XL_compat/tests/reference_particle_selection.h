// Filesystem/bitmap allocation adapters only; selection and image setup execute
// pinned LoadAddonBitmap and CParticleImageManager bodies
namespace particle_image_reference
{
struct CBitmap {
	int width = 128, height = 128, frames = 0, binds = 0, setups = 0;
	int Width() const
	{
		return width;
	}
	int Height() const
	{
		return height;
	}
	int FrameCount() const
	{
		return frames;
	}
	void SetFrameCount()
	{
		frames = height / width;
	}
	void SetTranspType(int) {}
	void Bind(int)
	{
		++binds;
	}
	void SetupTexture(int, int)
	{
		++setups;
	}
};
struct tParticleImageInfo {
	CBitmap *pBm;
	const char *szName;
	int nFrames, iFrame, bHave, bAnimate;
	float xBorder, yBorder;
};
#include "xl_reference_particle_image_table.h"
struct {
	struct {
		const char *szTextures[1] = { "mods/pmines/textures/" };
	} mods;
	struct {
		const char *szTextures[1] = { "textures/" };
	} game;
} gameFolders;
struct {
	struct {
		int enabled = 0;
		void EnableCartoonStyle()
		{
			++enabled;
		}
		void DisableCartoonStyle()
		{
			--enabled;
		}
	} render;
} gameStates;
bool mod_exists = false, mod_readable = true, base_readable = true;
std::vector<std::string> probes, reads;
struct CFile {
	bool Exist(const char *path, const char *, int)
	{
		probes.push_back(path);
		return mod_exists;
	}
};
struct CTGA {
	CBitmap *CreateAndRead(char *path)
	{
		reads.push_back(path);
		const bool mod = std::string(path).find("mods/") == 0;
		return (mod ? mod_readable : base_readable) ? new CBitmap : nullptr;
	}
};
struct CParticleImageManager {
	int32_t GetType(int32_t);
	int32_t Load(int32_t, int32_t);
} particleImageManager;
tParticleImageInfo &ParticleImageInfo(int type)
{
	return particleImageInfo[2][type];
}
bool Bind(int)
{
	return true;
}
#define FILENAME_LEN     4096
#define MAKE_SMOKE_IMAGE 0
#include "xl_reference_particle_images.h"
#undef FILENAME_LEN
#undef MAKE_SMOKE_IMAGE
} // namespace particle_image_reference

static void particle_image_selection_controls()
{
	using namespace particle_image_reference;
	auto &image = ParticleImageInfo(FIRE_PARTICLES);
	require(std::string(image.szName) == std::string(xl_compat::fire_image_name) + ".tga" && image.nFrames == int(xl_compat::fire_image_atlas_side) && !image.bAnimate, "Authored fire image profile differs from active source table");
	for (unsigned scenario = 0; scenario < 4; ++scenario) {
		mod_exists = scenario == 1 || scenario == 2;
		mod_readable = scenario != 2;
		base_readable = scenario != 3;
		probes.clear();
		reads.clear();
		const bool loaded = particleImageManager.Load(FIRE_PARTICLES, 0) != 0;
		const auto mod = xl_compat::addon_mission_texture_path("missions/pmines.hog", xl_compat::fire_image_name);
		const auto selected = mod_exists ? mod : "textures/d2x-xl/" + std::string(xl_compat::fire_image_name) + ".tga";
		require(probes == std::vector<std::string>{ mod } && reads == std::vector<std::string>{ selected } && loaded == (scenario < 2), "Pinned addon image selection/failure differs");
		if (loaded) {
			require(image.pBm && image.pBm->binds == 1 && image.pBm->setups == 1 && image.xBorder == 1.0f / 128 && image.yBorder == 1.0f / 128 && !image.bAnimate, "Pinned fire image border/texture setup differs");
			require(particleImageManager.Load(FIRE_PARTICLES, 0) && reads.size() == 1, "Source reread an already prepared image");
		}
		require(gameStates.render.enabled == 0, "Source addon style scope was not restored");
		delete image.pBm;
		image.pBm = nullptr;
		image.bHave = 0;
	}
	std::cout << "Pinned particle image selection: active quality-2 atlas, mod/default precedence, failed mod without fallback, cached success and one-pixel borders passed\n";
}
