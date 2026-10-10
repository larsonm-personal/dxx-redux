#include "checkpoint_slots.h"
#include <cstdio>
#include <cstdlib>
#include <thread>

#define CHECK(condition)                                                         \
	do {                                                                         \
		if (!(condition)) {                                                      \
			std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			std::abort();                                                        \
		}                                                                        \
	} while (0)

struct payload {
	unsigned sequence, input[64], output;
};

int main()
{
	checkpoint_slots<payload, 3> queue;
	/* Saturation, unsubmitted capture, and completion ownership */
	auto *p = queue.try_capture();
	CHECK(p && !queue.try_work());
	queue.abandon_capture();
	CHECK(queue.try_capture() == p);
	queue.abandon_capture();
	for (unsigned i = 0; i < 3; ++i) {
		p = queue.try_capture();
		CHECK(p);
		p->sequence = i;
		queue.submit();
	}
	CHECK(!queue.try_capture());
	p = queue.try_work();
	CHECK(p && p->sequence == 0);
	CHECK(!queue.try_collect() && !queue.try_capture());
	queue.finish_work();
	CHECK(!queue.try_capture());
	CHECK(queue.try_collect() == p);
	CHECK(!queue.try_capture());
	queue.release();
	for (unsigned i = 1; i < 3; ++i) {
		p = queue.try_work();
		CHECK(p && p->sequence == i);
		queue.finish_work();
		CHECK(queue.try_collect() == p);
		queue.release();
	}
	/* Actual concurrent publication: worker input must remain coherent until
	 * its output is collected, across many wraps of the bounded pool */
	constexpr unsigned count = 20000;
	std::thread consumer([&] {
		for (unsigned i = 0; i < count;) {
			auto *job = queue.try_work();
			if (!job) {
				std::this_thread::yield();
				continue;
			}
			CHECK(job->sequence == i);
			unsigned sum = 0;
			for (unsigned j = 0; j < 64; ++j) {
				CHECK(job->input[j] == i + j);
				sum += job->input[j];
			}
			job->output = sum;
			queue.finish_work();
			++i;
		}
	});
	unsigned submitted = 0, collected = 0;
	while (collected < count) {
		if (submitted < count) {
			if (auto *job = queue.try_capture()) {
				job->sequence = submitted;
				for (unsigned j = 0; j < 64; ++j) job->input[j] = submitted + j;
				queue.submit();
				++submitted;
			}
		}
		if (auto *job = queue.try_collect()) {
			CHECK(job->sequence == collected);
			CHECK(job->output == 64 * collected + 2016);
			queue.release();
			++collected;
		} else std::this_thread::yield();
	}
	consumer.join();
	std::puts("PASS: queue saturation, slot lifetime and 20000 concurrent checkpoint publications");
}
