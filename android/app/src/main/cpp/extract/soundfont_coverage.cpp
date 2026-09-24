// Diagnostic only: inspect the same pinned TSF's expanded regions and selection
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
// Compile the actual validated loader in this diagnostic to inspect TSF internals
#include "tsf_impl.c"
#include "hmp_tsf_state.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <tuple>
#include <vector>

#define CHECK(x)                                                      \
	do {                                                              \
		if (!(x)) {                                                   \
			std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
			std::exit(1);                                             \
		}                                                             \
	} while (0)

struct usage {
	unsigned count = 0, first_ms = 0;
};
struct sound {
	int regions = 0;
	double peak = 0;
};

static std::vector<unsigned char> read_file(const char *path)
{
	FILE *file = std::fopen(path, "rb");
	CHECK(file && !std::fseek(file, 0, SEEK_END));
	const long length = std::ftell(file);
	CHECK(length > 0 && length < 64 * 1024 * 1024 && !std::fseek(file, 0, SEEK_SET));
	std::vector<unsigned char> data((size_t) length);
	CHECK(std::fread(data.data(), 1, data.size(), file) == data.size());
	std::fclose(file);
	return data;
}

static sound inspect(tsf *bank, int preset, int key, int velocity)
{
	sound result;
	if (preset < 0 || preset >= bank->presetNum) return result;
	const auto &p = bank->presets[preset];
	for (int i = 0; i < p.regionNum; ++i) {
		const auto &r = p.regions[i];
		if (key >= r.lokey && key <= r.hikey && velocity >= r.lovel && velocity <= r.hivel && r.end > r.offset)
			++result.regions;
	}
	if (!result.regions) return result;
	// Fresh voices and neutral controllers isolate coverage from deliberate song mutes
	tsf *probe = tsf_copy(bank);
	CHECK(probe);
	tsf_set_output(probe, TSF_STEREO_INTERLEAVED, 48000, -10);
	CHECK(tsf_note_on(probe, preset, key, velocity / 127.0f));
	float pcm[1024];
	// Stop once audible; allow slow-attack samples up to two seconds
	for (int frames = 0; frames < 96000 && result.peak <= 1e-12; frames += 512) {
		tsf_render_float(probe, pcm, 512, 0);
		for (float sample : pcm) {
			CHECK(std::isfinite(sample));
			result.peak = std::max(result.peak, std::abs(double(sample)));
		}
	}
	tsf_close(probe);
	return result;
}

int main(int argc, char **argv)
{
	CHECK(argc == 2 || argc == 3);
	tsf *bank = music_soundfont_load(nullptr, argv[1]);
	if (!bank) {
		const auto data = read_file(argv[1]);
		const auto samples = music_soundfont_validate(data.data(), data.size());
		std::fprintf(stderr, "Rejected SF2: validated samples=%zu\n", samples);
		if (samples) {
			tsf *raw = tsf_load_memory(data.data(), int(data.size()));
			CHECK(raw);
			for (int p = 0; p < raw->presetNum; ++p)
				for (int i = 0; i < raw->presets[p].regionNum; ++i) {
					const auto &r = raw->presets[p].regions[i];
					if (r.offset >= r.end || r.end > samples || (r.loop_mode && (r.loop_start >= r.loop_end || r.loop_end >= r.end)))
						std::fprintf(stderr, "preset=%d region=%d sample=%u..%u loop=%u..%u mode=%d\n", p, i, r.offset, r.end, r.loop_start, r.loop_end, r.loop_mode);
				}
			tsf_close(raw);
		}
	}
	CHECK(bank);
	if (argc == 2) {
		std::printf("Loaded %d presets\n", tsf_get_presetcount(bank));
		tsf_close(bank);
		return 0;
	}
	const auto midi = read_file(argv[2]);
	tml_message *events = tml_load_memory(midi.data(), int(midi.size()));
	CHECK(events);
	tsf_set_output(bank, TSF_STEREO_INTERLEAVED, 48000, -10);
	const hmp_tsf_state initial{};
	hmp_tsf_begin(bank, &initial);
	int program[16] = {}, requested_bank[16] = {};
	// Channel / bank at last program change / program / resolved preset / key / velocity
	using key_type = std::tuple<int, int, int, int, int, int>;
	std::map<key_type, usage> used;
	for (const tml_message *m = events; m; m = m->next) {
		if (m->type < TML_NOTE_OFF || m->type >= 0xf0) continue;
		const int c = m->channel;
		CHECK(c >= 0 && c < 16);
		if (m->type == TML_PROGRAM_CHANGE) {
			program[c] = (unsigned char) m->program;
			requested_bank[c] = tsf_channel_get_preset_bank(bank, c) & 0x7fff;
			if (c == 9) requested_bank[c] |= 128;
			tsf_channel_set_presetnumber(bank, c, program[c], c == 9);
		} else if (m->type == TML_CONTROL_CHANGE) {
			hmp_tsf_control(bank, c, m->control, m->control_value);
		} else if (m->type == TML_NOTE_ON && m->velocity) {
			const int preset = tsf_channel_get_preset_index(bank, c);
			auto &entry = used[{ c, requested_bank[c], program[c], preset, (unsigned char) m->key, (unsigned char) m->velocity }];
			if (!entry.count) entry.first_ms = m->time;
			++entry.count;
		}
	}
	std::map<std::tuple<int, int, int>, sound> cache;
	std::puts("channel\trequested_bank\tprogram\tpreset\tkey\tvelocity\tnote_count\tfirst_ms\texact_preset\tactual_bank\tactual_program\tregions\tpeak\tname");
	for (const auto &entry : used) {
		int channel, requested, prog, preset, key, velocity;
		std::tie(channel, requested, prog, preset, key, velocity) = entry.first;
		const auto id = std::make_tuple(preset, key, velocity);
		if (!cache.count(id)) cache[id] = inspect(bank, preset, key, velocity);
		const sound &audio = cache[id];
		const auto &p = bank->presets[preset];
		std::printf("%d\t%d\t%d\t%d\t%d\t%d\t%u\t%u\t%d\t%u\t%u\t%d\t%.9g\t",
		            channel + 1, requested, prog, preset, key, velocity, entry.second.count, entry.second.first_ms,
		            tsf_get_presetindex(bank, requested, prog) == preset, unsigned(p.bank), unsigned(p.preset), audio.regions, audio.peak);
		for (int i = 0; i < 20 && p.presetName[i]; ++i) std::putchar(p.presetName[i] >= 32 && p.presetName[i] < 127 ? p.presetName[i] : ' ');
		std::putchar('\n');
	}
	tml_free(events);
	tsf_close(bank);
}
