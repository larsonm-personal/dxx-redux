#ifndef DXX_LEVEL_TEXTURE_DIAGNOSTICS_H
#define DXX_LEVEL_TEXTURE_DIAGNOSTICS_H

#ifdef __cplusplus
#include <string>
#include <vector>

std::vector<std::string> level_texture_diagnostic_notes();

extern "C" {
#endif

void level_texture_diagnostics_reset(void);
void level_texture_diagnostics_record(int texture);

#ifdef __cplusplus
}
#endif

#endif
