#include <cstring>
#include <stdexcept>
#include "resource_validation.h"
#include "../inventory.h"
#include <memory>
extern "C" {
#include "gameseq.h"
#include "wall.h"
#include "effects.h"
#include "pog_resources.h"
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

// Exercise the shared Android preflight and effect reader with authentic retained
// definitions. These sparse records are malformed-input controls, not game saves
void mod_identity_scoped_effect_validation(int effect)
{
	const int frames = xl_pog_effect_frames(&Effects[effect]);
	require(frames > Effects[effect].vc.num_frames, "No extended scoped effect for save validation");
	for (bool invalid : { false, true }) {
		const char *name = invalid ? "xl-scoped-effect-invalid.sg0" : "xl-scoped-effect-valid.sg0";
		rewind_file file;
		rewind_file_init_physfs(&file, PHYSFS_openWrite(name));
		require(file.physfs != nullptr, "Cannot create scoped effect save control");
		const int version = 43;
		rewind_file_write(&file, "DGSS", 1, 4);
		rewind_file_write(&file, &version, 4, 1);
		mod_save_effect_offset(&file);
		const int header[] = { 0, 0, 1 };
		const int row[] = { effect, Effects[effect].vc.frame_time, invalid ? frames : frames - 1, 0, -1, 0, Effects[effect].dest_bm_num };
		rewind_file_write(&file, header, 4, 3);
		rewind_file_write(&file, row, 4, 7);
		const bool written = mod_save_write(&file) != 0;
		rewind_file_close(&file);
		require(written, mod_save_reason());
		rewind_file_init_physfs(&file, PHYSFS_openRead(name));
		require(file.physfs != nullptr && rewind_file_seek(&file, 8), "Cannot read scoped effect save control");
		const bool prepared = mod_save_prepare(&file, Current_level_num, version, 0) != 0;
		const bool accepted = prepared && mod_save_validate_effects(&file, 0);
		rewind_file_close(&file);
		require(accepted == !invalid, invalid ? "Android preflight accepted an out-of-range scoped effect frame" : mod_save_reason());
	}
}
