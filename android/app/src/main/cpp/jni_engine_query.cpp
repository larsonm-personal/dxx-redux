// Read-only launcher query codec, compiled against each engine's protocol headers
#include <jni.h>
#include <cstring>
#include <vector>
#include <nlohmann/json.hpp>

extern "C" {
#include "pstypes.h"
#include "net_udp.h"
#include "vers_id.h"
#include "byteswap.h"
}

#ifdef DXX_BUILD_DESCENT_II
#define QUERY_JNI(name) Java_com_dxxredux_app_lobby_D2EngineQuery_##name
#else
#define QUERY_JNI(name) Java_com_dxxredux_app_lobby_D1EngineQuery_##name
#endif

extern "C" JNIEXPORT jbyteArray JNICALL QUERY_JNI(request)(JNIEnv *env, jobject)
{
	ubyte packet[UPID_GAME_INFO_REQ_SIZE] = {};
	packet[0] = UPID_GAME_INFO_REQ;
	memcpy(packet + 1, UDP_REQ_ID, 4);
	PUT_INTEL_SHORT(packet + 5, DXX_VERSION_MAJORi);
	PUT_INTEL_SHORT(packet + 7, DXX_VERSION_MINORi);
	PUT_INTEL_SHORT(packet + 9, DXX_VERSION_MICROi);
	PUT_INTEL_SHORT(packet + 11, MULTI_PROTO_VERSION);
	jbyteArray result = env->NewByteArray(sizeof(packet));
	if (result) env->SetByteArrayRegion(result, 0, sizeof(packet), reinterpret_cast<jbyte *>(packet));
	return result;
}

extern "C" JNIEXPORT jstring JNICALL QUERY_JNI(decode)(JNIEnv *env, jobject, jbyteArray input)
{
	const auto size = env->GetArrayLength(input);
	if (size < UPID_VERSION_DENY_SIZE || size > UPID_GAME_INFO_SIZE) return nullptr;
	std::vector<ubyte> packet(size);
	env->GetByteArrayRegion(input, 0, size, reinterpret_cast<jbyte *>(packet.data()));
	if (env->ExceptionCheck()) return nullptr;
	const auto *data = packet.data();
	if (data[0] == UPID_VERSION_DENY) {
		if (size != UPID_VERSION_DENY_SIZE) return nullptr;
		return env->NewStringUTF("{\"error\":\"version\"}");
	}
	if (data[0] != UPID_GAME_INFO || GET_INTEL_SHORT(data + 1) != DXX_VERSION_MAJORi ||
	    GET_INTEL_SHORT(data + 3) != DXX_VERSION_MINORi || GET_INTEL_SHORT(data + 5) != DXX_VERSION_MICROi)
		return nullptr;
	// Fixed prefix of net_udp_send_game_info(UPID_GAME_INFO), including Android auth
	// UPID_SYNC has a different layout and is never accepted by this codec
	constexpr size_t names = 7 + (MAX_PLAYERS + 4) *
	                                 (CALLSIGN_LEN + 1 + 4 + 37 + ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE + 1);
	constexpr size_t mission = names + NETGAME_NAME_LEN + 1 + MISSION_NAME_LEN + 1;
	constexpr size_t level = mission + 9;
	if (size < static_cast<int>(level + 13)) return nullptr;
	if (!memchr(data + names, 0, NETGAME_NAME_LEN + 1) || !memchr(data + mission, 0, 9)) return nullptr;
	for (size_t i = mission; i < mission + 9 && data[i]; ++i)
		if (data[i] < 32 || data[i] > 126 || data[i] == '/' || data[i] == '\\') return nullptr;
	const int mode = data[level + 4], difficulty = data[level + 6], status = data[level + 7];
	const int maximum = data[level + 9];
	if (mode > NETGAME_BOUNTY || difficulty > 4 || maximum < 1 || maximum > MAX_PLAYERS ||
	    data[level + 8] > MAX_PLAYERS || data[level + 10] > MAX_PLAYERS) return nullptr;
	if (status != NETSTAT_STARTING && status != NETSTAT_PLAYING &&
	    !(mode == NETGAME_COOPERATIVE && (status == NETSTAT_ENDLEVEL || status == NETSTAT_WAITING)))
		return env->NewStringUTF("{\"error\":\"unavailable\"}");
	const char *modes[] = { "anarchy", "team_anarchy", "robot_anarchy", "coop", "ctf", "hoard", "team_hoard", "bounty" };
	const auto result = nlohmann::json{
		{ "mission", reinterpret_cast<const char *>(data + mission) }, { "mode", modes[mode] }, { "difficulty", difficulty }, { "level", static_cast<int>(GET_INTEL_INT(data + level)) }, { "max_players", maximum }, { "players", data[level + 10] }
	}.dump();
	return env->NewStringUTF(result.c_str());
}
