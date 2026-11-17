// Intentionally included in two isolated namespaces for source DBG branches
using fix = std::int32_t;
constexpr int OBJ_ROBOT = 2, OBJ_PLAYER = 4, OBJ_HOSTAGE = 3, OBJ_POWERUP = 7,
              OBJ_DEBRIS = 8, OBJ_REACTOR = 9, GM_MULTI_ROBOTS = 1, N_LOCALPLAYER = 0,
              SOUND_SECRET_EXIT = 0;
#define BRP    ((void) 0)
#define I2X(x) ((x) * 65536)
#define X2F(x) (float(x) / 65536.0f)
bool IsMultiGame = false;
int nDbgTrigger = -1, nDbgType = -1, random_calls = 0;
int Rand(int bound)
{
	++random_calls;
	return 7 % bound;
}
bool ClientOnly()
{
	return false;
}
struct CObject {
	struct {
		int nType = OBJ_PLAYER;
	} info;
	bool IsPlayer() const
	{
		return info.nType == OBJ_PLAYER;
	}
	bool IsGuideBot() const
	{
		return false;
	}
	bool RemoteKilled() const
	{
		return false;
	}
} actor;
CObject *OBJECT(int index)
{
	return index < 0 ? nullptr : &actor;
}
struct {
	int nObject = 0;
	fix Shield() const
	{
		return I2X(100);
	}
	fix Energy() const
	{
		return I2X(100);
	}
	void UpdateShield(fix)
	{
		throw std::runtime_error("Unexpected shield action");
	}
	void UpdateEnergy(fix)
	{
		throw std::runtime_error("Unexpected energy action");
	}
} LOCALPLAYER;
struct {
	struct {
		int nSDLTicks[1]{ 1000 };
		bool bD1Mission = false, bD2XLevel = true, bPlayerIsDead = false;
	} app;
} gameStates;
struct CTrigger {
	int type = TT_CAMERA, flags = 0, times[2]{ -1, -1 };
	struct {
		fix tOperated = -1;
		int nObject = -1, nPlayer = -1, bShot = 0;
	} m_info;
	int &Type()
	{
		return type;
	}
	bool Flagged(int mask) const
	{
		return (flags & mask) != 0;
	}
	void SetFlags(int mask)
	{
		flags |= mask;
	}
	int GetTime(int index) const
	{
		return times[index];
	}
	void SetTime(int index, int value)
	{
		times[index] = value;
	}
	int GetValue() const
	{
		return 0;
	}
	int32_t Operate(int16_t, int32_t, int32_t, bool);
	bool IsDelayed();
	int32_t Delay();
	void Countdown(bool);
	template <class... T>
	int unexpected(T...)
	{
		throw std::runtime_error("Unexpected non-inert source action");
	}
	int DoExit(int v)
	{
		return unexpected(v);
	}
	int DoSecretExit(int v)
	{
		return unexpected(v);
	}
	void DoLink()
	{
		unexpected();
	}
	void DoCloseDoor()
	{
		unexpected();
	}
	void DoUnlockDoors()
	{
		unexpected();
	}
	void DoLockDoors()
	{
		unexpected();
	}
	int DoChangeWalls()
	{
		return unexpected();
	}
	bool WallIsForceField()
	{
		return unexpected();
	}
	void DoObjectProducer(bool v)
	{
		unexpected(v);
	}
	void DoIllusionOn()
	{
		unexpected();
	}
	void DoIllusionOff()
	{
		unexpected();
	}
	int DoLightOff()
	{
		return unexpected();
	}
	int DoLightOn()
	{
		return unexpected();
	}
	void DoTeleportBot(CObject *v)
	{
		unexpected(v);
	}
	void DoTeleport(int v)
	{
		unexpected(v);
	}
	void DoSpeedBoost(int v)
	{
		unexpected(v);
	}
	void DoChangeTexture()
	{
		unexpected();
	}
	void DoSpawnBots(CObject *v)
	{
		unexpected(v);
	}
	void DoSetSpawnPoints()
	{
		unexpected();
	}
	void DoShowMessage()
	{
		unexpected();
	}
	int DoPlaySound(int v)
	{
		return unexpected(v);
	}
	int DoMasterTrigger(int a, int b, bool c)
	{
		return unexpected(a, b, c);
	}
	void DoEnableTrigger()
	{
		unexpected();
	}
	void DoDisableTrigger()
	{
		unexpected();
	}
	void DoDisarmRobots()
	{
		unexpected();
	}
	void DoReprogramRobots()
	{
		unexpected();
	}
	void DoShakeMine()
	{
		unexpected();
	}
	void OperateD1(int a, int b, int c)
	{
		unexpected(a, b, c);
	}
	void PrintMessage(int, int, const char *)
	{
		unexpected();
	}
};
CTrigger current;
struct TriggerBuffer {
	int Index(CTrigger *p) const
	{
		return p == &current ? 0 : -1;
	}
	CTrigger *Buffer() const
	{
		return &current;
	}
} GEOTRIGGERS, OBJTRIGGERS;
CTrigger *GEOTRIGGER(int)
{
	return &current;
}
struct {
	struct {
		int delay[1]{ -1 };
		TriggerBuffer triggers[1];
	} trigData;
	struct {
		fix xGame = I2X(10);
	} timeData;
	struct {
		bool GameMode(int) const
		{
			return false;
		}
	} appData;
} gameData;
struct {
	void PlaySound(int, int)
	{
		throw std::runtime_error("Unexpected sound action");
	}
} audio;
void StartPlayerDeathSequence(CObject *)
{
	throw std::runtime_error("Unexpected death action");
}
void InitCountdown(CTrigger *, int, int)
{
	throw std::runtime_error("Unexpected countdown action");
}
#include "xl_reference_camera_trigger.h"
void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
void reset(int type, int flags, int actor_type = OBJ_PLAYER)
{
	current = {};
	current.type = type;
	current.flags = flags;
	actor.info.nType = actor_type;
	gameData.trigData.delay[0] = -1;
	gameData.timeData.xGame = I2X(10);
	gameStates.app.nSDLTicks[0] = 1000;
	random_calls = 0;
}
void controls()
{
	for (int type : { TT_CAMERA, TT_SMOKE_LIFE, TT_SMOKE_SPEED, TT_SMOKE_DENS, TT_SMOKE_SIZE, TT_SMOKE_DRIFT }) {
		for (int flags = 0; flags < 256; ++flags) {
			reset(type, flags);
			const bool disabled = (flags & TF_DISABLED) != 0;
			require(current.Operate(0, 0, 1, false) == int(disabled), "Inert source result differs");
			require(current.flags == (flags | ((!disabled && (flags & TF_ONE_SHOT)) ? TF_DISABLED : 0)) && current.type == type,
			        "Inert source flags or alternate type differ");
			require((current.m_info.tOperated >= 0) == !disabled && !random_calls, "Inert operation or RNG differs");
		}
		for (bool attached : { false, true }) {
			reset(type, TF_ONE_SHOT, OBJ_ROBOT);
			require(current.Operate(0, -1, 1, attached) == int(!attached), "Inert actor gate differs");
			require(bool(current.flags & TF_DISABLED) == attached, "Rejected actor changed flags");
		}
		for (int delay : { 500, -1000 })
			for (bool permanent : { false, true }) {
				reset(type, permanent ? TF_PERMANENT : 0);
				current.times[0] = delay;
				require(current.Operate(0, 0, 1, false) == 1, "Inert delay did not arm");
				const int selected = delay > 0 ? delay : 800;
				require(current.times[1] == selected && random_calls == int(delay < 0), "Inert delay selection differs");
				gameData.timeData.xGame += MSEC2X(selected) - 1;
				current.Countdown(false);
				require(!(current.flags & TF_DISABLED) && current.m_info.tOperated >= 0, "Inert delay fired early");
				++gameData.timeData.xGame;
				current.Countdown(false);
				require(bool(current.flags & TF_DISABLED) == !permanent && (current.m_info.tOperated < 0) == permanent,
				        "Inert delayed flags or rearming differ");
			}
	}
}
#undef Int3
#undef Assert
#undef BRP
#undef I2X
#undef X2F
#undef MSEC2X
