#ifndef INPUT_DEMO_FIXTURE_H
#define INPUT_DEMO_FIXTURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define INPUT_DEMO_FIXTURE_PACKED
#elif defined(__GNUC__)
#define INPUT_DEMO_FIXTURE_PACKED __attribute__((packed))
#else
#define INPUT_DEMO_FIXTURE_PACKED
#endif

typedef struct input_demo_rng_frame {
	uint32_t frame;
	uint32_t state;
	uint8_t has_call_count;
	uint32_t call_count;
} INPUT_DEMO_FIXTURE_PACKED input_demo_rng_frame;

typedef struct input_demo_rng_record {
	uint32_t frame;
	uint32_t run_length;
	uint32_t state;
	uint8_t has_call_count;
	uint32_t call_count;
} INPUT_DEMO_FIXTURE_PACKED input_demo_rng_record;

void input_demo_rng_frame_clear(input_demo_rng_frame *frame);
void input_demo_rng_record_clear(input_demo_rng_record *record);

#ifdef __cplusplus
}

#include <string>
#include <vector>

struct input_demo_stream_file {
	uint32_t player;
	std::string input_path;
	std::string rng_path;
};

struct input_demo_metadata {
	int version;
	std::string game;
	std::string mission;
	int level;
	int difficulty;
	std::string start_mode;
	std::string rng_mode;
	uint32_t frame_count;
	std::vector<input_demo_stream_file> streams;
	std::string classic_preview;
	std::string start_save;
	std::string result_path;
};

bool input_demo_rng_record_to_json_line(const input_demo_rng_record &record,
                                        std::string *line, std::string *error);
bool input_demo_rng_record_parse_json_line(const std::string &line,
                                           input_demo_rng_record *record, std::string *error);
bool input_demo_rng_records_coalesce_frames(const std::vector<input_demo_rng_frame> &frames,
                                            std::vector<input_demo_rng_record> *records, std::string *error);
bool input_demo_rng_records_write_jsonl_file(const char *path,
                                             const std::vector<input_demo_rng_record> &records, std::string *error);
bool input_demo_rng_records_read_jsonl_file(const char *path,
                                            std::vector<input_demo_rng_record> *records, std::string *error);
bool input_demo_metadata_to_json_text(const input_demo_metadata &metadata,
                                      std::string *text, std::string *error);
bool input_demo_metadata_write_json5_file(const char *path,
                                          const input_demo_metadata &metadata, std::string *error);

#endif

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef INPUT_DEMO_FIXTURE_PACKED

#endif