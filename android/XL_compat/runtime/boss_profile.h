#pragma once

namespace xl_compat
{
// Keep the signed HXM byte for source identity and the reference bEndsLevel bit
constexpr bool supported_boss_profile(int raw)
{
	return raw == 1 || raw == 2 || raw == -1 || raw == -21 || raw == -22 || raw == -23 || raw == -24 || raw == -25 || raw == -27 || raw == -28 || (raw >= 21 && raw <= 28);
}
constexpr int effective_boss_profile(int raw)
{
	return raw < 0 ? -raw : raw;
}
constexpr bool boss_ends_level(int raw)
{
	return raw > 0;
}
} // namespace xl_compat
