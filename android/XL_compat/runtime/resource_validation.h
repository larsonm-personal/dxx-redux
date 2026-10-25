#ifndef DXX_XL_RESOURCE_VALIDATION_H
#define DXX_XL_RESOURCE_VALIDATION_H
#include "physfs.h"

#ifdef __cplusplus
extern "C" {
#endif
void xl_resource_baseline_clear(void);
void xl_resource_baseline_capture(void);
void xl_resource_note_startup_ham(const char *name, PHYSFS_file *file);
void xl_resource_select_mission(int requires_xl);
void xl_resource_commit_mission(void);
/* New D2 texture capability, not a general package-ownership or gameplay flag */
int xl_resource_texture_scope_enabled(void);
const char *xl_resource_base_rejection(void);
void xl_discard_prepared_resources(void);
int xl_install_prepared_hxm(const char *level_filename);
int xl_install_prepared_pog(const char *level_filename);
int xl_prepared_texture_brightness(int texture, int *brightness);
#ifdef __cplusplus
}

#include <memory>
#include <string>
void xl_validate_level_graphics(const std::string &palette);
namespace xl_assets
{
struct ObjectDependencies;
}

// Own the definitions used by both companion and level dependency checks
class XlPreparedResources
{
  public:
	explicit XlPreparedResources(const char *level_filename);
	~XlPreparedResources();
	xl_assets::ObjectDependencies &dependencies();
	void install_hxm();
	void install_pog();
	bool texture_brightness(int texture, int &brightness) const;

  private:
	struct State;
	std::unique_ptr<State> state;
};
void xl_set_prepared_resources(const char *level_filename, std::unique_ptr<XlPreparedResources> resources);
#endif

#endif
