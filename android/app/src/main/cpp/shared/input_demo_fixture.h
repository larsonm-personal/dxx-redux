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

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef INPUT_DEMO_FIXTURE_PACKED

#ifdef __cplusplus
}

#include <string>
#include <vector>

#include "input_demo_controls.h"
#include "input_demo_result.h"

struct input_demo_metadata {
	int version;
	std::string game;
	std::string mission;
	int level;
	int difficulty;
	std::string start_mode;
	std::string rng_mode;
	uint32_t frame_count;
	std::string classic_preview;
	std::string start_save;
};

struct input_demo_checkpoint {
	std::string format;
	std::string encoding;
	uint32_t size;
	std::string sha256;
	std::string save_name;
	uint8_t has_start_gt;
	int64_t start_gt;
	uint8_t has_next_laser_fire_delta;
	int32_t next_laser_fire_delta;
	uint8_t has_next_missile_fire_delta;
	int32_t next_missile_fire_delta;
	uint8_t has_last_laser_fired_delta;
	int32_t last_laser_fired_delta;
	uint8_t has_auto_fire_fusion_delta;
	int32_t auto_fire_fusion_delta;
	std::string data;

	input_demo_checkpoint()
	    : size(0), has_start_gt(0), start_gt(0), has_next_laser_fire_delta(0), next_laser_fire_delta(0),
	      has_next_missile_fire_delta(0), next_missile_fire_delta(0), has_last_laser_fired_delta(0),
	      last_laser_fired_delta(0), has_auto_fire_fusion_delta(0), auto_fire_fusion_delta(0)
	{
	}
};

struct input_demo_file_frame {
	input_demo_control_record input;
	input_demo_rng_record rng;
};

struct input_demo_file {
	input_demo_metadata metadata;
	bool has_checkpoint;
	input_demo_checkpoint checkpoint;
	std::vector<input_demo_file_frame> frames;
	bool has_result;
	input_demo_result result;

	input_demo_file()
	    : has_checkpoint(false), has_result(false)
	{
		input_demo_result_clear(&result);
	}
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
bool input_demo_metadata_parse_header_line(const std::string &line,
                                           input_demo_metadata *metadata, std::string *error);
bool input_demo_metadata_to_header_line(const input_demo_metadata &metadata,
                                        std::string *line, std::string *error);
bool input_demo_file_parse_text(const std::string &text,
                                input_demo_file *demo, std::string *error);
bool input_demo_file_read(const char *path,
                          input_demo_file *demo, std::string *error);
bool input_demo_file_to_text(const input_demo_file &demo,
                             std::string *text, std::string *error);
bool input_demo_file_write(const char *path,
                           const input_demo_file &demo, std::string *error);

#endif

#endif