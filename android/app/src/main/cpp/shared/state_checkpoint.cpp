#include "checkpoint_slots.h"
#include "state_checkpoint.h"
#include "checkpoint_file.h"
#include <cerrno>
#include <new>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <fstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

extern "C" {
#include "game.h"
#include "state.h"
#include "state_android_shared.h"
#include "android_log.h"
#include "android_save_meta.h"
#include "coop/coop_save.h"
#include "guidebot_save_io.h"
#ifdef DXX_BUILD_DESCENT_II
#include "guidebot_metadata_snapshot.h"
#endif
}

namespace
{
int64_t clock_us()
{
	timespec t{};
	clock_gettime(CLOCK_MONOTONIC, &t);
	return int64_t(t.tv_sec) * 1000000 + t.tv_nsec / 1000;
}

struct checkpoint_job {
	rewind_memory_buffer buffer{};
#ifdef DXX_BUILD_DESCENT_II
	guidebot_metadata_snapshot metadata{};
#endif
	size_t metadata_offset = 0, metadata_bytes = 0;
	int64_t epoch = 0, worker_us = 0;
	int64_t submit_begin_us = 0, capture_us = 0, submit_us = 0, encode_us = 0;
	uint64_t token = 0;
	state_checkpoint_callback callback = nullptr;
	int ok = 0;
	unsigned attachment_failures = 0;
	std::string filename;
	struct attachment {
		std::string filename, text;
		int history_slot;
	};
	std::vector<attachment> attachments;
};

struct checkpoint_engine {
	checkpoint_slots<checkpoint_job, 3> slots;
	sem_t ready{};
	sem_t finished{};
	pthread_t thread{};
	std::atomic<bool> stopping{ false };
	bool started = false;
	size_t metadata_bytes = 0;
	state_checkpoint_stats stats{};
	~checkpoint_engine()
	{
		if (started) {
			stopping.store(true, std::memory_order_release);
			sem_post(&ready);
			pthread_join(thread, nullptr);
			sem_destroy(&ready);
			sem_destroy(&finished);
		}
		slots.for_each_storage([](checkpoint_job &job) { rewind_memory_buffer_discard(&job.buffer); });
	}
};

checkpoint_engine *engine;
checkpoint_job *capturing;

bool publish_attachment(const checkpoint_job::attachment &attachment)
{
	std::string text = attachment.text;
	if (attachment.history_slot >= 0) {
		/* The existing compact history reader expects slot to be the first key */
		using json = nlohmann::ordered_json;
		auto entry = json::parse(text, nullptr, false);
		if (entry.is_array() && entry.size() == 1) entry = json(entry[0]);
		if (!entry.is_object()) return false;
		json history = json::array();
		history.push_back(entry);
		std::ifstream stream(attachment.filename, std::ios::binary);
		std::string old_text(65536, '\0');
		stream.read(&old_text[0], old_text.size());
		old_text.resize(static_cast<size_t>(stream.gcount()));
		const auto old = json::parse(old_text, nullptr, false);
		if (old.is_array())
			for (const auto &item : old) {
				if (history.size() >= COOP_AUTOSAVE_SLOT_COUNT) break;
				if (item.is_object() && item.contains("slot") && item["slot"].is_number_integer() &&
				    item["slot"].get<int>() != attachment.history_slot) history.push_back(item);
			}
		text = history.dump(2) + "\n";
	}
	return checkpoint_file_publish(attachment.filename.c_str(), text.data(), text.size()) != 0;
}

void *worker(void *argument)
{
	auto &e = *static_cast<checkpoint_engine *>(argument);
	for (;;) {
		while (sem_wait(&e.ready) && errno == EINTR) {}
		if (e.stopping.load(std::memory_order_acquire)) break;
		while (auto *job = e.slots.try_work()) {
			const int64_t start = clock_us();
			job->ok = 1;
			job->attachment_failures = 0;
#ifdef DXX_BUILD_DESCENT_II
			if (job->metadata_bytes) {
				job->ok = job->metadata_offset <= job->buffer.size &&
				          job->metadata_bytes <= job->buffer.size - job->metadata_offset &&
				          guidebot_metadata_snapshot_encode(&job->metadata,
				                                            job->buffer.data + job->metadata_offset, job->metadata_bytes, job->epoch);
			}
#endif
			job->encode_us = clock_us() - start;
			if (job->ok && !job->filename.empty()) {
				android_save_meta_disk meta;
				job->ok = job->buffer.size >= sizeof(meta);
				if (job->ok) {
					memcpy(&meta, job->buffer.data + job->buffer.size - sizeof(meta), sizeof(meta));
					job->ok = android_save_meta_is_valid(&meta) &&
					          checkpoint_file_publish(job->filename.c_str(), job->buffer.data, job->buffer.size);
				}
				if (job->ok)
					for (const auto &attachment : job->attachments)
						if (!publish_attachment(attachment)) ++job->attachment_failures;
			}
			job->worker_us = clock_us() - start;
			e.slots.finish_work();
			sem_post(&e.finished);
		}
	}
	return nullptr;
}

struct shutdown_worker {
	~shutdown_worker()
	{
		delete engine;
	}
} shutdown;
} // namespace

int state_checkpoint_initialize(void)
{
	if (engine) return 1;
	auto *candidate = new (std::nothrow) checkpoint_engine;
	if (!candidate) return 0;
	bool allocated = true;
	candidate->slots.for_each_storage([&](checkpoint_job &job) {
		rewind_file file;
		rewind_file_init_memory_write(&file, &job.buffer);
		allocated = rewind_file_memory_reserve(&file, 2 * 1024 * 1024) && allocated;
		if (job.buffer.data) memset(job.buffer.data, 0, job.buffer.capacity);
		rewind_file_close(&file);
	});
#ifdef DXX_BUILD_DESCENT_II
	candidate->metadata_bytes = guidebot_metadata_snapshot_encoded_size();
	allocated = allocated && candidate->metadata_bytes;
#endif
	if (!allocated || sem_init(&candidate->ready, 0, 0)) {
		delete candidate;
		return 0;
	}
	if (sem_init(&candidate->finished, 0, 0)) {
		sem_destroy(&candidate->ready);
		delete candidate;
		return 0;
	}
	if (pthread_create(&candidate->thread, nullptr, worker, candidate)) {
		sem_destroy(&candidate->ready);
		sem_destroy(&candidate->finished);
		delete candidate;
		return 0;
	}
	candidate->started = true;
	engine = candidate;
	return 1;
}

int state_checkpoint_submit(const char *description, int save_kind, uint64_t token,
                            state_checkpoint_callback callback)
{
	return state_checkpoint_submit_disk(description, save_kind, nullptr, nullptr, 0, token, callback);
}

int state_checkpoint_submit_disk(const char *description, int save_kind, const char *filename,
                                 const state_checkpoint_attachment *attachments, unsigned attachment_count,
                                 uint64_t token, state_checkpoint_callback callback)
{
	const int64_t submit_begin = clock_us();
	if (!description || !callback || capturing || attachment_count > 8 ||
	    (attachment_count && !attachments) || !state_checkpoint_initialize()) return 0;
	auto *job = engine->slots.try_capture();
	if (!job) {
		++engine->stats.deferred;
		return 0;
	}
	job->metadata_bytes = 0;
	job->submit_begin_us = submit_begin;
	job->epoch = GameTime64;
	job->token = token;
	job->callback = callback;
	job->filename.clear();
	job->attachments.clear();
	if (filename) {
		const char *root = PHYSFS_getWriteDir();
		if (!root || !*filename) {
			engine->slots.abandon_capture();
			return 0;
		}
		const std::string prefix = std::string(root) + "/";
		job->filename = prefix + filename;
		char last_path[PATH_MAX], last_text[128];
		if (!state_android_capture_last_save_set(last_path, sizeof(last_path), last_text, sizeof(last_text))) {
			engine->slots.abandon_capture();
			return 0;
		}
		job->attachments.push_back({ prefix + last_path, last_text, -1 });
		for (unsigned i = 0; i < attachment_count; ++i) {
			if (!attachments[i].filename || !attachments[i].text) {
				engine->slots.abandon_capture();
				return 0;
			}
			job->attachments.push_back({ prefix + attachments[i].filename, attachments[i].text, attachments[i].history_slot });
		}
	}
	const int64_t start = clock_us();
	capturing = job;
	stop_time();
	const int ok = state_save_to_memory(&job->buffer, description, save_kind, 1);
	capturing = nullptr;
	engine->stats.capture_us = clock_us() - start;
	job->capture_us = engine->stats.capture_us;
	if (engine->stats.capture_us > engine->stats.max_capture_us)
		engine->stats.max_capture_us = engine->stats.capture_us;
	if (!ok) {
		engine->slots.abandon_capture();
		++engine->stats.failed;
		return 0;
	}
	job->submit_us = clock_us() - submit_begin;
	engine->slots.submit();
	++engine->stats.submitted;
	++engine->stats.pending;
	sem_post(&engine->ready);
	return 1;
}

void state_checkpoint_poll(void)
{
	if (!engine) return;
	while (auto *job = engine->slots.try_collect()) {
		sem_trywait(&engine->finished);
		--engine->stats.pending;
		if (job->ok) ++engine->stats.completed;
		else ++engine->stats.failed;
		engine->stats.worker_us = job->worker_us;
		if (job->attachment_failures)
			debug_log(DLOG_GAME, "checkpoint saved with %u sidecar write failures", job->attachment_failures);
		const int64_t collect_begin = clock_us();
		job->callback(job->token, job->ok, &job->buffer);
		const int64_t collect_us = clock_us() - collect_begin;
		debug_log(DLOG_PROFILING,
		          "checkpoint_v=1 kind=%s ok=%d bytes=%zu submit_begin_us=%lld submit_us=%lld capture_us=%lld encode_us=%lld publish_us=%lld worker_us=%lld collect_us=%lld sidecar_failures=%u",
		          job->filename.empty() ? "memory" : "disk", job->ok, job->buffer.size,
		          (long long) job->submit_begin_us, (long long) job->submit_us,
		          (long long) job->capture_us, (long long) job->encode_us,
		          (long long) (job->worker_us - job->encode_us), (long long) job->worker_us,
		          (long long) collect_us, job->attachment_failures);
		engine->slots.release();
	}
}

void state_checkpoint_drain(void)
{
	if (!engine || capturing) return;
	state_checkpoint_poll();
	while (engine->stats.pending) {
		while (sem_wait(&engine->finished) && errno == EINTR) {}
		state_checkpoint_poll();
	}
}

int state_checkpoint_defer_metadata(guidebot_save_stream *stream)
{
#ifdef DXX_BUILD_DESCENT_II
	if (!capturing || !stream->writing) return 0;
	if (!stream->ok) return 1;
	const size_t bytes = engine->metadata_bytes;
	if (stream->writing != 2) {
		auto *file = stream->file;
		if (!file || !rewind_file_is_memory(file) || !file->memory_buffer ||
		    capturing->metadata_bytes || file->position > SIZE_MAX - bytes ||
		    !rewind_file_memory_reserve(file, file->position + bytes)) {
			stream->ok = 0;
			return 1;
		}
		level_metadata_capture_snapshot(&capturing->metadata);
		capturing->metadata_offset = file->position;
		capturing->metadata_bytes = bytes;
		/* Unpublished hole: only the worker may encode it before completion */
		memset(file->memory_buffer->data + file->position, 0, bytes);
		file->position += bytes;
		file->memory_read_size = file->position;
		file->memory_buffer->size = file->position;
	}
	stream->bytes += bytes;
	return 1;
#else
	(void) stream;
	return 0;
#endif
}

int state_checkpoint_writes_file(void)
{
	return capturing && !capturing->filename.empty();
}

void state_checkpoint_get_stats(state_checkpoint_stats *stats)
{
	if (stats) *stats = engine ? engine->stats : state_checkpoint_stats{};
}
