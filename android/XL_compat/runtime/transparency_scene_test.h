// Included by the native implementation so the existing effect owner can
// observe dispatch without substituting a second queue or requiring a GL context
#include <iostream>
extern "C" {
#include "rng_shared.h"
}
extern "C" void xl_transparency_test_controls(int active)
{
	const auto check = [](bool value, const char *message) {
		if (!value) throw std::runtime_error(message);
	};
	if (!active) {
		xl_transparency_begin(nullptr, 0);
		check(!xl_transparency_face(-1, -1, 0, nullptr, -1, -1, nullptr, 0) && !xl_transparency_object(-1) &&
		          !xl_transparency_water(-1, -1, nullptr, nullptr),
		      "Stock render hooks entered XL transparency");
		xl_transparency_flush();
		check(items.empty() && !collecting, "Stock transparency retained a frame");
		return;
	}
	check(Highest_object_index >= 1 && Highest_segment_index >= 0, "Transparency controls require a loaded mine");
	struct Guard {
		object saved = Objects[1];
		vms_vector view = View_position;
		vms_vector scale = Matrix_scale;
		vms_matrix matrix = View_matrix;
		Window window;
		~Guard()
		{
			Objects[1] = saved;
			View_position = view;
			Matrix_scale = scale;
			View_matrix = matrix;
			window.restore();
			xl_transparency_clear();
		}
	} guard;
	const auto center = compile_classic_lighting_side(0, 0).center;
	View_position = { center[0], center[1], center[2] };
	auto &obj = Objects[1];
	obj.type = OBJ_WEAPON;
	obj.render_type = RT_POLYOBJ;
	obj.id = 0;
	obj.signature = 111;
	obj.pos = View_position;
	const short segment = 0;
	int vertices[4];
	uvl uvls[4]{};
	for (int i = 0; i < 4; ++i) {
		vertices[i] = Segments[0].verts[Side_to_verts_int[0][i]];
		uvls[i] = { i * 17, i * 31, i * 71 };
	}
	xl_transparency_begin(&segment, 1);
	Window_clip_left = 19;
	Current_seg_depth = 23;
	check(xl_transparency_face(0, 0, 4, vertices, 0, 0, uvls, WID_CLOAKED_FLAG), "Transparent native face was not queued");
	check(xl_transparency_face(0, 0, 4, vertices, 0, 0, uvls, WID_CLOAKED_FLAG) && items.size() == 1, "Second geometry pass duplicated a transparent face");
	Window_clip_left = 37;
	Current_seg_depth = 41;
	check(xl_transparency_object(1), "Source energy projectile was not queued");
	obj.id = 8;
	check(!xl_transparency_object(1), "Opaque missile was sent to the transparency queue");
	check(rendered_objects.count(1) == 1, "Opaque owner visit was lost before lightning visibility collection");
	obj.id = 0;
	// A particle ten units farther has the same bucket as this face/object
	add(Particle{}, { center[0] + 10 * 65536, center[1], center[2] }, 10 * 65536);
	Window_clip_left = 59;
	Current_seg_depth = 61;
	unsigned visited = 0;
	drain([&](Item &item) {
		check(!collecting && !xl_transparency_object(1), "Transparency dispatch recursively queued an object");
		if (visited == 0) check(std::holds_alternative<Particle>(item), "Shared queue lost particle depth bias or reverse tie order");
		else if (visited == 1) check(std::holds_alternative<QueuedObject>(item) && Window_clip_left == 37 && Current_seg_depth == 41, "Deferred object lost render context");
		else if (visited == 2) {
			const auto *face = std::get_if<Face>(&item);
			check(face && Window_clip_left == 19 && Current_seg_depth == 23 && face->vertices[3] == vertices[3] && face->uvls[2].l == uvls[2].l, "Deferred face lost copied vertices, light, or render context");
		} else check(false, "Unexpected shared transparency item");
		++visited;
	});
	check(visited == 3 && items.empty() && !collecting && Window_clip_left == 59 && Current_seg_depth == 61, "Transparency dispatch did not finish and restore its caller");
	{
		check(Num_walls > 0, "Additive queue control requires a native wall");
		struct WallGuard {
			wall saved = Walls[0];
			short wall_index = Segments[0].sides[0].wall_num;
			ubyte flags = GameBitmaps[Textures[0].index].bm_flags;
			int effect = TmapInfo[0].eclip_num;
			~WallGuard()
			{
				Walls[0] = saved;
				Segments[0].sides[0].wall_num = wall_index;
				GameBitmaps[Textures[0].index].bm_flags = flags;
				TmapInfo[0].eclip_num = effect;
				xl_preview_end_face();
			}
		} wall_guard;
		Segments[0].sides[0].wall_num = 0;
		Walls[0].type = WALL_CLOSED;
		Walls[0].state = WALL_DOOR_CLOSED;
		Walls[0].cloak_value = 0;
		Walls[0].flags = 0;
		GameBitmaps[Textures[0].index].bm_flags &= ~(BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT);
		TmapInfo[0].eclip_num = -1;
		xl_transparency_begin(&segment, 1);
		check(!xl_transparency_face(0, 0, 4, vertices, 0, 0, uvls, WID_RENDER_FLAG), "Ordinary opaque wall entered the additive queue");
		Walls[0].flags = wall_render_additive;
		check(xl_transparency_face(0, 0, 4, vertices, 0, 0, uvls, WID_RENDER_FLAG) && items.size() == 1, "Opaque authored additive wall bypassed transparency ordering");
		check(!xl_preview_additive_face(), "Queue classification leaked additive face binding");
		drain([&](Item &item) {
			check(std::holds_alternative<Face>(item) && ends_lightning_batch(item), "Additive wall did not terminate the preceding lightning batch");
		});
		check(items.empty() && !collecting, "Additive face retained its queue entry");
	}
	// Equal buckets reverse insertion; dispatch pair, object, singleton in that order
	xl_transparency_begin(&segment, 1);
	maximum_depth = 1000 * 65536;
	const ParticlePoint particle_position{ center[0] + 20 * 65536, center[1], center[2] };
	const ParticlePoint object_position{ center[0] + 10 * 65536, center[1], center[2] };
	const auto enqueue_particle = [&](int id) {
		Particle value{};
		value.scene.position = particle_position;
		value.scene.radius = 1;
		value.scene.quad.blend = ParticleBlend::weak_additive;
		value.scene.quad.vertices[0].color[0] = float(id);
		add(value, particle_position, 10 * 65536);
	};
	enqueue_particle(2);
	add(QueuedObject{ 1, obj.signature, {} }, object_position);
	enqueue_particle(1);
	enqueue_particle(0);
	std::vector<int> buffered_receipt;
	unsigned particle_visits = 0;
	drain_buffered([&](ParticleTexture *, const std::vector<ParticleQuad> &quads) {
		check(particle_visits >= 2, "Particle buffer submitted before visiting its members");
		for (const auto &quad : quads) buffered_receipt.push_back(int(quad.vertices[0].color[0])); }, [&](Item &item) {
		check(std::holds_alternative<QueuedObject>(item), "Unexpected item at particle buffer boundary");
		buffered_receipt.push_back(9); }, [&](Item &item) {
		if (std::holds_alternative<Particle>(item)) ++particle_visits;
		else check(buffered_receipt.empty(), "Pre-item flush hook ran after buffered particle drawing"); });
	check(buffered_receipt == std::vector<int>({ 0, 1, 9 }) && items.empty(), "Production buffered dispatch crossed an object boundary or drew a singleton");
	check(particle_visits == 3 && rendered_objects.empty(), "Particle visitation skipped a singleton or retained owner visibility");
	{
		unsigned random_state = 0;
		check(d_rand_get_stream_state(D_RNG_PRESENTATION, &random_state), "Deferred bubble control requires restorable effects RNG");
		const auto random_calls = d_rand_get_stream_call_count(D_RNG_PRESENTATION);
		struct RandomRestore {
			unsigned state, calls;
			~RandomRestore()
			{
				d_rand_set_stream_state(D_RNG_PRESENTATION, state);
				d_rand_set_stream_call_count(D_RNG_PRESENTATION, calls);
			}
		} random_restore{ random_state, random_calls };
		const int denominators[]{ 10 + d_rand_presentation() % 6, 10 + d_rand_presentation() % 6 };
		check(d_rand_set_stream_state(D_RNG_PRESENTATION, random_state), "Cannot rewind bubble control RNG");
		d_rand_set_stream_call_count(D_RNG_PRESENTATION, random_calls);
		std::array<BubbleParticle, 3> bubbles{};
		for (unsigned i = 0; i < bubbles.size(); ++i) {
			bubbles[i].motion.position = particle_position;
			bubbles[i].motion.width = bubbles[i].motion.height = 1;
			bubbles[i].motion.frame = int(10 + i);
			bubbles[i].color = { float(i), .9f, 1, .5f };
		}
		xl_transparency_begin(&segment, 1);
		maximum_depth = 1000 * 65536;
		const auto enqueue_bubble = [&](unsigned index) {
			Particle value{};
			value.scene.position = particle_position;
			value.scene.radius = 1;
			value.scene.subtype = 2;
			value.scene.quad.blend = ParticleBlend::alpha;
			BubbleSceneDraw draw{ &bubbles[index], int(index * 2 + 1), 64, 64, { { 0, 65536, 0 }, { 0, 0, 65536 } } };
			draw.eye = center;
			draw.camera = { { { 0, 65536, 0 }, { 0, 0, 65536 }, { 65536, 0, 0 } } };
			draw.camera_space = true;
			value.scene.bubble = draw;
			add(value, particle_position, 10 * 65536);
		};
		enqueue_bubble(2);
		add(QueuedObject{ 1, obj.signature, {} }, object_position);
		enqueue_bubble(1);
		Particle smoke{};
		smoke.scene.position = particle_position;
		smoke.scene.radius = 1;
		smoke.scene.subtype = 0;
		smoke.scene.quad.blend = ParticleBlend::alpha;
		add(smoke, particle_position, 10 * 65536);
		enqueue_bubble(0);
		check(d_rand_get_stream_call_count(D_RNG_PRESENTATION) == random_calls, "Bubble queue insertion consumed wiggle randomness");
		unsigned batches = 0, boundaries = 0;
		drain_buffered([&](ParticleTexture *, const std::vector<ParticleQuad> &quads) {
			++batches;
			check(quads.size() == 3 && quads[0].image == 2 && quads[1].image == 1 && quads[2].image == 2, "Mixed bubble/smoke buffer lost source submission order or texture selectors");
			for (unsigned i = 0; i < 2; ++i) {
				const auto &quad = quads[i * 2];
				const float wiggle = float(std::sin(double(float(i * 2 + 1) / 4.f) * 3.141592653589793240)) / denominators[i];
				const float expected_z = -((float(particle_position[0]) / 65536.f + wiggle) - float(center[0]) / 65536.f);
				check(quad.vertices[0].color[0] == float(i) && quad.vertices[0].position[2] == expected_z, "Bubble wiggle used traversal RNG, live frame or camera X instead of queued frame/world X");
				check(quad.vertices[0].uv[0] == float((10 + i) % 4) * .25f && quad.vertices[0].uv[1] == float((10 + i) / 4) * .25f, "Bubble atlas UV ignored its live animated frame");
			} }, [&](Item &item) {
			check(std::holds_alternative<QueuedObject>(item) && batches == 1, "Bubble batch crossed a native object boundary");
			++boundaries; });
		check(batches == 1 && boundaries == 1 && d_rand_get_stream_call_count(D_RNG_PRESENTATION) == random_calls + 2, "Suppressed bubble singleton consumed wiggle RNG or escaped its buffer");
		std::cout << "XL bubble deferred queue: mixed source order, queued-frame world-X wiggle, live atlas, boundary flush and singleton RNG suppression passed\n";
	}
	xl_transparency_begin(&segment, 1);
	check(xl_transparency_object(1), "Recycled-object control did not enqueue");
	++obj.signature;
	drain([&](Item &) { check(false, "Deferred object followed a recycled signature"); });
	xl_transparency_begin(&segment, 1);
	check(xl_transparency_object(1), "Fresh-frame control did not enqueue");
	xl_transparency_begin(&segment, 1);
	check(items.empty(), "New view inherited the previous queue");
	check(xl_transparency_object(1), "Throwing-dispatch control did not enqueue");
	bool caught = false;
	try {
		drain([](Item &) { Window_clip_left = -77; throw std::runtime_error("controlled dispatch failure"); });
	} catch (const std::runtime_error &) {
		caught = true;
	}
	check(caught && !collecting && items.empty() && Window_clip_left == 59, "Interrupted transparency dispatch leaked frame state");
	int powerup_clip = -1;
	for (int i = 0; i < VCLIP_MAXNUM; ++i)
		if (Vclip[i].num_frames > 0 && Vclip[i].frames[0].index < Num_bitmap_files) {
			powerup_clip = i;
			break;
		}
	check(powerup_clip >= 0, "Powerup sprite controls require a native vclip");
	obj.type = OBJ_POWERUP;
	obj.render_type = RT_POWERUP;
	obj.id = 1;
	obj.rtype.vclip_info.vclip_num = powerup_clip;
	obj.rtype.vclip_info.framenum = 0;
	obj.size = 65536;
	obj.pos = View_position;
	xl_transparency_begin(&segment, 1);
	check(xl_transparency_object(1), "Energy powerup did not enter native sprite queue");
	const auto *body = std::get_if<Sprite>(&items.back());
	check(body && body->bitmap == Vclip[powerup_clip].frames[0].index && body->quad.blend == ParticleBlend::alpha && body->quad.vertices[0].color[3] == 2.f / 3.f,
	      "Energy powerup lost source body bitmap or opacity");
	const auto copied = body->quad.vertices[0].position;
	obj.pos.x += 65536;
	++obj.signature;
	visited = 0;
	drain([&](Item &item) {
		const auto *sprite = std::get_if<Sprite>(&item);
		check(sprite != nullptr, "Powerup dispatch retained a live native object");
		if (!visited) check(sprite->bitmap >= 0 && sprite->quad.vertices[0].position == copied && ends_lightning_batch(item), "Powerup body lost copied position or reverse tie order");
		else check(sprite->bitmap < 0 && !ends_lightning_batch(item), "Negative corona ended pending lightning");
		++visited;
	});
	check(visited >= 1 && visited <= 2, "Powerup queue emitted unexpected sprites");
	xl_transparency_begin(&segment, 1);
	obj.id = 10;
	check(!xl_transparency_object(1) && items.empty(), "Ordinary powerup body was incorrectly deferred");
	check(!ends_lightning_batch(Sprite{}) && ends_lightning_batch(Sprite{ {}, 0 }), "Native sprite flush rule differs from source negative/zero blend dispatch");
	add(Sprite{}, point(View_position));
	add(Sprite{ {}, 0 }, point(View_position));
	visited = 0;
	drain([&](Item &item) {
		const auto &sprite = std::get<Sprite>(item);
		check((sprite.bitmap >= 0) == (visited == 0) && ends_lightning_batch(item) == (visited == 0), "Explicit corona/body pair lost source order or flush behavior");
		++visited;
	});
	check(visited == 2, "Explicit corona/body pair was not drawn");
	Matrix_scale = { 2 * 65536, 32768, 65536 };
	View_matrix = { { 2 * 65536, 0, 0 }, { 0, 32768, 0 }, { 0, 0, 65536 } };
	View_position = {};
	const auto billboard = powerup_billboard({ 3 * 65536, 4 * 65536, 5 * 65536 }, 65536, 65536, { 1, 1, 1, 1 }, ParticleBlend::alpha);
	check(billboard.vertices[0].position == std::array<float, 3>{ 4, 2.5f, -5 } &&
	          billboard.vertices[2].position == std::array<float, 3>{ 8, 1.5f, -5 } &&
	          billboard.vertices[0].uv == std::array<float, 2>{ 0, 0 } && billboard.vertices[2].uv == std::array<float, 2>{ 1, 1 },
	      "Native billboard lost window/zoom scaling or source corner/UV orientation");
	// Exercise the native bitmap bridge independently of which fixture has glare
	std::array<unsigned char, 4> indexed{ 5, 5, 8, 8 };
	const auto expected_color = sprite_bitmap_color(0, indexed.data(), 2, 2, 2, gr_current_pal);
	grs_bitmap bitmap{};
	bitmap.bm_w = bitmap.bm_h = bitmap.bm_rowsize = 2;
	bitmap.bm_type = BM_LINEAR;
	bitmap.bm_data = indexed.data();
	check(powerup_bitmap_color(bitmap) == expected_color, "Native indexed sprite color differs from source-qualified average");
	std::array<unsigned char, 12> small_rle{ 12, 0, 0, 0, 3, 3, 0xe2, 5, 0xe0, 0xe2, 8, 0xe0 };
	std::array<unsigned char, 14> big_rle{ 14, 0, 0, 0, 3, 0, 3, 0, 0xe2, 5, 0xe0, 0xe2, 8, 0xe0 };
	bitmap.bm_flags = BM_FLAG_RLE;
	bitmap.bm_data = small_rle.data();
	check(powerup_bitmap_color(bitmap) == expected_color, "Native small-RLE sprite average changed");
	bitmap.bm_flags |= BM_FLAG_RLE_BIG;
	bitmap.bm_data = big_rle.data();
	check(powerup_bitmap_color(bitmap) == expected_color, "Native big-RLE sprite average changed");
	bitmap.bm_type = BM_RGBA;
	bitmap.bm_flags = 0;
	for (unsigned value = 0; value < 256; ++value) {
		std::array<float, 3> expected{};
		for (unsigned axis = 0; axis < 3; ++axis) {
			const unsigned byte = (value + axis * 71) % 256;
			bitmap.avg_color_rgb[axis] = fix(byte * 65536 / 255);
			expected[axis] = float(byte) / 255.f;
		}
		check(powerup_bitmap_color(bitmap) == expected, "Native truecolor sprite lost original average bytes");
	}
	xl_transparency_clear();
	check(bounds.empty() && !collecting, "Level cleanup retained transparency geometry");
	std::cout << "XL transparency native frame: classic isolation, mixed tie/bias order, duplicate faces, copied inputs, render context, recycled owners, view replacement and interrupted cleanup passed\n";
	std::cout << "XL powerup sprite queue: ordinary immediate body, copied energy body/alpha, reverse corona tie order and pending lightning retention passed\n";
	std::cout << "XL sprite bitmap bridge: indexed, small/big RLE and256 truecolor byte averages passed\n";
}
