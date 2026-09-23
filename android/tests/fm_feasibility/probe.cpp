// Offline experiment: no dependency on the game or its audio device
#include "emu8950.h"
#include "player.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

// Optional register instrumentation for the separately generated BSD source
std::ofstream fm_trace;
uint32_t fm_trace_frame = 0;
void fm_trace_write(int chip, uint16_t reg, uint8_t value)
{
	if (fm_trace) fm_trace << fm_trace_frame << ' ' << (chip * 512 + reg) << ' ' << unsigned(value) << '\n';
}

static void le(std::ostream &out, uint32_t value, unsigned bytes)
{
	for (unsigned i = 0; i < bytes; ++i)
		out.put(static_cast<char>(value >> (8 * i)));
}

static void wav(const std::string &path, const std::vector<int16_t> &pcm, uint32_t rate)
{
	std::ofstream out(path, std::ios::binary);
	if (!out) throw std::runtime_error("Cannot open WAV output");
	const auto bytes = static_cast<uint32_t>(pcm.size() * 2);
	out.write("RIFF", 4);
	le(out, 36 + bytes, 4);
	out.write("WAVEfmt ", 8);
	le(out, 16, 4);
	le(out, 1, 2);
	le(out, 2, 2);
	le(out, rate, 4);
	le(out, rate * 4, 4);
	le(out, 4, 2);
	le(out, 16, 2);
	out.write("data", 4);
	le(out, bytes, 4);
	for (int16_t sample : pcm) le(out, static_cast<uint16_t>(sample), 2);
	if (!out) throw std::runtime_error("WAV write failed");
}

static void setup(OPLPlayer &player, const char *bank, uint32_t rate)
{
	player.setSampleRate(rate);
	player.setGain(1.0);
	player.setFilter(0.0);
	player.setStereo(false);
	player.setLoop(false);
	if (!player.loadPatches(bank)) throw std::runtime_error("Bank load failed");
}

static void song(const std::string &mode, const char *bank, const char *sequence,
                 std::vector<int16_t> &pcm, uint32_t rate)
{
	OPLPlayer player(1, mode == "file-opl3" ? OPLPlayer::ChipOPL3 : OPLPlayer::ChipOPL2);
	setup(player, bank, rate);
	const auto frames = static_cast<uint32_t>(pcm.size() / 2);
	if (mode != "live-opl2") {
		if (!player.loadSequence(sequence)) throw std::runtime_error("Sequence load failed");
		player.generate(pcm.data(), frames);
		return;
	}
	std::ifstream input(sequence);
	if (!input) throw std::runtime_error("Cannot read event stream");
	uint32_t frame, cursor = 0;
	unsigned status, a, b;
	while (input >> frame >> status >> a >> b) {
		if (frame > frames) break;
		if (frame < cursor || status < 128 || status >= 240 || a > 127 || b > 127)
			throw std::runtime_error("Invalid event stream");
		if (frame > cursor) player.generate(pcm.data() + cursor * 2, frame - cursor);
		fm_trace_frame = frame;
		player.midiEvent(static_cast<uint8_t>(status), static_cast<uint8_t>(a), static_cast<uint8_t>(b));
		cursor = frame;
	}
	if (cursor < frames) player.generate(pcm.data() + cursor * 2, frames - cursor);
}

static void registers(const std::string &mode, const char *path, std::vector<int16_t> &pcm)
{
	ymfm::ymfm_interface interface;
	ymfm::ymf262 opl3(interface);
	opl3.reset();
	ymfm::ym3812 left(interface), right(interface);
	left.reset();
	right.reset();
	OPL *emu[2] = { OPL_new(3579552, 49716), OPL_new(3579552, 49716) };
	if (!emu[0] || !emu[1]) throw std::runtime_error("OPL allocation failed");
	for (auto *chip : emu) {
		OPL_setChipType(chip, 2);
		OPL_reset(chip);
	}
	std::ifstream input(path);
	if (!input) throw std::runtime_error("Cannot read register trace");
	uint32_t next = 0, previous = 0;
	unsigned reg = 0, value = 0;
	bool pending = bool(input >> next >> reg >> value);
	for (uint32_t frame = 0; frame < pcm.size() / 2; ++frame) {
		while (pending && next <= frame) {
			if (next < previous || reg >= 512 || value > 255)
				throw std::runtime_error("Invalid register trace");
			previous = next;
			const auto byte = static_cast<uint8_t>(value);
			if (mode == "registers-opl3") {
				if (reg < 256) opl3.write_address(static_cast<uint8_t>(reg));
				else opl3.write_address_hi(static_cast<uint8_t>(reg));
				opl3.write_data(byte);
			} else if ((reg & 255) != 5 && (reg & 255) != 4) {
				// Diagnostic dual-OPL2 approximation: captured bank 0 is right, bank 1 left
				auto &chip = reg < 256 ? right : left;
				chip.write_address(static_cast<uint8_t>(reg));
				chip.write_data(byte);
				OPL_writeReg(emu[reg >> 8], reg & 255, byte);
			}
			pending = bool(input >> next >> reg >> value);
		}
		int samples[2];
		if (mode == "registers-opl3") {
			ymfm::ymf262::output_data out;
			opl3.generate(&out);
			samples[0] = out.data[0];
			samples[1] = out.data[1];
		} else if (mode == "registers-ymfm") {
			ymfm::ym3812::output_data l, r;
			left.generate(&l);
			right.generate(&r);
			samples[0] = l.data[0];
			samples[1] = r.data[0];
		} else {
			samples[0] = OPL_calc(emu[1]);
			samples[1] = OPL_calc(emu[0]);
		}
		for (int channel = 0; channel < 2; ++channel)
			pcm[frame * 2 + channel] = static_cast<int16_t>(std::max(-32768, std::min(32767, samples[channel])));
	}
	for (auto *chip : emu) OPL_delete(chip);
}

// Same nine-voice register stimulus and native sample rate for both OPL2 cores
template <typename Writer>
static void tone_registers(Writer write, bool on)
{
	static const int op[] = { 0, 1, 2, 8, 9, 10, 16, 17, 18 };
	write(1, 0x20);
	for (unsigned ch = 0; ch < 9; ++ch) {
		write(0x20 + op[ch], 0x21);
		write(0x23 + op[ch], 0x21);
		write(0x40 + op[ch], 0x18);
		write(0x43 + op[ch], 0x10);
		write(0x60 + op[ch], 0xf2);
		write(0x63 + op[ch], 0xf2);
		write(0x80 + op[ch], 0x44);
		write(0x83 + op[ch], 0x44);
		write(0xc0 + ch, 0x04);
		write(0xa0 + ch, 0x44 + ch * 3);
		write(0xb0 + ch, (on ? 0x20 : 0) | 0x11);
	}
}

static void tone(const std::string &mode, std::vector<int16_t> &pcm)
{
	ymfm::ymfm_interface interface;
	ymfm::ym3812 chip(interface);
	chip.reset();
	OPL *emu = OPL_new(3579552, 49716);
	if (!emu) throw std::runtime_error("OPL allocation failed");
	OPL_setChipType(emu, 2);
	OPL_reset(emu);
	auto write = [&](unsigned reg, unsigned value) {
		if (mode == "tone-ymfm") {
			chip.write_address(static_cast<uint8_t>(reg));
			chip.write_data(static_cast<uint8_t>(value));
		} else OPL_writeReg(emu, reg, static_cast<uint8_t>(value));
	};
	tone_registers(write, true);
	for (size_t frame = 0; frame < pcm.size() / 2; ++frame) {
		if (frame == pcm.size() / 4) tone_registers(write, false);
		int sample;
		if (mode == "tone-ymfm") {
			ymfm::ym3812::output_data out;
			chip.generate(&out);
			sample = out.data[0];
		} else sample = OPL_calc(emu);
		pcm[frame * 2] = pcm[frame * 2 + 1] = static_cast<int16_t>(std::max(-32768, std::min(32767, sample)));
	}
	OPL_delete(emu);
}

int main(int argc, char **argv)
{
	try {
		if (argc != 6 && argc != 7) throw std::runtime_error("Usage: fm_probe MODE BANK SEQUENCE OUTPUT SECONDS [REGISTER_TRACE]");
		const std::string mode = argv[1];
		const bool chipOnly = mode == "tone-ymfm" || mode == "tone-emu8950";
		const bool registerReplay = mode == "registers-opl3" || mode == "registers-ymfm" || mode == "registers-emu8950";
		if (!chipOnly && !registerReplay && mode != "file-opl2" && mode != "file-opl3" && mode != "live-opl2")
			throw std::runtime_error("Unknown renderer mode");
		const double seconds = std::stod(argv[5]);
		if (!(seconds > 0 && seconds <= 600)) throw std::runtime_error("Duration must be in (0, 600]");
		const uint32_t rate = chipOnly || registerReplay ? 49716 : 48000;
		if (argc == 7) {
			if (mode != "live-opl2") throw std::runtime_error("Register instrumentation requires live-opl2");
			fm_trace.open(argv[6]);
			if (!fm_trace) throw std::runtime_error("Cannot create register trace");
		}
		std::vector<int16_t> pcm(static_cast<size_t>(seconds * rate) * 2);
		const auto start = std::chrono::steady_clock::now();
		if (chipOnly) tone(mode, pcm);
		else if (registerReplay) registers(mode, argv[3], pcm);
		else song(mode, argv[2], argv[3], pcm, rate);
		const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
		wav(argv[4], pcm, rate);
		std::cout << "{\"mode\":\"" << mode << "\",\"seconds\":" << seconds
		          << ",\"render_seconds\":" << elapsed << ",\"realtime_multiple\":" << seconds / elapsed << "}\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
