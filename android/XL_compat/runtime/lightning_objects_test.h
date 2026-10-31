#include "lightning_objects.h"

static void lightning_objects_native_case(bool active)
{
	using namespace xl_compat;
	const auto sim = d_rand_get_stream_call_count(D_RNG_SIM);
	const auto fx = d_rand_get_stream_call_count(D_RNG_FX);
	if (!active) {
		xl_lightning_objects_clock();
		xl_lightning_objects_frame();
		require(lightning_objects_snapshot().empty() && !lightning_object_emitter(1) && !xl_lightning_object_set_enabled(1, 1), "Stock level accepted an XL lightning owner");
		require(d_rand_get_stream_call_count(D_RNG_SIM) == sim && d_rand_get_stream_call_count(D_RNG_FX) == fx, "Stock lightning hook consumed RNG");
		return;
	}
	PHYSFS_file *file = PHYSFS_openRead(Gamesave_current_filename);
	require(file && PHYSFS_fileLength(file) > 0, "Lightning owner source missing");
	Bytes bytes(std::size_t(PHYSFS_fileLength(file)));
	const auto read = PHYSFS_readBytes(file, bytes.data(), bytes.size());
	require(PHYSFS_close(file) && read == PHYSFS_sint64(bytes.size()), "Lightning owner source truncated");
	auto source = inspect_level(bytes);
	const auto original = source;
	struct Guard {
		object first = Objects[1], second = Objects[2], actor = Objects[3];
		trigger action = Triggers[0];
		~Guard()
		{
			Objects[1] = first;
			Objects[2] = second;
			Objects[3] = actor;
			Triggers[0] = action;
			xl_lightning_objects_clear();
		}
	} guard;
	for (int index : { 1, 2 }) {
		auto &native = Objects[index];
		native.type = 20;
		native.id = 1;
		native.signature = 200 + index;
		native.control_type = CT_NONE;
		native.movement_type = MT_NONE;
		native.render_type = RT_NONE;
		native.segnum = 0;
		compute_segment_center(&native.pos, &Segments[0]);
		auto &authored = source.objects[index];
		authored.type = 20;
		authored.id = authored.reader_id = 1;
		authored.control = authored.movement = 0;
		authored.render = authored.reader_render = 12;
		authored.segment = 0;
		authored.position = { native.pos.x, native.pos.y, native.pos.z };
		authored.render_data = { 300, 50, 18, 5, 1, -1, 3, 0, 0, 8, 0, 2, 20, 0, 0, 0, 0, 1, 1, 1, 0, 0, 52, 149, 128, index == 1 };
	}
	bind_lightning_objects(source);
	require(!lightning_object_emitter(1) && !lightning_object_emitter(2) && d_rand_get_stream_call_count(D_RNG_FX) == fx, "Lightning bind allocated transient bolts or consumed RNG");
	const auto saved = lightning_objects_snapshot();
	auto invalid = source;
	invalid.objects[2].segment = -1;
	bool rejected = false;
	try {
		bind_lightning_objects(invalid);
	} catch (const std::exception &) {
		rejected = true;
	}
	require(rejected && lightning_objects_snapshot() == saved, "Failed lightning bind replaced valid ownership");
	// A controlled absolute clock uses the same global 40-Hz accumulation as production
	lightning_objects_clock(1000000);
	lightning_objects_frame();
	require(lightning_object_emitter(1) && lightning_object_emitter(2) && !lightning_object_emitter(2)->enabled, "Source creates disabled lightning systems too");
	require(lightning_object_emitter(1)->updated == -1 && lightning_object_emitter(1)->bolts[0].draw_positions.empty(), "New lightning owner animated in its creation frame");
	require(!lightning_object_render_frame(1, 0) && !lightning_object_render_frame(2, 0), "New lightning owner created render geometry before animation");
	const int disabled_ttl = lightning_object_emitter(2)->bolts[0].lifetime.ttl;
	lightning_objects_clock(1000007);
	lightning_objects_frame();
	require(lightning_object_emitter(1)->updated == 1000007 && !lightning_object_emitter(1)->bolts[0].draw_positions.empty(), "Absolute-clock first lightning update was delayed by a relative epoch");
	require(lightning_object_emitter(2)->bolts[0].lifetime.ttl == disabled_ttl, "Disabled lightning animated");
	require(lightning_object_render_frame(1, 0) && !lightning_object_render_frame(2, 0), "Render geometry ignored source activation ordering");
	Objects[1].pos.x += 1000;
	lightning_objects_clock(1000033);
	lightning_objects_frame();
	const auto *emitter = lightning_object_emitter(1);
	const auto *frame = lightning_object_render_frame(1, 0);
	require(frame && frame->core.vertices.size() == emitter->bolts[0].draw_positions.size() + 4 && !frame->glow.vertices.empty(), "Lightning did not retain complete render geometry");
	for (unsigned node = 0; node < emitter->bolts[0].draw_positions.size(); ++node)
		for (unsigned axis = 0; axis < 3; ++axis)
			require(frame->core.vertices[node][axis] == float(emitter->bolts[0].draw_positions[node][axis]) / 65536.0f, "Lightning core was rebuilt from moved nodes instead of staged draw coordinates");
	const auto frozen_core = frame->core.vertices, frozen_glow = frame->glow.vertices;
	const LightningPoint scene_eye{ Objects[1].pos.x, Objects[1].pos.y, Objects[1].pos.z };
	const auto scene_rng = d_rand_get_stream_call_count(D_RNG_FX);
	auto queued = lightning_scene_items({ 0 }, {}, scene_eye, INT32_MAX);
	require(queued.size() == emitter->bolts.size(), "Visible lightning did not collect every active bolt");
	require(lightning_scene_items({}, {}, scene_eye, INT32_MAX).empty(), "Hidden lightning entered the scene");
	require(lightning_scene_items({}, { 1 }, scene_eye, INT32_MAX).size() == queued.size(), "Rendered lightning owner did not override segment visibility");
	for (const auto &item : queued) require(item.object == 1 && lightning_scene_item_valid(item), "Lightning scene collected a disabled or stale owner");
	require(queued[0].frame.core.vertices == frozen_core && queued[0].frame.glow.vertices == frozen_glow, "Lightning queue lost its captured frame");
	queued[0].frame.core.vertices[0][0] += 1;
	require(frame->core.vertices == frozen_core, "Queued lightning geometry aliases its live owner");
	++Objects[1].signature;
	require(!lightning_scene_item_valid(queued[0]), "Queued lightning followed a recycled native object");
	--Objects[1].signature;
	require(d_rand_get_stream_call_count(D_RNG_FX) == scene_rng, "Lightning collection consumed draw-time RNG");
	if (emitter->bolts[0].lifetime.nodes > 0)
		require(emitter->bolts[0].position == LightningPoint{ Objects[1].pos.x, Objects[1].pos.y, Objects[1].pos.z }, "Lightning did not follow its native owner");
	const auto before_validation = d_rand_get_stream_call_count(D_RNG_FX);
	const int before_pause = emitter->updated;
	stop_time();
	lightning_objects_clock(1000060);
	lightning_objects_frame();
	start_time();
	require(emitter->updated == before_pause && d_rand_get_stream_call_count(D_RNG_FX) == before_validation, "Paused lightning advanced or consumed RNG");
	require(lightning_object_render_frame(1, 0)->core.vertices == frozen_core && lightning_object_render_frame(1, 0)->glow.vertices == frozen_glow, "Paused lightning replaced its render snapshot");
	for (int variant = 0; variant < 7; ++variant) {
		auto state = saved;
		if (variant == 0) state[0][0] = 2;
		if (variant == 1) state[0][1] = -1;
		if (variant == 2) state[0][1] = UINT64_MAX;
		if (variant == 3) state[0][2] = 1;
		if (variant == 4) state[0][3] = false;
		if (variant == 5) state.erase(state.begin());
		if (variant == 6) state[0].push_back(0);
		require(!restore_lightning_objects(state, true) && lightning_objects_snapshot() == saved && lightning_object_emitter(1) == emitter, "Invalid lightning restore mutated live state");
	}
	require(xl_lightning_object_set_enabled(1, 0) && xl_lightning_object_set_enabled(2, 1), "Lightning activation failed");
	require(restore_lightning_objects(saved, true) && lightning_objects_snapshot() == saved && !lightning_object_emitter(1), "Lightning restore retained transient bolts or lost activation");
	require(!lightning_object_render_frame(1, 0), "Lightning restore retained stale camera geometry");
	require(lightning_scene_items({ 0 }, { 1 }, scene_eye, INT32_MAX).empty(), "Lightning restore retained queued render geometry");
	require(d_rand_get_stream_call_count(D_RNG_FX) == before_validation, "Lightning restore consumed effects RNG");
	Triggers[0] = {};
	Triggers[0].type = 32;
	Triggers[0].time = -1;
	Triggers[0].num_links = 2;
	for (int link = 0; link < 2; ++link) {
		Triggers[0].seg[link] = static_cast<short>(link + 1);
		Triggers[0].side[link] = -1;
	}
	xl_wall_trigger_reset();
	require(xl_wall_trigger(0, Players[Player_num].objnum, 0) == 0 && lightning_objects_snapshot()[1][2].get<bool>(), "Wall trigger did not enable lightning");
	auto actions = source;
	actions.object_trigger_records.clear();
	ObjectTrigger disable{};
	Objects[3].type = OBJ_POWERUP;
	disable.object = 3;
	disable.action.type = 33;
	disable.action.flags = 128;
	disable.action.time = -1;
	disable.action.targets = { { 2, -1 } };
	actions.object_trigger_records.push_back(disable);
	xl_object_trigger_load(actions);
	xl_object_trigger_frame();
	xl_object_trigger_state action_state{};
	require(xl_object_trigger_state_get(0, &action_state) && action_state.operated && lightning_objects_snapshot() == saved, "Automatic object trigger did not disable lightning");
	xl_object_trigger_load(original);
	Objects[3] = guard.actor;
	Triggers[0] = guard.action;
	constexpr const char *path = "xl-lightning-state.tmp";
	require(!PHYSFS_exists(path), "Lightning save control needs an unused temporary path");
	struct FileGuard {
		~FileGuard()
		{
			PHYSFS_delete("xl-lightning-state.tmp");
		}
	} file_guard;
	file = PHYSFS_openWrite(path);
	REWIND_PHYSFS_FILE(writer, file);
	require(file && xl_wall_trigger_write(writer) && PHYSFS_close(file), "Cannot capture lightning runtime trailer");
	require(xl_lightning_object_set_enabled(1, 0) && xl_lightning_object_set_enabled(2, 1), "Cannot disturb saved lightning activation");
	file = PHYSFS_openRead(path);
	REWIND_PHYSFS_FILE(reader, file);
	require(file && xl_wall_trigger_read(reader) && PHYSFS_close(file) && lightning_objects_snapshot() == saved, "Full runtime trailer did not restore lightning activation");
	lightning_objects_frame();
	require(lightning_object_emitter(1) && lightning_object_emitter(1)->updated == -1, "Lightning restore did not rebuild a fresh owner");
	++Objects[1].signature;
	lightning_objects_frame();
	require(!lightning_object_emitter(1) && !xl_lightning_object_set_enabled(1, 1), "Recycled native object inherited lightning");
	Objects[2].type = OBJ_NONE;
	lightning_objects_frame();
	require(!lightning_object_emitter(2), "Deleted object retained lightning");
	require(d_rand_get_stream_call_count(D_RNG_SIM) == sim, "Lightning used simulation RNG");
	std::cout << "XL lightning native ownership: creation, clock, movement, activation, atomic restore and signature lifetime passed\n";
}
