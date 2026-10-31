#ifndef DXX_XL_RUNTIME_PARTICLE_RENDERER_H
#define DXX_XL_RUNTIME_PARTICLE_RENDERER_H
#ifdef __cplusplus
extern "C" {
#endif
void xl_particle_images_discard_context(int abandoned);
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
	ParticleTexture(const PogBitmap &pixels, const char *name);
	~ParticleTexture();
	void draw(const ParticleQuad *quads, std::size_t count);
	static void discard_context(bool abandoned);

  private:
	struct State;
	std::unique_ptr<State> state;
};
// Vertices use the current renderer's camera coordinates and projection
void draw_particle_quads(_grs_bitmap &image, const ParticleQuad *quads, std::size_t count);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
#endif
