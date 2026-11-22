#ifndef DXX_XL_RESOURCE_VALIDATION_H
#define DXX_XL_RESOURCE_VALIDATION_H
#include <stddef.h>
#include "physfs.h"

#ifdef __cplusplus
extern "C" {
#endif
void xl_resource_baseline_clear(void);
void xl_resource_baseline_capture(void);
int xl_resource_load_vertigo(void);
int xl_resource_load_mod_data(void);
void xl_resource_note_startup_ham(const char *name, PHYSFS_file *file);
void xl_resource_select_mission(int requires_xl);
void xl_resource_commit_mission(void);
struct bim_song_info;
void xl_apply_mission_music(struct bim_song_info **songs, int *count, const char *song_list_owner, int enabled);
int xl_mission_music_path(const char *track, char *path, size_t capacity);
int xl_secret_music_slot(int ordinal, int fallback);
/* New D2 texture capability, not a general package-ownership or gameplay flag */
int xl_resource_texture_scope_enabled(void);
int xl_resource_owns_installation(const char *source);
const char *xl_resource_base_rejection(void);
void xl_discard_prepared_resources(void);
int xl_install_prepared_hxm(const char *level_filename);
void xl_initialize_camera_objects(void);
int xl_install_prepared_pog(const char *level_filename);
int xl_prepared_texture_brightness(int texture, int *brightness);
int xl_prepared_pog_receipt(int bitmap, unsigned *bytes, unsigned *fnv1a);
int xl_custom_model_radius(int model, int fallback);
struct object;
struct vms_vector;
int xl_model_gun_point(int model, int gun, struct vms_vector *point);
int xl_robot_gun_point(const struct object *robot, int gun, struct vms_vector *point);
void xl_hitbox_begin_frame(void);
int xl_camera_query(const struct object *camera, const struct object *moving, const struct vms_vector *start, const struct vms_vector *end, int radius, struct vms_vector *hit, struct vms_vector *normal);
int xl_camera_hitbox_query(const struct object *camera, const struct object *moving, const struct vms_vector *start, const struct vms_vector *end, struct vms_vector *hit, struct vms_vector *normal);
#ifdef __cplusplus
}

#include <memory>
#include <map>
#include <string>
#include "particle_resources.h"
const std::map<std::string, std::string> &xl_resource_mod_sources();
bool xl_resource_same_mod_sources(const std::map<std::string, std::string> &saved, const std::map<std::string, std::string> &incoming);
std::map<std::string, std::string> xl_resource_sources_for_level(int ordinal, std::map<int, int> *effect_frames = nullptr);
struct object;
struct vms_vector;
void xl_validate_level_graphics(const std::string &palette);
namespace xl_assets
{
struct ObjectDependencies;
}
namespace xl_compat
{
struct Inventory;
struct MissionLightingCompanions;
struct ParticleImageResource;
struct AnimatedImageResource;
struct ScopedTextureResources;
struct ScopedTexturePlayback;
class ModelHitboxTransform;
class TargetSources;
struct RenderModelSphere;
struct AfterburnerModel;
} // namespace xl_compat

// Own the definitions used by both companion and level dependency checks
class XlPreparedHam;
struct mission_preparation;
class XlPreparedResources
{
  public:
	explicit XlPreparedResources(const char *level_filename, const xl_compat::Inventory *level = nullptr, int ordinal = 0);
	explicit XlPreparedResources(XlPreparedHam incoming);
	// NULL uses the already selected mission; non-NULL never consults its identity
	static std::unique_ptr<XlPreparedResources> prepare_mission(mission_preparation *mission);
	static std::unique_ptr<XlPreparedResources> prepare_level(mission_preparation *mission, int ordinal);
	// Retain native level inputs for dependency inspection; does not admit gameplay
	static std::unique_ptr<XlPreparedResources> prepare_level_inputs(mission_preparation *mission, int ordinal);
	const xl_compat::Inventory &level_inventory() const;
	const xl_compat::ScopedTextureResources *scoped_textures() const;
	const xl_compat::ScopedTexturePlayback *scoped_texture_playback() const;
	const xl_compat::ScopedTexturePlayback *wall_texture_playback() const;
	std::map<int, int> effect_frame_counts() const;
	bool has_scoped_textures() const;
	const std::map<std::string, std::string> &sources() const;
	XlPreparedResources(XlPreparedResources &&) noexcept;
	XlPreparedResources &operator=(XlPreparedResources &&) noexcept;
	~XlPreparedResources();
	static void check_selected_definitions();
	xl_assets::ObjectDependencies &dependencies();
	std::string prepare_hxm(PHYSFS_file *file, bool *replaced_robots = nullptr);
	bool mission_replaces_robots() const;
	std::string prepare_vham(PHYSFS_file *file);
	std::string prepare_vham(std::vector<unsigned char> bytes);
	void prepare_banks(const std::string &palette);
	void prepare_pog(PHYSFS_file *file, const xl_compat::Inventory *level = nullptr);
	void install_hxm();
	void install_pog();
	void install_messages();
	void prepare_camera_model(PHYSFS_file *file = nullptr);
	int camera_robot_id() const;
	int camera_model_id() const;
	int camera_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, int radius, vms_vector &hit, vms_vector &normal);
	xl_compat::ModelHitboxTransform &model_hitboxes(int model);
	const xl_compat::RenderModelSphere &model_sphere(int model);
	int model_collision_radius(int model);
	xl_compat::AfterburnerModel afterburner_model(int model, bool player);
	int camera_hitbox_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, int frame, vms_vector &hit, vms_vector &normal);
	int camera_hitbox_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, vms_vector &hit, vms_vector &normal);
	void begin_hitbox_frame();
	void initialize_camera_objects();
	bool texture_brightness(int texture, int &brightness) const;
	const xl_compat::MissionLightingCompanions *classic_lighting() const;
	bool pog_receipt(int bitmap, unsigned *bytes, unsigned *fnv1a) const;
	int custom_model_radius(int model, int fallback) const;
	const xl_compat::ParticleImageResource *particle_image(int subtype = 3) const;
	xl_compat::ParticleImages particle_images() const;
	const xl_compat::ParticleImageResource *lightning_image() const;
	const xl_compat::ParticleImageResource *powerup_glare_image() const;
	const xl_compat::AnimatedImageResource *time_equipment_image(unsigned index) const;
	bool time_equipment_required() const;

  private:
	void prepare_scoped_textures(const xl_compat::Inventory &level, int ordinal, xl_compat::TargetSources &target);
	void prepare_level_resources(const char *filename, const xl_compat::Inventory *level, int ordinal, mission_preparation *mission, xl_compat::TargetSources *target);
	void validate_camera_query(const object &camera, const object &moving) const;
	struct State;
	std::unique_ptr<State> state;
};
void xl_set_prepared_resources(const char *level_filename, std::unique_ptr<XlPreparedResources> resources);
xl_compat::AfterburnerModel xl_prepared_afterburner_model(int model, bool player);
const xl_compat::ParticleImageResource *xl_prepared_particle_image(int subtype = 3);
xl_compat::ParticleImages xl_prepared_particle_images();
const xl_compat::ParticleImageResource *xl_prepared_lightning_image();
const xl_compat::ParticleImageResource *xl_prepared_powerup_glare_image();
const xl_compat::AnimatedImageResource *xl_prepared_time_equipment_image(unsigned index);
bool xl_prepared_time_equipment_required();
const xl_compat::MissionLightingCompanions *xl_prepared_classic_lighting();
#endif

#endif
