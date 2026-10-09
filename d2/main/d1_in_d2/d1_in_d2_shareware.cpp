#include <vector>
#include <map>
#include <memory>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>

#include "pstypes.h"
#include "inferno.h"
#include "gr.h"
#include "bm.h"
#include "u_mem.h"
#include "dxxerror.h"
#include "object.h"
#include "vclip.h"
#include "effects.h"
#include "polyobj.h"
#include "wall.h"
#include "textures.h"
#include "game.h"
#include "multi.h"
#include "iff.h"
#include "hostage.h"
#include "powerup.h"
#include "laser.h"
#include "sounds.h"
#include "piggy.h"
#include "aistruct.h"
#include "robot.h"
#include "weapon.h"
#include "gauges.h"
#include "player.h"
#include "fuelcen.h"
#include "endlevel.h"
#include "cntrlcen.h"
#include "args.h"
#include "text.h"
#include "strutil.h"
#include "d1_shareware_table.h"
#ifdef EDITOR
#include "editor/texpage.h"
#endif

}
#include "d1_shareware_definitions.hpp"

extern "C" {
#include "d1_in_d2_assets.h"
#include "d1_pig_validation.h"
#include "d1_shareware_model.h"
#include "d1_shareware_joints.h"
}

namespace {
using Bytes = std::vector<ubyte>;
class SourceHog {
 Bytes bytes;
 std::map<std::string, std::pair<size_t, size_t>> entries;
 static std::string key(const char *name) {
  std::string result(name);
  for (char &c : result) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
  return result;
 }
public:
 SourceHog() {
  std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFSX_openReadBuffered("descent.hog"), PHYSFS_close);
  if (!file) throw std::runtime_error("Missing descent.hog");
  auto size = PHYSFS_fileLength(file.get());
  if (size < 3 || size > 64 * 1024 * 1024) throw std::runtime_error("Invalid D1 HOG size");
  bytes.resize(static_cast<size_t>(size));
  if (PHYSFS_read(file.get(), bytes.data(), 1, static_cast<PHYSFS_uint32>(size)) != size || memcmp(bytes.data(), "DHF", 3))
   throw std::runtime_error("Cannot read D1 HOG");
  for (size_t p = 3; p < bytes.size();) {
   if (bytes.size() - p < 17) throw std::runtime_error("Truncated D1 HOG directory");
   char name[14]; memcpy(name, bytes.data() + p, 13); name[13] = 0;
   size_t length = d1_pof_u32(bytes.data() + p + 13); p += 17;
   if (length > bytes.size() - p || !entries.emplace(key(name), std::make_pair(p, length)).second)
    throw std::runtime_error("Invalid D1 HOG entry");
   p += length;
  }
 }
 Bytes read(const char *name) const {
  auto entry = entries.find(key(name));
  if (entry == entries.end()) throw std::runtime_error(std::string("Missing D1 HOG member: ") + name);
  return Bytes(bytes.begin() + entry->second.first, bytes.begin() + entry->second.first + entry->second.second);
 }
 bool has(const char *name) const { return entries.count(key(name)) != 0; }
};
struct TableInput {
 const Bytes &bytes; size_t position = 0;
 static int next(void *context) {
  auto &input = *static_cast<TableInput *>(context);
  return input.position == input.bytes.size() ? -1 : input.bytes[input.position++];
 }
};
vms_vector source_vector(d1_pof_vector source) {
 vms_vector result; result.x = source.x; result.y = source.y; result.z = source.z; return result;
}
class ImportedD1TableParser final : public D1TableParser {
 d1_asset_generation &generation;
 SourceHog archive;
 int tmap_list[800] = {}, hostage_clip[1] = {};
 char robot_names[30][16] = {}, powerup_names[29][16] = {};
 static std::string basename(const char *name) {
  if (!name || strlen(name) > 12) throw std::runtime_error("Invalid D1 bitmap name");
  std::string result(name); auto dot = result.find('.'); if (dot != std::string::npos) result.resize(dot); return result;
 }
 int bitmap(const std::string &name) {
  for (int i = 1; i <= generation.bitmap_data->bitmap_count; ++i)
   if (!d_stricmp(generation.bitmap_data->names[i], name.c_str())) return i;
  return 0;
 }
 bitmap_index bm_load_sub(int skip, char *name) override {
  bitmap_index result = {0}; if (skip) return result;
  result.index = bitmap(basename(name));
  if (!result.index) throw std::runtime_error(std::string("Missing D1 bitmap: ") + name);
  return result;
 }
 void ab_load(int skip, char *name, bitmap_index *images, int *count) override {
  if (skip) { images[0].index = 0; *count = 1; return; }
  std::string base = basename(name); *count = 0;
  for (int i = 0; i < 30; ++i) {
   int index = bitmap(base + "#" + std::to_string(i)); if (!index) break;
   images[(*count)++].index = index;
  }
  if (!*count) throw std::runtime_error(std::string("Missing D1 animation: ") + name);
 }
 int ds_load(int skip, char *name) override {
  if (skip) return 0;
  std::string base = basename(name);
  for (int i = 0; i < generation.sound_bank.count; ++i)
   if (!d_stricmp(generation.sound_bank.names[i], base.c_str())) return i;
  throw std::runtime_error(std::string("Missing D1 sound: ") + name);
 }
 char *texture_name(int index) override {
  if (index < 0 || index >= 800) throw std::runtime_error("Invalid D1 texture index");
  return generation.texture_names[index];
 }
 int read_model_guns(char *name, vms_vector *points, vms_vector *directions, int *models) override {
  Bytes bytes = archive.read(name); d1_pof_source source;
  const char *error = d1_pof_read(bytes.data(), bytes.size(), &source);
  if (error) throw std::runtime_error(error);
  if (source.version < 7 || (!models && source.gun_count > D1_MAX_CONTROLCEN_GUNS)) throw std::runtime_error("Invalid reactor guns");
  for (int i = 0; i < source.gun_count; ++i) {
   if (models) models[i] = source.gun_models[i];
   else if (source.gun_models[i]) throw std::runtime_error("Invalid reactor gun submodel");
   points[i] = source_vector(source.gun_points[i]); directions[i] = source_vector(source.gun_directions[i]);
  }
  return source.gun_count;
 }
 int load_polygon_model(char *name, int textures, int first, robot_info *robot) override {
  if (generation.num_polygon_models >= 85 || textures < 0 || first < 0 || textures + first > 210)
   throw std::runtime_error("Invalid D1 model count or textures");
  Bytes bytes = archive.read(name); d1_pof_source source;
  const char *error = d1_pof_read(bytes.data(), bytes.size(), &source);
  if (error) throw std::runtime_error(error);
  int index = generation.num_polygon_models++;
  d1_pof_bounds bounds;
  error = d1_pof_find_bounds(&source, &bounds);
  if (error) throw std::runtime_error(error);
  polymodel &model = generation.models[index];
  model.mins = source_vector(bounds.mins); model.maxs = source_vector(bounds.maxs);
  model.n_models = source.model_count; model.rad = source.radius;
  model.n_textures = textures; model.first_texture = first;
  model.model_data_size = static_cast<int>(source.instruction_size);
  model.model_data = static_cast<ubyte *>(d_malloc(source.instruction_size));
  if (!model.model_data) throw std::bad_alloc();
  memcpy(model.model_data, source.instructions, source.instruction_size);
  for (int i = 0; i < source.model_count; ++i) {
   model.submodel_parents[i] = source.parents[i]; model.submodel_ptrs[i] = source.offsets[i];
   model.submodel_rads[i] = source.radii[i]; model.submodel_offsets[i] = source_vector(source.translations[i]);
   model.submodel_norms[i] = source_vector(source.normals[i]); model.submodel_pnts[i] = source_vector(source.points[i]);
   model.submodel_mins[i] = source_vector(bounds.submodel_mins[i]); model.submodel_maxs[i] = source_vector(bounds.submodel_maxs[i]);
   if (!d1_pig_validate_model_stream(model.model_data, model.model_data_size, source.offsets[i]))
    throw std::runtime_error("Invalid D1 model instructions");
  }
  if (robot) {
   robot->n_guns = source.gun_count;
   for (int i = 0; i < source.gun_count; ++i) {
    robot->gun_submodels[i] = source.gun_models[i]; robot->gun_points[i] = source_vector(source.gun_points[i]);
   }
   if (source.has_animation) {
    vms_angvec angles[N_ANIM_STATES][MAX_SUBMODELS] = {};
    for (int f = 0; f < N_ANIM_STATES; ++f) for (int m = 0; m < source.model_count; ++m) {
     angles[f][m].p = source.animation[f][m].p; angles[f][m].b = source.animation[f][m].b; angles[f][m].h = source.animation[f][m].h;
    }
    if (!d1_shareware_build_joints(robot, &model, angles, generation.joints, &generation.num_robot_joints, 600))
     throw std::runtime_error("Invalid D1 joint table");
   }
  }
  return index;
 }
public:
 explicit ImportedD1TableParser(d1_asset_generation &value) : generation(value) {
  Textures = value.textures; TmapInfo = value.texture_info; Gauges = value.gauges; cockpit_bitmap = value.cockpits;
  ObjBitmaps = value.obj_bitmaps; ObjBitmapPtrs = value.obj_bitmap_ptrs; Vclip = value.vclips; Effects = value.effects;
  WallAnims = value.wall_anims; Robot_info = value.robots; Weapon_info = value.weapons; Powerup_info = value.powerups;
  Polygon_models = value.models; Player_ship = &value.ship; Reactors = &value.control_center;
  Sounds = value.sound_maps[0]; AltSounds = value.sound_maps[1]; ObjStrength = value.object_strength;
  ObjType = reinterpret_cast<sbyte *>(value.object_types); ObjId = reinterpret_cast<sbyte *>(value.object_ids);
  Dying_modelnums = value.dying_models; Dead_modelnums = value.dead_models;
  GameBitmaps.bind(value.bitmap_data->bitmaps, value.bitmap_data->bitmap_count + 1);
  TmapList = tmap_list; Hostage_vclip_num = hostage_clip; Robot_names = robot_names; Powerup_names = powerup_names;
 }
 void prepare() {
  bool encoded = !archive.has("bitmaps.tbl"); Bytes bytes = archive.read(encoded ? "bitmaps.bin" : "bitmaps.tbl");
  TableInput input{bytes}; d1_shareware_table_reader reader;
  d1_shareware_table_init(&reader, TableInput::next, &input, encoded); read(reader, 1);
  generation.num_textures = NumTextures; generation.num_wall_anims = Num_wall_anims;
  generation.num_gauges = 80; generation.num_cockpits = Num_cockpits; generation.num_powerups = N_powerup_types;
  generation.num_object_types = Num_total_object_types; generation.first_multi_bitmap = First_multi_bitmap_num;
  generation.exit_model = exit_modelnum; generation.destroyed_exit_model = destroyed_exit_modelnum;
  generation.num_robot_types = N_robot_types; generation.num_weapon_types = N_weapon_types;
  generation.num_vclips = 70; generation.num_effects = Num_effects; generation.control_center.model_num = -1;
  for (int i = 0; i < Num_total_object_types; ++i)
   if (generation.object_types[i] == D1_CONTROL_CENTER_OBJECT_TYPE) generation.control_center.model_num = generation.object_ids[i];
  for (int i = 0; i < D1_MAX_EFFECTS; ++i) generation.excluded_effects[i] = excluded_effects[i];
  for (int i = 0; i < N_robot_types; ++i) generation.unavailable_robots[i] = generation.robots[i].model_num == -1;
  for (auto &texture : generation.texture_info) texture.destroyed = -1;
  for (auto &robot : generation.robots) { robot.weapon_type2 = -1; robot.behavior = AIB_NORMAL; robot.aim = 255; }
 }
};
}

extern "C" int d1_in_d2_read_shareware_definitions(d1_asset_generation *generation, const char **error)
{
 static thread_local std::string message;
 try { ImportedD1TableParser parser(*generation); parser.prepare(); return 1; }
 catch (const std::exception &failure) { message = failure.what(); *error = message.c_str(); return 0; }
}
