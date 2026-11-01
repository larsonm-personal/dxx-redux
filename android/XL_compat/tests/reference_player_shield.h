namespace player_shield_source
{
static xl_compat::PlayerShieldInputs inputs;
struct CFloatVector {
	union {
		struct {
			float r, g, b, a;
		} c;
		float values[4];
	} v;
	float Red()
	{
		return v.c.r;
	}
	float Green()
	{
		return v.c.g;
	}
	float Blue()
	{
		return v.c.b;
	}
};
struct CObject {
	struct {
		int nId = 0;
	} info;
	bool Appearing()
	{
		return inputs.appearing;
	}
};
struct tCloakInfo {
	int nFadeValue;
};
static int GetCloakInfo(CObject *, int, int, tCloakInfo *info)
{
	info->nFadeValue = inputs.cloak_fade;
	return inputs.cloak_visible;
}
struct Player {
	unsigned flags = 0;
	int32_t Shield()
	{
		return inputs.shield;
	}
	int32_t MaxShield()
	{
		return inputs.maximum_shield;
	}
} player;
struct Pulse : shield_pulse_source::CPulseData {
	unsigned setups = 0;
	float speed = 0, minimum = 0;
	void Setup(float a, float b)
	{
		++setups;
		speed = a;
		minimum = b;
		CPulseData::Setup(a, b);
	}
};
static unsigned surfaces = 0, stencil_off = 0, stencil_on = 0;
static xl_compat::PlayerShieldPresentation presentation;
struct Surface {
	void SetupSurface(Pulse *, void *)
	{
		++surfaces;
	}
} surface;
struct {
	struct {
		int bWasHit[1];
		int32_t nLastHitTime[1];
		Pulse spherePulse[1];
	} multiplayer;
	struct {
		Surface *shield = &surface;
	} renderData;
} gameData;
struct {
	struct {
		int nShadowPass;
	} render;
	struct {
		int32_t nSDLTicks[1];
	} app;
} gameStates;
struct {
	struct {
		struct {
			bool bShields, bOnlyShieldHits;
		} effects;
	} render;
} options;
static auto *gameOpts = &options;
struct {
	void *Bitmap()
	{
		return nullptr;
	}
} shield[1];
struct {
	int StencilOff()
	{
		++stencil_off;
		return 7;
	}
	void StencilOn(int value)
	{
		require(value == 7, "Source shield stencil restore token changed");
		++stencil_on;
	}
} ogl;
static void DrawShieldSphere(CObject *, float r, float g, float b, float a, int additive)
{
	require(additive == 1, "Player shield additive mode changed");
	presentation.draw = true;
	presentation.color = { r, g, b, a };
}
#define SHOW_OBJ_FX               inputs.object_effects
#define SHOW_SHADOWS              inputs.shadows
#define FAST_SHADOWS              inputs.fast_shadows
#define PLAYER(i)                 player
#define PLAYER_FLAGS_CLOAKED      2048
#define PLAYER_FLAGS_INVULNERABLE 1
#define FADE_LEVELS               31
#define RENDER_HITBOX             0
#undef EGI_FLAG
#define EGI_FLAG(a, b, c, d) inputs.shields
#include "xl_reference_player_shield.h"
#undef EGI_FLAG
#define EGI_FLAG(...) 0
#undef RENDER_HITBOX
#undef FADE_LEVELS
#undef PLAYER_FLAGS_INVULNERABLE
#undef PLAYER_FLAGS_CLOAKED
#undef PLAYER
#undef FAST_SHADOWS
#undef SHOW_SHADOWS
#undef SHOW_OBJ_FX
} // namespace player_shield_source

static void compare_player_shield()
{
	namespace source = player_shield_source;
	unsigned comparisons = 0, drawn = 0, surface_only = 0, new_hits = 0, expired = 0;
	for (int duration : { 500, 2000 })
		for (unsigned mask = 0; mask < 512; ++mask)
			for (int pass : { 0, 1, 3 })
				for (int fade : { 0, 15, 31 })
					for (int hit : { -1, 0, 1 })
						for (int age : { -1, 0, 299, 300, 2000 })
							for (int shield : { 0, 50 * 65536, 150 * 65536 }) {
								xl_compat::PlayerShieldInputs input;
								input.object_effects = (mask & 1) != 0;
								input.shadows = (mask & 2) != 0;
								input.fast_shadows = (mask & 4) != 0;
								input.shields = (mask & 8) != 0;
								input.only_hits = (mask & 16) != 0;
								input.appearing = (mask & 32) != 0;
								input.cloaked = (mask & 64) != 0;
								input.invulnerable = (mask & 128) != 0;
								input.cloak_visible = (mask & 256) != 0;
								input.shadow_pass = pass;
								input.cloak_fade = fade;
								input.duration = duration;
								input.ticks = 5000 + age;
								input.shield = shield;
								xl_compat::PlayerShieldState state;
								state.was_hit = hit;
								state.last_hit = 5000;
								source::inputs = input;
								source::gameStates.render.nShadowPass = pass;
								source::gameStates.app.nSDLTicks[0] = input.ticks;
								source::options.render.effects = { input.shields, input.only_hits };
								source::gameData.multiplayer.bWasHit[0] = hit;
								source::gameData.multiplayer.nLastHitTime[0] = 5000;
								source::gameData.multiplayer.spherePulse[0] = source::Pulse{};
								source::player.flags = (input.cloaked ? 2048 : 0) | (input.invulnerable ? 1 : 0);
								source::surfaces = source::stencil_off = source::stencil_on = 0;
								source::presentation = {};
								shield_effect_time = duration;
								source::CObject object;
								source::RenderPlayerShield(&object);
								const auto actual = xl_compat::player_shield_presentation(state, input);
								const auto &pulse = source::gameData.multiplayer.spherePulse[0];
								require(actual.draw == source::presentation.draw, "Player shield draw gate differs from source");
								for (unsigned c = 0; c < 4; ++c)
									require(actual.color[c] == source::presentation.color[c] || (std::isnan(actual.color[c]) && std::isnan(source::presentation.color[c])),
									        "Player shield color differs from source");
								require(actual.setup_surface == (source::surfaces != 0) && source::stencil_off == source::surfaces,
								        "Player shield surface timing differs from source");
								require(source::stencil_on == unsigned(actual.draw), "Source stencil restore sequence changed");
								require(state.was_hit == source::gameData.multiplayer.bWasHit[0] && state.last_hit == source::gameData.multiplayer.nLastHitTime[0],
								        "Player shield hit transition differs from source");
								auto source_pulse = pulse;
								require(state.pulse.scale == source_pulse.Scale() && state.pulse.valid == source_pulse.Valid(), "Player shield pulse differs from source");
								if (pulse.setups) require(state.pulse.speed == pulse.speed && state.pulse.minimum == pulse.minimum && state.pulse.direction == pulse.speed,
									                      "Player shield pulse setup parameters differ from source");
								drawn += actual.draw;
								surface_only += actual.setup_surface && !actual.draw;
								new_hits += hit < 0 && state.was_hit == 1;
								expired += hit > 0 && state.was_hit == 0;
								++comparisons;
							}
	require(drawn && surface_only && new_hits && expired, "Player shield missed gate/transition witnesses");
	std::cout << "Pinned player shield: " << comparisons << " decision/color comparisons, " << drawn << " draws, "
	          << surface_only << " surface-only, " << new_hits << " new hits, " << expired << " expired hits passed\n";
}
