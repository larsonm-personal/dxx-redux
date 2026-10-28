#include "reference_motion.h"
#include "xl_reference_teleport.h"

int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected runtime transform samples");
		std::ifstream input(argv[1]);
		unsigned count = 0;
		fix values[18];
		while (input >> values[0]) {
			for (int i = 1; i < 18; ++i)
				if (!(input >> values[i])) throw std::runtime_error("Truncated transform sample");
			segment.normal.Set(values[0], values[1], values[2]);
			object.mType.physInfo.velocity.Set(values[3], values[4], values[5]);
			// DoTeleport calls StopSpeedBoost before this transform, even without a boost
			CFixVector::Normalize(object.mType.physInfo.velocity);
			object.mType.physInfo.velocity *= I2X(60);
			TriggerSetObjOrient(0, 0, 0, 1, 0);
			int at = 6;
			for (const auto *vector : { &object.info.position.mOrient.m.dir.r, &object.info.position.mOrient.m.dir.u, &object.info.position.mOrient.m.dir.f, &object.mType.physInfo.velocity }) {
				for (const fix value : { vector->v.coord.x, vector->v.coord.y, vector->v.coord.z })
					if (value != values[at++]) throw std::runtime_error("Teleport transform differs at sample " + std::to_string(count) + " field " + std::to_string(at - 1) + " expected " + std::to_string(value) + " actual " + std::to_string(values[at - 1]));
			}
			++count;
		}
		if (count < 200) throw std::runtime_error("Insufficient transform samples");
		std::cout << count << " extracted-reference teleport transforms passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
