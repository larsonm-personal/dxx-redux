#ifndef DXX_HOST_CONFIG_PROBE_H
#define DXX_HOST_CONFIG_PROBE_H

/* Test-only desktop bridge, deliberately separate from Android automation */
#ifdef DXX_HOST_CONFIG_PROBE
void host_config_probe_tick(void);
void host_config_probe_menu(const char *title, const char *subtitle, int selected, int count, const void *items);
void host_config_probe_listbox(const char *title, int selected, int count, char *const *items);
void *host_config_probe_bindings_begin(const char *title, int selected, int changing);
void host_config_probe_binding(void *snapshot, int index, const char *text, const char *binding, int value, int up, int down, int left, int right);
void host_config_probe_bindings_end(void *snapshot);
#else
#define host_config_probe_tick()                                        ((void) 0)
#define host_config_probe_menu(title, subtitle, selected, count, items) ((void) 0)
#define host_config_probe_listbox(title, selected, count, items)        ((void) 0)
#endif

#endif
