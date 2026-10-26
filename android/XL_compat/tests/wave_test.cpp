#include "wave.h"
#include "scoped_sound.h"
#include <set>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using Bytes = std::vector<std::uint8_t>;
static void check(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}
static void put(Bytes &b, std::uint32_t value, unsigned count)
{
	while (count--) {
		b.push_back(std::uint8_t(value));
		value >>= 8;
	}
}
static void size_header(Bytes &b)
{
	const auto size = b.size() - 8;
	for (unsigned i = 0; i < 4; ++i) b[4 + i] = std::uint8_t(size >> (i * 8));
}
static void chunk(Bytes &b, const char *tag, const Bytes &value)
{
	b.insert(b.end(), tag, tag + 4);
	put(b, static_cast<std::uint32_t>(value.size()), 4);
	b.insert(b.end(), value.begin(), value.end());
	if (value.size() & 1) b.push_back(0);
}
static Bytes wave(unsigned channels, unsigned bits, const Bytes &samples)
{
	Bytes b{ 'R', 'I', 'F', 'F', 0, 0, 0, 0, 'W', 'A', 'V', 'E' }, f;
	put(f, 1, 2);
	put(f, channels, 2);
	put(f, 44100, 4);
	put(f, 44100 * channels * bits / 8, 4);
	put(f, channels * bits / 8, 2);
	put(f, bits, 2);
	chunk(b, "JUNK", { 1 });
	chunk(b, "data", samples);
	chunk(b, "fmt ", f);
	size_header(b);
	return b;
}
static void reject(const Bytes &b)
{
	try {
		(void) xl_compat::decode_wave(b.data(), b.size());
	} catch (const std::runtime_error &) {
		return;
	}
	throw std::runtime_error("Malformed WAV accepted");
}
static void original(const char *path, std::size_t frames, std::uint64_t hash)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	check(bool(file) && file.tellg() > 0 && file.tellg() <= std::streamoff(xl_compat::max_wave_bytes), "Original WAV unavailable or too large");
	Bytes bytes(static_cast<std::size_t>(file.tellg()));
	file.seekg(0);
	file.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	check(bool(file), "Short original WAV read");
	const auto result = xl_compat::decode_wave(bytes.data(), bytes.size());
	check(result.rate == 44100 && result.source_channels == 2 && result.source_bits == 16 && result.mono.size() == frames, "Original WAV format or duration changed");
	std::uint64_t actual = 14695981039346656037ull;
	for (const auto sample : result.mono) {
		const auto value = static_cast<std::uint16_t>(sample);
		actual = (actual ^ (value & 255)) * 1099511628211ull;
		actual = (actual ^ (value >> 8)) * 1099511628211ull;
	}
	check(actual == hash, "Original mono conversion differs from independent golden samples");
}
int main(int argc, char **argv)
{
	try {
		xl_compat::WavePcm playback;
		playback.rate = 44100;
		playback.mono = { -32768, -1, 0, 32767 };
		check(xl_compat::wave_unsigned8(playback, 44100) == Bytes({ 0, 127, 128, 255 }), "Unsigned playback extrema and silence");
		check(xl_compat::wave_unsigned8(playback, 22050) == Bytes({ 0, 128 }), "Fixed-rate playback frame selection");
		check(xl_compat::wave_unsigned8(playback, 88200) == Bytes({ 0, 0, 127, 127, 128, 128, 255, 255 }), "Playback upsample selection");
		std::set<std::string> owned{ "mods/pilot/sounds/boss01.wav", "mods/pilot/sounds/level01/boss01.wav", "mods/pilot/sounds/slevel01/boss01.wav" };
		const auto lookup = [&](const std::string &path) { return owned.count(path) ? path : std::string{}; };
		check(xl_compat::resolve_scoped_sound("pilot", "boss01", 1, 22050, lookup).scope == "level", "Level sound precedence");
		check(xl_compat::resolve_scoped_sound("pilot", "boss01", -1, 22050, lookup).scope == "secret_level", "Signed secret ordinal lost");
		check(xl_compat::resolve_scoped_sound("pilot", "boss01", 2, 22050, lookup).scope == "mission", "Mission fallback lost");
		check(xl_compat::resolve_scoped_sound("other", "boss01", 1, 22050, lookup).path.empty(), "Sibling sound leaked");
		owned.insert("mods/pilot/pilot.s22");
		check(xl_compat::resolve_scoped_sound("pilot", "boss01", 1, 22050, lookup).bank, "Mission bank did not suppress WAV");
		check(!xl_compat::resolve_scoped_sound("pilot", "boss01", 1, 11025, lookup).bank, "Wrong-rate bank suppressed WAV");
		owned.insert("mods/pilot/pilot.s11");
		check(xl_compat::resolve_scoped_sound("pilot", "boss01", 1, 11025, lookup).bank, "Low-rate bank gating lost");
		for (const char *invalid : { "..", "../pilot", "C:pilot", "pilot\\other", "" }) {
			bool rejected = false;
			try {
				(void) xl_compat::resolve_scoped_sound(invalid, "boss01", 1, 22050, lookup);
			} catch (const std::runtime_error &) {
				rejected = true;
			}
			check(rejected, "Invalid sound identity accepted");
		}
		const auto bytes = wave(2, 16, { 0, 128, 0, 128, 255, 127, 255, 127, 0, 128, 255, 127, 0, 0, 0, 0 });
		const auto decoded = xl_compat::decode_wave(bytes.data(), bytes.size());
		check(decoded.mono == std::vector<std::int16_t>({ -32768, 32767, 0, 0 }), "Stereo clipping, cancellation or silence mismatch");
		const auto u8 = wave(1, 8, { 0, 128, 255 });
		check(xl_compat::decode_wave(u8.data(), u8.size()).mono == std::vector<std::int16_t>({ -32768, 0, 32512 }), "Unsigned PCM bias mismatch");
		for (std::size_t i = 0; i < bytes.size(); ++i) {
			Bytes truncated(bytes.begin(), bytes.begin() + i);
			reject(truncated);
			if (i >= 12) {
				size_header(truncated);
				reject(truncated);
			}
		}
		for (unsigned channels : { 0u, 3u }) reject(wave(channels, 16, { 0, 0, 0, 0 }));
		reject(wave(1, 24, { 0, 0, 0 }));
		reject(wave(2, 16, { 0, 0 }));
		reject(wave(1, 8, {}));
		reject(Bytes(xl_compat::max_wave_bytes + 1, 0));
		reject(wave(1, 8, Bytes(xl_compat::max_wave_bytes / 2 + 1, 128)));
		auto duplicate = bytes;
		chunk(duplicate, "data", { 0, 0, 0, 0 });
		size_header(duplicate);
		reject(duplicate);
		duplicate = bytes;
		chunk(duplicate, "fmt ", Bytes(bytes.end() - 16, bytes.end()));
		size_header(duplicate);
		reject(duplicate);
		auto excessive = bytes;
		for (int i = 0; i < 1024; ++i) chunk(excessive, "JUNK", {});
		size_header(excessive);
		reject(excessive);
		auto bad = bytes;
		bad[bad.size() - 16] = 3;
		reject(bad); // Float codec
		bad = bytes;
		bad[bad.size() - 12] = 0;
		reject(bad); // Inconsistent rate
		bad = bytes;
		bad[bad.size() - 4] = 0;
		reject(bad); // Invalid block alignment
		bad = bytes;
		chunk(bad, "JUNK", { 0 });
		bad.pop_back();
		size_header(bad);
		reject(bad);
		if (argc == 3) {
			original(argv[1], 135128, 0x6c6729efe4dd2fa6ull);
			original(argv[2], 120578, 0x3c272da347f23ae0ull);
		}
		std::cout << "PCM WAV bounds and mono conversion passed; no resolver or playback qualification\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
