#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include <chromaprint.h>

#include "chromaprint_db.h"

static constexpr int kFingerprintLength = 20;
static uint32_t s_raw_fp[kFingerprintLength];

static std::string make_entry_json(const char *name, const char *encoded)
{
	return std::string("[{\"name\":\"") + name +
	       "\",\"disc_id\":\"disc\",\"track\":7,\"duration_ms\":123000,"
	       "\"chromaprint\":\"" +
	       encoded + "\"}]";
}

static void check_match(const char *expected_name, std::atomic<bool> &failed)
{
	chromaprint_db_match_t match = {};
	if (chromaprint_db_match(s_raw_fp, kFingerprintLength, 123000, &match) !=
	    CHROMAPRINT_DB_MATCH_FOUND) {
		failed = true;
		return;
	}
	if (strcmp(match.name, expected_name) != 0 || strcmp(match.disc_id, "disc") != 0 ||
	    match.track_num != 7 || match.confidence != 1.0f)
		failed = true;
	chromaprint_db_match_free(&match);
}

int main()
{
	for (int i = 0; i < kFingerprintLength; i++)
		s_raw_fp[i] = 0x12345678U + (uint32_t) i;
	char *encoded = nullptr;
	int encoded_size = 0;
	if (!chromaprint_encode_fingerprint(s_raw_fp, kFingerprintLength,
	                                    CHROMAPRINT_ALGORITHM_DEFAULT,
	                                    &encoded, &encoded_size, 1))
		return 1;

	const std::string json_a = make_entry_json("generation-a", encoded);
	const std::string json_b = make_entry_json("generation-b", encoded);
	chromaprint_dealloc(encoded);
	if (!chromaprint_db_set_threshold(0.4f) ||
	    chromaprint_db_load(json_a.data(), (int) json_a.size()) != 1)
		return 1;

	std::atomic<bool> failed = false;
	std::atomic<bool> start = false;
	std::vector<std::thread> threads;
	for (int writer = 0; writer < 2; writer++) {
		threads.emplace_back([&, writer] {
			while (!start.load()) std::this_thread::yield();
			for (int i = 0; i < 250; i++) {
				chromaprint_db_set_threshold(i % 2 ? 0.4f : 0.6f);
				chromaprint_db_set_duration_tolerance(i % 2 ? 0.1f : 0.2f);
				if ((i + writer) % 11 == 0) chromaprint_db_free();
				const std::string &json = (i + writer) % 2 ? json_a : json_b;
				if (chromaprint_db_load(json.data(), (int) json.size()) != 1)
					failed = true;
			}
		});
	}
	for (int reader = 0; reader < 6; reader++) {
		threads.emplace_back([&] {
			while (!start.load()) std::this_thread::yield();
			for (int i = 0; i < 2000; i++) {
				chromaprint_db_match_t match = {};
				if (chromaprint_db_match(s_raw_fp, kFingerprintLength, 123000, &match) ==
				    CHROMAPRINT_DB_MATCH_FOUND) {
					if ((strcmp(match.name, "generation-a") != 0 &&
					     strcmp(match.name, "generation-b") != 0) ||
					    strcmp(match.disc_id, "disc") != 0 || match.track_num != 7)
						failed = true;
					chromaprint_db_match_free(&match);
				}
				int count = chromaprint_db_count();
				if (count != 0 && count != 1) failed = true;
			}
		});
	}
	start = true;
	for (auto &thread : threads) thread.join();

	if (chromaprint_db_load(json_a.data(), (int) json_a.size()) != 1) failed = true;
	threads.clear();
	for (int reader = 0; reader < 8; reader++)
		threads.emplace_back([&] {
			for (int i = 0; i < 1000; i++) check_match("generation-a", failed);
		});
	for (auto &thread : threads) thread.join();

	chromaprint_db_free();
	if (failed) return 1;
	printf("chromaprint DB concurrency tests passed\n");
	return 0;
}
