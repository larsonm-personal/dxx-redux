#ifndef D2_ESCORT_EXIT_POLICY_H
#define D2_ESCORT_EXIT_POLICY_H

/*
 * Preserve classic D2 Guide-Bot behavior when a level has an external
 * flythrough endpoint. Trigger-only levels use their TT_EXIT surface as the
 * fallback destination.
 */
static inline int escort_exit_segment_preferred(int external_segment,
                                                int trigger_segment)
{
	return external_segment >= 0 ? external_segment : trigger_segment;
}

#endif
