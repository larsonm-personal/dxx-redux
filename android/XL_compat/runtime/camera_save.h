#pragma once
#include <nlohmann/json.hpp>
nlohmann::json xl_camera_attachments_snapshot();
bool xl_camera_attachments_restore(const nlohmann::json &saved, bool apply);
