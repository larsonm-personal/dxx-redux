// Private host fixture export uses the same source definitions as Android saves
#include <nlohmann/json.hpp>
#include "checkpoint_export.h"
#include "mod_save_compat.h"
extern "C" {
#include "physfsx.h"
}
extern "C" void xl_checkpoint_capture_sources(const char *level)
{
	mod_save_capture_definitions();
	mod_save_capture_level(level);
}
extern "C" void xl_checkpoint_effect_offset(PHYSFS_File *raw)
{
	rewind_file file;
	rewind_file_init_physfs(&file, raw);
	mod_save_effect_offset(&file);
}
extern "C" int xl_checkpoint_finish_export(const char *filename)
{
	auto *raw = PHYSFS_openAppend(filename);
	if (!raw) return 0;
	rewind_file file;
	rewind_file_init_physfs(&file, raw);
	const int result = mod_save_write(&file);
	const int closed = rewind_file_close(&file);
	return result && closed;
}
