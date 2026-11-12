#pragma once
#include <nlohmann/json.hpp>
nlohmann::json xl_camera_attachments_snapshot();
nlohmann::json xl_camera_trace_snapshot();
bool xl_camera_attachments_restore(const nlohmann::json &saved, bool apply);
nlohmann::json xl_camera_motion_snapshot();
bool xl_camera_motion_restore(const nlohmann::json &saved, bool apply);
