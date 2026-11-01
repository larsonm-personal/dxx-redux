// Android automation readback isolates real lightning composition in the scene
namespace
{
xl_lightning_scene_result lightning_probe_result{};
std::string lightning_scene_failure;
std::vector<unsigned char> lightning_scene_before;
int lightning_probe_owner = -1;
unsigned lightning_probe_pending = 0;
bool lightning_probe_reading = false;

void lightning_probe_before()
{
	lightning_probe_reading = false;
	if (lightning_probe_owner < 0 || lightning_probe_result.completed) return;
	try {
		SceneReadback read;
		lightning_scene_before = read.pixels();
		lightning_probe_result.samples = unsigned(read.samples);
		lightning_probe_reading = true;
	} catch (const std::exception &error) {
		lightning_scene_failure = error.what();
		lightning_probe_result.completed = 1;
	}
}
void lightning_probe_after(unsigned bolts)
{
	if (!lightning_probe_reading) return;
	lightning_probe_reading = false;
	try {
		SceneReadback read;
		const auto after = read.pixels();
		if (after.size() != lightning_scene_before.size()) throw std::runtime_error("Lightning composition changed its viewport during readback");
		++lightning_probe_result.batches;
		lightning_probe_result.bolts += bolts;
		for (std::size_t i = 0; i < after.size(); i += 4)
			if (!std::equal(after.begin() + i, after.begin() + i + 3, lightning_scene_before.begin() + i)) ++lightning_probe_result.changed;
		lightning_probe_result.passed = lightning_probe_result.changed != 0;
		lightning_probe_result.completed = lightning_probe_result.passed || lightning_probe_result.batches >= 16;
		lightning_scene_before.clear();
	} catch (const std::exception &error) {
		lightning_scene_failure = error.what();
		lightning_probe_result.completed = 1;
	}
}
void lightning_probe_item(const xl_compat::LightningSceneItem &item)
{
	if (item.object != lightning_probe_owner || lightning_probe_result.completed) return;
	const auto lights = xl_compat::lightning_object_lights();
	unsigned lit = 0;
	for (const auto &[segment, light] : lights)
		if (segment >= 0 && segment <= Highest_segment_index && light.samples && light.brightness > 0) ++lit;
	lightning_probe_result.light_segments = std::max(lightning_probe_result.light_segments, lit);
	if (!item.frame.glow.vertices.empty() && item.frame.glow.distance_scale != 0) ++lightning_probe_pending;
}
void finish_scene_lightning_batch()
{
	if (lightning_probe_pending) lightning_probe_before();
	xl_compat::finish_lightning_batch();
	if (lightning_probe_pending) lightning_probe_after(lightning_probe_pending);
	lightning_probe_pending = 0;
}
} // namespace
extern "C" void xl_lightning_scene_probe_request(int object)
{
	lightning_probe_result = {};
	lightning_scene_failure.clear();
	lightning_scene_before.clear();
	lightning_probe_owner = object;
	lightning_probe_pending = 0;
	lightning_probe_reading = false;
}
extern "C" void xl_lightning_scene_probe_result(xl_lightning_scene_result *result)
{
	*result = lightning_probe_result;
}
extern "C" const char *xl_lightning_scene_probe_error(void)
{
	return lightning_scene_failure.c_str();
}
