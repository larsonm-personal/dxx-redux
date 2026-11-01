#include "lightning_audio.h"
#include <array>
#include <iostream>
#include <stdexcept>
namespace lightning_audio_reference
{
struct CLightning {
	int m_nNodes;
};
struct Bolts {
	std::array<CLightning, 3> values;
	CLightning *Buffer()
	{
		return values.data();
	}
};
struct CLightningEmitter {
	int m_bValid, m_bSound, m_nBolts = 3;
	Bolts m_lightning;
	xl_compat::LightningSoundAction action = xl_compat::LightningSoundAction::none;
	void UpdateSound(int32_t);
	void Mute();
	void CreateSound(int sound, int)
	{
		m_bSound = sound;
		action = xl_compat::LightningSoundAction::create;
	}
	void DestroySound()
	{
		action = xl_compat::LightningSoundAction::destroy;
	}
};
#define ENTER(...)
#define RETURN return;
#include "xl_reference_lightning_audio.h"
#undef ENTER
#undef RETURN
} // namespace lightning_audio_reference
void lightning_audio_controls()
{
	using namespace xl_compat;
	unsigned count = 0;
	for (int valid : { -1, 0, 1 })
		for (int sound : { -1, 0, 1, 2 })
			for (unsigned mask = 0; mask < 8; ++mask)
				for (unsigned operations = 0; operations < 64; ++operations) {
					lightning_audio_reference::CLightningEmitter source{ valid, sound };
					int native = sound;
					for (unsigned i = 0; i < 3; ++i) source.m_lightning.values[i].m_nNodes = mask & (1u << i) ? 8 : -8;
					for (unsigned step = 0; step < 6; ++step) {
						if (operations & (1u << step)) {
							source.Mute();
							lightning_sound_mute(native);
						} else {
							source.action = LightningSoundAction::none;
							source.UpdateSound(0);
							if (lightning_sound_update(native, valid > 0, mask != 0) != source.action)
								throw std::runtime_error("Lightning audio action differs from pinned UpdateSound");
						}
						if (native != source.m_bSound) throw std::runtime_error("Lightning audio state differs from pinned emitter");
					}
					++count;
				}
	std::cout << count << " lightning audio update/mute traces match pinned source\n";
}
