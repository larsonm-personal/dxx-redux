// Included by the native implementation so the existing effect owner can
// observe dispatch without substituting a second queue or requiring a GL context
#include <iostream>
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
		Window window;
		~Guard()
		{
			Objects[1] = saved;
			View_position = view;
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
	// Equal buckets reverse insertion; dispatch pair, object, singleton in that order
	xl_transparency_begin(&segment, 1);
	maximum_depth = 1000 * 65536;
	const ParticlePoint particle_position{ center[0] + 20 * 65536, center[1], center[2] };
	const ParticlePoint object_position{ center[0] + 10 * 65536, center[1], center[2] };
	const auto enqueue_particle = [&](int id) {
		Particle value{};
		value.position = particle_position;
		value.radius = 1;
		value.quad.blend = ParticleBlend::weak_additive;
		value.quad.vertices[0].color[0] = float(id);
		add(value, particle_position, 10 * 65536);
	};
	enqueue_particle(2);
	add(QueuedObject{ 1, obj.signature, {} }, object_position);
	enqueue_particle(1);
	enqueue_particle(0);
	std::vector<int> buffered_receipt;
	drain_buffered([&](ParticleTexture *, const std::vector<ParticleQuad> &quads) {
		for (const auto &quad : quads) buffered_receipt.push_back(int(quad.vertices[0].color[0])); }, [&](Item &item) {
		check(std::holds_alternative<QueuedObject>(item), "Unexpected item at particle buffer boundary");
		buffered_receipt.push_back(9); });
	check(buffered_receipt == std::vector<int>({ 0, 1, 9 }) && items.empty(), "Production buffered dispatch crossed an object boundary or drew a singleton");
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
	xl_transparency_clear();
	check(bounds.empty() && !collecting, "Level cleanup retained transparency geometry");
	std::cout << "XL transparency native frame: classic isolation, mixed tie/bias order, duplicate faces, copied inputs, render context, recycled owners, view replacement and interrupted cleanup passed\n";
}
