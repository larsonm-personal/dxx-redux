#include "graphics_safety_store.h"
#include "graphics_config_transaction.h"
#include "android_render_resolution.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/file.h>
#include <unistd.h>
#endif

const char *const graphics_safety_keys[GRAPHICS_SAFE_FIELD_COUNT] = {
	"TexFilt", "AnisoLevel", "MsaaLevel", "MenuTexFilt", "HudTexFilt",
	"ResolutionX", "ResolutionY", "AspectX", "AspectY", "ColorDepth"
};

namespace
{
using json = nlohmann::json;
std::recursive_mutex process_mutex;
struct file_lock {
	std::string root;
	int depth = 1;
#ifdef _WIN32
	HANDLE handle = INVALID_HANDLE_VALUE;
	OVERLAPPED overlap = {};
#else
	int fd = -1;
#endif
};
thread_local file_lock *held_lock = nullptr;

struct guard {
	void *lock;
	explicit guard(const char *root) : lock(graphics_safety_lock(root)) {}
	~guard()
	{
		graphics_safety_unlock(lock);
	}
	explicit operator bool() const
	{
		return lock != nullptr;
	}
};

std::string path(const char *root, const char *leaf)
{
	return std::string(root) + "/" + leaf;
}

std::vector<std::string> config_paths(const char *root)
{
	std::vector<std::string> paths = { path(root, "descent.cfg") };
	for (const char *game : { "d1x-redux", "d2x-redux" }) {
		const auto directory = path(root, game);
#ifdef _WIN32
		const DWORD attributes = GetFileAttributesA(directory.c_str());
		const bool exists = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
#else
		const bool exists = access(directory.c_str(), F_OK) == 0;
#endif
		if (exists) paths.emplace_back(directory + "/descent.cfg");
	}
	return paths;
}

json snapshot_json(const graphics_safety_snapshot &snapshot)
{
	json result = json::object();
	for (int i = 0; i < GRAPHICS_SAFE_FIELD_COUNT; ++i)
		result[graphics_safety_keys[i]] = snapshot.values[i];
	return result;
}

bool parse_snapshot(const json &value, graphics_safety_snapshot &snapshot)
{
	if (!value.is_object()) return false;
	for (int i = 0; i < GRAPHICS_SAFE_FIELD_COUNT; ++i) {
		const auto found = value.find(graphics_safety_keys[i]);
		if (found == value.end() || !found->is_number_integer()) return false;
		const auto number = found->get<int64_t>();
		if (number < INT_MIN || number > INT_MAX) return false;
		snapshot.values[i] = static_cast<int>(number);
	}
	return graphics_safety_normalize(&snapshot) != 0;
}

bool write_record(const char *root, const graphics_safety_record &record)
{
	json attempt = nullptr;
	json pending = json::object();
	for (int i = 0; i < GRAPHICS_SAFE_FIELD_COUNT; ++i)
		if (record.pending_mask & (1u << i)) pending[graphics_safety_keys[i]] = record.pending.values[i];
	if (record.phase != GRAPHICS_SAFE_IDLE) {
		attempt = { { "candidate", snapshot_json(record.candidate) }, { "trial_id", record.trial_id }, { "deadline_ms", record.deadline_ms }, { "owner_pid", record.owner_pid }, { "owner_session", record.owner_session }, { "phase", record.phase }, { "reason", record.reason } };
	}
	const auto bytes = json({ { "accepted", snapshot_json(record.accepted) }, { "attempt", attempt }, { "pending", pending } }).dump(2) + "\n";
	return graphics_config_atomic_replace(path(root, "graphics_safety.json").c_str(), bytes.data(), bytes.size()) ==
	       GRAPHICS_CONFIG_TRANSACTION_OK;
}

bool read_record(const char *root, graphics_safety_record &record)
{
	record = {};
	const auto filename = path(root, "graphics_safety.json");
	errno = 0;
	FILE *file = std::fopen(filename.c_str(), "rb");
	if (!file) {
		if (errno != ENOENT) return false;
		graphics_safety_defaults(&record.accepted);
		return write_record(root, record);
	}
	char bytes[16385];
	const auto size = std::fread(bytes, 1, sizeof(bytes), file);
	const bool good = !std::ferror(file) && size < sizeof(bytes);
	const bool closed = std::fclose(file) == 0;
	if (!good || !closed) return false;
	const auto value = json::parse(bytes, bytes + size, nullptr, false);
	if (value.is_discarded() || !value.contains("accepted") || !value.contains("attempt") ||
	    !value.contains("pending") || !value["pending"].is_object() ||
	    !parse_snapshot(value["accepted"], record.accepted)) return false;
	for (const auto &entry : value["pending"].items()) {
		const int index = graphics_safety_field_for_key(entry.key().c_str());
		if (index < 0 || !entry.value().is_number_integer()) return false;
		const auto number = entry.value().get<int64_t>();
		if (number < INT_MIN || number > INT_MAX) return false;
		record.pending.values[index] = static_cast<int>(number);
		record.pending_mask |= 1u << index;
	}
	const auto &attempt = value["attempt"];
	if (attempt.is_null()) return true;
	if (!attempt.is_object() || !attempt.contains("candidate") ||
	    !parse_snapshot(attempt["candidate"], record.candidate)) return false;
	record.trial_id = attempt.at("trial_id").get<uint64_t>();
	record.deadline_ms = attempt.at("deadline_ms").get<uint64_t>();
	record.owner_pid = attempt.at("owner_pid").get<int>();
	record.owner_session = attempt.at("owner_session").get<uint64_t>();
	record.phase = attempt.at("phase").get<int>();
	std::snprintf(record.reason, sizeof(record.reason), "%s", attempt.at("reason").get<std::string>().c_str());
	return record.trial_id != 0 && record.owner_pid > 0 &&
	       record.phase >= GRAPHICS_SAFE_ATTEMPT && record.phase <= GRAPHICS_SAFE_RESTORING;
}

bool alive(int pid)
{
	if (pid <= 0) return false;
#ifdef _WIN32
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
	if (!process) return GetLastError() == ERROR_ACCESS_DENIED;
	DWORD status = 0;
	const bool result = GetExitCodeProcess(process, &status) && status == STILL_ACTIVE;
	CloseHandle(process);
	return result;
#else
	return kill(pid, 0) == 0 || errno == EPERM;
#endif
}

uint64_t process_session(int pid)
{
#ifdef _WIN32
	HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
	if (!process) return 0;
	FILETIME creation, exit, kernel, user;
	const bool good = GetProcessTimes(process, &creation, &exit, &kernel, &user) != 0;
	CloseHandle(process);
	return good ? (static_cast<uint64_t>(creation.dwHighDateTime) << 32) | creation.dwLowDateTime : 0;
#else
	std::ifstream stream("/proc/" + std::to_string(pid) + "/stat");
	std::string line, token;
	if (!std::getline(stream, line)) return 0;
	const auto close = line.rfind(')');
	if (close == std::string::npos) return 0;
	std::istringstream fields(line.substr(close + 1));
	for (int number = 3; number <= 22; ++number)
		if (!(fields >> token)) return 0;
	try {
		return std::stoull(token);
	} catch (...) {
		return 0;
	}
#endif
}

bool owner_alive(const graphics_safety_record &record)
{
	return record.owner_session && alive(record.owner_pid) && process_session(record.owner_pid) == record.owner_session;
}

void merge_pending(const graphics_safety_record &record, graphics_safety_snapshot &snapshot)
{
	for (int i = 0; i < GRAPHICS_SAFE_FIELD_COUNT; ++i)
		if (record.pending_mask & (1u << i)) snapshot.values[i] = record.pending.values[i];
}

int gcd(int a, int b)
{
	while (b) {
		const int remainder = a % b;
		a = b;
		b = remainder;
	}
	return a;
}
} // namespace

extern "C" void *graphics_safety_lock(const char *root)
try {
	if (!root || !*root) return nullptr;
	std::unique_lock<std::recursive_mutex> process_lock(process_mutex);
	if (held_lock) {
		if (held_lock->root != root) {
			return nullptr;
		}
		++held_lock->depth;
		process_lock.release();
		return held_lock;
	}
	std::unique_ptr<file_lock> lock(new file_lock);
	lock->root = root;
	const auto filename = path(root, ".graphics_safety.lock");
#ifdef _WIN32
	lock->handle = CreateFileA(filename.c_str(), GENERIC_READ | GENERIC_WRITE,
	                           FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	const bool good = lock->handle != INVALID_HANDLE_VALUE &&
	                  LockFileEx(lock->handle, LOCKFILE_EXCLUSIVE_LOCK, 0, MAXDWORD, MAXDWORD, &lock->overlap);
#else
	lock->fd = open(filename.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	int result = -1;
	if (lock->fd >= 0) do {
			result = flock(lock->fd, LOCK_EX);
		} while (result < 0 && errno == EINTR);
	const bool good = result == 0;
#endif
	if (!good) {
#ifdef _WIN32
		if (lock->handle != INVALID_HANDLE_VALUE) CloseHandle(lock->handle);
#else
		if (lock->fd >= 0) close(lock->fd);
#endif
		return nullptr;
	}
	held_lock = lock.release();
	process_lock.release();
	return held_lock;
} catch (...) {
	return nullptr;
}

extern "C" void graphics_safety_unlock(void *opaque)
{
	if (!opaque) return;
	auto *lock = static_cast<file_lock *>(opaque);
	if (--lock->depth == 0) {
#ifdef _WIN32
		UnlockFileEx(lock->handle, 0, MAXDWORD, MAXDWORD, &lock->overlap);
		CloseHandle(lock->handle);
#else
		close(lock->fd);
#endif
		held_lock = nullptr;
		delete lock;
	}
	process_mutex.unlock();
}

extern "C" void graphics_safety_defaults(graphics_safety_snapshot *snapshot)
{
	*snapshot = { { 0, 0, 0, 0, 0, 640, 480, 3, 4, 0 } };
}

extern "C" int graphics_safety_normalize(graphics_safety_snapshot *snapshot)
{
	if (!snapshot) return 0;
	auto &v = snapshot->values;
	if (v[GRAPHICS_SAFE_WIDTH] < 0 || v[GRAPHICS_SAFE_HEIGHT] < 0 ||
	    !android_render_resolution_valid(static_cast<unsigned>(v[GRAPHICS_SAFE_WIDTH]),
	                                     static_cast<unsigned>(v[GRAPHICS_SAFE_HEIGHT])) ||
	    v[GRAPHICS_SAFE_ASPECT_X] <= 0 || v[GRAPHICS_SAFE_ASPECT_Y] <= 0 ||
	    v[GRAPHICS_SAFE_ASPECT_X] > 4096 || v[GRAPHICS_SAFE_ASPECT_Y] > 4096) return 0;
	v[GRAPHICS_SAFE_TEXFILT] = std::max(0, std::min(2, v[GRAPHICS_SAFE_TEXFILT]));
	v[GRAPHICS_SAFE_ANISO] = std::max(0, std::min(16, v[GRAPHICS_SAFE_ANISO]));
	v[GRAPHICS_SAFE_MSAA] = std::max(0, std::min(4, v[GRAPHICS_SAFE_MSAA]));
	v[GRAPHICS_SAFE_MENU_FILTER] = v[GRAPHICS_SAFE_MENU_FILTER] != 0;
	v[GRAPHICS_SAFE_HUD_FILTER] = v[GRAPHICS_SAFE_HUD_FILTER] != 0;
	v[GRAPHICS_SAFE_COLOR_DEPTH] = v[GRAPHICS_SAFE_COLOR_DEPTH] == 1;
	const int divisor = gcd(v[GRAPHICS_SAFE_ASPECT_X], v[GRAPHICS_SAFE_ASPECT_Y]);
	v[GRAPHICS_SAFE_ASPECT_X] /= divisor;
	v[GRAPHICS_SAFE_ASPECT_Y] /= divisor;
	return 1;
}

extern "C" int graphics_safety_equal(const graphics_safety_snapshot *a, const graphics_safety_snapshot *b)
{
	return a && b && std::equal(a->values, a->values + GRAPHICS_SAFE_FIELD_COUNT, b->values);
}

extern "C" int graphics_safety_all_off(const graphics_safety_snapshot *snapshot)
{
	return snapshot && snapshot->values[GRAPHICS_SAFE_TEXFILT] == 0 &&
	       snapshot->values[GRAPHICS_SAFE_ANISO] == 0 && snapshot->values[GRAPHICS_SAFE_MSAA] == 0;
}

extern "C" int graphics_safety_field_for_key(const char *key)
{
	if (key)
		for (int i = 0; i < GRAPHICS_SAFE_FIELD_COUNT; ++i)
			if (!std::strcmp(key, graphics_safety_keys[i])) return i;
	return -1;
}

extern "C" int graphics_safety_read_requested(const char *root, const char *game, graphics_safety_snapshot *snapshot)
try {
	guard lock(root);
	if (!lock || !snapshot) return 0;
	graphics_safety_defaults(snapshot);
	// Requested defaults match config.c; the conservative accepted bootstrap disables HUD filtering
	snapshot->values[GRAPHICS_SAFE_HUD_FILTER] = 1;
	std::vector<std::string> paths = { path(root, "descent.cfg") };
	if (game && (!std::strcmp(game, "d1") || !std::strcmp(game, "d2")))
		paths.emplace_back(path(root, !std::strcmp(game, "d1") ? "d1x-redux/descent.cfg" : "d2x-redux/descent.cfg"));
	for (const auto &filename : paths) {
		std::ifstream stream(filename);
		std::string line;
		while (std::getline(stream, line)) {
			const auto separator = line.find('=');
			if (separator == std::string::npos) continue;
			const auto key = line.substr(0, separator);
			const int field = graphics_safety_field_for_key(key.c_str());
			if (field < 0) continue;
			try {
				snapshot->values[field] = std::stoi(line.substr(separator + 1));
			} catch (...) {
				return 0;
			}
		}
		if (stream.bad()) return 0;
	}
	return graphics_safety_normalize(snapshot);
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_read_record(const char *root, graphics_safety_record *record)
try {
	guard lock(root);
	return lock && record && read_record(root, *record);
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_read_staged(const char *root, const char *game, graphics_safety_snapshot *snapshot)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !snapshot || !read_record(root, record) || !graphics_safety_read_requested(root, game, snapshot)) return 0;
	merge_pending(record, *snapshot);
	return graphics_safety_normalize(snapshot);
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_flush_staged(const char *root)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !read_record(root, record)) return 0;
	if (!record.pending_mask || record.phase == GRAPHICS_SAFE_PREPARING ||
	    record.phase == GRAPHICS_SAFE_CHALLENGE || record.phase == GRAPHICS_SAFE_RESTORING) return 1;
	graphics_safety_snapshot snapshot;
	if (!graphics_safety_read_requested(root, nullptr, &snapshot)) return 0;
	merge_pending(record, snapshot);
	if (!graphics_safety_normalize(&snapshot) || !graphics_safety_patch_snapshot(root, &snapshot)) return 0;
	record.pending_mask = 0;
	// A failed journal clear is harmless: the same explicit edits will be published again
	return write_record(root, record) ? 2 : 0;
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_stage(const char *root, const graphics_config_update *updates, size_t count)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !updates || !count || count > GRAPHICS_SAFE_FIELD_COUNT || !read_record(root, record)) return 0;
	unsigned seen = 0;
	for (size_t i = 0; i < count; ++i) {
		const int index = graphics_safety_field_for_key(updates[i].key);
		if (index < 0 || (seen & (1u << index))) return 0;
		seen |= 1u << index;
		record.pending_mask |= 1u << index;
		record.pending.values[index] = updates[i].value;
	}
	graphics_safety_snapshot proposed;
	if (!graphics_safety_read_requested(root, nullptr, &proposed)) return 0;
	merge_pending(record, proposed);
	if (!graphics_safety_normalize(&proposed) || !write_record(root, record)) return 0;
	if (record.phase == GRAPHICS_SAFE_PREPARING || record.phase == GRAPHICS_SAFE_CHALLENGE ||
	    record.phase == GRAPHICS_SAFE_RESTORING) return 2;
	return graphics_safety_flush_staged(root) == 2 ? 1 : 0;
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_begin_attempt(const char *root, const graphics_safety_snapshot *candidate,
                                             uint64_t trial_id, int owner_pid, int preparing)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !candidate || !trial_id || owner_pid <= 0 || !read_record(root, record)) return 0;
	if (record.phase == GRAPHICS_SAFE_RESTORING ||
	    (record.phase != GRAPHICS_SAFE_IDLE && record.owner_pid != owner_pid && owner_alive(record))) return 0;
	record.candidate = *candidate;
	if (!graphics_safety_normalize(&record.candidate)) return 0;
	if (graphics_safety_equal(&record.candidate, &record.accepted) || graphics_safety_all_off(&record.candidate)) {
		record.phase = GRAPHICS_SAFE_IDLE;
		return write_record(root, record);
	}
	record.trial_id = trial_id;
	record.owner_pid = owner_pid;
	record.owner_session = process_session(owner_pid);
	record.deadline_ms = 0;
	record.phase = preparing ? GRAPHICS_SAFE_PREPARING : GRAPHICS_SAFE_ATTEMPT;
	record.reason[0] = 0;
	return write_record(root, record);
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_arm(const char *root, uint64_t trial_id, uint64_t now_ms)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !read_record(root, record) || record.trial_id != trial_id ||
	    record.phase != GRAPHICS_SAFE_PREPARING || now_ms > UINT64_MAX - 5000) return 0;
	record.deadline_ms = now_ms + 5000;
	record.phase = GRAPHICS_SAFE_CHALLENGE;
	return write_record(root, record);
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_patch_snapshot(const char *root, const graphics_safety_snapshot *snapshot)
try {
	guard lock(root);
	if (!lock || !snapshot) return 0;
	auto normalized = *snapshot;
	if (!graphics_safety_normalize(&normalized)) return 0;
	const auto strings = config_paths(root);
	std::vector<const char *> paths;
	for (const auto &filename : strings) paths.push_back(filename.c_str());
	graphics_config_update updates[GRAPHICS_SAFE_FIELD_COUNT];
	for (int i = 0; i < GRAPHICS_SAFE_FIELD_COUNT; ++i)
		updates[i] = { graphics_safety_keys[i], normalized.values[i] };
	return graphics_config_patch_batch(paths.data(), paths.size(), updates, GRAPHICS_SAFE_FIELD_COUNT) ==
	       GRAPHICS_CONFIG_TRANSACTION_OK;
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_decide(const char *root, uint64_t trial_id, int accept,
                                      uint64_t now_ms, const char *reason)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !read_record(root, record)) return -1;
	if (record.trial_id != trial_id || record.phase == GRAPHICS_SAFE_IDLE) return 0;
	if (record.phase == GRAPHICS_SAFE_RESTORING) return 2;
	if (accept && record.phase != GRAPHICS_SAFE_CHALLENGE) return 0;
	if (accept && record.phase == GRAPHICS_SAFE_CHALLENGE && now_ms < record.deadline_ms) {
		auto accepted = record;
		accepted.accepted = record.candidate;
		accepted.phase = GRAPHICS_SAFE_IDLE;
		if (write_record(root, accepted)) return 1;
		/* A failed OK retains the old accepted tuple and takes the restore path */
		reason = "accept_persist_failed";
	}
	record.phase = GRAPHICS_SAFE_RESTORING;
	std::snprintf(record.reason, sizeof(record.reason), "%s", reason ? reason : "cancel");
	if (!write_record(root, record)) return -1;
	/* Marker remains until both file publication and renderer recovery acknowledge */
	if (!graphics_safety_patch_snapshot(root, &record.accepted)) return -1;
	return 2;
} catch (...) {
	return -1;
}

extern "C" int graphics_safety_complete_restore(const char *root, uint64_t trial_id)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !read_record(root, record) || record.trial_id != trial_id ||
	    record.phase != GRAPHICS_SAFE_RESTORING || !graphics_safety_patch_snapshot(root, &record.accepted)) return 0;
	record.phase = GRAPHICS_SAFE_IDLE;
	return write_record(root, record);
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_recover(const char *root, int current_pid)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !read_record(root, record)) return 0;
	if (record.phase == GRAPHICS_SAFE_IDLE) return graphics_safety_flush_staged(root) ? 1 : 0;
	const bool owns_attempt = owner_alive(record);
	if (owns_attempt && record.owner_pid != current_pid) return 1;
	if (owns_attempt && record.phase != GRAPHICS_SAFE_RESTORING) return 1;
	record.phase = GRAPHICS_SAFE_RESTORING;
	std::snprintf(record.reason, sizeof(record.reason), "abandoned_attempt");
	if (!write_record(root, record) || !graphics_safety_complete_restore(root, record.trial_id)) return 0;
	return graphics_safety_flush_staged(root) ? 2 : 0;
} catch (...) {
	return 0;
}

extern "C" int graphics_safety_clean_exit(const char *root, int owner_pid)
try {
	guard lock(root);
	graphics_safety_record record;
	if (!lock || !read_record(root, record)) return 0;
	if (record.phase == GRAPHICS_SAFE_IDLE) return graphics_safety_flush_staged(root) != 0;
	if (record.owner_pid != owner_pid || record.owner_session != process_session(owner_pid)) return 1;
	if (record.phase == GRAPHICS_SAFE_CHALLENGE || record.phase == GRAPHICS_SAFE_PREPARING)
		return graphics_safety_decide(root, record.trial_id, 0, 0, "exit") == 2 &&
		       graphics_safety_complete_restore(root, record.trial_id);
	if (record.phase == GRAPHICS_SAFE_RESTORING)
		return graphics_safety_complete_restore(root, record.trial_id);
	record.phase = GRAPHICS_SAFE_IDLE;
	return write_record(root, record) && graphics_safety_flush_staged(root);
} catch (...) {
	return 0;
}
