#include "music_fluid.h"
#include "music_soundfont.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

namespace
{
// Shared gameplay/preview calibration at the app's default -10 dB
// Twice the original 0.2 gain (+6.02 dB), bringing SF2 closer to FM
constexpr double default_gain = 0.4;
// In FluidSynth 2.6, deprecated 7THORDER selects expensive 25-point sinc
// Pin the real-time interpolation explicitly, including after DSP recreation
constexpr int interpolation = FLUID_INTERP_4THORDER;

struct source {
	const unsigned char *data;
	size_t size, at;
	const char *name;
};
// The loader is synchronous, with dynamic sample loading disabled. Each load has
// a unique cache identity and its own bounded snapshot, including APK assets
thread_local const source *loading = nullptr;
std::atomic<unsigned long long> next_source{ 0 };
void *open_source(const char *name)
{
	if (!loading || std::strcmp(name, loading->name)) return nullptr;
	return new (std::nothrow) source(*loading);
}
int read_source(void *buffer, fluid_long_long_t count, void *handle)
{
	auto &s = *static_cast<source *>(handle);
	if (count < 0 || static_cast<unsigned long long>(count) > s.size - s.at) return FLUID_FAILED;
	std::memcpy(buffer, s.data + s.at, size_t(count));
	s.at += size_t(count);
	return FLUID_OK;
}
int seek_source(void *handle, fluid_long_long_t offset, int origin)
{
	auto &s = *static_cast<source *>(handle);
	const auto base = origin == SEEK_SET ? 0 : origin == SEEK_CUR ? s.at
	                                                              : s.size;
	if (origin != SEEK_SET && origin != SEEK_CUR && origin != SEEK_END) return FLUID_FAILED;
	if (offset < -static_cast<fluid_long_long_t>(base) || offset > static_cast<fluid_long_long_t>(s.size - base)) return FLUID_FAILED;
	s.at = size_t(static_cast<fluid_long_long_t>(base) + offset);
	return FLUID_OK;
}
fluid_long_long_t tell_source(void *handle)
{
	return static_cast<fluid_long_long_t>(static_cast<source *>(handle)->at);
}
int close_source(void *handle)
{
	delete static_cast<source *>(handle);
	return FLUID_OK;
}
} // namespace

bool music_fluid::load(AAssetManager *assets, const char *path)
{
	size_t size = 0;
	std::unique_ptr<void, decltype(&std::free)> bytes(music_soundfont_read(assets, path, &size), std::free);
	if (!bytes) return false;
	settings.reset(new_fluid_settings());
	if (!settings) return false;
	auto *cfg = settings.get();
	if (fluid_settings_setnum(cfg, "synth.sample-rate", rate) != FLUID_OK ||
	    fluid_settings_setnum(cfg, "synth.gain", default_gain) != FLUID_OK ||
	    fluid_settings_setint(cfg, "synth.polyphony", 128) != FLUID_OK ||
	    fluid_settings_setint(cfg, "synth.cpu-cores", 1) != FLUID_OK ||
	    fluid_settings_setint(cfg, "synth.threadsafe-api", 0) != FLUID_OK ||
	    fluid_settings_setint(cfg, "synth.dynamic-sample-loading", 0) != FLUID_OK ||
	    fluid_settings_setstr(cfg, "synth.midi-bank-select", "gs") != FLUID_OK ||
	    fluid_settings_setstr(cfg, "synth.reverb.engine", "fdn") != FLUID_OK ||
	    fluid_settings_setnum(cfg, "synth.reverb.room-size", 0.2) != FLUID_OK ||
	    fluid_settings_setnum(cfg, "synth.reverb.damp", 0) != FLUID_OK ||
	    fluid_settings_setnum(cfg, "synth.reverb.width", 0.5) != FLUID_OK ||
	    fluid_settings_setnum(cfg, "synth.reverb.level", 0.3) != FLUID_OK ||
	    fluid_settings_setint(cfg, "synth.chorus.nr", 3) != FLUID_OK ||
	    fluid_settings_setnum(cfg, "synth.chorus.level", 1) != FLUID_OK ||
	    fluid_settings_setnum(cfg, "synth.chorus.speed", 0.3) != FLUID_OK ||
	    fluid_settings_setnum(cfg, "synth.chorus.depth", 8) != FLUID_OK) return false;
	synth.reset(new_fluid_synth(cfg));
	if (!synth) return false;
	auto *loader = new_fluid_defsfloader(cfg);
	if (!loader) return false;
	if (fluid_sfloader_set_callbacks(loader, open_source, read_source, seek_source, tell_source, close_source) != FLUID_OK) {
		delete_fluid_sfloader(loader);
		return false;
	}
	fluid_synth_add_sfloader(get(), loader);
	char name[80];
	std::snprintf(name, sizeof(name), "dxx-sf2-%llu", ++next_source);
	source data{ static_cast<const unsigned char *>(bytes.get()), size, 0, name };
	loading = &data;
	const int id = fluid_synth_sfload(get(), name, 1);
	loading = nullptr;
	if (id < 0) return false;
	auto *font = fluid_synth_get_sfont_by_id(get(), id);
	fluid_sfont_iteration_start(font);
	while (fluid_sfont_iteration_next(font)) ++presets;
	if (fluid_synth_set_interp_method(get(), -1, interpolation) != FLUID_OK) return false;
	effects(reverb, chorus);
	return presets > 0;
}

bool music_fluid::recreate()
{
	// System reset clears delay lines but preserves chorus phase and buffered
	// samples. Recreate DSP for deterministic seeks, transferring the loaded bank
	// only after destroying old voices; no file I/O or sample decoding is needed
	std::unique_ptr<fluid_synth_t, decltype(&delete_fluid_synth)> fresh(new_fluid_synth(settings.get()), delete_fluid_synth);
	if (!fresh) return false;
	auto *font = fluid_synth_get_sfont(get(), 0);
	for (int c = 0; c < 16; ++c) fluid_synth_unset_program(get(), c);
	if (!font || fluid_synth_remove_sfont(get(), font) != FLUID_OK) return false;
	synth.reset();
	synth = std::move(fresh);
	if (fluid_synth_add_sfont(get(), font) < 0) return false;
	if (fluid_synth_set_interp_method(get(), -1, interpolation) != FLUID_OK) return false;
	effects(reverb, chorus);
	return true;
}

void music_fluid::reset()
{
	if (!recreate()) {
		fluid_synth_system_reset(get());
		fluid_synth_all_sounds_off(get(), -1);
	}
}
void music_fluid::output(int sample_rate, float gain_db)
{
	// The caller changes rate only before playback; runtime volume keeps voices
	if (sample_rate != rate) {
		if (fluid_settings_setnum(settings.get(), "synth.sample-rate", sample_rate) != FLUID_OK) return;
		if (recreate()) rate = sample_rate;
	}
	const double gain = std::max(0.0, std::min(10.0, default_gain * std::pow(10.0, (gain_db + 10.0) / 20.0)));
	fluid_settings_setnum(settings.get(), "synth.gain", gain);
	fluid_synth_set_gain(get(), float(gain));
}
void music_fluid::voices(int count)
{
	fluid_settings_setint(settings.get(), "synth.polyphony", count);
	fluid_synth_set_polyphony(get(), count);
}
void music_fluid::effects(bool use_reverb, bool use_chorus)
{
	reverb = use_reverb;
	chorus = use_chorus;
	fluid_settings_setint(settings.get(), "synth.reverb.active", reverb);
	fluid_settings_setint(settings.get(), "synth.chorus.active", chorus);
	fluid_synth_reverb_on(get(), -1, reverb);
	fluid_synth_chorus_on(get(), -1, chorus);
	for (int c = 0; c < 16; ++c)
		if (c != 9) fluid_synth_cc(get(), c, 93, chorus ? 24 : 0);
}
void music_fluid::render(short *out, int frames, int mixing)
{
	float pcm[512];
	while (frames > 0) {
		const int count = std::min(frames, 256);
		if (fluid_synth_write_float(get(), count, pcm, 0, 2, pcm, 1, 2) != FLUID_OK) std::fill_n(pcm, count * 2, 0.f);
		for (int i = 0; i < count * 2; ++i) {
			const double value = (std::isfinite(pcm[i]) ? double(pcm[i]) * 32768 : 0) + (mixing ? out[i] : 0);
			out[i] = short(std::lround(std::max(-32768.0, std::min(32767.0, value))));
		}
		out += count * 2;
		frames -= count;
	}
}
void music_fluid::capture(hmp_tsf_state *state)
{
	for (int c = 0; c < 16; ++c) {
		int id, bank, program, msb, lsb;
		fluid_synth_get_program(get(), c, &id, &bank, &program);
		state->preset[c] = program;
		fluid_synth_get_cc(get(), c, 0, &msb);
		fluid_synth_get_cc(get(), c, 32, &lsb);
		state->bank[c] = (msb << 7) | lsb;
		fluid_synth_get_cc(get(), c, 10, &msb);
		fluid_synth_get_cc(get(), c, 42, &lsb);
		state->pan[c] = unsigned((msb << 7) | lsb);
	}
	state->valid = 1;
}
void music_fluid::begin(const hmp_tsf_state *state)
{
	for (int c = 0; c < 16; ++c) {
		if (state->valid) {
			fluid_synth_cc(get(), c, 0, state->bank[c] >> 7);
			fluid_synth_cc(get(), c, 32, state->bank[c] & 127);
			fluid_synth_program_change(get(), c, state->preset[c]);
			fluid_synth_cc(get(), c, 10, int(state->pan[c] >> 7));
			fluid_synth_cc(get(), c, 42, int(state->pan[c] & 127));
		}
		fluid_synth_cc(get(), c, 39, 0);
		fluid_synth_cc(get(), c, 7, 0);
	}
}
void music_fluid::control(int c, int cc, int value)
{
	if (cc != 121) {
		fluid_synth_cc(get(), c, cc, value);
		return;
	}
	int bank_msb, bank_lsb, pan_msb, pan_lsb;
	fluid_synth_get_cc(get(), c, 0, &bank_msb);
	fluid_synth_get_cc(get(), c, 32, &bank_lsb);
	fluid_synth_get_cc(get(), c, 10, &pan_msb);
	fluid_synth_get_cc(get(), c, 42, &pan_lsb);
	fluid_synth_cc(get(), c, cc, value);
	fluid_synth_cc(get(), c, 0, bank_msb);
	fluid_synth_cc(get(), c, 32, bank_lsb);
	fluid_synth_cc(get(), c, 10, pan_msb);
	fluid_synth_cc(get(), c, 42, pan_lsb);
	fluid_synth_cc(get(), c, 39, 0);
	fluid_synth_cc(get(), c, 64, 0);
	fluid_synth_pitch_bend(get(), c, 8192);
}
