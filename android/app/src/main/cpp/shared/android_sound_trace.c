/* Android diagnostic snapshots do not change sample data or cache lifetime */
#include "android_sound_trace.h"
#include "sound_trace_fingerprint.h"
#include "android_log.h"
#include "gr.h"
#include "piggy.h"
#include "mission.h"
#include "gameseq.h"
#include "robot.h"
#include "sounds.h"
#include "digi.h"
#include "args.h"
#include "physfsx.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#define SOUND_TRACE_CHANGES_PER_LEVEL 8

typedef struct sound_trace_sample {
	char name[13];
	unsigned int bank;
	long long offset;
	int read_ok;
	sound_trace_fingerprint loaded;
	sound_trace_fingerprint converted_input;
	sound_trace_fingerprint converted_output;
	int source_rate, output_rate, output_format, channels;
	sound_trace_fingerprint last_input, last_output;
	int last_rate;
	unsigned int reports;
} sound_trace_sample;

static sound_trace_sample samples[MAX_SOUND_FILES];
static unsigned int bank_generation;
static unsigned int level_generation;
static int context_logged;

static sound_trace_fingerprint current_sample(int sample)
{
	return sound_trace_fingerprint_bytes(GameSounds[sample].data, GameSounds[sample].length);
}

static int mapped_sample(int logical)
{
	if (logical < 0 || logical >= MAX_SOUNDS)
		return -1;
	if (GameArg.SysLowMem)
		logical = AltSounds[logical];
	if (logical < 0 || logical >= MAX_SOUNDS)
		return -1;
	return Sounds[logical] < MAX_SOUND_FILES ? Sounds[logical] : -1;
}

void android_sound_trace_asset_open(const char *role, const char *filename)
{
	char corrected[PATH_MAX];
	const char *origin;
	snprintf(corrected, sizeof(corrected), "%s", filename);
	PHYSFSEXT_locateCorrectCase(corrected);
	origin = PHYSFS_getRealDir(corrected);
	debug_log_force(DLOG_GAME, "[SFX] asset_role=%s file='%s' opened_from='%s'",
	                role, corrected, origin ? origin : "unknown");
}

void android_sound_trace_bank_open(const char *filename)
{
	char corrected[PATH_MAX];
	const char *origin;
	++bank_generation;
	snprintf(corrected, sizeof(corrected), "%s", filename);
	PHYSFSEXT_locateCorrectCase(corrected);
	origin = PHYSFS_getRealDir(corrected);
	/* Sparse load event, retained even if Game Logs is enabled after loading */
	debug_log_force(DLOG_GAME, "[SFX] bank=%u file='%s' loaded_from='%s' hash=fnv1a64",
	                bank_generation, corrected, origin ? origin : "unknown");
}

void android_sound_trace_loaded(int sample, const char *name, long long offset, int read_ok)
{
	sound_trace_sample *entry;
	if (sample < 0 || sample >= MAX_SOUND_FILES)
		return;
	entry = &samples[sample];
	snprintf(entry->name, sizeof(entry->name), "%.12s", name);
	entry->bank = bank_generation;
	entry->offset = offset;
	entry->read_ok = read_ok;
	/* Do not fingerprint uninitialized bytes after a short read */
	entry->loaded = read_ok == 1 ? current_sample(sample) : (sound_trace_fingerprint) { 0, 0, 0 };
	entry->reports = 0;
}

void android_sound_trace_level(void)
{
	int i;
	++level_generation;
	context_logged = 0;
	for (i = 0; i < MAX_SOUND_FILES; ++i) {
		samples[i].reports = 0;
		/* Verify resident bytes even when a robot has not emitted this sample yet */
		if (debug_log_enabled[DLOG_GAME] && samples[i].bank == bank_generation && samples[i].loaded.valid) {
			const sound_trace_fingerprint resident = current_sample(i);
			debug_log(DLOG_GAME,
			          "[SFX] context=%u resident_sample=%d bank=%u loaded_hash=%016llx loaded_bytes=%zu bank_match=%d",
			          level_generation, i, bank_generation, (unsigned long long) samples[i].loaded.hash,
			          samples[i].loaded.length, sound_trace_fingerprint_match(resident, samples[i].loaded));
		}
	}
}

static void log_context(void)
{
	char **paths;
	int i;
	if (context_logged)
		return;
	context_logged = 1;
	debug_log(DLOG_GAME, "[SFX] trace_v=1 context=%u mission='%s' level=%d lowmem=%d backend=SDL_mixer",
	          level_generation, Current_mission ? Current_mission->path : "none",
	          Current_level_num, GameArg.SysLowMem);
	paths = PHYSFS_getSearchPath();
	if (paths) {
		for (i = 0; paths[i]; ++i)
			debug_log(DLOG_GAME, "[SFX] context=%u search_priority=%d path='%s'",
			          level_generation, i, paths[i]);
		PHYSFS_freeList(paths);
	}
	/* Mapping candidates, not a claim about which object emitted a sound */
	for (i = 0; i < N_robot_types && i < MAX_ROBOT_TYPES; ++i) {
		const robot_info *robot = &Robot_info[i];
		debug_log(DLOG_GAME,
		          "[SFX] context=%u robot=%d see=%d:%d attack=%d:%d claw=%d:%d exp1=%d:%d exp2=%d:%d",
		          level_generation, i, robot->see_sound, mapped_sample(robot->see_sound),
		          robot->attack_sound, mapped_sample(robot->attack_sound), robot->claw_sound,
		          mapped_sample(robot->claw_sound), robot->exp1_sound_num, mapped_sample(robot->exp1_sound_num),
		          robot->exp2_sound_num, mapped_sample(robot->exp2_sound_num));
#ifdef DXX_BUILD_DESCENT_II
		debug_log(DLOG_GAME, "[SFX] context=%u robot=%d taunt=%d:%d deathroll=%d:%d",
		          level_generation, i, robot->taunt_sound, mapped_sample(robot->taunt_sound),
		          robot->deathroll_sound, mapped_sample(robot->deathroll_sound));
#endif
	}
}

void android_sound_trace_converted(int sample, const void *data, unsigned int length,
                                   int source_rate, int output_rate, int output_format, int channels)
{
	sound_trace_sample *entry;
	if (sample < 0 || sample >= MAX_SOUND_FILES)
		return;
	entry = &samples[sample];
	/* Keep the conversion baseline even when continuous logging is disabled */
	entry->converted_input = current_sample(sample);
	entry->converted_output = sound_trace_fingerprint_bytes(data, length);
	entry->source_rate = source_rate;
	entry->output_rate = output_rate;
	entry->output_format = output_format;
	entry->channels = channels;
	entry->reports = 0;
}

void android_sound_trace_play(int sample, const void *data, unsigned int length, int source_rate,
                              int channel)
{
	sound_trace_sample *entry;
	sound_trace_fingerprint input, output;
	int bank_match, cache_input_match, cache_output_match;
	if (!debug_log_enabled[DLOG_GAME] || sample < 0 || sample >= MAX_SOUND_FILES)
		return;
	entry = &samples[sample];
	if (entry->reports > SOUND_TRACE_CHANGES_PER_LEVEL)
		return;
	input = current_sample(sample);
	output = sound_trace_fingerprint_bytes(data, length);
	if (entry->reports && sound_trace_fingerprint_match(input, entry->last_input) == 1 &&
	    sound_trace_fingerprint_match(output, entry->last_output) == 1 && source_rate == entry->last_rate)
		return;
	log_context();
	if (entry->reports++ == SOUND_TRACE_CHANGES_PER_LEVEL) {
		debug_log(DLOG_GAME, "[SFX] context=%u sample=%d further_changes_suppressed=1",
		          level_generation, sample);
		return;
	}
	entry->last_input = input;
	entry->last_output = output;
	entry->last_rate = source_rate;
	bank_match = sound_trace_fingerprint_match(input, entry->loaded);
	cache_input_match = sound_trace_fingerprint_match(input, entry->converted_input);
	cache_output_match = sound_trace_fingerprint_match(output, entry->converted_output);
	debug_log(DLOG_GAME,
	          "[SFX] context=%u sample=%d name='%s' bank=%u offset=%lld read_ok=%d loaded_hash=%016llx loaded_bytes=%zu",
	          level_generation, sample, entry->name[0] ? entry->name : "untracked", entry->bank,
	          entry->offset, entry->read_ok, (unsigned long long) entry->loaded.hash, entry->loaded.length);
	debug_log(DLOG_GAME,
	          "[SFX] context=%u sample=%d channel=%d input_hash=%016llx input_bytes=%zu input_valid=%d bank_match=%d cache_input_match=%d cache_output_match=%d",
	          level_generation, sample, channel, (unsigned long long) input.hash, input.length, input.valid,
	          bank_match, cache_input_match, cache_output_match);
	debug_log(DLOG_GAME,
	          "[SFX] context=%u sample=%d cache_input_hash=%016llx cache_input_bytes=%zu output_hash=%016llx cached_output_hash=%016llx output_bytes=%zu source_rate=%d cached_rate=%d output_rate=%d output_format=%d channels=%d",
	          level_generation, sample, (unsigned long long) entry->converted_input.hash,
	          entry->converted_input.length, (unsigned long long) output.hash,
	          (unsigned long long) entry->converted_output.hash, output.length, source_rate, entry->source_rate,
	          entry->output_rate, entry->output_format, entry->channels);
}
