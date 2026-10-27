#ifndef XL_TRIGGER_FLAGS_H
#define XL_TRIGGER_FLAGS_H
namespace xl_compat
{
constexpr int trigger_flags_mask = 255;
enum class TriggerCue { ForcefieldOff,
	                    IllusionOff };
constexpr bool trigger_cue_enabled(int flags, TriggerCue cue)
{
	// Pinned XL's illusion-off condition is intentionally opposite to forcefields
	return cue == TriggerCue::IllusionOff ? (flags & 64) != 0 : (flags & 64) == 0;
}
} // namespace xl_compat
#endif
