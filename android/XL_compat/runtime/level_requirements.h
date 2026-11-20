#ifndef DXX_XL_LEVEL_REQUIREMENTS_H
#define DXX_XL_LEVEL_REQUIREMENTS_H
struct mission_preparation;
namespace xl_compat
{
struct Inventory;
class TargetSources;
} // namespace xl_compat
namespace xl_assets
{
struct ObjectDependencies;
}
// Shared admission walk with explicit target identity and retained dependencies
// Read-only dependency collection is not gameplay admission
void xl_collect_incoming_dependencies(const xl_compat::Inventory &level, bool water_pilot,
                                      xl_assets::ObjectDependencies &graph);
void xl_validate_incoming_requirements(const xl_compat::Inventory &level, bool multiplayer, bool water_pilot,
                                       xl_assets::ObjectDependencies &graph, mission_preparation *mission, xl_compat::TargetSources &sources,
                                       bool owned_scoped_textures = false, bool mission_robot_replacements = false);
#endif
