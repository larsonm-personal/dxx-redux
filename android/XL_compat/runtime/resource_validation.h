#ifndef DXX_XL_RESOURCE_VALIDATION_H
#define DXX_XL_RESOURCE_VALIDATION_H
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
void xl_hitbox_begin_frame(void);
int xl_camera_query(const struct object *camera, const struct object *moving, const struct vms_vector *start, const struct vms_vector *end, int radius, struct vms_vector *hit, struct vms_vector *normal);
int xl_camera_hitbox_query(const struct object *camera, const struct object *moving, const struct vms_vector *start, const struct vms_vector *end, struct vms_vector *hit, struct vms_vector *normal);
#ifdef __cplusplus
}

#include <memory>
#include <map>
#include <string>
const std::map<std::string, std::string> &xl_resource_mod_sources();
std::map<std::string, std::string> xl_resource_sources_for_level(int ordinal);
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
struct ParticleImageResource;
struct AnimatedImageResource;
class ModelHitboxTransform;
struct RenderModelSphere;
} // namespace xl_compat

// Own the definitions used by both companion and level dependency checks
class XlPreparedHam;
class XlPreparedResources
{
  public:
	explicit XlPreparedResources(const char *level_filename, const xl_compat::Inventory *level = nullptr, int ordinal = 0);
	explicit XlPreparedResources(XlPreparedHam incoming);
	~XlPreparedResources();
	xl_assets::ObjectDependencies &dependencies();
	std::string prepare_hxm(PHYSFS_file *file);
	void install_hxm();
	void install_pog();
	void install_messages();
	void prepare_camera_model();
	int camera_robot_id() const;
	int camera_model_id() const;
	int camera_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, int radius, vms_vector &hit, vms_vector &normal);
	xl_compat::ModelHitboxTransform &model_hitboxes(int model);
	const xl_compat::RenderModelSphere &model_sphere(int model);
	int model_collision_radius(int model);
	int camera_hitbox_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, int frame, vms_vector &hit, vms_vector &normal);
	int camera_hitbox_query(const object &camera, const object &moving, const vms_vector &start, const vms_vector &end, vms_vector &hit, vms_vector &normal);
	void begin_hitbox_frame();
	void initialize_camera_objects();
	bool texture_brightness(int texture, int &brightness) const;
	bool pog_receipt(int bitmap, unsigned *bytes, unsigned *fnv1a) const;
	int custom_model_radius(int model, int fallback) const;
	const xl_compat::ParticleImageResource *particle_image(int subtype = 3) const;
	const xl_compat::ParticleImageResource *lightning_image() const;
	const xl_compat::ParticleImageResource *powerup_glare_image() const;
	const xl_compat::AnimatedImageResource *time_equipment_image(unsigned index) const;

  private:
	void validate_camera_query(const object &camera, const object &moving) const;
	struct State;
	std::unique_ptr<State> state;
};
void xl_set_prepared_resources(const char *level_filename, std::unique_ptr<XlPreparedResources> resources);
const xl_compat::ParticleImageResource *xl_prepared_particle_image(int subtype = 3);
const xl_compat::ParticleImageResource *xl_prepared_lightning_image();
const xl_compat::ParticleImageResource *xl_prepared_powerup_glare_image();
const xl_compat::AnimatedImageResource *xl_prepared_time_equipment_image(unsigned index);
#endif

#endif
