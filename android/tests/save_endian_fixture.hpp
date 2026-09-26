// Included by the shared host integration fixture after engine declarations
// Describe the current writers' wire layouts without using their swap helpers
// Raw fields follow host byte order; explicit little-endian records stay fixed
class save_endian_fixture
{
	bytes data;
	size_t pos = 0;
	bool swapped;
	int weapons = 0;

	void skip(size_t size)
	{
		require(pos <= data.size() && size <= data.size() - pos, "complete endian fixture field");
		pos += size;
	}
	void words(size_t width, size_t count = 1)
	{
		for (size_t i = 0; i < count; ++i) {
			const size_t begin = pos;
			skip(width);
			std::reverse(data.begin() + begin, data.begin() + pos);
		}
	}
	int integer()
	{
		const size_t begin = pos;
		words(4);
		int result;
		const auto *source = data.data() + begin;
		ubyte native[4];
		std::copy(source, source + 4, native);
		if (!swapped) std::reverse(native, native + 4);
		std::memcpy(&result, native, 4);
		return result;
	}
	int count()
	{
		const int value = integer();
		require(value >= 0 && value <= 100000, "bounded endian fixture record count");
		return value;
	}
	void finish_record(size_t start, size_t length)
	{
		require(pos <= start + length, "endian fixture record fits its disk layout");
		skip(start + length - pos);
	}
	void player_record()
	{
		const size_t start = pos;
		skip(16); // callsign, address, connection byte
		words(4, 6);
		skip(4); // lives, level, laser, starting level
		words(2);
#ifdef DXX_BUILD_DESCENT_II
		words(2, 2 + MAX_PRIMARY_WEAPONS + MAX_SECONDARY_WEAPONS);
		skip(2); // alignment, not a player value
#else
		skip(2); // weapon flags
		words(2, MAX_PRIMARY_WEAPONS + MAX_SECONDARY_WEAPONS);
#endif
		words(4, 6);
#ifdef DXX_BUILD_DESCENT_II
		words(2, 9);
#else
		words(2, 8);
#endif
		skip(2);
		words(4);
		skip(2);
		require(pos - start == sizeof(player_rw), "complete serialized player layout");
	}
	void object_record()
	{
		static_assert(sizeof(object_rw) == 264 && offsetof(object_rw, mtype) == 94 &&
		                  offsetof(object_rw, ctype) == 158 && offsetof(object_rw, rtype) == 188,
		              "object disk boundaries");
		const size_t start = pos;
		require(data.size() - pos >= 264, "complete serialized object");
		const int type = data[pos + 4], control = data[pos + 10], movement = data[pos + 11], render = data[pos + 12];
#ifdef DXX_BUILD_DESCENT_II
		const bool native_ai = d1_in_d2_use_d1_gameplay() && type == OBJ_ROBOT && !Robot_info[data[pos + 5]].companion;
#endif
		if (type != OBJ_NONE && control == CT_WEAPON) ++weapons;
		words(4);
		skip(2);
		words(2, 2);
		skip(4);
		words(2, 2);
		words(4, 17); // position, matrix, size, shields, last position
		skip(4);
		words(4);
		if (movement == MT_PHYSICS) {
			words(4, 15);
			words(2, 2);
		} else if (movement == MT_SPINNING) words(4, 3);
		finish_record(start, 158);
		switch (control) {
			case CT_WEAPON:
				words(2, 2);
				words(4, 2);
				words(2, 2);
				words(4);
				break;
			case CT_EXPLOSION:
				words(4, 2);
				words(2, 4);
				break;
			case CT_AI:
			case CT_MORPH:
				skip(12);
#ifdef DXX_BUILD_DESCENT_II
				if (!native_ai) {
					words(2, 3);
					skip(2);
					words(2);
					words(4, 2);
					break;
				}
#endif
				words(2, 6);
				words(4);
				words(2);
				break;
			case CT_LIGHT: words(4); break;
			case CT_POWERUP:
#ifdef DXX_BUILD_DESCENT_II
				words(4, 3);
#else
				words(4);
#endif
				break;
		}
		finish_record(start, 188);
		if (render == RT_POLYOBJ || render == RT_MORPH || (render == RT_NONE && type == OBJ_GHOST)) {
			words(4);
			words(2, MAX_SUBMODELS * 3);
			words(4, 3);
		} else if (render == RT_WEAPON_VCLIP || render == RT_HOSTAGE || render == RT_POWERUP || render == RT_FIREBALL)
			words(4, 2);
		finish_record(start, 264);
	}
	void ai_records()
	{
		words(4, 2);
		for (int i = 0; i < MAX_OBJECTS; ++i) {
#ifdef DXX_BUILD_DESCENT_II
			words(4, 15);
#else
			skip(6);
			words(2);
			words(4, 9);
#endif
			words(2, MAX_SUBMODELS * 6);
			skip(MAX_SUBMODELS * 2);
		}
		words(4, MAX_POINT_SEGS * 4);
#ifdef DXX_BUILD_DESCENT_II
		words(4, MAX_AI_CLOAK_INFO * 5);
		words(4, 17); // boss clocks/state and escort state
		skip(MAX_STOLEN_ITEMS);
		words(4); // path pool cursor
		const int teleports = count(), gates = count();
		words(2, gates + teleports);
#else
		words(4, MAX_AI_CLOAK_INFO * 4);
		words(4, 14); // boss clocks/state and path pool cursor
#endif
		for (int i = count(); i; --i) {
			words(2, 2);
			skip(12); // awareness position is explicitly little-endian
		}
		skip(12); // believed player position is explicitly little-endian
#ifdef DXX_BUILD_DESCENT_II
		words(4);
		skip(12); // last fired-upon position uses the same vector writer
#endif
	}
	void runtime_records(int version)
	{
		words(4, 17);          // firing, RNG, tick and object extent
		words(2, MAX_OBJECTS); // allocator
		words(4, 8);           // signature, homing and common laser state
#ifdef DXX_BUILD_DESCENT_II
		words(4, 3); // helix, smart mines, omega
#endif
		for (int i = 0; i < weapons; ++i) {
			words(4);
			skip(MAX_OBJECTS);
		}
		for (int i = count(); i; --i) {
			words(4, 2);         // morph object/signature
			skip(MAX_VECS * 24); // morph points and deltas use the LE vector writer
			words(4, MAX_VECS + MAX_SUBMODELS * 3 + 1);
			skip(2); // original control/movement
			skip(24);
			words(4, 3);
			skip(24);
			words(2, 2); // saved physics
		}
		words(4); // active stuck count; all registry slots follow
		for (int i = 0; i < MAX_STUCK_OBJECTS; ++i) {
			words(2, 2);
			words(4);
		}
#ifdef DXX_BUILD_DESCENT_II
		words(4, 2 + MAX_OBJECTS); // reactor runtime and afterburner clocks
		words(4, 2);               // path collection and companion polish ticks
#else
		words(4); // reactor death silence
		words(4); // path collection tick
#endif
		words(2);
		words(4, 4); // player path
		words(4, 2); // effect clock: low then high, each word in host order
		const int effects = count();
		words(4, effects * 7);
		skip(SECRET_AREA_IDENTITY_SAVE_SIZE); // explicit little-endian schema
		words(4, 4);                          // pending autoselection
#ifdef DXX_BUILD_DESCENT_II
		for (int i = count(); i; --i) {
			words(2, 2);
			words(4, 3);
		}
		words(4, 2); // native boss pending/damage history
		const int triggers = count();
		skip(triggers * 2);
#endif
		words(4, 6); // cadence low/high words
#ifdef DXX_BUILD_DESCENT_II
		skip(4); // Guide-Bot routing mode is explicitly little-endian
#else
		if (version >= 19) words(4, MAX_EXPLODING_WALLS * 3);
#endif
	}

  public:
	explicit save_endian_fixture(const bytes &source) : data(source)
	{
		require(data.size() >= 8 && !std::memcmp(data.data(), "DGSS", 4), "endian fixture starts with a complete save header");
		unsigned version;
		std::memcpy(&version, data.data() + 4, 4);
		swapped = (version & 0xffff0000u) != 0;
	}
	bytes convert()
	{
		require(!(Game_mode & GM_MULTI_COOP), "single-player endian fixture framing");
		skip(4);
		const int version = integer();
#ifdef DXX_BUILD_DESCENT_II
		require(version == 38 || version == 40, "current ordinary/imported D2 save fixture");
#else
		require(version == 18 || version == 19, "current native D1 save fixture");
#endif
		skip(20 + 100 * 50 * 3); // description and RGB thumbnail
		require(integer() == 0, "endian fixture is an in-level save");
		skip(9);
		words(4, 3); // mission, levels, clock
		player_record();
		skip(2);     // selected weapons
		words(4, 5); // difficulty history and cheats
#ifdef DXX_BUILD_DESCENT_II
		if (version == 40) {
			const int length = count();
			skip(length);
		}
#else
		words(4); // turbo
#endif
		for (int i = count(); i; --i) object_record();
		for (int i = count(); i; --i) {
			words(4, 4);
			skip(8);
		}
#ifdef DXX_BUILD_DESCENT_II
		const int exploding = count();
		words(4, exploding * 3);
#endif
		for (int i = count(); i; --i) {
			words(4);
			words(2, 4);
			words(4);
		}
#ifdef DXX_BUILD_DESCENT_II
		for (int i = count(); i; --i) {
			words(2, 2);
			words(4, 9);
		}
#endif
		for (int i = count(); i; --i) {
#ifdef DXX_BUILD_DESCENT_II
			skip(4);
			words(4, 2);
#else
			skip(1);
			words(2);
			words(4, 2);
			skip(1);
			words(2);
#endif
			words(2, MAX_WALLS_PER_LINK * 2);
		}
		words(2, (Highest_segment_index + 1) * 18);
		words(4, 2); // reactor destroyed and countdown
		for (int i = count(); i; --i) {
#ifdef DXX_BUILD_DESCENT_II
			words(4, 4);
#else
			words(4, 3);
#endif
			words(2, 2);
		}
		words(2, sizeof(control_center_triggers) / 2);
		for (int i = count(); i; --i) {
			words(4, 2);
			skip(4);
			words(4, 7);
		}
		words(4, 5); // reactor state
		ai_records();
		const size_t segments = (std::max) (Highest_segment_index + 1, MAX_SEGMENTS_ORIGINAL);
		skip(segments); // automap
#ifdef DXX_BUILD_DESCENT_II
		words(4, 4 + NUM_MARKERS);
		skip(NUM_MARKERS * (CALLSIGN_LEN + 1 + MARKER_MESSAGE_LEN));
		words(4); // afterburner charge
		skip(MAX_PRIMARY_WEAPONS + MAX_SECONDARY_WEAPONS);
		words(4, 5);    // flash and palette
		skip(segments); // subtracted light flags
		words(4, 2);    // secret visit and omega charge
#else
		words(4, 6); // game ID and cheats
#endif
		runtime_records(version);
		if (pos != data.size()) std::fprintf(stderr, "Endian fixture consumed %zu of %zu save bytes\n", pos, data.size());
		require(pos == data.size(), "endian fixture accounts for the complete save through EOF");
		return std::move(data);
	}
};
