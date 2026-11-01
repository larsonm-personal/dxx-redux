#ifndef DXX_XL_RUNTIME_PARTICLE_RENDERER_H
#define DXX_XL_RUNTIME_PARTICLE_RENDERER_H
#ifdef __cplusplus
extern "C" {
#endif
// Nonzero only inside a camera-owned GL capture callback
int xl_camera_target_active_dimensions(int *width, int *height);
void xl_particle_images_discard_context(int abandoned);
void xl_particle_soft_begin_scene(void);
void xl_particle_soft_end_scene(void);
#ifdef __cplusplus
}
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include <cstddef>
#include <memory>
struct _grs_bitmap;
namespace xl_compat
{
struct ParticleQuad;
struct PogBitmap;
// Pixel storage must outlive this texture; ParticleImageResource owns both
class ParticleTexture
{
  public:
	ParticleTexture(const PogBitmap &pixels, const char *name, bool require_soft_depth = true);
	~ParticleTexture();
	void draw(const ParticleQuad *quads, std::size_t count);
	// Source powerup corona: additive sprite, radius-5 soft depth, transparent border
	void draw_soft_sprite(const ParticleQuad &quad, bool soft = true);
	unsigned handle();
	// Transparent border at every mip; sample original UVs with uv/4+1/4
	unsigned border_handle();
	static void discard_context(bool abandoned);

  private:
	struct State;
	std::unique_ptr<State> state;
};
// Vertices use the current renderer's camera coordinates and projection
void draw_particle_quads(_grs_bitmap &image, const ParticleQuad *quads, std::size_t count);
void draw_multi_particle_quads(ParticleTexture &smoke, ParticleTexture &bubble, const ParticleQuad *quads, std::size_t count, bool soft = true);
void draw_bitmap_sprite(_grs_bitmap &image, ParticleQuad quad);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
#endif
