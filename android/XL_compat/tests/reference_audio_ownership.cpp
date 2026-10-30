// Actual pinned owner/container consumers with independent channel and object storage
#include "audio_owners.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace ownership_reference
{
using fix = int;
#define ENTER(a, b)
#define RETVAL(x) return x;
#define RETURN    return;
#define DBG       0
#include "xl_reference_audio_constants.h"
template <class _T, unsigned N>
struct Stack {
	std::array<_T, N> storage{};
	struct {
		_T *buffer;
		unsigned length;
	} m_data{ storage.data(), N };
	unsigned m_tos = 0, m_growth = 0;
	bool Resize(unsigned)
	{
		return false;
	}
	unsigned ToS() const
	{
		return m_tos;
	}
	_T *Buffer()
	{
		return storage.data();
	}
	_T *Top()
	{
		return m_tos ? &storage[m_tos - 1] : nullptr;
	}
	_T &operator[](unsigned i)
	{
		return storage.at(i);
	}
	_T *operator+(unsigned i)
	{
		return storage.data() + i;
	}
#include "xl_reference_audio_stack.h"
};
struct CFixVector {
	int x = 0, y = 0, z = 0;
};
struct CObject {
	int signature = 0;
	struct {
		struct {
			CFixVector vPos;
		} position;
	} info;
	int Signature() const
	{
		return signature;
	}
};
std::array<CObject, 64> objects;
CObject *object(int index)
{
	return index >= 0 && index < int(objects.size()) ? &objects[unsigned(index)] : nullptr;
}
#define OBJECT(i) object(i)
struct {
	int nObject = 63;
} localplayer;
#define LOCALPLAYER localplayer
constexpr int ND_STATE_RECORDING = 1;
struct {
	struct {
		int nState = 0;
	} demoData;
	struct {
		int bMoving = 0;
	} multiplayer;
} gameData;
struct {
	struct {
		bool bDontStartObjects = false;
	} sound;
} gameStates;
void NDRecordDestroySoundObject(int) {}
int Max(int a, int b)
{
	return (std::max) (a, b);
}
struct CSoundObject {
	int m_nSignature = 0, m_flags = 0, m_channel = -1, m_volume = 0, m_soundClass = 0, m_nSound = 0;
	int m_pan = 0, m_nLoopStart = -1, m_nLoopEnd = -1;
	char m_szSound[32]{};
	struct {
		struct {
			int nObject = -1, nObjSig = -1;
		} obj;
		struct {
			int nSegment = 0, nSide = 0;
			CFixVector position;
		} pos;
	} m_linkType;
	void Stop();
	bool Start();
};
struct CAudioChannel {
	struct {
		int nSoundObj = -1, nIndex = -1, bPlaying = 0;
	} m_info;
	CAudioChannel(int owner = -1, bool playing = false)
	{
		m_info.nSoundObj = owner;
		m_info.bPlaying = playing;
	}
	int SoundObject() const
	{
		return m_info.nSoundObj;
	}
	void SetSoundObj(int index)
	{
		m_info.nSoundObj = index;
	}
	void SetIndex(int index)
	{
		m_info.nIndex = index;
	}
	void SetPlaying(int32_t);
	void Stop();
};
struct Channels {
	std::array<CAudioChannel, 128> storage;
	CAudioChannel &operator[](int i)
	{
		return storage.at(unsigned(i));
	}
	CAudioChannel *operator+(int i)
	{
		return &storage.at(unsigned(i));
	}
};
struct CAudio {
	Stack<CSoundObject, 500> m_objects;
	Stack<int, 128> m_usedChannels;
	Channels m_channels;
	struct {
		int nActiveObjects = 0;
	} m_info;
	bool fail_start = false;
	std::vector<int> stopped;
	struct Request {
		int sound, sound_class, volume, pan, loop, start, end, owner, speed;
	} request{};
	auto &Objects()
	{
		return m_objects;
	}
	int GetMaxChannels() const
	{
		return 128;
	}
	void ActivateObject()
	{
		++m_info.nActiveObjects;
	}
	void DeactivateObject()
	{
		--m_info.nActiveObjects;
	}
#include "xl_reference_audio_active.h"
	void StopSound(int channel)
	{
		stopped.push_back(channel);
		m_channels[channel].Stop();
	}
	int StartSound(int sound, int sound_class, int volume, int pan, int loop, int start, int end, int owner, int speed, const char *, CFixVector *)
	{
		request = { sound, sound_class, volume, pan, loop, start, end, owner, speed };
		if (fail_start) return -1;
		for (int i = 0; i < 128; ++i)
			if (!m_channels[i].m_info.bPlaying) {
				m_channels[i] = { owner, true };
				if (!m_usedChannels.Grow()) throw std::runtime_error("Source channel registry overflow");
				m_channels[i].SetIndex(int(m_usedChannels.ToS() - 1));
				*m_usedChannels.Top() = i;
				return i;
			}
		return -1;
	}
	int XlatSound(int sound)
	{
		return sound;
	}
	void DeleteSoundObject(int32_t);
	int32_t DestroyObjectSound(int32_t, int16_t = -1);
	int32_t DestroySegmentSound(int16_t, int16_t, int16_t);
	void EndSoundObject(int32_t);
	void StopObjectSounds();
	bool SuspendObjectSound(int32_t);
	void Cleanup();
	void UnregisterChannel(int32_t);
} audio;
#include "xl_reference_audio_ownership.h"
void CAudioChannel::Stop()
{
	audio.UnregisterChannel(m_info.nIndex);
	m_info = {};
}
void reset(int count)
{
	audio.m_objects.m_tos = 0;
	audio.m_usedChannels.m_tos = 0;
	audio.m_info.nActiveObjects = count;
	audio.stopped.clear();
	audio.fail_start = false;
	gameStates.sound.bDontStartObjects = false;
	for (auto &channel : audio.m_channels.storage) channel = {};
	for (unsigned i = 0; i < objects.size(); ++i) objects[i].signature = 100 + int(i);
	for (int i = 0; i < count; ++i) {
		if (!audio.m_objects.Grow()) throw std::runtime_error("Source object fixture exceeded capacity");
		auto &owner = audio.m_objects[unsigned(i)];
		owner = {};
		owner.m_nSignature = 1000 + i;
		owner.m_flags = SOF_USED | SOF_LINK_TO_OBJ | SOF_PLAY_FOREVER;
		owner.m_volume = (i + 1) * 1024;
		owner.m_linkType.obj = { i % 64, objects[unsigned(i % 64)].signature };
		if (i < 128) {
			owner.m_channel = i;
			audio.m_channels[i] = { i, true };
			audio.m_channels[i].SetIndex(i);
			audio.m_usedChannels.storage[audio.m_usedChannels.m_tos++] = i;
		}
	}
}
} // namespace ownership_reference
namespace
{
struct NativeOwners {
	xl_compat::AudioOwners owners;
	xl_compat::AudioVoices mixer;
};
std::string owner_case;
void require_owner(bool value, const char *message)
{
	if (!value) throw std::runtime_error(owner_case + ": " + message);
}
NativeOwners native_owners()
{
	using namespace ownership_reference;
	NativeOwners result;
	for (unsigned i = 0; i < audio.m_objects.ToS(); ++i) {
		const auto &source = audio.m_objects[i];
		xl_compat::AudioOwner entry;
		entry.flags = unsigned(source.m_flags);
		entry.sound = source.m_nSound;
		entry.sound_class = source.m_soundClass;
		entry.volume = source.m_volume;
		entry.pan = source.m_pan;
		entry.loop_start = source.m_nLoopStart;
		entry.loop_end = source.m_nLoopEnd;
		entry.object = source.m_linkType.obj.nObject;
		entry.signature = source.m_linkType.obj.nObjSig;
		entry.segment = source.m_linkType.pos.nSegment;
		entry.side = source.m_linkType.pos.nSide;
		entry.external = source.m_szSound[0] != 0;
		require_owner(result.owners.add(entry) == int(i), "Cannot prepare native ownership control");
		if (source.m_channel >= 0) {
			auto &added = result.owners.entries[i];
			added.channel = source.m_channel;
			added.channel_serial = 10000 + unsigned(source.m_channel);
			auto &voice = result.mixer.voices[unsigned(source.m_channel)];
			voice = { added.channel_serial, 0, 0, int(i), 1, { source.m_soundClass, source.m_volume / 2, true }, {}, xl_compat::AudioOwnerKind::object };
		}
	}
	result.mixer.next_serial = 20000;
	require_owner(result.owners.valid(result.mixer), "Initial owner bindings are invalid");
	return result;
}
void compare_owners(const NativeOwners &native, bool source_bindings = true)
{
	using namespace ownership_reference;
	require_owner(native.owners.valid(native.mixer), "Owned registry lost a voice or bound another lifetime");
	require_owner(native.owners.count == audio.m_objects.ToS() && native.owners.active() == unsigned(audio.ActiveObjects()), "Owner count or active membership differs from source");
	for (unsigned i = 0; i < native.owners.count; ++i) {
		const auto &actual = native.owners.entries[i];
		const auto &source = audio.m_objects[i];
		require_owner(actual.serial == unsigned(source.m_nSignature - 999) && actual.flags == unsigned(source.m_flags) && actual.channel == source.m_channel && actual.volume == source.m_volume && actual.sound == source.m_nSound && actual.object == source.m_linkType.obj.nObject && actual.signature == source.m_linkType.obj.nObjSig, "Source owner order, target selection or lifetime differs");
	}
	for (unsigned i = 0; i < native.mixer.voices.size(); ++i) {
		const auto &actual = native.mixer.voices[i];
		const auto &source = audio.m_channels[int(i)];
		require_owner(bool(actual.remaining) == bool(source.m_info.bPlaying), "Operation stopped or started an unrelated channel");
		if (source_bindings && source.m_info.bPlaying) require_owner(actual.owner == source.m_info.nSoundObj, "Source and owned live back-references differ");
	}
}
} // namespace
void audio_ownership_source_controls()
{
	using namespace ownership_reference;
	const auto require = [](bool v, const char *message) {if(!v)throw std::runtime_error(message); };
	static_assert(MAX_SOUND_OBJECTS == 500);
	reset(500);
	require(!audio.m_objects.Grow() && audio.m_objects.ToS() == 500, "Source sound-owner capacity changed");
	reset(3);
	audio.DeleteSoundObject(0);
	require(audio.m_objects.ToS() == 2 && audio.m_objects[0].m_nSignature == 1001 && audio.m_objects[1].m_nSignature == 1002, "Actual source stack deletion no longer shifts owner order");
	require(audio.m_channels[1].SoundObject() == 1 && audio.m_channels[2].SoundObject() == 0, "Source deletion no longer has the recorded shifted-owner mismatch");
	audio.Cleanup();
	require(audio.m_objects.ToS() == 0 && !audio.m_channels[1].m_info.bPlaying && !audio.m_channels[2].m_info.bPlaying, "Source cleanup no longer drops the mismatched surviving owners");
	reset(3);
	audio.DeleteSoundObject(1);
	audio.Cleanup();
	require(audio.m_objects.ToS() == 2 && audio.m_objects[0].m_nSignature == 1000 && audio.m_objects[1].m_nSignature == 1002, "Single-shift source deletion control failed");
	std::cout << "PASS: actual 500-owner source capacity, ordered-container deletion and reproduced unrelated-owner cleanup loss\n";
	unsigned cases = 0;
	for (int count : { 1, 2, 3, 5, 128, 500 })
		for (int index : { 0, count / 2, count - 1 })
			for (int operation : { 0, 1, 2, 3 }) {
				owner_case = "lifecycle count=" + std::to_string(count) + " index=" + std::to_string(index) + " operation=" + std::to_string(operation);
				reset(count);
				if (operation == 3) audio.m_objects[unsigned(index)].m_flags &= ~SOF_PLAY_FOREVER;
				auto native = native_owners();
				if (operation == 0) {
					audio.m_objects[unsigned(index)].Stop();
					native.owners.stop(unsigned(index), native.mixer);
				} else if (operation == 1) {
					audio.DeleteSoundObject(index);
					native.owners.erase(unsigned(index), native.mixer);
				} else {
					// The actual SetPlaying(false) caller retires the channel before
					// EndSoundObject. Invoking it on a live channel is not a source event
					const int slot = audio.m_objects[unsigned(index)].m_channel;
					if (slot >= 0) {
						audio.m_channels[slot].SetPlaying(0);
						native.mixer.stop(slot);
					}
					if (slot < 0) audio.EndSoundObject(index);
					native.owners.end(unsigned(index), native.mixer);
				}
				compare_owners(native, operation == 0 || operation == 2 || index >= count - 2);
				++cases;
			}
	for (int count : { 0, 1, 41, 42, 43, 127 })
		for (int threshold : { 0, 1023, 1024, 4096, 65536 })
			for (bool ties : { false, true }) {
				owner_case = "suspend count=" + std::to_string(count) + " threshold=" + std::to_string(threshold);
				reset(count);
				if (ties)
					for (unsigned i = 0; i < audio.m_objects.ToS(); ++i) audio.m_objects[i].m_volume = 1024;
				auto native = native_owners();
				require(audio.SuspendObjectSound(threshold) == native.owners.suspend(threshold, native.mixer), "Source suspension selection differs");
				compare_owners(native);
				++cases;
			}
	for (int active : { 0, 1, 41, 42, 43, 127 })
		for (int flags : { SOF_USED | SOF_LINK_TO_OBJ, SOF_USED | SOF_LINK_TO_OBJ | SOF_PLAY_FOREVER, SOF_USED | SOF_LINK_TO_OBJ | SOF_PERMANENT, SOF_USED | SOF_LINK_TO_OBJ | SOF_PERMANENT | SOF_PLAY_FOREVER })
			for (int volume : { 0, 1, 1024, 1025, 65536, 131072 })
				for (bool deferred : { false, true })
					for (bool available : { false, true }) {
						owner_case = "start active=" + std::to_string(active) + " flags=" + std::to_string(flags) + " volume=" + std::to_string(volume) + " deferred=" + std::to_string(deferred) + " available=" + std::to_string(available);
						reset(active + 1);
						auto &entry = audio.m_objects[unsigned(active)];
						entry.m_channel = -1;
						audio.m_channels[active].Stop();
						entry.m_flags = flags;
						entry.m_volume = volume;
						gameStates.sound.bDontStartObjects = deferred;
						audio.fail_start = !available;
						auto native = native_owners();
						require(entry.Start() == native.owners.start(unsigned(active), 32768, deferred, native.mixer, available), "Permanent threshold, deferred or failed source start differs");
						compare_owners(native);
						if (entry.m_channel >= 0) {
							const auto &voice = native.mixer.voices[unsigned(entry.m_channel)];
							require(voice.owner == audio.request.owner && voice.priority.persistent && voice.priority.sound_class == -1 && voice.priority.volume == volume / 2, "Linked start lost source owner, class or priority volume");
							require(voice.remaining == ((flags & SOF_PLAY_FOREVER) ? -1 : 1), "Linked start changed finite or infinite playback");
						}
						++cases;
					}
	for (int count : { 1, 3, 67, 128, 500 })
		for (int target : { 0, 1, 63, 64 })
			for (int sound : { -1, 0, 2 }) {
				owner_case = "destroy object count=" + std::to_string(count) + " target=" + std::to_string(target) + " sound=" + std::to_string(sound);
				reset(count);
				for (unsigned i = 0; i < audio.m_objects.ToS(); ++i) audio.m_objects[i].m_nSound = int(i % 3);
				auto native = native_owners();
				require(bool(audio.DestroyObjectSound(target, int16_t(sound))) == native.owners.destroy_object(target, sound, native.mixer), "Explicit source owner destruction differs");
				compare_owners(native, false);
				++cases;
			}
	for (int count : { 1, 3, 67, 128, 500 })
		for (int state : { 0, 1, 2 })
			for (bool forever : { false, true }) {
				owner_case = "missing count=" + std::to_string(count) + " state=" + std::to_string(state) + " forever=" + std::to_string(forever);
				reset(count);
				if (state == 1) ++objects[0].signature;
				if (state == 2)
					for (unsigned i = 0; i < audio.m_objects.ToS(); ++i) audio.m_objects[i].m_linkType.obj.nObject = -1;
				if (!forever)
					for (unsigned i = 0; i < audio.m_objects.ToS(); ++i) audio.m_objects[i].m_flags &= ~SOF_PLAY_FOREVER;
				auto native = native_owners();
				const bool expected = audio.DestroyObjectSound(-1) != 0;
				const bool actual = native.owners.destroy_missing([](int index, int signature) {const auto *obj=object(index);return obj&&obj->Signature()==signature; }, native.mixer);
				require(expected == actual, "Source missing-lifetime destruction result differs");
				compare_owners(native, false);
				++cases;
			}
	for (int count : { 1, 3, 128, 500 })
		for (int segment : { 0, 1, 2 })
			for (int side : { 0, 1, 2 })
				for (int sound : { -1, 0, 1, 2 }) {
					owner_case = "segment count=" + std::to_string(count) + " segment=" + std::to_string(segment) + " side=" + std::to_string(side) + " sound=" + std::to_string(sound);
					reset(count);
					for (unsigned i = 0; i < audio.m_objects.ToS(); ++i) {
						auto &entry = audio.m_objects[i];
						entry.m_flags = SOF_USED | SOF_LINK_TO_POS | SOF_PLAY_FOREVER;
						entry.m_linkType.pos.nSegment = int(i % 3);
						entry.m_linkType.pos.nSide = int((i / 3) % 3);
						entry.m_nSound = int(i % 2);
					}
					auto native = native_owners();
					require(bool(audio.DestroySegmentSound(int16_t(segment), int16_t(side), int16_t(sound))) == native.owners.destroy_segment(segment, side, sound, native.mixer), "Source segment/side/sample filtering differs");
					compare_owners(native, false);
					++cases;
				}
	std::cout << "PASS: " << cases << " source owner lifecycle comparisons; ordered removal retains every surviving voice through repaired back-references\n";
}
