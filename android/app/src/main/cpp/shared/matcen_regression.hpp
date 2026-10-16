#ifndef DXX_MATCEN_REGRESSION_HPP
#define DXX_MATCEN_REGRESSION_HPP
#include <cstdint>
extern "C" {
#include "matcen_regression_capture.h"
#include "segment.h"
#include "wall.h"
#include "robot.h"
#include "polyobj.h"
}

/* Stable integer identities, never pointer values or inferred author intent.
 * Missing disk records remain observable without dereferencing unsafe joins. */
template<class Json> Json matcen_regression_serialize(bool admitted = true)
{
    const auto &c = Matcen_regression_capture;
    Json out = {{"schema_version", 1}, {"admission", admitted ? "loaded" : "rejected_or_missing"}, {"capture_status", c.complete ? "complete" : "incomplete"},
        {"generated_count", c.generated_count}, {"disk_count", c.disk_count},
        {"declared_disk_count", c.declared_disk_count}, {"scope", "fresh_level_load"},
        {"trigger_representation", "decoded_disk_before_runtime_linking"}};
    Json authored = Json::array(), records = Json::array(), triggers = Json::array(), runtime = Json::array();
    auto record = [](const matcen_info &r) {
        Json masks = Json::array();
        masks.push_back(static_cast<uint32_t>(r.robot_flags[0]));
#ifdef DXX_BUILD_DESCENT_II
        masks.push_back(static_cast<uint32_t>(r.robot_flags[1]));
#endif
        return Json{{"segment", +r.segnum}, {"station", +r.fuelcen_num},
            {"robot_flags", masks}, {"hit_points", +r.hit_points}, {"interval", +r.interval}};
    };
    for (int i = 0; i < c.disk_count; ++i) {
        Json row = record(c.records[i]); row["ordinal"] = i; records.push_back(row);
    }
    for (int i = 0; i < c.generated_count; ++i) {
        const auto &a = c.segments[i];
        authored.push_back({{"segment", a.segment}, {"generated_ordinal", a.ordinal},
            {"serialized_matcen", a.raw_matcen}, {"serialized_station", a.raw_station}});
        Json row = {{"segment", a.segment}, {"generated_ordinal", a.ordinal}};
        if (a.segment < 0 || a.segment > Highest_segment_index) {
            row["binding_status"] = "invalid_segment"; runtime.push_back(row); continue;
        }
#ifdef DXX_BUILD_DESCENT_II
        const auto &s = Segment2s[a.segment];
#else
        const auto &s = Segments[a.segment];
#endif
        const int index = s.matcen_num;
        row["effective_ordinal"] = index;
        row["segment_station"] = s.value;
        if (index < 0 || index >= MAX_ROBOT_CENTERS) {
            row["binding_status"] = "physical_out_of_bounds"; runtime.push_back(row); continue;
        }
        const auto &r = RobotCenters[index];
        row["effective_record"] = record(r);
        Json eligible = Json::array();
        for (unsigned word = 0; word < sizeof(r.robot_flags) / sizeof(r.robot_flags[0]); ++word)
            for (unsigned bit = 0; bit < 32; ++bit) if (static_cast<uint32_t>(r.robot_flags[word]) & (1u << bit)) {
                const unsigned type = word * 32 + bit;
                Json robot = {{"type", type}};
                if (type >= static_cast<unsigned>(N_robot_types) || type >= MAX_ROBOT_TYPES) robot["status"] = "invalid_type";
                else {
                    const int model = Robot_info[type].model_num;
                    robot["model"] = model;
                    robot["status"] = model >= 0 && model < N_polygon_models ? "valid_model" : "invalid_model";
                }
                eligible.push_back(robot);
            }
        row["eligible_robots"] = eligible;
        row["binding_status"] = index >= c.disk_count ? "logical_disk_unbound" : "disk_bound";
        const int station = r.fuelcen_num;
        if (station < 0 || station >= Num_fuelcenters || station >= MAX_NUM_FUELCENS) {
            row["station_status"] = "invalid_station";
        } else {
            const auto &f = Station[station];
            row["station_status"] = f.Type != SEGMENT_IS_ROBOTMAKER ? "wrong_station_type" :
                (f.segnum != a.segment ? "different_station_segment" : "matching_station");
            row["station"] = {{"index", station}, {"segment", f.segnum}, {"type", f.Type}};
        }
        runtime.push_back(row);
    }
    for (int i = 0; i < c.trigger_count; ++i) {
        const auto &t = c.triggers[i];
        Json links = Json::array(), walls = Json::array();
        for (int j = 0; j < t.num_links && j < MAX_WALLS_PER_LINK; ++j)
            links.push_back({{"ordinal", j}, {"segment", +t.seg[j]}, {"side", +t.side[j]}});
        for (int j = 0; j < Num_walls && j < MAX_WALLS; ++j)
            if (Walls[j].trigger == i) walls.push_back({{"wall", j}, {"segment", +Walls[j].segnum}, {"side", +Walls[j].sidenum}});
        triggers.push_back({{"index", i}, {"type", +t.type}, {"flags", +t.flags},
            {"value", +t.value}, {"time", +t.time}, {"declared_link_count", +t.num_links},
            {"links", links}, {"runtime_source_walls", walls}});
    }
    out["authored_segments"] = authored; out["disk_records"] = records;
    out["triggers"] = triggers; out["runtime_bindings"] = runtime;
    return out;
}
#endif
