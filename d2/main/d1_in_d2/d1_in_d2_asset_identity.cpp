/* Source identity belongs to the published asset generation */
#include <cstdint>
#include <cstring>
#include "picosha2.h"

extern "C" {
#include "d1_in_d2.h"
#include "d1_in_d2_assets.h"
#include "d1_in_d2_levels.h"
#include "d1_in_d2_net.h"
#include "console.h"
#include "mission.h"
}

int d1_in_d2_saved_format_supported(int version)
{
	if (Current_mission && (Current_mission->descent_version != 1 || version == D1_IN_D2_SAVE_VERSION)) return 1;
	// Earlier imported saves do not bind all original/custom/extension definitions
	con_printf(CON_URGENT, "Cannot restore D1-in-D2 save version %d: its asset namespace is unsupported (current %d)\n", version, D1_IN_D2_SAVE_VERSION);
	return 0;
}

static void hash_word(picosha2::hash256_one_by_one &hash, uint32_t value)
{
	const unsigned char bytes[4] = { static_cast<unsigned char>(value), static_cast<unsigned char>(value >> 8),
		                             static_cast<unsigned char>(value >> 16), static_cast<unsigned char>(value >> 24) };
	hash.process(bytes, bytes + sizeof(bytes));
}

static int hash_source(picosha2::hash256_one_by_one &hash, const char *path, bool optional = false)
{
	PHYSFS_file *file = path ? PHYSFSX_openReadBuffered(path) : nullptr;
	if (!file) {
		if (!optional || (path && PHYSFSX_exists(path, 1))) return 0;
		hash_word(hash, 0);
		return 1;
	}
	if (optional) hash_word(hash, 1);
	const PHYSFS_sint64 size = PHYSFS_fileLength(file);
	bool valid = size >= 0;
	hash_word(hash, static_cast<uint32_t>(size));
	hash_word(hash, static_cast<uint32_t>(static_cast<uint64_t>(size) >> 32));
	unsigned char buffer[65536];
	for (PHYSFS_sint64 remaining = size; valid && remaining > 0;) {
		const auto length = static_cast<PHYSFS_uint32>(remaining < static_cast<PHYSFS_sint64>(sizeof(buffer)) ? remaining : sizeof(buffer));
		valid = PHYSFS_read(file, buffer, 1, length) == length;
		if (valid) hash.process(buffer, buffer + length);
		remaining -= length;
	}
	return PHYSFS_close(file) && valid;
}

int d1_in_d2_hash_base_source(const char *pig, const char *palette, ubyte digest[32])
{
	picosha2::hash256_one_by_one hash;
	const char schema[] = "d1-base-sources-v1";
	hash.process(schema, schema + sizeof(schema) - 1);
	if (!hash_source(hash, pig) || !hash_source(hash, palette)) return 0;
	hash.finish();
	hash.get_hash_bytes(digest, digest + 32);
	return 1;
}

int d1_in_d2_hash_custom_sources(const ubyte base[32], const char *const paths[3], ubyte digest[32])
{
	picosha2::hash256_one_by_one hash;
	const char schema[] = "d1-custom-sources-v1";
	hash.process(schema, schema + sizeof(schema) - 1);
	hash.process(base, base + 32);
	// PG1, DTX and HX1 in native precedence order; absence is explicit
	for (int i = 0; i < 3; ++i)
		if (!hash_source(hash, paths[i], true)) return 0;
	hash.finish();
	hash.get_hash_bytes(digest, digest + 32);
	return 1;
}

int d1_in_d2_hash_guidebot_source(const d1_guidebot_source *source, const int mapping[][MAX_BITMAP_FILES], size_t kinds, ubyte digest[32])
{
	picosha2::hash256_one_by_one hash;
	// Identity schema and decoding rate are independent of paths and host byte order
	hash_word(hash, 1);
	hash_word(hash, source->sample_rate);
	for (const char *path : { source->ham_path, source->pig_path, source->palette_path, source->sound_path })
		if (!hash_source(hash, path)) return 0;
	for (size_t kind = 0; kind < kinds; ++kind)
		for (int slot = 0; slot < MAX_BITMAP_FILES; ++slot)
			hash_word(hash, static_cast<uint32_t>(mapping[kind][slot]));
	hash.finish();
	hash.get_hash_bytes(digest, digest + 32);
	return 1;
}

int d1_in_d2_capture_asset_identity(ubyte identity[64])
{
	const ubyte *definitions = d1_in_d2_definition_identity(), *companion = d1_in_d2_guidebot_identity();
	if (!d1_in_d2_use_d1_gameplay() || !definitions) return 0;
	std::memcpy(identity, definitions, 32);
	if (companion) std::memcpy(identity + 32, companion, 32);
	return companion ? 64 : 32;
}

const char *d1_in_d2_check_asset_identity(const ubyte *identity, int length)
{
	const ubyte *definitions = d1_in_d2_definition_identity(), *companion = d1_in_d2_guidebot_identity();
	if ((length != 32 && length != 64) || !d1_in_d2_use_d1_gameplay() || !definitions ||
	    std::memcmp(identity, definitions, 32))
		return "D1 base/custom source identity is invalid or changed";
	// Native-only data remains usable when optional assets become available
	if (length == 64 && (!companion || std::memcmp(identity + 32, companion, 32)))
		return "D1 optional Guide-Bot source assets are missing or changed";
	return nullptr;
}

void d1_in_d2_write_network_identity(ubyte record[D1_IN_D2_NET_ASSET_SIZE])
{
	std::memset(record, 0, D1_IN_D2_NET_ASSET_SIZE);
	if (d1_in_d2_use_d1_gameplay()) {
		const int length = d1_in_d2_capture_asset_identity(record + 1);
		record[0] = length ? length : 255;
	}
}

const char *d1_in_d2_check_network_identity(const ubyte *record, int size)
{
	if (!record || size != D1_IN_D2_NET_ASSET_SIZE ||
	    (record[0] != 0 && record[0] != 32 && record[0] != 64))
		return "Invalid network asset identity";
	for (int i = 1 + record[0]; i < D1_IN_D2_NET_ASSET_SIZE; ++i)
		if (record[i]) return "Invalid network asset identity padding";
	if (!record[0])
		return d1_in_d2_use_d1_gameplay() ? "Host and client gameplay asset namespaces differ" : nullptr;
	if (const char *error = d1_in_d2_check_asset_identity(record + 1, record[0])) return error;
	// Every peer can create objects, so optional availability must agree both ways
	if (record[0] == 32 && d1_in_d2_guidebot_identity())
		return "Host and client optional Guide-Bot assets differ";
	return nullptr;
}

// Source files above are PhysFS; save records below also support memory rewind
#include "rewind_file_compat.h"

int d1_in_d2_write_saved_asset_identity(rewind_file *fp)
{
	ubyte identity[64];
	const int length = d1_in_d2_capture_asset_identity(identity);
	if (!length) return 0;
	return PHYSFS_write(fp, &length, sizeof(length), 1) == 1 &&
	       PHYSFS_write(fp, identity, 1, length) == length;
}

int d1_in_d2_read_saved_asset_identity(rewind_file *fp, int swap)
{
	int length;
	ubyte saved[64];
	if (PHYSFS_read(fp, &length, sizeof(length), 1) != 1) return 0;
	if (swap) length = SWAPINT(length);
	if ((length != 32 && length != 64) || PHYSFS_read(fp, saved, 1, length) != length) return 0;
	if (const char *error = d1_in_d2_check_asset_identity(saved, length)) {
		con_printf(CON_URGENT, "Cannot restore D1 save: %s\n", error);
		return 0;
	}
	return 1;
}
