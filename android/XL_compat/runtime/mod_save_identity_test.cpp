#include <cstring>
#include <stdexcept>
#include "resource_validation.h"
#include "../inventory.h"
#include <memory>
extern "C" {
#include "gameseq.h"
#include "wall.h"
}
static void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
#include "../../app/src/main/cpp/shared/mod_save_compat.h"

// Exercise save admission with an empty synthetic world, not a gameplay restore
void mod_identity_save()
{
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> input(PHYSFS_openRead("pmlev1.rl2"), PHYSFS_close);
	require(input != nullptr, "Cannot read original level for save identity fixture");
	const auto size = PHYSFS_fileLength(input.get());
	require(size > 0 && size <= 64 * 1024 * 1024, "Invalid save identity level size");
	xl_compat::Bytes bytes(static_cast<std::size_t>(size));
	require(PHYSFS_read(input.get(), bytes.data(), 1, static_cast<PHYSFS_uint32>(bytes.size())) == size,
	        "Cannot read save identity level bytes");
	const auto level = xl_compat::inspect_level(bytes);
	XlPreparedResources resources("pmlev1.rl2", &level, 11);
	resources.install_hxm();
	Current_level_num = 11;
	Highest_segment_index = 0;
	std::memset(&Segments[0], 0, sizeof(Segments[0]));
	Highest_object_index = -1;
	Num_walls = 0;
	// Mission loading captures definitions and extra HAM sources through the real hooks
	// Recapturing here would erase the extension identities before writing the save
	mod_save_capture_level("pmlev1.rl2");
	rewind_file file;
	rewind_file_init_physfs(&file, PHYSFS_openWrite("xl-source-identity.sg0"));
	require(file.physfs != nullptr, "Cannot create owned save identity fixture");
	const int version = 43;
	rewind_file_write(&file, "DGSS", 1, 4);
	rewind_file_write(&file, &version, 4, 1);
	mod_save_effect_offset(&file);
	const int empty_effects[] = { 0, 0, 0 };
	rewind_file_write(&file, empty_effects, 4, 3);
	const bool written = mod_save_write(&file) != 0;
	rewind_file_close(&file);
	require(written, mod_save_reason());
}

bool mod_identity_restore()
{
	rewind_file file;
	rewind_file_init_physfs(&file, PHYSFS_openRead("xl-source-identity.sg0"));
	require(file.physfs != nullptr, "Cannot open owned save identity fixture");
	rewind_file_seek(&file, 8);
	const bool accepted = mod_save_prepare(&file, 11, 43, 0) != 0;
	rewind_file_close(&file);
	return accepted;
}
