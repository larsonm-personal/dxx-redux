#include "checkpoint_slots.h"
#include "state_checkpoint.h"
#include "checkpoint_file.h"
#include <cerrno>
#include <new>
#include <memory>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <fstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

extern "C" {
#include "game.h"
#include "args.h"
#include "state.h"
#include "state_android_shared.h"
#include "android_log.h"
#include "android_save_meta.h"
#include "coop/coop_save.h"
#include "coop/coop_save_format.h"
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
	std::string companion_filename;
	std::shared_ptr<const std::vector<unsigned char>> companion;
	std::shared_ptr<const unsigned char> campaign;
	size_t campaign_size = 0, campaign_offset = 0;
	uint32_t campaign_checksum = 0;
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
	/* Only the game thread replaces this pointer; published jobs own generations */
	std::shared_ptr<const std::vector<unsigned char>> companion;
	bool companion_valid = false;
	std::shared_ptr<const unsigned char> campaign;
	size_t campaign_size = 0;
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

static int finish_campaign(checkpoint_job &job)
{
	if (!job.campaign_size) return 1;
	const size_t original = job.buffer.size, offset = job.campaign_offset, bytes = job.campaign_size;
	if (!job.campaign || offset > original || sizeof(coop_save_footer) > original - offset || bytes > SIZE_MAX - original) return 0;
	coop_save_footer footer;
	memcpy(&footer, job.buffer.data + offset, sizeof(footer));
	if (footer.tag != COOP_SAVE_FOOTER_TAG || footer.version != COOP_SAVE_META_VER || footer.campaign_size != bytes) return 0;
	rewind_file file;
	rewind_file_init_memory_write(&file, &job.buffer);
	if (!rewind_file_memory_reserve(&file, original + bytes)) return 0;
	memmove(job.buffer.data + offset + bytes, job.buffer.data + offset, original - offset);
	memcpy(job.buffer.data + offset, job.campaign.get(), bytes);
	footer.checksum = coop_save_checksum(job.campaign.get(), bytes, job.campaign_checksum);
	memcpy(job.buffer.data + offset + bytes, &footer, sizeof(footer));
	file.position = original + bytes;
	return rewind_file_close(&file);
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
			if (job->ok) job->ok = finish_campaign(*job);
			job->encode_us = clock_us() - start;
			if (job->ok && !job->filename.empty()) {
				android_save_meta_disk meta;
				job->ok = job->buffer.size >= sizeof(meta);
				if (job->ok) {
					memcpy(&meta, job->buffer.data + job->buffer.size - sizeof(meta), sizeof(meta));
					job->ok = android_save_meta_is_valid(&meta);
					if (job->ok) {
						if (job->companion_filename.empty())
							job->ok = checkpoint_file_publish(job->filename.c_str(), job->buffer.data, job->buffer.size);
						else
							job->ok = checkpoint_file_publish_pair(job->filename.c_str(), job->buffer.data, job->buffer.size,
							                                       job->companion_filename.c_str(),
							                                       job->companion ? job->companion->data() : nullptr,
							                                       job->companion ? job->companion->size() : 0);
					}
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

static int submit_disk(const char *description, int save_kind, const char *filename,
                       const state_checkpoint_attachment *attachments, unsigned attachment_count,
                       uint64_t token, state_checkpoint_callback callback, const char *companion_filename)
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
	job->companion_filename.clear();
	job->companion.reset();
	job->campaign.reset();
	job->campaign_size = 0;
	job->attachments.clear();
	if (filename) {
		const char *root = PHYSFS_getWriteDir();
		if (!root || !*filename) {
			engine->slots.abandon_capture();
			return 0;
		}
		const std::string prefix = std::string(root) + "/";
		job->filename = prefix + filename;
		if (companion_filename) {
			if (!engine->companion_valid) {
				engine->slots.abandon_capture();
				return 0;
			}
			job->companion_filename = prefix + companion_filename;
			job->companion = engine->companion;
		}
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

int state_checkpoint_submit_disk(const char *description, int save_kind, const char *filename,
                                 const state_checkpoint_attachment *attachments, unsigned attachment_count,
                                 uint64_t token, state_checkpoint_callback callback)
{
	return submit_disk(description, save_kind, filename, attachments, attachment_count, token, callback, nullptr);
}

int state_checkpoint_refresh_companion(const char *source)
{
	if (!source || !state_checkpoint_initialize()) return 0;
	engine->companion_valid = false;
	engine->companion.reset();
	if (!PHYSFS_exists(source)) {
		engine->companion_valid = true;
		return 1;
	}
	PHYSFS_File *file = PHYSFS_openRead(source);
	if (!file) {
		debug_log(DLOG_GAME, "checkpoint secret companion open failed; periodic save will retry");
		return 0;
	}
	const PHYSFS_sint64 size = PHYSFS_fileLength(file);
	bool ok = size > 0 && static_cast<uint64_t>(size) <= SIZE_MAX;
	std::shared_ptr<std::vector<unsigned char>> bytes;
	if (ok) {
		bytes = std::make_shared<std::vector<unsigned char>>(static_cast<size_t>(size));
		ok = PHYSFS_readBytes(file, bytes->data(), bytes->size()) == size;
	}
	if (!PHYSFS_close(file)) ok = false;
	if (ok) engine->companion = std::move(bytes);
	engine->companion_valid = ok;
	if (!ok) debug_log(DLOG_GAME, "checkpoint secret companion capture failed");
	return ok;
}

int state_checkpoint_submit_slot(const char *description, int save_kind, int slot,
                                 uint64_t token, state_checkpoint_callback callback)
{
	char filename[PATH_MAX];
	if (!state_android_build_save_filename(filename, sizeof(filename), slot, 0, 1)) return 0;
#ifdef DXX_BUILD_DESCENT_II
	char companion[PATH_MAX];
	if (!state_android_build_secret_filename(companion, sizeof(companion), slot)) return 0;
	/* The scheduler already backs off failed submissions. Retry an invalid
	 * capture at that deadline even when no level/secret transition occurred */
	if (!state_checkpoint_initialize() ||
	    (!engine->companion_valid && !state_checkpoint_refresh_companion(SECRETC_FILENAME))) return 0;
#else
	const char *companion = nullptr;
#endif
	return submit_disk(description, save_kind, filename, nullptr, 0, token, callback, companion);
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
		job->companion.reset();
		job->campaign.reset();
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

int state_checkpoint_cache_campaign(unsigned char *data, size_t size)
{
	if (!state_checkpoint_initialize()) {
		free(data);
		return 0;
	}
	engine->campaign.reset();
	engine->campaign_size = 0;
	engine->stats.campaign_bytes = 0;
	if (!data) return size == 0;
	try {
		engine->campaign = std::shared_ptr<const unsigned char>(data, free);
		engine->campaign_size = size;
		engine->stats.campaign_bytes = size;
		++engine->stats.campaign_generations;
		return 1;
	} catch (const std::bad_alloc &) {
		/* shared_ptr invokes the supplied deleter when allocation fails */
		return 0;
	}
}

void state_checkpoint_get_campaign(const unsigned char **data, size_t *size)
{
	*data = engine ? engine->campaign.get() : nullptr;
	*size = engine ? engine->campaign_size : 0;
}

int state_checkpoint_defer_campaign(rewind_file *file, const unsigned char *data, size_t size, uint32_t checksum)
{
	if (!capturing || !size) return 0;
	if (!file || file->memory_buffer != &capturing->buffer || capturing->campaign_size ||
	    data != engine->campaign.get() || size != engine->campaign_size) return -1;
	capturing->campaign = engine->campaign;
	capturing->campaign_size = size;
	capturing->campaign_offset = file->position;
	capturing->campaign_checksum = checksum;
	return 1;
}

void state_checkpoint_get_stats(state_checkpoint_stats *stats)
{
	if (stats) *stats = engine ? engine->stats : state_checkpoint_stats{};
}
