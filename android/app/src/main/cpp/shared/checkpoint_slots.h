#ifndef CHECKPOINT_SLOTS_H
#define CHECKPOINT_SLOTS_H

#include <atomic>
#include <cstddef>

/* One game-thread producer/collector and one worker. A slot cannot be reused
 * until its completion has been collected and explicitly released */
template <typename T, std::size_t N>
class checkpoint_slots
{
	static_assert(N > 0, "A checkpoint queue needs storage");
	static_assert(ATOMIC_INT_LOCK_FREE == 2, "Checkpoint ownership requires lock-free atomics");
	enum : unsigned { free_slot,
		              capturing,
		              queued,
		              working,
		              complete,
		              collecting };
	struct slot {
		std::atomic<unsigned> state{ free_slot };
		T value{};
	};
	slot slots[N];
	std::size_t producer = 0, worker = 0, collector = 0;

  public:
	/* Initialization/teardown only, before starting or after joining the worker */
	template <typename F>
	void for_each_storage(F visit)
	{
		for (auto &s : slots) visit(s.value);
	}
	T *try_capture()
	{
		auto &s = slots[producer];
		if (s.state.load(std::memory_order_acquire) != free_slot) return nullptr;
		s.state.store(capturing, std::memory_order_relaxed);
		return &s.value;
	}
	void abandon_capture()
	{
		slots[producer].state.store(free_slot, std::memory_order_release);
	}
	void submit()
	{
		slots[producer].state.store(queued, std::memory_order_release);
		producer = (producer + 1) % N;
	}
	T *try_work()
	{
		auto &s = slots[worker];
		if (s.state.load(std::memory_order_acquire) != queued) return nullptr;
		s.state.store(working, std::memory_order_relaxed);
		return &s.value;
	}
	void finish_work()
	{
		slots[worker].state.store(complete, std::memory_order_release);
		worker = (worker + 1) % N;
	}
	T *try_collect()
	{
		auto &s = slots[collector];
		if (s.state.load(std::memory_order_acquire) != complete) return nullptr;
		s.state.store(collecting, std::memory_order_relaxed);
		return &s.value;
	}
	void release()
	{
		slots[collector].state.store(free_slot, std::memory_order_release);
		collector = (collector + 1) % N;
	}
};

#endif
