/* Shared SF2/FM renderer. No sequencing or Android UI policy lives here */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "music_synth.h"
#include "music_fluid.h"
#include "music_decode_limits.h"
#include "music_fm_resampler.h"
#include "player.h"
#include "tml.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#ifdef __ANDROID__
#include <android/log.h>
#define MUSIC_LOG(...) __android_log_print(ANDROID_LOG_INFO, "DXX-MusicSynth", __VA_ARGS__)
#else
#define MUSIC_LOG(...)                     \
	do {                                   \
		std::fprintf(stderr, __VA_ARGS__); \
		std::fputc('\n', stderr);          \
	} while (0)
#endif

// The experiment patch supports optional register diagnostics
#ifdef DXX_MUSIC_SYNTH_TEST_TRACE
extern "C" void music_synth_test_trace(int chip, uint16_t reg, uint8_t value);
void fm_trace_write(int chip, uint16_t reg, uint8_t value)
{
	music_synth_test_trace(chip, reg, value);
}
#else
void fm_trace_write(int, uint16_t, uint8_t) {}
#endif

struct music_synth {
	music_fluid sf;
	std::unique_ptr<OPLPlayer> fm;
	music_fm_resampler fm_resampler;
	double fm_gain = std::pow(10.0, -10.0 / 20.0);
	std::unique_ptr<unsigned char, decltype(&std::free)> fm_sequence{ nullptr, std::free };
	int fm_sequence_size = 0;
	bool prefer_fm = false;
	int rate = 48000;
	float gain = -10;
	std::array<std::array<bool, 128>, 16> notes{};
	std::array<std::array<bool, 128>, 16> held{};
	std::array<bool, 16> sustain{};
	std::array<bool, 256> available{};
};

static unsigned le16(const unsigned char *p)
{
	return p[0] | (unsigned(p[1]) << 8);
}
static unsigned le32(const unsigned char *p)
{
	return le16(p) | (le16(p + 2) << 16);
}

// D1 ADLIB and D2 AMLIB/ANLIB share this layout, verified against DOS OPL writes
static bool bank_to_wopl(const unsigned char *data, size_t size, unsigned char *out)
{
	if (!data || size < 28 || size > 65536) return false;
	if (std::memcmp(data + 2, "ADLIB-", 6) && std::memcmp(data + 2, "AMLIB-", 6) &&
	    std::memcmp(data + 2, "ANLIB-", 6)) return false;
	unsigned count = le16(data + 10), names = le32(data + 12), records = le32(data + 16);
	if (!count || count > 128 || le16(data + 8) > count || names < 28 || names > size ||
	    count * 12 > size - names || records < names + count * 12 || records > size) return false;
	for (unsigned i = 0; i < 128; ++i) {
		auto *patch = out + i * 62;
		patch[39] = 4; // blank unless this bank supplies the patch
		if (i >= count) continue;
		const auto *name = data + names + i * 12;
		size_t offset = records + le16(name) * 30u;
		if (offset > size || size - offset < 30 || name[2] > 127) return false;
		const auto *raw = data + offset;
		if (raw[0] > 1) return false;
		// Original HMI plays the rhythm-marked D1 entries as paired melodic voices
		// It does not enable OPL rhythm mode for these bank records
		patch[39] = 0;
		std::memcpy(patch, name + 3, 9);
		patch[38] = name[2];
		patch[40] = (raw[4] << 1) | raw[14];
		for (unsigned op = 0; op < 2; ++op) {
			const auto *p = raw + 2 + op * 13;
			auto *target = patch + 42 + (1 - op) * 5;
			// HMI shifts full bytes and then truncates; masking each field changes
			// the hamdrum rhythm-marked patches' noncanonical carrier mode fields
			target[0] = (p[9] << 7) | (p[10] << 6) | (p[5] << 5) | (p[11] << 4) | p[1];
			target[1] = (p[0] << 6) | p[8];
			target[2] = (p[3] << 4) | p[6];
			target[3] = (p[4] << 4) | p[7];
			target[4] = raw[28 + op];
		}
	}
	return true;
}

static std::string identity(const char *name)
{
	std::string result(name ? name : "");
	for (char &c : result)
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	return result;
}

static bool safe_bank_name(const char *name)
{
	return *name && !std::strchr(name, '/') && !std::strchr(name, '\\') && !std::strchr(name, ':') &&
	       identity(name).find("..") == std::string::npos;
}

static bool bank_names(music_bank_reader read, void *context, const char *song, char *melodic, char *drums)
{
	std::string bank_song = identity(song);
	if (bank_song.size() > 4 && bank_song.compare(bank_song.size() - 4, 4, ".hmq") == 0)
		bank_song.back() = 'p';
	// Use the first SNG containing this song, matching gameplay's precedence
	for (const char *sng : { "dxx-r.sng", "descent.sng", "descent2.sng" }) {
		size_t size = 0;
		std::unique_ptr<unsigned char, decltype(&std::free)> data(read(context, sng, &size), std::free);
		if (!data || size > 65536) continue;
		std::string text(reinterpret_cast<char *>(data.get()), size);
		size_t start = 0;
		while (start < text.size()) {
			size_t end = text.find('\n', start);
			std::string line = text.substr(start, end == std::string::npos ? end : end - start);
			char file[81], mel[81], drum[81];
			if (std::sscanf(line.c_str(), "%80s %80s %80s", file, mel, drum) == 3 &&
			    (identity(file) == identity(song) || identity(file) == bank_song) &&
			    safe_bank_name(mel) && safe_bank_name(drum)) {
				std::strcpy(melodic, mel);
				std::strcpy(drums, drum);
				return true;
			}
			if (end == std::string::npos) break;
			start = end + 1;
		}
	}
	return false;
}

music_synth *music_synth_load(AAssetManager *assets, const char *sf2, int prefer_fm)
{
	try {
		auto s = std::unique_ptr<music_synth>(new music_synth);
		if (!s->sf.load(assets, sf2)) return nullptr;
#ifdef __ANDROID__
		__android_log_print(ANDROID_LOG_INFO, "DXX-Soundfont", "Loaded path=%s presets=%d renderer=fluidsynth",
		                    sf2 && *sf2 ? sf2 : "bundled:gm.sf2", s->sf.preset_count());
#endif
		s->prefer_fm = prefer_fm != 0;
		return s.release();
	} catch (...) {
		return nullptr;
	}
}

void music_synth_close(music_synth *s)
{
	delete s;
}
int music_synth_is_fm(const music_synth *s)
{
	return s && bool(s->fm);
}

const unsigned char *music_synth_fm_sequence(const music_synth *s, int *size)
{
	*size = s && s->fm ? s->fm_sequence_size : 0;
	return *size ? s->fm_sequence.get() : nullptr;
}

int music_synth_prepare(music_synth *s, const char *song, music_bank_reader read, void *context)
{
	if (!s) return 0;
	s->fm.reset();
	s->fm_sequence.reset();
	s->fm_sequence_size = 0;
	s->notes = {};
	try {
		char mel[81], drums[81];
		if (s->prefer_fm && read && song && bank_names(read, context, song, mel, drums)) {
			size_t msize = 0, dsize = 0;
			std::unique_ptr<unsigned char, decltype(&std::free)> m(read(context, mel, &msize), std::free);
			std::unique_ptr<unsigned char, decltype(&std::free)> d(read(context, drums, &dsize), std::free);
			std::vector<unsigned char> wopl(19 + 68 + 256 * 62);
			std::memcpy(wopl.data(), "WOPL3-BANK", 11);
			wopl[11] = 2;
			wopl[14] = 1;
			wopl[16] = 1;
			if (bank_to_wopl(m.get(), msize, wopl.data() + 87) &&
			    bank_to_wopl(d.get(), dsize, wopl.data() + 87 + 128 * 62)) {
				for (size_t i = 0; i < 256; ++i) s->available[i] = wopl[87 + i * 62 + 39] == 0;
				auto fm = std::unique_ptr<OPLPlayer>(new OPLPlayer(1, OPLPlayer::ChipOPL3));
				fm->setLoop(false);
				fm->setStereo(true);
				fm->setFilter(0);
				fm->setSampleRate(music_fm_resampler::native_rate);
				fm->setGain(1);
				s->fm_resampler.configure(s->rate);
				s->fm_resampler.reset();
				if (fm->loadPatches(wopl.data(), wopl.size())) {
					std::string sequence = song;
					if (sequence.size() > 4 && identity(sequence.c_str()).compare(sequence.size() - 4, 4, ".hmp") == 0) {
						sequence.back() = 'q';
						size_t size = 0;
						s->fm_sequence.reset(read(context, sequence.c_str(), &size));
						if (s->fm_sequence && size && size <= MUSIC_MIDI_ENCODED_MAX_BYTES)
							s->fm_sequence_size = int(size);
						else s->fm_sequence.reset();
					}
					s->fm = std::move(fm);
					MUSIC_LOG("renderer=ymfm song=%s melodic=%s drums=%s sequence=%s", song, mel, drums,
					          s->fm_sequence ? sequence.c_str() : song);
					return 1;
				}
			}
		}
	} catch (...) {
		s->fm.reset();
	}
	MUSIC_LOG("renderer=fluidsynth song=%s reason=%s", song ? song : "MIDI", s->prefer_fm ? "unsupported-or-missing-FM-banks" : "selected");
	return 0;
}

int music_synth_accepts_midi(music_synth *s, const unsigned char *midi, int size)
{
	if (!s->fm) return 1;
	tml_message *messages = tml_load_memory(midi, size);
	if (!messages) {
		MUSIC_LOG("FM validation failed: MIDI parser rejected sequence");
		return 0;
	}
	int programs[16] = { 0 };
	bool supported = true;
	for (const tml_message *m = messages; m; m = m->next) {
		/* TinyMidi meta messages do not initialize the MIDI channel/key fields */
		if (m->type < TML_NOTE_OFF) continue;
		if ((unsigned char) m->channel >= 16 ||
		    ((m->type == TML_PROGRAM_CHANGE || m->type == TML_NOTE_ON) && (unsigned char) m->key >= 128)) {
			MUSIC_LOG("FM validation failed: type=%u channel=%u key=%u", (unsigned char) m->type,
			          (unsigned char) m->channel, (unsigned char) m->key);
			supported = false;
			break;
		}
		if (m->type == TML_PROGRAM_CHANGE) programs[m->channel] = (unsigned char) m->program;
		if (m->type == TML_NOTE_ON && m->velocity) {
			unsigned patch = m->channel == 9 ? 128u + (unsigned char) m->key : (unsigned) programs[m->channel];
			if (!s->available[patch]) {
				MUSIC_LOG("FM validation failed: missing patch=%u channel=%u", patch, (unsigned char) m->channel);
				supported = false;
				break;
			}
		}
	}
	tml_free(messages);
	return supported;
}

void music_synth_reset(music_synth *s)
{
	if (s->fm) s->fm->reset();
	else s->sf.reset();
	s->fm_resampler.reset();
	s->notes = {};
	s->held = {};
	s->sustain = {};
}
void music_synth_set_output(music_synth *s, TSFOutputMode mode, int rate, float gain)
{
	if (rate < 8000 || rate > 192000 || !std::isfinite(gain)) return;
	if (!s->fm && rate > 96000) return; // FluidSynth's documented rate range
	s->rate = rate;
	s->gain = gain;
	s->fm_gain = std::pow(10.0, gain / 20.0);
	if (s->fm) {
		s->fm_resampler.configure(rate);
	} else s->sf.output(rate, gain);
	(void) mode; // All shared callers use interleaved stereo
}
void music_synth_set_max_voices(music_synth *s, int voices)
{
	if (voices >= 8 && voices <= 256) s->sf.voices(voices);
}
void music_synth_set_eq(music_synth *s, int preset)
{
	if (s) s->sf.eq(preset);
}
int music_synth_get_eq(const music_synth *s)
{
	return s && !s->fm ? s->sf.eq_preset() : 0;
}
int music_synth_get_presetcount(music_synth *s)
{
	return s->fm ? 256 : s->sf.preset_count();
}
int music_synth_active_voice_count(music_synth *s)
{
	if (!s->fm) return fluid_synth_get_active_voice_count(s->sf.get());
	int count = 0;
	for (const auto &channel : s->notes) count += int(std::count(channel.begin(), channel.end(), true));
	return std::min(count, 9); // keyed-note diagnostic; paired banks share nine voices
}
void music_synth_render_short(music_synth *s, short *out, int frames, int mixing)
{
	if (!s->fm) {
		s->sf.render(out, frames, mixing);
		return;
	}
	float samples[music_fm_resampler::block_frames * 2];
	while (frames > 0) {
		const int count = std::min(frames, int(music_fm_resampler::block_frames));
		s->fm_resampler.render(samples, count, [s](float *buffer, int n) { s->fm->generate(buffer, unsigned(n)); });
		for (int i = 0; i < count * 2; ++i) {
			// Preserve fractional filter/gain output until the PCM16 queue boundary
			const double value = double(samples[i]) * s->fm_gain * 32768 + (mixing ? out[i] : 0);
			out[i] = short(std::lround(std::max(-32768.0, std::min(32767.0, value))));
		}
		out += count * 2;
		frames -= count;
	}
}
void music_synth_channel_note_on(music_synth *s, int c, int key, float velocity)
{
	if (c < 0 || c >= 16 || key < 0 || key >= 128 || !std::isfinite(velocity) || velocity < 0 || velocity > 1) return;
	if (s->fm) {
		if (velocity <= 0) {
			music_synth_channel_note_off(s, c, key);
			return;
		}
		s->fm->midiNoteOn(uint8_t(c), uint8_t(key), uint8_t(velocity * 127 + 0.5f));
		s->notes[c][key] = s->held[c][key] = true;
	} else if (velocity > 0) fluid_synth_noteon(s->sf.get(), c, key, int(velocity * 127 + 0.5f));
	else fluid_synth_noteoff(s->sf.get(), c, key);
}
void music_synth_channel_note_off(music_synth *s, int c, int key)
{
	if (c < 0 || c >= 16 || key < 0 || key >= 128) return;
	if (s->fm) {
		s->held[c][key] = false;
		s->fm->midiNoteOff(uint8_t(c), uint8_t(key));
		if (!s->sustain[c]) {
			s->notes[c][key] = false;
		}
	} else fluid_synth_noteoff(s->sf.get(), c, key);
}
void music_synth_channel_set_presetnumber(music_synth *s, int c, int program, int drums)
{
	if (c < 0 || c >= 16 || program < 0 || program >= 128) return;
	if (s->fm) s->fm->midiProgramChange(uint8_t(c), uint8_t(program));
	else {
		fluid_synth_set_channel_type(s->sf.get(), c, drums ? CHANNEL_TYPE_DRUM : CHANNEL_TYPE_MELODIC);
		fluid_synth_program_change(s->sf.get(), c, program);
	}
}
void music_synth_channel_midi_control(music_synth *s, int c, int control, int value)
{
	if (c < 0 || c >= 16 || control < 0 || control >= 128 || value < 0 || value >= 128) return;
	if (s->fm) {
		if (control == 64) {
			s->sustain[c] = value >= 64;
			s->fm->midiControlChange(uint8_t(c), uint8_t(control), uint8_t(value));
			if (!s->sustain[c])
				for (int key = 0; key < 128; ++key)
					if (!s->held[c][key]) {
						s->notes[c][key] = false;
					}
		} else if (control == 123) {
			for (int key = 0; key < 128; ++key) music_synth_channel_note_off(s, c, key);
		} else if (control == 120) {
			s->fm->midiControlChange(uint8_t(c), uint8_t(control), uint8_t(value));
			s->notes[c] = {};
			s->held[c] = {};
		} else if (control == 121) {
			music_synth_channel_midi_control(s, c, 64, 0);
			music_synth_channel_midi_control(s, c, 11, 127);
			s->fm->midiPitchControl(uint8_t(c), 0);
		} else s->fm->midiControlChange(uint8_t(c), uint8_t(control), uint8_t(value));
	} else fluid_synth_cc(s->sf.get(), c, control, value);
}
void music_synth_channel_set_pitchwheel(music_synth *s, int c, int value)
{
	if (c < 0 || c >= 16 || value < 0 || value >= 16384) return;
	if (s->fm) s->fm->midiPitchControl(uint8_t(c), (value - 8192) / 8192.0);
	else fluid_synth_pitch_bend(s->sf.get(), c, value);
}
void music_synth_hmp_begin(music_synth *s, const hmp_tsf_state *state)
{
	if (!s->fm) s->sf.begin(state);
}
void music_synth_hmp_capture(music_synth *s, hmp_tsf_state *state)
{
	if (!s->fm) s->sf.capture(state);
}
void music_synth_hmp_control(music_synth *s, int c, int control, int value)
{
	if (s->fm) music_synth_channel_midi_control(s, c, control, value);
	else s->sf.control(c, control, value);
}

void music_synth_set_effects(music_synth *s, int reverb, int chorus)
{
	if (s) s->sf.effects(reverb != 0, chorus != 0);
}
