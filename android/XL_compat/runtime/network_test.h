// Scoped transport for native multiplayer dispatch controls; no live peers or sockets
extern "C" void (*xl_test_network_send)(const unsigned char *, int, int);
struct ScopedNetworkTest {
	static inline unsigned packets = 0, sounds = 0;
	decltype(xl_test_network_send) previous = xl_test_network_send;
	static void record(const unsigned char *data, int size, int priority)
	{
		require(size > 0 && priority >= 0 && priority <= 2, "invalid fixture network packet");
		++packets;
		if (data[0] == MULTI_PLAY_SOUND) {
			require(size == 4 && data[1] == Player_num, "invalid fixture selection sound packet");
			++sounds;
		}
	}
	ScopedNetworkTest()
	{
		xl_test_network_send = record;
	}
	~ScopedNetworkTest()
	{
		xl_test_network_send = previous;
	}
};
