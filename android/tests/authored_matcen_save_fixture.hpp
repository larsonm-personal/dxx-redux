// Ordinary on-disk save continuation on original mission geometry and trigger links.
// The owner runs write/restore in different processes with identical hashed inputs.
static void exercise_authored_matcen_save(const char *mission, int level, int trigger, const char *mode)
{
 using nlohmann::json;
 const bool restore = std::strcmp(mode, "restore") == 0;
 const bool control = std::strcmp(mode, "no-dispatch") == 0;
 require(restore || control || std::strcmp(mode, "write") == 0, "known authored save mode");
 require(trigger >= 0 && trigger < Num_triggers && trigger < MAX_TRIGGERS, "bounded authored trigger");
 const auto &t = Triggers[trigger];
#ifdef DXX_BUILD_DESCENT_II
 require(t.type == TT_MATCEN && !(t.flags & TF_DISABLED), "enabled native D2 matcen-only trigger");
#else
 require((t.flags & TRIGGER_MATCEN) && !(t.flags & ~(TRIGGER_MATCEN | TRIGGER_ON | TRIGGER_ONE_SHOT)), "native D1 matcen-only trigger");
#endif
 require(t.num_links > 0 && t.num_links <= MAX_WALLS_PER_LINK, "bounded nonempty original trigger links");
 std::vector<int> centers;
 for (int link = 0; link < t.num_links; ++link) {
  const int seg = t.seg[link];
  require(seg >= 0 && seg <= Highest_segment_index && seg < MAX_SEGMENTS && t.side[link] >= 0 && t.side[link] < 6, "valid authored target");
#ifdef DXX_BUILD_DESCENT_II
  const auto &s = Segment2s[seg];
#else
  const auto &s = Segments[seg];
#endif
  const int c = s.matcen_num;
  require(s.special == SEGMENT_IS_ROBOTMAKER && c >= 0 && c < Num_robot_centers && c < MAX_ROBOT_CENTERS, "defined loaded robot-center ordinal");
  const int st = RobotCenters[c].fuelcen_num;
  require(st >= 0 && st < Num_fuelcenters && st < MAX_NUM_FUELCENS && st < 128, "bounded representable station");
  require(Station[st].Type == SEGMENT_IS_ROBOTMAKER && Station[st].segnum == seg && s.value == st && ConsoleObject->segnum != seg, "original station binding and unobstructed player");
  bool nonempty = false;
  for (unsigned w = 0; w < sizeof(RobotCenters[c].robot_flags) / sizeof(RobotCenters[c].robot_flags[0]); ++w)
   for (unsigned b = 0; b < 32; ++b) if (static_cast<uint32_t>(RobotCenters[c].robot_flags[w]) & (1u << b)) {
    const unsigned type = w * 32 + b;
    require(type < N_robot_types && type < MAX_ROBOT_TYPES && Robot_info[type].model_num >= 0 && Robot_info[type].model_num < N_polygon_models, "valid authored robot definition");
    nonempty = true;
   }
  require(nonempty, "nonvacuous authored mask");
  if (std::find(centers.begin(), centers.end(), c) == centers.end()) centers.push_back(c);
 }
 auto snapshot = [&]() {
  json rows = json::array();
  for (const int c : centers) {
   require(c >= 0 && c < Num_robot_centers && c < MAX_ROBOT_CENTERS, "restored ordinal remains bounded");
   const int st = RobotCenters[c].fuelcen_num;
   require(st >= 0 && st < Num_fuelcenters && st < MAX_NUM_FUELCENS && st < 128, "restored station remains bounded");
   const auto &f = Station[st];
   json mask = json::array();
   for (unsigned w = 0; w < sizeof(RobotCenters[c].robot_flags) / sizeof(RobotCenters[c].robot_flags[0]); ++w) mask.push_back(static_cast<uint32_t>(RobotCenters[c].robot_flags[w]));
   rows.push_back({{"ordinal", c}, {"station", st}, {"segment", +f.segnum}, {"mask", mask},
    {"enabled", +f.Enabled}, {"lives", +f.Lives}, {"capacity", +f.Capacity}, {"timer", +f.Timer}, {"flag", +f.Flag}, {"disable_time", +f.Disable_time}});
  }
  unsigned sim = 0;
  require(d_rand_get_state(&sim), "capture save continuation SIM state");
  return json{{"centers", rows}, {"sim_state", sim}};
 };
 char filename[] = "authored-matcen.sav", description[21] = "Authored matcen";
 FrameTime = F1_0 / 60;
 if (restore) {
#ifdef DXX_BUILD_DESCENT_II
  require(state_restore_all_sub(filename, 0), "cold restore actual ordinary D2 save");
#else
  require(state_restore_all_sub(filename), "cold restore actual ordinary D1 save");
#endif
 } else {
  // Controlled production probe: retire only blocking robots, without inventing kills.
  for (int obj = 0; obj <= Highest_object_index; ++obj) if (Objects[obj].type == OBJ_ROBOT)
   for (int c : centers) if (Objects[obj].segnum == Station[RobotCenters[c].fuelcen_num].segnum) { obj_delete(obj); break; }
  d_srand(1234);
  GameTime64 = 0;
  if (!control) check_trigger_sub(trigger, Player_num, 0);
  for (int frame = 0; frame < 10; ++frame) { GameTime64 += FrameTime; fuelcen_update_all(); }
  if (!control) {
   stop_time();
   require(state_save_all_sub(filename, description), "write actual ordinary authored save");
  }
 }
 require(Current_level_num == level && Current_mission && std::strcmp(Current_mission->filename, mission) == 0, "saved mission and secret identity retained");
 const char *level_file = level < 0 ? Secret_level_names[-level - 1] : Level_names[level - 1];
 json result = {{"identity", {{"mission", Current_mission->filename}, {"level", Current_level_num}, {"level_file", level_file}}}, {"checkpoint", snapshot()}};
 for (const auto &c : result["checkpoint"]["centers"]) require(c["enabled"].get<int>() == (control ? 0 : 1), "checkpoint contains real enabled matcen or no-dispatch control");
 std::vector<int> existing;
 for (int obj = 0; obj <= Highest_object_index; ++obj) if (Objects[obj].type != OBJ_NONE) existing.push_back(Objects[obj].signature);
 json births = json::array();
 for (int frame = 0; frame < 1200; ++frame) {
  FrameTime = F1_0 / 60;
  GameTime64 += FrameTime;
  fuelcen_update_all();
  for (int obj = 0; obj <= Highest_object_index; ++obj) {
   const auto &r = Objects[obj];
   if (r.type != OBJ_ROBOT || std::find(existing.begin(), existing.end(), r.signature) != existing.end()) continue;
   require(static_cast<ubyte>(r.matcen_creator) & 0x80, "observed production has matcen creator");
   const int st = static_cast<ubyte>(r.matcen_creator) & 0x7f;
   int center = -1;
   for (int c : centers) if (RobotCenters[c].fuelcen_num == st) center = c;
   require(center >= 0 && r.id < N_robot_types && r.id < MAX_ROBOT_TYPES, "birth belongs to selected authored station");
   require((static_cast<uint32_t>(RobotCenters[center].robot_flags[r.id / 32]) & (1u << (r.id % 32))) && r.segnum == Station[st].segnum, "birth retains mask and segment");
   births.push_back({{"frame", frame}, {"ordinal", center}, {"station", st}, {"segment", +r.segnum}, {"type", +r.id}, {"shields", +r.shields}});
   obj_delete(obj); // Permit subsequent births; no AI/combat or kill-count simulation.
  }
 }
 require(control ? births.empty() : !births.empty(), "nonvacuous save continuation and no-dispatch control");
 if (!control) for (int c : centers) require(std::any_of(births.begin(), births.end(), [c](const json &b) { return b["ordinal"].get<int>() == c; }), "every selected station produces after save");
 result["births"] = births;
 result["terminal"] = snapshot();
 const std::string path = std::string(mode) + ".json", data = result.dump(2) + "\n";
 FILE *out = std::fopen(path.c_str(), "wb");
 require(out && std::fwrite(data.data(), 1, data.size(), out) == data.size(), "write complete authored save report");
 require(std::fclose(out) == 0, "close authored save report");
}
