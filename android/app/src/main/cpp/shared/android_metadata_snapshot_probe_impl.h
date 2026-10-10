/* Included only by secretarea.c on Android D2, after the live serializer */
#include "android_metadata_snapshot_probe.h"
#include <pthread.h>
#include <time.h>

#define METADATA_PROBE_SLOTS 12
typedef struct metadata_probe_snapshot {
#define METADATA_STRUCT(type, name, serializer) type name;
#define METADATA_VALUE(type, name, kind)        type name;
#include "guidebot_metadata_save_members.h"
#undef METADATA_STRUCT
#undef METADATA_VALUE
} metadata_probe_snapshot;

static struct {
	metadata_probe_snapshot *pool, *decoded;
	rewind_memory_buffer reference, encoded, roundtrip;
	android_metadata_snapshot_result result;
	pthread_t thread;
	atomic_int done;
	unsigned slot;
	int pending;
	int64_t epoch;
} Metadata_probe;

static int64_t metadata_probe_clock(clockid_t id)
{
	struct timespec t;
	return clock_gettime(id, &t) ? 0 : (int64_t) t.tv_sec * 1000000 + t.tv_nsec / 1000;
}

static void metadata_probe_serialize(guidebot_save_stream *s, metadata_probe_snapshot *snapshot)
{
#define METADATA_STRUCT(type, name, serializer) serializer(s, &snapshot->name);
#define METADATA_VALUE(type, name, kind)        GB_FIELD(s, snapshot->name, kind);
#include "guidebot_metadata_save_members.h"
#undef METADATA_STRUCT
#undef METADATA_VALUE
}

static int metadata_probe_encode(metadata_probe_snapshot *snapshot, rewind_memory_buffer *buffer)
{
	rewind_file file;
	guidebot_save_stream stream = { 0 };
	rewind_file_init_memory_write(&file, buffer);
	stream.file = &file;
	stream.writing = stream.ok = 1;
	stream.epoch = Metadata_probe.epoch;
	if (snapshot) metadata_probe_serialize(&stream, snapshot);
	else level_metadata_save_runtime(&stream);
	return rewind_file_close(&file) && stream.ok;
}

static int metadata_probe_equal(const rewind_memory_buffer *a, const rewind_memory_buffer *b)
{
	return a->size == b->size && !memcmp(a->data, b->data, a->size);
}

static void *metadata_probe_worker(void *unused)
{
	const struct timespec delay = { 0, 50000000 };
	int64_t wall, cpu;
	int ok;
	rewind_file file;
	guidebot_save_stream stream = { 0 };
	(void) unused;
	/* Ensure encoding starts after the capture frame has returned */
	nanosleep(&delay, NULL);
	wall = metadata_probe_clock(CLOCK_MONOTONIC);
	cpu = metadata_probe_clock(CLOCK_THREAD_CPUTIME_ID);
	ok = metadata_probe_encode(&Metadata_probe.pool[Metadata_probe.slot], &Metadata_probe.encoded);
	Metadata_probe.result.worker_cpu_us = metadata_probe_clock(CLOCK_THREAD_CPUTIME_ID) - cpu;
	Metadata_probe.result.worker_wall_us = metadata_probe_clock(CLOCK_MONOTONIC) - wall;
	Metadata_probe.result.equal = ok && metadata_probe_equal(&Metadata_probe.reference, &Metadata_probe.encoded);
	Metadata_probe.result.encoded_bytes = Metadata_probe.encoded.size;
	rewind_file_init_memory_read(&file, Metadata_probe.encoded.data, Metadata_probe.encoded.size);
	stream.file = &file;
	stream.apply = stream.ok = 1;
	stream.epoch = Metadata_probe.epoch;
	metadata_probe_serialize(&stream, Metadata_probe.decoded);
	Metadata_probe.result.roundtrip_equal = stream.ok && stream.bytes == Metadata_probe.encoded.size &&
	                                        metadata_probe_encode(Metadata_probe.decoded, &Metadata_probe.roundtrip) &&
	                                        metadata_probe_equal(&Metadata_probe.encoded, &Metadata_probe.roundtrip);
	atomic_store_explicit(&Metadata_probe.done, 1, memory_order_release);
	return NULL;
}

void android_metadata_snapshot_discard(void)
{
	/* Cleanup only: a running experiment owns these buffers until it finishes */
	if (Metadata_probe.pending) pthread_join(Metadata_probe.thread, NULL);
	free(Metadata_probe.pool);
	free(Metadata_probe.decoded);
	rewind_memory_buffer_discard(&Metadata_probe.reference);
	rewind_memory_buffer_discard(&Metadata_probe.encoded);
	rewind_memory_buffer_discard(&Metadata_probe.roundtrip);
	memset(&Metadata_probe, 0, sizeof(Metadata_probe));
	atomic_init(&Metadata_probe.done, 0);
}

int android_metadata_snapshot_reset(void)
{
	guidebot_save_stream measure = { 0 };
	rewind_memory_buffer *buffers[3];
	unsigned i;
	android_metadata_snapshot_discard();
	Metadata_probe.pool = calloc(METADATA_PROBE_SLOTS, sizeof(*Metadata_probe.pool));
	Metadata_probe.decoded = calloc(1, sizeof(*Metadata_probe.decoded));
	if (!Metadata_probe.pool || !Metadata_probe.decoded) goto fail;
	/* Touch all destination pages before measured captures */
	memset(Metadata_probe.pool, 0xa5, METADATA_PROBE_SLOTS * sizeof(*Metadata_probe.pool));
	measure.writing = 2;
	measure.ok = 1;
	level_metadata_save_runtime(&measure);
	if (!measure.ok) goto fail;
	buffers[0] = &Metadata_probe.reference;
	buffers[1] = &Metadata_probe.encoded;
	buffers[2] = &Metadata_probe.roundtrip;
	for (i = 0; i < 3; ++i) {
		rewind_file file;
		rewind_file_init_memory_write(&file, buffers[i]);
		if (!rewind_file_memory_reserve(&file, measure.bytes)) goto fail;
		rewind_file_close(&file);
	}
	return 1;
fail:
	android_metadata_snapshot_discard();
	return 0;
}

int android_metadata_snapshot_start(int64_t epoch)
{
	metadata_probe_snapshot *snapshot;
	int64_t wall, cpu;
	if (!Metadata_probe.pool || Metadata_probe.pending) return 0;
	Metadata_probe.slot = (Metadata_probe.slot + 1) % METADATA_PROBE_SLOTS;
	Metadata_probe.epoch = epoch;
	snapshot = &Metadata_probe.pool[Metadata_probe.slot];
	memset(&Metadata_probe.result, 0, sizeof(Metadata_probe.result));
	Metadata_probe.result.native_bytes = sizeof(*snapshot);
	Metadata_probe.result.pool_bytes = METADATA_PROBE_SLOTS * sizeof(*snapshot);
	Metadata_probe.result.atomic_lock_free = atomic_is_lock_free(&Metadata_probe.done);
	wall = metadata_probe_clock(CLOCK_MONOTONIC);
	cpu = metadata_probe_clock(CLOCK_THREAD_CPUTIME_ID);
#define METADATA_STRUCT(type, name, serializer) memcpy(&snapshot->name, &Level_metadata_##name, sizeof(snapshot->name));
#define METADATA_VALUE(type, name, kind)        memcpy(&snapshot->name, &Level_metadata_##name, sizeof(snapshot->name));
#include "guidebot_metadata_save_members.h"
#undef METADATA_STRUCT
#undef METADATA_VALUE
	Metadata_probe.result.capture_cpu_us = metadata_probe_clock(CLOCK_THREAD_CPUTIME_ID) - cpu;
	Metadata_probe.result.capture_wall_us = metadata_probe_clock(CLOCK_MONOTONIC) - wall;
	wall = metadata_probe_clock(CLOCK_MONOTONIC);
	cpu = metadata_probe_clock(CLOCK_THREAD_CPUTIME_ID);
	if (!metadata_probe_encode(NULL, &Metadata_probe.reference)) return 0;
	Metadata_probe.result.direct_cpu_us = metadata_probe_clock(CLOCK_THREAD_CPUTIME_ID) - cpu;
	Metadata_probe.result.direct_wall_us = metadata_probe_clock(CLOCK_MONOTONIC) - wall;
	atomic_store_explicit(&Metadata_probe.done, 0, memory_order_relaxed);
	if (pthread_create(&Metadata_probe.thread, NULL, metadata_probe_worker, NULL)) return 0;
	Metadata_probe.pending = 1;
	return 1;
}

int android_metadata_snapshot_finish(android_metadata_snapshot_result *result)
{
	if (!Metadata_probe.pending || !atomic_load_explicit(&Metadata_probe.done, memory_order_acquire)) return 0;
	pthread_join(Metadata_probe.thread, NULL);
	Metadata_probe.pending = 0;
	/* Compare current state at the original epoch to prove useful live-state variation */
	if (!metadata_probe_encode(NULL, &Metadata_probe.roundtrip)) return 0;
	Metadata_probe.result.live_changed = !metadata_probe_equal(&Metadata_probe.reference, &Metadata_probe.roundtrip);
	*result = Metadata_probe.result;
	return result->equal && result->roundtrip_equal;
}
