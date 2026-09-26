#ifndef DXX_MUSIC_FLUID_H
#define DXX_MUSIC_FLUID_H
#include "hmp_tsf_state.h"
#include "music_eq.h"
#include <fluidsynth.h>
#include <memory>

struct AAssetManager;
// Access is serialized by the existing preview/game render workers
class music_fluid
{
	std::unique_ptr<fluid_settings_t, decltype(&delete_fluid_settings)> settings{ nullptr, delete_fluid_settings };
	std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)> synth{ nullptr, delete_fluid_synth };
	int rate = 48000;
	bool reverb = true, chorus = true;
	int presets = 0;
	music_eq equalizer;
	bool eq_compatible = false;
	bool recreate();

  public:
	bool load(AAssetManager *assets, const char *path);
	fluid_synth_t *get() const
	{
		return synth.get();
	}
	int preset_count() const
	{
		return presets;
	}
	void reset();
	void output(int sample_rate, float gain_db);
	void voices(int count);
	void effects(bool use_reverb, bool use_chorus);
	void eq(int preset)
	{
		equalizer.configure(eq_compatible ? preset : 0, rate);
	}
	int eq_preset() const
	{
		return equalizer.preset();
	}
	void render(short *out, int frames, int mixing);
	void begin(const hmp_tsf_state *state);
	void capture(hmp_tsf_state *state);
	void control(int channel, int controller, int value);
};
#endif
