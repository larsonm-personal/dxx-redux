#if defined(DXX_BUILD_DESCENT_II) && defined(USE_UDP)
extern "C" {
#include "net_udp.h"
extern UDP_sequence_packet UDP_sync_player;
extern unsigned my_player_token, netgame_token;
void net_udp_init(void);
void net_udp_close(void);
int udp_open_socket(int socket, int port);
void net_udp_send_objects(void);
void net_udp_read_object_packet(ubyte *data, int length);
void net_udp_send_game_info(struct _sockaddr address, ubyte type, ubyte observers, unsigned token);
int net_udp_read_sync_packet(ubyte *data, int length, struct _sockaddr address);
int net_udp_process_game_info(ubyte *data, int length, struct _sockaddr address, int lite, ubyte sync);
}

struct network_asset_packets {
	bytes objects, end, sync, lobby;
	struct _sockaddr sender;
	int robot, retro;
};

// Capture actual engine writers through localhost UDP, including the completion
// marker and final sync, rather than rebuilding their layouts in the test
static network_asset_packets network_capture_assets(int actor, int retro = 0)
{
	const object robot = Objects[actor], player = *ConsoleObject;
	Player_num = 0;
	net_udp_init();
	require(udp_open_socket(0, 0) >= 0, "open the engine's ephemeral host socket");
	const auto receiver = socket(_af, SOCK_DGRAM, IPPROTO_UDP);
	require(receiver != decltype(receiver)(-1), "open fixture UDP receiver");
	struct _sockaddr address = {};
#ifdef IPv6
	address.sin6_family = _af;
	address.sin6_addr = in6addr_loopback;
#else
	address.sin_family = _af;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#endif
	require(bind(receiver, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0, "bind fixture receiver on localhost");
#ifdef _WIN32
	int address_size = sizeof(address);
#else
	socklen_t address_size = sizeof(address);
#endif
	require(getsockname(receiver, reinterpret_cast<sockaddr *>(&address), &address_size) == 0, "read assigned receiver port");
	network_asset_packets packets = {};
	packets.robot = robot.id;
	packets.retro = retro;
	const auto receive = [&](int type) {
		fd_set reads;
		FD_ZERO(&reads);
		FD_SET(receiver, &reads);
		timeval deadline = { 2, 0 };
		require(select(static_cast<int>(receiver) + 1, &reads, nullptr, nullptr, &deadline) == 1, "actual engine writer reaches the localhost receiver");
		bytes data(UPID_MAX_SIZE);
		address_size = sizeof(packets.sender);
		const int length = recvfrom(receiver, reinterpret_cast<char *>(data.data()), static_cast<int>(data.size()), 0,
		                            reinterpret_cast<sockaddr *>(&packets.sender), &address_size);
		require(length > 0 && data[0] == type, "receive the expected real engine packet");
		data.resize(length);
		return data;
	};
	init_objects();
	for (int i = 0; i < 3; ++i) {
		if (i) require(obj_allocate() == i, "allocate actual host object slots");
		Objects[i] = i == 2 ? robot : player;
		if (i == 1) Objects[i].id = 1;
		const int seg = Objects[i].segnum;
		Objects[i].next = Objects[i].prev = Objects[i].segnum = -1;
		obj_link(i, seg);
	}
	Players[0].objnum = 0;
	Players[1].objnum = 1;
	std::strcpy(Players[0].callsign, "nethost");
	std::strcpy(Players[1].callsign, "netasset");
	std::strcpy(Netgame.players[0].callsign, "nethost");
	std::strcpy(Netgame.players[1].callsign, "netasset");
	Players[0].connected = Players[1].connected = CONNECT_PLAYING;
	reset_network_objects();
	N_players = 2;
	Netgame.max_numplayers = Netgame.numplayers = Netgame.numconnected = 2;
	Netgame.gamemode = NETGAME_COOPERATIVE;
	Netgame.game_status = NETSTAT_PLAYING;
	Netgame.segments_checksum = my_segments_checksum;
	Netgame.RetroProtocol = retro;
	Netgame.players[1].protocol.udp.addr = address;
	netgame_token = 7654321;
	my_player_token = 12345;
	UDP_sync_player = {};
	UDP_sync_player.player.connected = 1;
	UDP_sync_player.player.protocol.udp.addr = address;
	UDP_sync_player.token = my_player_token;
	Network_send_objects = 1;
	Network_player_added = 0;
	Network_send_objnum = -1;
	Network_send_object_mode = 0;
	Game_mode = GM_NETWORK | GM_MULTI_COOP;
	SDL_Delay(25);
	timer_update();
	net_udp_send_objects();
	packets.objects = receive(UPID_OBJECT_DATA);
	require(GET_INTEL_INT(packets.objects.data() + 5) == 4, "actual writer emits clear marker and three host objects");
	SDL_Delay(25);
	timer_update();
	net_udp_send_objects();
	packets.end = receive(UPID_OBJECT_DATA);
	packets.sync = receive(UPID_SYNC);
	require(packets.end.size() == UPID_OBJECT_HEADER_SIZE + 9 && GET_INTEL_INT(packets.end.data() + UPID_OBJECT_HEADER_SIZE) == -2,
	        "completion marker uses the same admitted namespace and header");
	net_udp_send_game_info(address, UPID_GAME_INFO, 0, 0);
	packets.lobby = receive(UPID_GAME_INFO);
#ifdef _WIN32
	closesocket(receiver);
#else
	close(receiver);
#endif
	net_udp_close();
	Game_mode = 0;
	fixture_close_order_game();
	return packets;
}

static void network_client_level(const char *mission)
{
	char name[32];
	std::snprintf(name, sizeof(name), "%s", mission);
	require(load_mission_by_name(name), "select actual client mission");
	Player_num = 0;
	std::strcpy(Players[0].callsign, "netasset");
	std::strcpy(Players[1].callsign, "netasset");
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
}

static nlohmann::json network_receive_assets(network_asset_packets packets, bool accepted)
{
	using nlohmann::json;
	const std::vector<object> before(Objects, Objects + MAX_OBJECTS);
	const int highest = Highest_object_index;
	const auto untouched = [&] { return highest == Highest_object_index && std::memcmp(before.data(), Objects, MAX_OBJECTS * sizeof(object)) == 0; };
	N_players = 2;
	Game_mode = GM_NETWORK | GM_MULTI_COOP;
	Network_status = NETSTAT_WAITING;
	Network_rejoined = multi_received_objects = 0;
	Netgame.RetroProtocol = packets.retro;
	const netgame_info info_before = Netgame;
	unsigned seed = 0, after_seed = 0;
	require(d_rand_get_state(&seed), "observe SIM seed before network admission");
	const auto calls = d_rand_get_call_count();
	set_default_handler(classic_conversion_message_event);
	const int initial = net_udp_read_sync_packet(packets.sync.data(), static_cast<int>(packets.sync.size()), packets.sender);
	const bool info_unchanged = std::memcmp(&Netgame, &info_before, sizeof(Netgame)) == 0;
	std::fprintf(stderr, "Initial sync: accepted=%d result=%d status=%d player=%d length=%u\n", accepted, initial, Network_status, Player_num, static_cast<unsigned>(packets.sync.size()));
	require(accepted ? initial && Network_status == NETSTAT_PLAYING : !initial && untouched() && info_unchanged && Network_status == NETSTAT_MENU,
	        "actual initial sync admits source identity before metadata and world changes");
	Network_status = NETSTAT_WAITING;
	net_udp_read_object_packet(packets.objects.data(), static_cast<int>(packets.objects.size()));
	const bool received = multi_received_objects && Objects[2].type == OBJ_ROBOT && Objects[2].id == packets.robot;
	require(accepted ? received && packets.robot < N_robot_types : !received && untouched() && Network_status == NETSTAT_MENU,
	        "actual late join admits definitions before installing host objects");
	if (accepted) {
		net_udp_read_object_packet(packets.end.data(), static_cast<int>(packets.end.size()));
		require(net_udp_read_sync_packet(packets.sync.data(), static_cast<int>(packets.sync.size()), packets.sender) &&
		            Network_status == NETSTAT_PLAYING && Network_rejoined && Objects[2].id == packets.robot,
		        "real transfer completes and returns the joined client to gameplay");
	}
	set_default_handler(nullptr);
	require(d_rand_get_state(&after_seed) && seed == after_seed && calls == d_rand_get_call_count(), "source admission does not consume SIM RNG");
	json result = { { "initial_sync", initial }, { "received", received }, { "robots", N_robot_types }, { "network_status", Network_status }, { "world_unchanged", untouched() }, { "metadata_unchanged", info_unchanged }, { "sim_rng_unchanged", true } };
	Game_mode = 0;
	fixture_close_order_game();
	return result;
}

static nlohmann::json network_reject_malformed(const network_asset_packets &packets)
{
	const std::vector<object> before(Objects, Objects + MAX_OBJECTS);
	const int highest = Highest_object_index;
	Netgame.RetroProtocol = packets.retro;
	const netgame_info info = Netgame;
	unsigned rejected = 0, seed = 0, after_seed = 0;
	require(d_rand_get_state(&seed), "observe SIM state before malformed packet checks");
	const auto calls = d_rand_get_call_count();
	set_default_handler(classic_conversion_message_event);
	const auto check = [&](bytes data, bool sync) {
		ubyte empty = 0;
		ubyte *const payload = data.empty() ? &empty : data.data();
		Network_status = NETSTAT_WAITING;
		Network_rejoined = multi_received_objects = 0;
		Game_mode = GM_NETWORK | GM_MULTI_COOP;
		if (sync) require(!net_udp_read_sync_packet(payload, static_cast<int>(data.size()), packets.sender), "malformed sync rejected");
		else net_udp_read_object_packet(payload, static_cast<int>(data.size()));
		require(!multi_received_objects && !Network_rejoined && highest == Highest_object_index &&
		            std::memcmp(before.data(), Objects, MAX_OBJECTS * sizeof(object)) == 0 && std::memcmp(&info, &Netgame, sizeof(info)) == 0,
		        "rejected network record leaves all objects and metadata untouched");
		++rejected;
	};
	for (bool sync : { false, true }) {
		const bytes &original = sync ? packets.sync : packets.objects;
		const size_t identity = sync ? original.size() - D1_IN_D2_NET_ASSET_SIZE : 9;
		for (int tag : { 1, 31, 33, 63, 65, 255 }) {
			bytes changed = original;
			changed[identity] = static_cast<ubyte>(tag);
			check(changed, sync);
		}
		for (int offset : { 1, 33 }) {
			bytes changed = original;
			changed[identity + offset] ^= 1;
			check(changed, sync);
		}
		for (size_t length : { size_t(0), size_t(8), size_t(9), size_t(UPID_OBJECT_HEADER_SIZE - 1), original.size() - 1 })
			check(bytes(original.begin(), original.begin() + length), sync);
		bytes extra = original;
		extra.push_back(0);
		check(extra, sync);
	}
	for (int count : { -1, 0, 5, INT_MAX }) {
		bytes changed = packets.objects;
		set_int(changed, 5, count);
		check(changed, false);
	}
	set_default_handler(nullptr);
	require(d_rand_get_state(&after_seed) && seed == after_seed && calls == d_rand_get_call_count(), "malformed network admission leaves SIM RNG unchanged");
	Game_mode = 0;
	fixture_close_order_game();
	return { { "rejected", rejected }, { "world_unchanged", true }, { "metadata_unchanged", true }, { "sim_rng_unchanged", true } };
}

static void exercise_network_asset_missions(const char *d2_directory)
{
	using nlohmann::json;
	const std::string hog = std::string(d2_directory) + "/descent2.hog";
	require(PHYSFS_mount(d2_directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "mount original optional package for network recording");
	new_player_config();
	GameArg.SysUsePlayersDir = 0;
	GameArg.SndDigiSampleRate = SAMPLE_RATE_11K;
	texmerge_init(10);
	init_game();
	network_client_level("descent");
	const int buddy = create_buddy_bot_at_player(0);
	require(buddy > 0 && Robot_info[Objects[buddy].id].companion, "create actual companion before host object transfer");
	const fix mass = Robot_info[Objects[buddy].id].mass;
	const auto packets = network_capture_assets(buddy);
	write_fixture("network-object-packet.bin", packets.objects);
	write_fixture("network-sync-packet.bin", packets.sync);
	json report = json::array();
	const auto record = [&](const char *scenario, json result) {
		result["scenario"] = scenario;
		report.push_back(result);
		const auto text = report.dump(2) + "\n";
		write_fixture("network-asset-report.json", bytes(text.begin(), text.end()));
		std::fprintf(stderr, "Network assets %s: %s\n", scenario, result.dump().c_str());
	};
	for (const char *scenario : { "same", "changed", "missing", "restored" }) {
		if (!std::strcmp(scenario, "changed")) {
			PHYSFS_file *file = PHYSFSX_openReadBuffered("descent2.ham");
			require(file && PHYSFS_fileLength(file) > 8, "open original optional HAM");
			bytes data(static_cast<size_t>(PHYSFS_fileLength(file)));
			require(PHYSFS_readBytes(file, data.data(), data.size()) == static_cast<PHYSFS_sint64>(data.size()) && PHYSFS_seek(file, 8), "copy optional HAM into isolated fixture");
			for (int width : { 22, 2, 82, 130, 126 }) {
				const int count = PHYSFSX_readInt(file);
				require(count >= 0 && PHYSFS_seek(file, PHYSFS_tell(file) + static_cast<PHYSFS_sint64>(count) * width), "locate companion source table");
			}
			require(PHYSFSX_readInt(file) > 33, "source HAM contains companion");
			const size_t offset = static_cast<size_t>(PHYSFS_tell(file)) + 33 * 480 + 136;
			require(offset + 4 <= data.size() && PHYSFS_close(file), "locate companion mass");
			set_int(data, offset, mass + F1_0);
			write_fixture("descent2.ham", data);
		} else if (!std::strcmp(scenario, "missing"))
			require(PHYSFS_delete("descent2.ham") && PHYSFS_unmount(hog.c_str()) && PHYSFS_unmount(d2_directory), "remove fixture's optional source before cold client entry");
		else if (!std::strcmp(scenario, "restored"))
			require(PHYSFS_mount(d2_directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "restore original optional source for client recovery");
		network_client_level("descent");
		record(scenario, network_receive_assets(packets, !std::strcmp(scenario, "same") || !std::strcmp(scenario, "restored")));
	}
	network_client_level("descent");
	record("native-malformed", network_reject_malformed(packets));
	// A real custom native robot also exercises native-only (32-byte) identities
	require(PHYSFS_unmount(hog.c_str()) && PHYSFS_unmount(d2_directory), "prepare native-only host");
	std::string custom_name = Level_names[0];
	custom_name.replace(custom_name.find_last_of('.'), std::string::npos, ".hx1");
	const bytes custom = d1_custom_definition_fixture();
	write_fixture(custom_name.c_str(), custom);
	network_client_level("descent");
	int actor = obj_create(OBJ_ROBOT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient,
	                       Polygon_models[Robot_info[0].model_num].rad, CT_AI, MT_PHYSICS, RT_POLYOBJ);
	require(actor > 0, "create custom native host robot");
	Objects[actor].rtype.pobj_info.model_num = Robot_info[0].model_num;
	const auto custom_packets = network_capture_assets(actor, 1);
	for (const char *scenario : { "custom-same", "custom-changed", "custom-missing", "custom-restored", "optional-added" }) {
		if (!std::strcmp(scenario, "custom-changed")) {
			bytes changed = custom;
			set_int(changed, 16 + 136, 8 * F1_0);
			write_fixture(custom_name.c_str(), changed);
		} else if (!std::strcmp(scenario, "custom-missing")) require(PHYSFS_delete(custom_name.c_str()), "remove only fixture-owned custom source");
		else if (!std::strcmp(scenario, "custom-restored")) write_fixture(custom_name.c_str(), custom);
		else if (!std::strcmp(scenario, "optional-added"))
			require(PHYSFS_mount(d2_directory, nullptr, 1) && PHYSFS_mount(hog.c_str(), nullptr, 1), "client gains optional assets without changing host's namespace");
		network_client_level("descent");
		record(scenario, network_receive_assets(custom_packets, !std::strcmp(scenario, "custom-same") || !std::strcmp(scenario, "custom-restored")));
	}
	require(PHYSFS_delete(custom_name.c_str()), "retire custom test data");
	network_client_level("d2");
	actor = create_buddy_bot_at_player(0);
	require(actor > 0, "ordinary D2 companion control");
	const auto ordinary = network_capture_assets(actor);
	network_client_level("d2");
	record("ordinary-d2", network_receive_assets(ordinary, true));
	network_client_level("d2");
	record("ordinary-malformed", network_reject_malformed(ordinary));
	network_client_level("d2");
	record("d1-host-d2-client", network_receive_assets(packets, false));
	network_client_level("descent");
	record("d2-host-d1-client", network_receive_assets(ordinary, false));
}
#elif defined(DXX_BUILD_DESCENT_II)
static void exercise_network_asset_missions(const char *)
{
	require(false, "network asset integration requires USE_UDP");
}
#endif
