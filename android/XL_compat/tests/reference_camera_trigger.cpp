#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include "xl_reference_camera_constants.h"

// Compile the actual source dispatch and diagnostic macros in both modes
namespace camera_debug_reference
{
#define DBG 1
#include "reference_camera_trigger_adapter.h"
#undef DBG
} // namespace camera_debug_reference
namespace camera_release_reference
{
#define DBG 0
#include "reference_camera_trigger_adapter.h"
#undef DBG
} // namespace camera_release_reference
void camera_trigger_source_controls()
{
	camera_debug_reference::controls();
	camera_release_reference::controls();
	std::cout << "Pinned camera/smoke dispatch: Debug and Release flags, actor gates and delays passed\n";
}
