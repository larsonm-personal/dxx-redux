/* Opt-in desktop test instrumentation: observe engine state and queue SDL input */
#ifdef DXX_HOST_CONFIG_PROBE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include "config.h"
#include "game.h"
#include "gameseq.h"
#include "playsave.h"
#include "newmenu.h"
#include "window.h"
#include "automap.h"
#include "object.h"
#include "host_config_probe.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#endif

#ifdef USE_SDLMIXER
void host_music_probe_snapshot(FILE *out);
#endif

static unsigned long probe_sequence;
static unsigned long probe_frame;
static Uint32 last_write;
static Uint32 last_menu;
static SDLKey held_key;
static int held_modifiers;
static unsigned long held_sequence;
static Uint32 release_at;

static int probe_path(char *out, size_t size, const char *name)
{
	const char *directory = getenv("DXX_HOST_PROBE_DIR");
	if (!directory || !*directory)
		return 0;
	return snprintf(out, size, "%s/%s", directory, name) < (int) size;
}

static void quote(FILE *out, const char *text)
{
	const unsigned char *p = (const unsigned char *) (text ? text : "");
	fputc('"', out);
	for (; *p; ++p) {
		if (*p == '"' || *p == '\\')
			fprintf(out, "\\%c", *p);
		else if (*p < 32 || *p >= 127)
			fprintf(out, "\\u%04x", *p);
		else
			fputc(*p, out);
	}
	fputc('"', out);
}

static FILE *begin_snapshot(const char *kind, const char *title, const char *subtitle, int selected)
{
	char path[4096];
	FILE *out;
	Uint32 now = SDL_GetTicks();
	if (now - last_write < 25 || !probe_path(path, sizeof(path), "snapshot.tmp"))
		return NULL;
	last_write = now;
	out = fopen(path, "wb");
	if (!out)
		return NULL;
	fprintf(out, "{\"schema\":1,\"sequence\":%lu,\"frame\":%lu,\"ticks\":%u,\"kind\":", probe_sequence, ++probe_frame, now);
	quote(out, kind);
#ifdef DXX_BUILD_DESCENT_II
	fputs(",\"engine\":\"d2\"", out);
	fprintf(out, ",\"presentation_version\":%d", d1_in_d2_use_d1_gameplay() ? 1 : 2);
#else
	fputs(",\"engine\":\"d1\"", out);
	fputs(",\"presentation_version\":1", out);
#endif
	fprintf(out, ",\"game_mode\":%d,\"difficulty\":%d", Game_mode, Difficulty_level);
	fputs(",\"title\":", out);
	quote(out, title);
	fputs(",\"subtitle\":", out);
	quote(out, subtitle);
	fprintf(out, ",\"selected\":%d,\"game\":{\"active\":%s,\"level\":%d},\"config\":{", selected, Game_wind ? "true" : "false", Current_level_num);
#define FIELD(name) fprintf(out, "\"" #name "\":%d,", (int) GameCfg.name)
	FIELD(DigiVolume);
	FIELD(MusicVolume);
	FIELD(ReverseStereo);
	FIELD(MusicType);
	FIELD(ResolutionX);
	FIELD(ResolutionY);
	FIELD(AspectX);
	FIELD(AspectY);
	FIELD(TexFilt);
	FIELD(MenuTexFilt);
	FIELD(HudTexFilt);
	FIELD(VSync);
	FIELD(WindowMode);
	FIELD(Multisample);
	FIELD(AnisoLevel);
	FIELD(MsaaLevel);
	FIELD(ClassicDepth);
	FIELD(Grabinput);
#undef FIELD
	fprintf(out, "\"GammaLevel\":%d}", GameCfg.GammaLevel);
	fputs(",\"player\":{", out);
#define PLAYER_FIELD(name) fprintf(out, "\"" #name "\":%d,", (int) PlayerCfg.name)
	PLAYER_FIELD(ControlType);
	PLAYER_FIELD(MouseControlStyle);
	PLAYER_FIELD(AlphaEffects);
	PLAYER_FIELD(DynLightColor);
	PLAYER_FIELD(DisableCockpit);
	PLAYER_FIELD(StickyRearview);
	PLAYER_FIELD(MouseFSIndicator);
	PLAYER_FIELD(ReticleType);
	PLAYER_FIELD(ReticleSize);
	PLAYER_FIELD(HudMode);
	PLAYER_FIELD(PreferredCockpitMode);
	PLAYER_FIELD(CurrentCockpitMode);
#undef PLAYER_FIELD
	fprintf(out, "\"KeyboardSens0\":%d,\"MouseSens0\":%d}", PlayerCfg.KeyboardSens[0], PlayerCfg.MouseSens[0]);
#ifdef USE_SDLMIXER
	fputs(",\"music\":", out);
	host_music_probe_snapshot(out);
#endif
	fputs(",\"mission\":{\"name\":", out);
	quote(out, Current_mission ? Current_mission->mission_name : "");
	fputs(",\"filename\":", out);
	quote(out, Current_mission ? Current_mission->filename : "");
#ifdef DXX_BUILD_DESCENT_II
	fprintf(out, ",\"version\":%d}", Current_mission ? Current_mission->descent_version : 0);
#else
	fprintf(out, ",\"version\":%d}", Current_mission ? 1 : 0);
#endif
	fprintf(out, ",\"world\":{\"game_time\":%lld,\"score\":%d,\"energy\":%d,\"shields\":%d,\"lives\":%d}", (long long) GameTime64, Players[Player_num].score, Players[Player_num].energy, Players[Player_num].shields, Players[Player_num].lives);
	if (Game_wind && ConsoleObject) {
		fprintf(out, ",\"ship\":{\"position\":[%d,%d,%d],\"forward\":[%d,%d,%d]}", ConsoleObject->pos.x, ConsoleObject->pos.y, ConsoleObject->pos.z, ConsoleObject->orient.fvec.x, ConsoleObject->orient.fvec.y, ConsoleObject->orient.fvec.z);
	}
	fputs(",\"items\":[", out);
	return out;
}

static void end_snapshot(FILE *out)
{
	char from[4096], to[4096];
	fputs("]}\n", out);
	fclose(out);
	if (probe_path(from, sizeof(from), "snapshot.tmp") && probe_path(to, sizeof(to), "snapshot.json"))
		rename(from, to);
}

void host_config_probe_menu(const char *title, const char *subtitle, int selected, int count, const void *raw_items)
{
	const newmenu_item *items = (const newmenu_item *) raw_items;
	FILE *out;
	last_menu = SDL_GetTicks();
	out = begin_snapshot("menu", title, subtitle, selected);
	int i;
	if (!out)
		return;
	for (i = 0; i < count; ++i) {
		if (i)
			fputc(',', out);
		fputs("{\"text\":", out);
		quote(out, items[i].text);
		fprintf(out, ",\"type\":%d", items[i].type);
		if (items[i].type == NM_TYPE_CHECK || items[i].type == NM_TYPE_RADIO || items[i].type == NM_TYPE_NUMBER || items[i].type == NM_TYPE_SLIDER)
			fprintf(out, ",\"value\":%d", items[i].value);
		if (items[i].type == NM_TYPE_NUMBER || items[i].type == NM_TYPE_SLIDER)
			fprintf(out, ",\"min\":%d,\"max\":%d", items[i].min_value, items[i].max_value);
		fputc('}', out);
	}
	end_snapshot(out);
}

void host_config_probe_listbox(const char *title, int selected, int count, char *const *items)
{
	FILE *out;
	last_menu = SDL_GetTicks();
	out = begin_snapshot("listbox", title, "", selected);
	int i;
	if (!out)
		return;
	for (i = 0; i < count; ++i) {
		if (i)
			fputc(',', out);
		fputs("{\"text\":", out);
		quote(out, items[i]);
		fputs("}", out);
	}
	end_snapshot(out);
}

static void push_key(SDLKey key, int pressed)
{
	SDL_Event event;
	memset(&event, 0, sizeof(event));
	event.type = pressed ? SDL_KEYDOWN : SDL_KEYUP;
	event.key.state = pressed ? SDL_PRESSED : SDL_RELEASED;
	event.key.keysym.sym = key;
	event.key.keysym.unicode = key < 128 ? key : 0;
	SDL_PushEvent(&event);
}

void *host_config_probe_bindings_begin(const char *title, int selected, int changing)
{
	last_menu = SDL_GetTicks();
	return begin_snapshot("bindings", title, changing ? "changing" : "idle", selected);
}

void host_config_probe_binding(void *snapshot, int index, const char *text, const char *binding, int value, int up, int down, int left, int right)
{
	FILE *out = (FILE *) snapshot;
	if (!out) return;
	if (index) fputc(',', out);
	fputs("{\"text\":", out);
	quote(out, text);
	fputs(",\"binding\":", out);
	quote(out, binding);
	fprintf(out, ",\"value\":%d,\"up\":%d,\"down\":%d,\"left\":%d,\"right\":%d}", value, up, down, left, right);
}

void host_config_probe_bindings_end(void *snapshot)
{
	if (snapshot) end_snapshot((FILE *) snapshot);
}

void host_config_probe_tick(void)
{
	char path[4096];
	FILE *input;
	unsigned long sequence;
	int key, modifiers, hold_ms = 0;
	if (!probe_path(path, sizeof(path), "command.txt"))
		return;
	if (held_key && (Sint32) (SDL_GetTicks() - release_at) >= 0) {
		push_key(held_key, 0);
		if (held_modifiers & KMOD_SHIFT) push_key(SDLK_LSHIFT, 0);
		if (held_modifiers & KMOD_CTRL) push_key(SDLK_LCTRL, 0);
		if (held_modifiers & KMOD_ALT) push_key(SDLK_LALT, 0);
		probe_sequence = held_sequence;
		held_key = SDLK_UNKNOWN;
	}
	input = held_key ? NULL : fopen(path, "rb");
	if (input) {
		if (fscanf(input, "%lu %d %d %d", &sequence, &key, &modifiers, &hold_ms) >= 3 && sequence > probe_sequence && key > SDLK_UNKNOWN && key < SDLK_LAST && hold_ms >= 0 && hold_ms <= 2000) {
			if (modifiers & KMOD_ALT) push_key(SDLK_LALT, 1);
			if (modifiers & KMOD_CTRL) push_key(SDLK_LCTRL, 1);
			if (modifiers & KMOD_SHIFT) push_key(SDLK_LSHIFT, 1);
			push_key((SDLKey) key, 1);
			if (hold_ms) {
				held_key = (SDLKey) key;
				held_modifiers = modifiers;
				held_sequence = sequence;
				release_at = SDL_GetTicks() + hold_ms;
			} else {
				push_key((SDLKey) key, 0);
				if (modifiers & KMOD_SHIFT) push_key(SDLK_LSHIFT, 0);
				if (modifiers & KMOD_CTRL) push_key(SDLK_LCTRL, 0);
				if (modifiers & KMOD_ALT) push_key(SDLK_LALT, 0);
				probe_sequence = sequence;
			}
		}
		fclose(input);
		remove(path);
	}
	if ((Game_wind && window_get_front() == Game_wind) || Automap_active || SDL_GetTicks() - last_menu > 100) {
		const char *kind = Automap_active ? "automap" : ((Game_wind && window_get_front() == Game_wind) ? "game" : "other");
		FILE *out = begin_snapshot(kind, "", "", -1);
		if (out)
			end_snapshot(out);
	}
}
#endif
