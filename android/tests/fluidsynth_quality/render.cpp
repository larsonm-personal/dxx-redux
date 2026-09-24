// Offline experiment: production MIDI scheduling, FluidSynth rendering, no audio device
#include <fluidsynth.h>
#define TML_IMPLEMENTATION
#define TML_NO_STDIO
#include "tml.h"
extern "C" {
#include "midi_seek_timeline.h"
}
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define CHECK(x)                                                      \
	do {                                                              \
		if (!(x)) {                                                   \
			std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
			std::exit(1);                                             \
		}                                                             \
	} while (0)

struct synth {
	fluid_settings_t *settings = nullptr;
	fluid_synth_t *fluid = nullptr;
	int id = -1;
	int chorus_send = 0;
	double worst_ms = 0;
	unsigned clipped = 0;
	int peak_voices = 0;

	synth(const char *sf2, int reverb, int chorus, int send, int voices = 48, int interpolation = FLUID_INTERP_7THORDER) : chorus_send(send)
	{
		settings = new_fluid_settings();
		CHECK(settings);
		CHECK(fluid_settings_setnum(settings, "synth.sample-rate", 48000) == FLUID_OK);
		CHECK(fluid_settings_setnum(settings, "synth.gain", 0.2) == FLUID_OK);
		CHECK(fluid_settings_setint(settings, "synth.polyphony", voices) == FLUID_OK);
		CHECK(fluid_settings_setint(settings, "synth.cpu-cores", 1) == FLUID_OK);
		CHECK(fluid_settings_setint(settings, "synth.threadsafe-api", 0) == FLUID_OK);
		CHECK(fluid_settings_setint(settings, "synth.reverb.active", reverb) == FLUID_OK);
		CHECK(fluid_settings_setint(settings, "synth.chorus.active", chorus) == FLUID_OK);
		// Pin the older FDN algorithm to isolate effects without changing engines between releases
		CHECK(fluid_settings_setstr(settings, "synth.reverb.engine", "fdn") == FLUID_OK);
		CHECK(fluid_settings_setnum(settings, "synth.reverb.room-size", 0.2) == FLUID_OK);
		CHECK(fluid_settings_setnum(settings, "synth.reverb.damp", 0.0) == FLUID_OK);
		CHECK(fluid_settings_setnum(settings, "synth.reverb.width", 0.5) == FLUID_OK);
		CHECK(fluid_settings_setnum(settings, "synth.reverb.level", 0.3) == FLUID_OK);
		CHECK(fluid_settings_setint(settings, "synth.chorus.nr", 3) == FLUID_OK);
		CHECK(fluid_settings_setnum(settings, "synth.chorus.level", 1.0) == FLUID_OK);
		CHECK(fluid_settings_setnum(settings, "synth.chorus.speed", 0.3) == FLUID_OK);
		CHECK(fluid_settings_setnum(settings, "synth.chorus.depth", 8.0) == FLUID_OK);
		CHECK(fluid_settings_setstr(settings, "synth.midi-bank-select", "gs") == FLUID_OK);
		fluid = new_fluid_synth(settings);
		CHECK(fluid);
		id = fluid_synth_sfload(fluid, sf2, 1);
		CHECK(id >= 0);
		CHECK(fluid_synth_set_interp_method(fluid, -1, interpolation) == FLUID_OK);
		for (int c = 0; c < 16; ++c) {
			// Match cold-start HMP volume; preserve the font/renderer's other defaults
			CHECK(fluid_synth_cc(fluid, c, 39, 0) == FLUID_OK);
			CHECK(fluid_synth_cc(fluid, c, 7, 0) == FLUID_OK);
			if (chorus_send && c != 9) CHECK(fluid_synth_cc(fluid, c, 93, chorus_send) == FLUID_OK);
		}
	}
	~synth()
	{
		if (fluid) delete_fluid_synth(fluid);
		if (settings) delete_fluid_settings(settings);
	}

	void control(int c, int cc, int value)
	{
		// HMI controller reset retains bank/pan, as in hmp_tsf_control
		if (cc == 121) {
			int bank_msb, bank_lsb, pan_msb, pan_lsb;
			CHECK(fluid_synth_get_cc(fluid, c, 0, &bank_msb) == FLUID_OK);
			CHECK(fluid_synth_get_cc(fluid, c, 32, &bank_lsb) == FLUID_OK);
			CHECK(fluid_synth_get_cc(fluid, c, 10, &pan_msb) == FLUID_OK);
			CHECK(fluid_synth_get_cc(fluid, c, 42, &pan_lsb) == FLUID_OK);
			CHECK(fluid_synth_cc(fluid, c, cc, value) == FLUID_OK);
			fluid_synth_cc(fluid, c, 0, bank_msb);
			fluid_synth_cc(fluid, c, 32, bank_lsb);
			fluid_synth_cc(fluid, c, 10, pan_msb);
			fluid_synth_cc(fluid, c, 42, pan_lsb);
			fluid_synth_cc(fluid, c, 39, 0);
			fluid_synth_cc(fluid, c, 64, 0);
			fluid_synth_pitch_bend(fluid, c, 8192);
		} else CHECK(fluid_synth_cc(fluid, c, cc, value) == FLUID_OK);
	}
	void render(short *out, int frames)
	{
		float pcm[512];
		while (frames) {
			const int count = std::min(frames, 256);
			const auto start = std::chrono::steady_clock::now();
			CHECK(fluid_synth_write_float(fluid, count, pcm, 0, 2, pcm, 1, 2) == FLUID_OK);
			peak_voices = std::max(peak_voices, fluid_synth_get_active_voice_count(fluid));
			worst_ms = std::max(worst_ms, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
			for (int i = 0; i < count * 2; ++i) {
				CHECK(std::isfinite(pcm[i]));
				if (pcm[i] < -1 || pcm[i] >= 1) ++clipped;
				out[i] = short(std::lround(std::clamp(double(pcm[i]) * 32768, -32768.0, 32767.0)));
			}
			out += count * 2;
			frames -= count;
		}
	}
};

static unsigned event_time(const void *event)
{
	return static_cast<const tml_message *>(event)->time;
}
static const void *event_next(const void *event)
{
	return static_cast<const tml_message *>(event)->next;
}
static void dispatch(void *context, const void *event)
{
	auto &s = *static_cast<synth *>(context);
	const auto &m = *static_cast<const tml_message *>(event);
	switch (m.type) {
		case TML_NOTE_ON:
			if (m.velocity) CHECK(fluid_synth_noteon(s.fluid, m.channel, m.key, m.velocity) == FLUID_OK);
			else fluid_synth_noteoff(s.fluid, m.channel, m.key);
			break;
		case TML_NOTE_OFF: fluid_synth_noteoff(s.fluid, m.channel, m.key); break;
		case TML_PROGRAM_CHANGE: CHECK(fluid_synth_program_change(s.fluid, m.channel, m.program) == FLUID_OK); break;
		case TML_CONTROL_CHANGE: s.control(m.channel, m.control, m.control_value); break;
		case TML_PITCH_BEND: CHECK(fluid_synth_pitch_bend(s.fluid, m.channel, m.pitch_bend) == FLUID_OK); break;
		default: break;
	}
}
static void render(void *context, short *out, int frames)
{
	static_cast<synth *>(context)->render(out, frames);
}
static const midi_seek_timeline_ops ops{ event_time, event_next, dispatch, render };

static void write_le(FILE *file, unsigned value, int bytes)
{
	for (int i = 0; i < bytes; ++i) CHECK(std::fputc((value >> (8 * i)) & 255, file) != EOF);
}

static void contracts(const char *sf2)
{
	// Pause does not advance the synth. Rendering in different buffer sizes must agree
	synth a(sf2, 0, 0, 0), b(sf2, 0, 0, 0), wet(sf2, 1, 1, 32);
	std::vector<short> aa(48000 * 2), bb(aa.size()), ww(aa.size());
	for (auto *s : { &a, &b, &wet }) {
		s->control(0, 7, 100);
		CHECK(fluid_synth_noteon(s->fluid, 0, 60, 100) == FLUID_OK);
	}
	a.render(aa.data(), 48000);
	for (int at = 0; at < 48000;) {
		const int n = std::min(137, 48000 - at);
		b.render(bb.data() + at * 2, n);
		at += n;
	}
	wet.render(ww.data(), 48000);
	CHECK(aa == bb);
	CHECK(aa != ww);
	CHECK(std::any_of(aa.begin(), aa.end(), [](short v) { return v != 0; }));
	a.control(0, 10, 23);
	a.control(0, 42, 17);
	a.control(0, 0, 8);
	a.control(0, 121, 0);
	int value;
	CHECK(fluid_synth_get_cc(a.fluid, 0, 10, &value) == FLUID_OK && value == 23);
	CHECK(fluid_synth_get_cc(a.fluid, 0, 42, &value) == FLUID_OK && value == 17);
	CHECK(fluid_synth_get_cc(a.fluid, 0, 0, &value) == FLUID_OK && value == 8);
	// System reset after controller modulation can leave a voice active in 2.6.1
	// Finish voices after the reset's controller changes, not before them
	CHECK(fluid_synth_system_reset(a.fluid) == FLUID_OK);
	CHECK(fluid_synth_all_sounds_off(a.fluid, -1) == FLUID_OK);
	a.render(aa.data(), 48000);
	CHECK(std::all_of(aa.begin() + 2048, aa.end(), [](short v) { return v == 0; }));
	CHECK(fluid_synth_get_active_voice_count(a.fluid) == 0);

	// Real scheduler reconstruction must reproduce the same wet audio after a seek
	synth continuous(sf2, 1, 1, 24), seeking(sf2, 1, 1, 24);
	tml_message events[3]{};
	events[0].type = TML_CONTROL_CHANGE;
	events[0].control = 7;
	events[0].control_value = 100;
	events[1].type = TML_NOTE_ON;
	events[1].key = 60;
	events[1].velocity = 100;
	events[2].type = TML_NOTE_OFF;
	events[2].key = 60;
	events[2].time = 500;
	events[0].next = &events[1];
	events[1].next = &events[2];
	midi_seek_timeline ta, tb;
	midi_seek_timeline_init(&ta, events, 48000, 2, &continuous, &ops);
	midi_seek_timeline_init(&tb, events, 48000, 2, &seeking, &ops);
	CHECK(midi_seek_timeline_set_range(&ta, events, 0, 1000));
	CHECK(midi_seek_timeline_set_range(&tb, events, 0, 1000));
	std::vector<short> all(120000 * 2), rebuilt(120000 * 2);
	CHECK(midi_seek_timeline_render(&ta, all.data(), 120000) == 120000);
	short scratch[512];
	int prefill;
	const int target = 12345;
	CHECK(midi_seek_timeline_reconstruct(&tb, target, scratch, 256, &prefill));
	std::copy(scratch, scratch + prefill * 2, rebuilt.begin());
	CHECK(midi_seek_timeline_render(&tb, rebuilt.data() + prefill * 2, 120000 - target - prefill) == 120000 - target - prefill);
	CHECK(std::equal(all.begin() + target * 2, all.end(), rebuilt.begin()));
	CHECK(ta.frame == tb.frame && ta.frame == 24000);
	std::puts("PASS buffer-size invariance, audible effects, HMI controller retention, reset silence, wet seek and loop continuity");
}

int main(int argc, char **argv)
{
	if (argc == 3 && !std::strcmp(argv[1], "--test")) {
		contracts(argv[2]);
		return 0;
	}
	if (argc != 8 && argc != 11) {
		std::fprintf(stderr, "Usage: fluid_render font.sf2 input.mid output.wav seconds reverb chorus melodic_chorus_send [voices channel_or_minus1 interpolation]\n");
		return 2;
	}
	const int seconds = std::atoi(argv[4]), reverb = std::atoi(argv[5]), chorus = std::atoi(argv[6]), send = std::atoi(argv[7]);
	CHECK(seconds > 0 && seconds <= 600 && reverb >= 0 && reverb <= 1 && chorus >= 0 && chorus <= 1 && send >= 0 && send <= 127);
	FILE *file = std::fopen(argv[2], "rb");
	CHECK(file && !std::fseek(file, 0, SEEK_END));
	const long length = std::ftell(file);
	CHECK(length > 0 && length < 64 * 1024 * 1024 && !std::fseek(file, 0, SEEK_SET));
	std::vector<unsigned char> data(static_cast<size_t>(length));
	CHECK(std::fread(data.data(), 1, data.size(), file) == data.size());
	std::fclose(file);
	tml_message *messages = tml_load_memory(data.data(), int(data.size()));
	CHECK(messages);
	const int voices = argc == 11 ? std::atoi(argv[8]) : 48;
	const int channel = argc == 11 ? std::atoi(argv[9]) : -1;
	const int interpolation = argc == 11 ? std::atoi(argv[10]) : FLUID_INTERP_7THORDER;
	CHECK(voices >= 16 && voices <= 256 && channel >= -1 && channel < 16);
	CHECK(interpolation == 0 || interpolation == 1 || interpolation == 4 || interpolation == 7);
	if (channel >= 0)
		for (auto *m = messages; m; m = m->next)
			if (m->type == TML_NOTE_ON && m->channel != channel) m->velocity = 0;
	synth s(argv[1], reverb, chorus, send, voices, interpolation);
	midi_seek_timeline timeline;
	midi_seek_timeline_init(&timeline, messages, 48000, 2, &s, &ops);
	const int frames = seconds * 48000;
	std::vector<short> pcm(frames * 2);
	const auto start = std::chrono::steady_clock::now();
	CHECK(midi_seek_timeline_render(&timeline, pcm.data(), frames) == frames);
	const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	file = std::fopen(argv[3], "wb");
	CHECK(file);
	CHECK(std::fwrite("RIFF", 1, 4, file) == 4);
	write_le(file, 36 + frames * 4, 4);
	CHECK(std::fwrite("WAVEfmt ", 1, 8, file) == 8);
	write_le(file, 16, 4);
	write_le(file, 1, 2);
	write_le(file, 2, 2);
	write_le(file, 48000, 4);
	write_le(file, 192000, 4);
	write_le(file, 4, 2);
	write_le(file, 16, 2);
	CHECK(std::fwrite("data", 1, 4, file) == 4);
	write_le(file, frames * 4, 4);
	for (short sample : pcm) write_le(file, static_cast<unsigned short>(sample), 2);
	CHECK(!std::fclose(file));
	std::printf("{\"version\":\"%s\",\"seconds\":%d,\"render_seconds\":%.6f,\"worst_render_call_ms\":%.6f,\"clipped_samples\":%u,\"peak_voices\":%d}\n", fluid_version_str(), seconds, elapsed, s.worst_ms, s.clipped, s.peak_voices);
	tml_free(messages);
}
