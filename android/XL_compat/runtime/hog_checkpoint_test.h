#ifndef XL_HOG_CHECKPOINT_TEST_H
#define XL_HOG_CHECKPOINT_TEST_H
#include "preview.h"
#include "resource_validation.h"
#include <nlohmann/json.hpp>
#include <stdexcept>
extern "C" {
#include "inferno.h"
#include "gameseq.h"
#include "gamesave.h"
#include "mission.h"
#include "physfsx.h"
#include "player.h"
#include "segment.h"
}

// Capture values owned by the running game before a cross-level restore
static nlohmann::json hog_checkpoint_observation()
{
	const char *owner = PHYSFS_getRealDir(Gamesave_current_filename);
	if (!Current_mission || !owner || !mission_owns_resource_archive(owner) || Num_segments <= 0)
		throw std::runtime_error("Checkpoint has no running mission-owned mine");
	return { { "mission", Current_mission->filename }, { "archive", owner }, { "level", Current_level_num }, { "filename", Gamesave_current_filename }, { "xl", xl_level_extensions_active() != 0 }, { "segments", Num_segments }, { "vertices", Num_vertices }, { "energy", Players[Player_num].energy }, { "resources", xl_resource_mod_sources() } };
}
#endif
