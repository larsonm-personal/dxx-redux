// Included inside classic_reference; exact source startup with byte-flow adapters
using Uint8 = std::uint8_t;
struct StartupBuffer {
	Uint8 *data = nullptr;
	int length = 0;
	Uint8 *Buffer()
	{
		return data;
	}
	int Length() const
	{
		return length;
	}
	void SetBuffer(Uint8 *value, int, int count)
	{
		data = value;
		length = count;
	}
};
struct StartupChunk {
	enum Route { decoded_wave,
		         quick_wave,
		         raw_stock } route;
	Uint8 *bytes;
	int length;
};
StartupChunk startup_chunk{};
StartupBuffer startup_stream;
StartupBuffer *SDL_RWFromMem(Uint8 *bytes, int length)
{
	startup_stream = { bytes, length };
	return &startup_stream;
}
StartupChunk *Mix_LoadWAV_RW(StartupBuffer *stream, int)
{
	startup_chunk = { StartupChunk::decoded_wave, stream->data, stream->length };
	return stream->data && stream->length ? &startup_chunk : nullptr;
}
StartupChunk *Mix_QuickLoad_WAV(Uint8 *bytes)
{
	startup_chunk = { StartupChunk::quick_wave, bytes, -1 };
	return bytes ? &startup_chunk : nullptr;
}
StartupChunk *Mix_QuickLoad_RAW(Uint8 *bytes, int length)
{
	startup_chunk = { StartupChunk::raw_stock, bytes, length };
	return bytes && length ? &startup_chunk : nullptr;
}
struct StartupSample {
	StartupBuffer data[2];
	int nLength[2]{}, bCustom = 0, bHires = 0;
};
struct StartupChannel {
	struct {
		StartupBuffer sample;
		StartupChunk *pMixChunk = nullptr;
	} m_info;
	int conversions = 0, speedups = 0;
	int Resample(StartupSample *sound, int, int)
	{
		++conversions;
		m_info.sample = sound->data[sound->bCustom];
		return m_info.sample.Length();
	}
	int Speedup(StartupSample *, int)
	{
		++speedups;
		return m_info.sample.Length();
	}
	int Start(StartupSample *pSound, int nSpeed)
	{
#define RETVAL(value) return value;
#define I2X(value)    ((value) * 65536)
#define MAKE_WAV      0
#include "xl_reference_classic_start.h"
#undef MAKE_WAV
#undef I2X
#undef RETVAL
		return m_info.pMixChunk ? 1 : -1;
	}
};
void classic_start_controls()
{
	unsigned cases = 0;
	for (int profile : { 0, 1, 2 })
		for (int high : { 0, 1 })
			for (int custom : { 0, 1 })
				for (int speed : { 32768, 65536 }) {
					options.sound.bHires[0] = profile;
					Bytes first{ 1, 2, 3, 4 }, second{ 5, 6, 7, 8 };
					StartupSample sample{ { { first.data(), 4 }, { second.data(), 4 } }, { 4, 4 }, custom, high };
					StartupChannel channel;
					const auto result = channel.Start(&sample, speed);
					if (high && !profile) {
						require(result == -1 && !startup_chunk.bytes && !startup_chunk.length && !channel.conversions,
						        "Pinned mixed-bank startup no longer exposes the empty channel-buffer defect");
					} else {
						require(result == 1 && startup_chunk.bytes == sample.data[custom].data,
						        "Pinned startup selected different sample bytes");
						require(startup_chunk.route == (high ? StartupChunk::quick_wave : StartupChunk::raw_stock) &&
						            channel.conversions == !high && channel.speedups == int(!high && speed < 65536),
						        "Pinned replacement/stock startup route or speed decision differs");
					}
					++cases;
				}
	std::cout << "Classic startup source: " << cases << " exact dispatch cases; mixed-bank empty-buffer defect reproduced, high-resolution WAV bypass and stock conversion distinguished\n";
}
