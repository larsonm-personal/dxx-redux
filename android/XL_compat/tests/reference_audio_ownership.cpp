// Actual pinned owner/container consumers with independent channel and object storage
#include "audio_owners.h"
#include "audio_creation_receipt.h"
#include "audio_logical.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace ownership_reference
{
using fix = int;
#define ENTER(a, b)
#define RETVAL(x)      return x;
#define RETURN         return;
#define DBG            0
#define USE_SDL_MIXER  1
#define MIX_MAX_VOLUME 128
#include "xl_reference_audio_constants.h"
std::uint8_t Sounds[2][MAX_SOUNDS]{}, AltSounds[2][MAX_SOUNDS]{};
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
	int Index(const _T *entry) const
	{
		return int(entry - storage.data());
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
	static int Dist(CFixVector a, CFixVector b)
	{
		if (a.x == b.x && a.y == b.y && a.z == b.z) return 0;
		throw std::runtime_error("Noncoincident duplicate distance is outside the creation fixture");
	}
};
struct CFixMatrix {
};
struct CObject {
	int signature = 0;
	struct {
		struct {
			CFixVector vPos;
			CFixMatrix mOrient;
		} position;
		int nType = 1, nId = 0, nSignature = 0, nSegment = 0;
	} info;
	struct {
		struct {
			bool bEnabled = true;
		} soundInfo, lightningInfo;
	} rType;
	int Signature() const
	{
		return signature;
	}
	int Index() const;
	CFixVector &Position()
	{
		return info.position.vPos;
	}
};
std::array<CObject, 64> objects;
int CObject::Index() const
{
	return int(this - objects.data());
}
CObject *object(int index)
{
	return index >= 0 && index < int(objects.size()) ? &objects[unsigned(index)] : nullptr;
}
#define OBJECT(i) object(i)
#define OBJPOS(p) (&(p)->info.position)
#define OBJSEG(p) ((p)->info.nSegment)
struct {
	CObject *Buffer()
	{
		return objects.data();
	}
} object_store;
#define OBJECTS object_store
constexpr int OBJ_NONE = 255, OBJ_EFFECT = 20, SOUND_ID = 2, LIGHTNING_ID = 1;
bool show_lightning = true;
#define SHOW_LIGHTNING(i) show_lightning
int NDFindObject(int signature)
{
	for (unsigned i = 0; i < objects.size(); ++i)
		if (objects[i].info.nSignature == signature) return int(i);
	return -1;
}
struct {
	int nObject = 63;
	int flags = 0;
} localplayer;
#define LOCALPLAYER localplayer
constexpr int ND_STATE_RECORDING = 1, ND_STATE_PLAYBACK = 2;
struct {
	struct {
		int nState = 0;
	} demoData;
	struct {
		int bMoving = 0;
	} multiplayer;
	struct {
		CObject *pViewer = &objects[63];
		int nLastObject[1]{ 63 };
	} objData;
	struct {
		struct {
			struct {
				const char *data = "fixture";
			} sounds[2][MAX_SOUNDS];
		} sound;
	} pigData;
	struct {
		int xAfterburnerCharge = 0;
	} physicsData;
} gameData;
struct {
	struct {
		bool bDontStartObjects = false, bWasRecording = false;
		int bD1Sound = 0;
	} sound;
	struct {
		bool bEndLevelSequence = false, bPlayerIsDead = false;
	} app;
	struct {
		int bLastAfterburnerState = 0, xLastAfterburnerCharge = 0;
	} gameplay;
	struct {
		int nGameType = 0;
	} multi;
} gameStates;
struct {
	int afterburnerState = 0;
} controls[1];
constexpr bool IsMultiGame = false;
constexpr int UDP_GAME = 1;
void MultiSendSoundFunction(int, int) {}
void NDRecordDestroySoundObject(int) {}
void NDRecordCreateObjectSound(int, int, int, int, int, int) {}
void NDRecordSound3DOnce(int, int, int) {}
void NDRecordSound3D(int, int, int) {}
void Int3()
{
	throw std::runtime_error("Unexpected source sound trap");
}
bool segment(int index)
{
	return index >= 0;
}
#define SEGMENT(i) segment(i)
struct {
	void Process() {}
} soundQueue;
struct Options {
	struct {
		bool bUseSDLMixer = true;
		int xCustomSoundVolume = 65536;
		int audioSampleRate = 22050;
	} sound;
} options;
auto *gameOpts = &options;
struct ChannelOutput {
	int volume = 128;
	std::array<std::uint8_t, 2> pan{ 127, 127 };
};
std::array<ChannelOutput, 128> outputs;
int Mix_Volume(int channel, int volume)
{
	auto &out = outputs.at(unsigned(channel));
	const int old = out.volume;
	if (volume >= 0) out.volume = (std::min) (128, volume);
	return old;
}
int Mix_SetPanning(int channel, std::uint8_t left, std::uint8_t right)
{
	outputs.at(unsigned(channel)).pan = { left, right };
	return 1;
}
void Mix_VolPan(int32_t, int32_t, int32_t);
int spatial_volume = 32768, spatial_pan = 32767;
unsigned source_spatial_calls = 0;
const xl_audio_creation_receipt::Fields *creation_input = nullptr;
int FixMul(int a, int b)
{
	return int(std::int64_t(a) * b / 65536);
}

int Max(int a, int b)
{
	return (std::max) (a, b);
}
struct CSoundObject {
	int m_nSignature = 0, m_flags = 0, m_channel = -1, m_volume = 0, m_soundClass = 0, m_nSound = 0;
	int m_pan = 0, m_nLoopStart = -1, m_nLoopEnd = -1;
	int m_audioVolume = -1, m_maxVolume = 65536, m_maxDistance = 256 * 65536, m_nDecay = 0;
	bool m_bAmbient = false, m_bCustom = false;
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
		int nVolume = 0, nPan = 32767, nChannel = 0, bAmbient = 0;
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
	void SetVolume(int32_t);
	void SetPan(int32_t);
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
		int nNextSignature = 1;
		bool bLoMem = false;
		int16_t nLoopingSound = -1, nLoopingVolume = 0, nLoopingStart = -1, nLoopingEnd = -1;
		int32_t nLoopingChannel = -1;
	} m_info;
	std::array<int, 2> volumes{ 32768, 32768 };
	unsigned cursor = 0;
	bool fail_start = false;
	unsigned start_calls = 0;
	std::vector<int> stopped;
	struct Request {
		int sound, sound_class, volume, pan, loop, start, end, owner, speed;
		std::string filename;
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
	int StartSound(int sound, int sound_class, int volume, int pan, int loop, int start, int end, int owner = -1, int speed = 65536, const char *filename = nullptr, CFixVector * = nullptr)
	{
		request = { sound, sound_class, volume, pan, loop, start, end, owner, speed, filename ? filename : "" };
		++start_calls;
		if (fail_start) return -1;
		for (int step = 0; step < 128; ++step) {
			const int i = int((cursor + unsigned(step)) % 128);
			if (!m_channels[i].m_info.bPlaying) {
				m_channels[i] = { owner, true };
				m_channels[i].m_info.nVolume = int(std::int64_t(volume) * volumes[0] / 65536);
				m_channels[i].m_info.nPan = pan;
				m_channels[i].m_info.nChannel = i;
				m_channels[i].m_info.bAmbient = sound_class == SOUNDCLASS_AMBIENT;
				Mix_VolPan(i, m_channels[i].m_info.nVolume, pan);
				cursor = (unsigned(i) + 1) % 128;
				if (!m_usedChannels.Grow()) throw std::runtime_error("Source channel registry overflow");
				m_channels[i].SetIndex(int(m_usedChannels.ToS() - 1));
				*m_usedChannels.Top() = i;
				return i;
			}
		}
		return -1;
	}
	int16_t XlatSound(int16_t sound);
	bool Available() const
	{
		return true;
	}
	int Volume(int ambient = 0) const
	{
		return volumes.at(unsigned(ambient));
	}
	CAudioChannel *Channel()
	{
		return m_channels.storage.data();
	}
	bool ChannelIsPlaying(int index) const
	{
		return index >= 0 && index < 128 && m_channels.storage[unsigned(index)].m_info.bPlaying;
	}
	void SetVolume(int channel, int volume)
	{
		m_channels[channel].SetVolume(volume);
	}
	void SetPan(int channel, int pan)
	{
		m_channels[channel].SetPan(pan);
	}
	void RecordSoundObjects() {}
	void SyncSounds();
	void StartLoopingSound();
	void PlayLoopingSound(int16_t, fix, int32_t, int32_t);
	void ChangeLoopingVolume(fix);
	void StopLoopingSound();
	void GetVolPan(CFixMatrix, CFixVector, int, CFixVector position, int segment_index, int maximum, int *volume, int *pan, int range, int decay, int = 0)
	{
		if (creation_input) {
			const auto &input = *creation_input;
			if (maximum != input.at("volume") || range != input.at("range") || segment_index != input.at("segment") || position.x != input.at("x") || position.y != input.at("y") || position.z != input.at("z"))
				throw std::runtime_error("Source creation requested different geometry inputs");
			*volume = int(input.at("volume" + std::to_string(decay)));
			*pan = int(input.at("pan" + std::to_string(decay)));
		} else {
			*volume = spatial_volume;
			*pan = spatial_pan;
		}
		++source_spatial_calls;
	}
	int32_t PlaySound(int16_t, int32_t, fix, int32_t, int32_t, int32_t, const char *, CFixVector *);
	CSoundObject *FindObjectSound(int16_t, CObject *, CFixVector * = nullptr);
	int32_t FindObjectSound(int32_t, int16_t);
	CSoundObject *FindSegmentSound(int16_t, int16_t, int16_t, CFixVector &);
	int32_t CreateObjectSound(int16_t, int32_t, int16_t, int32_t = 0, fix = 65536, fix = 256 * 65536, int32_t = -1, int32_t = -1, const char * = nullptr, int32_t = 0, uint8_t = 0, int32_t = 0);
	int32_t CreateSegmentSound(int16_t, int16_t, int16_t, CFixVector &, int32_t, fix, fix, const char *);
	void DeleteSoundObject(int32_t);
	int32_t DestroyObjectSound(int32_t, int16_t = -1);
	int32_t DestroySegmentSound(int16_t, int16_t, int16_t);
	void EndSoundObject(int32_t);
	void StopObjectSounds();
	bool SuspendObjectSound(int32_t);
	void Cleanup();
	void UnregisterChannel(int32_t);
} audio;
#include "xl_reference_audio_controls.h"
#include "xl_reference_audio_ownership.h"
#include "xl_reference_audio_creation.h"
#include "xl_reference_afterburner.h"
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
	audio.start_calls = 0;
	audio.volumes = { 32768, 32768 };
	audio.cursor = 0;
	audio.m_info.bLoMem = false;
	audio.m_info.nLoopingSound = audio.m_info.nLoopingChannel = -1;
	audio.m_info.nLoopingVolume = 0;
	audio.m_info.nLoopingStart = audio.m_info.nLoopingEnd = -1;
	gameStates.sound.bD1Sound = 0;
	for (unsigned profile = 0; profile < 2; ++profile)
		for (unsigned i = 0; i < MAX_SOUNDS; ++i) Sounds[profile][i] = AltSounds[profile][i] = std::uint8_t(i);
	outputs.fill({});
	gameData.demoData.nState = 0;
	show_lightning = true;
	source_spatial_calls = 0;
	gameStates.sound.bDontStartObjects = false;
	for (auto &channel : audio.m_channels.storage) channel = {};
	for (unsigned i = 0; i < objects.size(); ++i) {
		objects[i] = {};
		objects[i].info.nSignature = objects[i].signature = 100 + int(i);
	}
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
#include "reference_audio_creation.h"
#include "reference_audio_logical.h"
#include "reference_afterburner_controls.h"
#include "reference_external_audio.h"
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
		entry.audio_volume = source.m_audioVolume;
		entry.ambient = source.m_bAmbient;
		entry.custom = source.m_bCustom;
		entry.max_volume = source.m_maxVolume;
		entry.max_distance = source.m_maxDistance;
		entry.decay = source.m_nDecay;
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

void audio_sync_source_controls()
{
	using namespace ownership_reference;
	unsigned cases = 0;
	const auto run = [&](int old_volume, int new_volume, int old_setting, int main_gain, int ambient_gain, unsigned config, unsigned object_state) {
		owner_case = "sync config=" + std::to_string(config) + " object=" + std::to_string(object_state) + " old=" + std::to_string(old_volume) + " new=" + std::to_string(new_volume) + " setting=" + std::to_string(old_setting);
		reset(1);
		auto &entry = audio.m_objects[0];
		entry.m_flags = SOF_USED;
		if (!(config & 128)) entry.m_flags |= (config & 64) ? SOF_LINK_TO_POS : SOF_LINK_TO_OBJ;
		if (config & 2) entry.m_flags |= SOF_PLAY_FOREVER;
		entry.m_volume = old_volume;
		entry.m_pan = int((config % 3) * 32768);
		entry.m_audioVolume = old_setting;
		entry.m_bCustom = (config & 4) != 0;
		entry.m_bAmbient = (config & 8) != 0;
		entry.m_soundClass = entry.m_bAmbient ? SOUNDCLASS_AMBIENT : 3;
		audio.volumes = { main_gain, ambient_gain };
		gameStates.sound.bDontStartObjects = (config & 16) != 0;
		audio.fail_start = (config & 32) != 0;
		if (!(config & 1)) entry.Stop();
		if (object_state == 1) entry.m_linkType.obj.nObject = -1;
		if (object_state == 2) objects[0].info.nType = OBJ_NONE;
		if (object_state == 3) ++objects[0].info.nSignature;
		if (object_state >= 4) {
			objects[0].info.nType = OBJ_EFFECT;
			objects[0].info.nId = (object_state == 4 || object_state == 7) ? SOUND_ID : LIGHTNING_ID;
			objects[0].rType.soundInfo.bEnabled = object_state != 4;
			objects[0].rType.lightningInfo.bEnabled = object_state != 5;
			show_lightning = object_state != 6;
		}
		auto native = native_owners();
		if (entry.m_channel >= 0) {
			const int effective = int(std::int64_t(old_volume) * (old_setting < 0 ? 32768 : old_setting) / 65536);
			auto &channel = audio.m_channels[entry.m_channel];
			channel.m_info.nVolume = effective;
			channel.m_info.nPan = entry.m_pan;
			channel.m_info.nChannel = entry.m_channel;
			channel.m_info.bAmbient = entry.m_bAmbient;
			Mix_VolPan(entry.m_channel, effective, entry.m_pan);
			auto &voice = native.mixer.voices[unsigned(entry.m_channel)];
			voice.priority.volume = effective;
			voice.output = xl_compat::audio_start_output(effective, entry.m_pan);
		}
		for (unsigned frame = 0; frame < 3; ++frame) {
			spatial_volume = frame == 0 ? new_volume : frame == 1 ? 0
			                                                      : 65536;
			spatial_pan = frame == 0 ? 32767 : frame == 1 ? 0
			                                              : 65536;
			if (frame == 1) {
				// Enabled effects can return, but a deleted lifetime cannot revive
				objects[0].info.nType = 1;
				objects[0].info.nSignature = objects[0].signature;
				objects[0].rType.soundInfo.bEnabled = objects[0].rType.lightningInfo.bEnabled = true;
				show_lightning = true;
			}
			if (frame == 2) {
				gameStates.sound.bDontStartObjects = false;
				audio.fail_start = false;
				audio.volumes = { 16384, 8192 };
			}
			unsigned native_spatial_calls = 0;
			source_spatial_calls = 0;
			audio.SyncSounds();
			native.owners.sync([&](const xl_compat::AudioOwner &owner) {
				if (!(owner.flags & xl_compat::sound_link_position)) {
					const auto *obj = object(owner.object);
					if (!obj || obj->info.nType == OBJ_NONE || obj->info.nSignature != owner.signature)
						return xl_compat::AudioOwnerUpdate{ false, true, 0, 0 };
					const bool enabled = obj->info.nType != OBJ_EFFECT ||
					                     !((obj->info.nId == SOUND_ID && !obj->rType.soundInfo.bEnabled) ||
					                       (obj->info.nId == LIGHTNING_ID && !(show_lightning && obj->rType.lightningInfo.bEnabled)));
					if (!enabled) return xl_compat::AudioOwnerUpdate{ true, false, 0, 0 };
				}
				++native_spatial_calls;
				return xl_compat::AudioOwnerUpdate{ true, true, spatial_volume, spatial_pan };
			},
			                   native.mixer, audio.volumes, gameStates.sound.bDontStartObjects, !audio.fail_start);
			compare_owners(native);
			require_owner(native_spatial_calls == source_spatial_calls, "Sync evaluated geometry for a missing, disabled or unlinked owner");
			for (unsigned i = 0; i < native.owners.count; ++i) {
				const auto &actual = native.owners.entries[i];
				const auto &expected = audio.m_objects[i];
				require_owner(actual.pan == expected.m_pan && actual.audio_volume == expected.m_audioVolume, "Source owner pan or remembered effects gain differs");
				if (actual.channel >= 0) {
					const auto &voice = native.mixer.voices[unsigned(actual.channel)];
					const auto &channel = audio.m_channels[actual.channel];
					const auto &output = outputs[unsigned(actual.channel)];
					require_owner(voice.priority.volume == channel.m_info.nVolume && voice.output.volume == output.volume && voice.output.pan == output.pan, "Sync changed gain or pan differently from source channel consumers");
				}
			}
			++cases;
		}
	};
	constexpr std::array<int, 10> volumes{ 0, 1, 127, 128, 129, 255, 256, 32768, 65536, 131072 };
	constexpr std::array<int, 4> settings{ -1, 0, 8192, 32768 };
	for (unsigned config = 0; config < 256; ++config)
		for (unsigned state = 0; state < 9; ++state)
			run(volumes[(config + state) % volumes.size()], volumes[(config * 3 + state * 7) % volumes.size()], settings[(config + 2 * state) % settings.size()], state % 3 ? 32768 : 0, config % 3 ? 8192 : 32768, config, state);
	for (int volume = 0; volume <= 65536; volume += 128)
		for (int delta : { -1, 0, 1 })
			run(volume, (std::max) (0, volume + delta), 32768, 32768, 8192, 3, 0);
	std::cout << "PASS: " << cases << " actual source linked updates, quantized gain, ambient/start gain, custom channels, missing/disabled owners and restart decisions\n";
}
