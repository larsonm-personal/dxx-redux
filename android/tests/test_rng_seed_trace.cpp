#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

extern "C" {
#include "maths.h"
}
#include "input_demo_rng_trace.h"

using nlohmann::json;

int main(int argc, char **argv)
{
	if (argc != 2) {
		std::cerr << "Expected trace output path\n";
		return 1;
	}
	const unsigned int seeds[] = { 0u, 1u, 0x80000000u, 0xffffffffu };
	std::vector<json> expected;
	unsigned int counts[D_RNG_STREAM_COUNT] = { 100u, 200u };
	const bool has_state = d_rand_get_replay_mode() == D_RAND_REPLAY_MODE_LCG_STATE;
	for (int stream = 0; stream < D_RNG_STREAM_COUNT; ++stream) {
		d_rand_set_stream_state(static_cast<d_rng_stream>(stream), 0x12345678u + stream);
		d_rand_set_stream_call_count(static_cast<d_rng_stream>(stream), counts[stream]);
	}
	input_demo_rng_trace_start();
	input_demo_rng_trace_set_context(7, 123456);
	for (unsigned int seed : seeds) {
		for (int stream = 0; stream < D_RNG_STREAM_COUNT; ++stream) {
			const auto selected = static_cast<d_rng_stream>(stream);
			unsigned int before = 0;
			d_rand_get_stream_state(selected, &before);
			d_srand_annotated(selected, seed, "rng-seed-test", "seed", 42);
			json reseed = { { "type", "srand" }, { "stream", stream }, { "seed", seed }, { "call_count", counts[stream] } };
			if (has_state) {
				reseed["state_before"] = before;
				reseed["state_after"] = seed;
			}
			expected.push_back(reseed);
			const int result = d_rand_annotated(selected, "rng-seed-test", "draw", 43);
			json draw = { { "type", "rand" }, { "stream", stream }, { "result", result }, { "call_count", ++counts[stream] } };
			if (has_state) {
				unsigned int after = 0;
				d_rand_get_stream_state(selected, &after);
				draw["state_before"] = seed;
				draw["state_after"] = after;
			}
			expected.push_back(draw);
		}
	}
	char error[256] = {};
	if (!input_demo_rng_trace_write_to_path(argv[1], error, sizeof(error))) {
		std::cerr << error << '\n';
		input_demo_rng_trace_reset();
		return 1;
	}
	input_demo_rng_trace_reset();
	try {
		std::ifstream input(argv[1]);
		std::string line;
		if (!std::getline(input, line)) throw std::runtime_error("Missing trace metadata");
		const json metadata = json::parse(line);
		if (metadata.at("type") != "meta" || metadata.at("events") != expected.size() || metadata.at("truncated") != false)
			throw std::runtime_error("Incorrect trace metadata");
		for (size_t i = 0; i < expected.size(); ++i) {
			if (!std::getline(input, line)) throw std::runtime_error("Missing trace event");
			json actual = json::parse(line);
			if (actual.value("stream", 0) == 0) actual["stream"] = 0;
			if (actual.at("seq") != i || actual.at("frame") != 7 || actual.at("gt") != 123456 || actual.at("file") != "rng-seed-test")
				throw std::runtime_error("Incorrect event context");
			if (actual.contains("state_before") != has_state || actual.contains("state_after") != has_state)
				throw std::runtime_error("Incorrect state availability");
			for (auto field = expected[i].begin(); field != expected[i].end(); ++field) {
				if (actual.at(field.key()) != field.value()) {
					std::cerr << "Event " << i << ' ' << field.key() << ": expected " << field.value() << ", got " << actual.at(field.key()) << '\n';
					return 1;
				}
			}
		}
		if (std::getline(input, line)) throw std::runtime_error("Unexpected extra trace event");
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
	std::cout << "PASS: SIM/FX reseed and draw trace fields (" << (has_state ? "LCG" : "libc") << ")\n";
	return 0;
}
