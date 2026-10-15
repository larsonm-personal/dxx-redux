#ifndef INPUT_DEMO_NEWDEMO_SHARED_H
#define INPUT_DEMO_NEWDEMO_SHARED_H

int maybe_start_input_demo_recording(int is_autorecord);
void input_demo_stop_recording_common(int is_manual);
int input_demo_stop_quick_recording_common(void);
int input_demo_stop_quick_recording_after_frame(void);
int input_demo_defer_quick_recording_stop(void);
void input_demo_finish_deferred_quick_recording_stop(void);
int input_demo_toggle_quick_recording_common(void);
int input_demo_start_quick_recording_common(void);

#endif
