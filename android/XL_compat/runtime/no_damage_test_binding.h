#ifndef DXX_XL_NO_DAMAGE_TEST_BINDING_H
#define DXX_XL_NO_DAMAGE_TEST_BINDING_H
#ifdef DXX_XL_DESCRIPTOR_TEST
#include "../inventory.h"
// Headless qualification only; does not enable runtime admission or lighting
void xl_test_bind_no_damage(const xl_compat::Inventory &level);
#endif
#endif
