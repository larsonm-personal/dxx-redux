#ifndef DXX_XL_DOOR_STRIP_PHASE_H
#define DXX_XL_DOOR_STRIP_PHASE_H
namespace xl_compat
{
// The source side field is signed and survives authored texture changes
inline int door_strip_initial_phase(bool base_texture)
{
	return base_texture ? -1 : 1;
}
inline int door_texture_phase(int frame, int strip_frames)
{
	return strip_frames > 1 ? -(frame + 1 > strip_frames ? strip_frames : frame + 1) : -frame;
}
inline int door_strip_frame(int phase, int frames)
{
	return phase < 0 && phase >= -frames ? -phase - 1 : 0;
}
inline int door_effect_frame_count(int native_frames, int alternate_frames)
{
	return alternate_frames > 1 ? alternate_frames : native_frames;
}
} // namespace xl_compat
#endif
