#ifndef DXX_MAC_D1_SOUND_RESOURCE_H
#define DXX_MAC_D1_SOUND_RESOURCE_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Initial supported bank: retail MacPlay D1, snd IDs 10000..10097
 * Keep the original resource fork as descent.rsrc; no generated game assets
 * are distributed with the application */
#define MAC_D1_SOUND_COUNT        98
#define MAC_D1_RESOURCE_FILE      "descent.rsrc"
#define MAC_D1_RESOURCE_MAX_BYTES (16u * 1024u * 1024u)

struct mac_d1_sample {
	const unsigned char *data;
	uint32_t length;
};

static uint32_t mac_d1_be16(const unsigned char *p)
{
	return ((uint32_t) p[0] << 8) | p[1];
}

static uint32_t mac_d1_be32(const unsigned char *p)
{
	return (mac_d1_be16(p) << 16) | mac_d1_be16(p + 2);
}

static int mac_d1_range(size_t offset, size_t length, size_t size)
{
	return offset <= size && length <= size - offset;
}

/* Validate the whole bank before exposing samples or registering any sounds
 * Only standard mono 11025 Hz Sound Manager format-2 bufferCmd resources are
 * supported; compressed/extended sound headers must not be treated as PCM */
static int mac_d1_parse_sound_resource(const unsigned char *bytes, size_t size,
                                       struct mac_d1_sample samples[MAC_D1_SOUND_COUNT])
{
	if (!bytes || !samples || size < 16 || size > MAC_D1_RESOURCE_MAX_BYTES) return 0;
	memset(samples, 0, sizeof(*samples) * MAC_D1_SOUND_COUNT);
	const size_t data_offset = mac_d1_be32(bytes);
	const size_t map_offset = mac_d1_be32(bytes + 4);
	const size_t data_size = mac_d1_be32(bytes + 8);
	const size_t map_size = mac_d1_be32(bytes + 12);
	if (!mac_d1_range(data_offset, data_size, size) ||
	    !mac_d1_range(map_offset, map_size, size) || map_size < 28) return 0;
	const unsigned char *map = bytes + map_offset;
	const size_t types = mac_d1_be16(map + 24);
	const size_t names = mac_d1_be16(map + 26);
	if (!mac_d1_range(types, 2, map_size) || names > map_size) return 0;
	const uint32_t type_count = mac_d1_be16(map + types) + 1;
	if (!mac_d1_range(types + 2, type_count * 8, map_size)) return 0;
	int found = 0;
	size_t sample_bytes = 0;
	for (uint32_t type = 0; type < type_count; ++type) {
		const unsigned char *entry = map + types + 2 + type * 8;
		if (memcmp(entry, "snd ", 4)) continue;
		if (found || mac_d1_be16(entry + 4) + 1 != MAC_D1_SOUND_COUNT) return 0;
		const size_t refs = types + mac_d1_be16(entry + 6);
		if (!mac_d1_range(refs, MAC_D1_SOUND_COUNT * 12, map_size)) return 0;
		for (int i = 0; i < MAC_D1_SOUND_COUNT; ++i) {
			const unsigned char *ref = map + refs + i * 12;
			const uint32_t id = mac_d1_be16(ref);
			if (id < 10000 || id >= 10000 + MAC_D1_SOUND_COUNT) return 0;
			const unsigned index = id - 10000;
			if (samples[index].data) return 0;
			const size_t name = names + mac_d1_be16(ref + 2);
			char expected[] = "SND0000.AIF";
			expected[5] = (char) ('0' + index / 10);
			expected[6] = (char) ('0' + index % 10);
			if (!mac_d1_range(name, sizeof(expected), map_size) ||
			    map[name] != sizeof(expected) - 1 || memcmp(map + name + 1, expected, sizeof(expected) - 1)) return 0;
			const size_t offset = ((size_t) ref[5] << 16) | ((size_t) ref[6] << 8) | ref[7];
			if (!mac_d1_range(offset, 4, data_size)) return 0;
			const unsigned char *resource = bytes + data_offset + offset;
			const size_t length = mac_d1_be32(resource);
			if (!mac_d1_range(offset + 4, length, data_size) || length < 36) return 0;
			resource += 4;
			if (mac_d1_be16(resource) != 2 || mac_d1_be16(resource + 4) != 1 ||
			    mac_d1_be16(resource + 6) != 0x8050 || mac_d1_be16(resource + 8) != 0) return 0;
			const size_t header = mac_d1_be32(resource + 10);
			if (header < 14 || !mac_d1_range(header, 22, length)) return 0;
			const unsigned char *sound = resource + header;
			const uint32_t sample_length = mac_d1_be32(sound + 4);
			if (mac_d1_be32(sound) != 0 || mac_d1_be32(sound + 8) != (11025u << 16) ||
			    sound[20] != 0 || !sample_length || !mac_d1_range(header + 22, sample_length, length)) return 0;
			/* Bound total runtime allocations even if malformed references overlap */
			if (sample_length > data_size - sample_bytes) return 0;
			sample_bytes += sample_length;
			samples[index].data = sound + 22;
			samples[index].length = sample_length;
		}
		found = 1;
	}
	return found;
}

#endif
