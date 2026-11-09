#pragma once

#include <array>

// Bounded developer receipt for source events, not a swept path or no-hit proof.
namespace pumo_weapon_lifecycle_observer
{
template <typename Sample>
class PlanBound
{
  public:
	static constexpr unsigned event_capacity = 256;
	static constexpr unsigned slot_capacity = 32768;
	static constexpr unsigned frame_capacity = 10000;
	enum Phase { inactive,
		         weapon_input,
		         game_time_advance,
		         world_update };
	struct Frame {
		long long start_raw, after_weapon_input_raw, after_game_time_raw, end_raw;
	};
	struct Event {
		Sample sample;
		unsigned frame, load_generation;
		Phase phase;
		long long bracket_start_raw;
	};

	bool configure(int level, int player, int signature, unsigned generation,
	               unsigned frames, unsigned events)
	{
		reset();
		if (level < 1 || player < 0 || signature < 0 || !frames || frames > frame_capacity ||
		    !events || events > event_capacity)
			return false;
		level_ = level;
		player_ = player;
		signature_ = signature;
		generation_ = generation;
		frame_limit_ = frames;
		event_limit_ = events;
		configured_ = true;
		return true;
	}
	void reset()
	{
		slots_.fill({});
		generation_ = frame_limit_ = event_limit_ = frame_ = event_count_ = 0;
		level_ = 0;
		player_ = signature_ = -1;
		bracket_start_raw_ = phase_clock_raw_ = last_clock_raw_ = 0;
		phase_ = inactive;
		configured_ = invalid_ = truncated_ = false;
	}
	void invalidate()
	{
		invalid_ = true;
		phase_ = inactive;
	}
	bool seed(int slot, int signature)
	{
		if (!configured_ || frame_ || slot < 0 || unsigned(slot) >= slot_capacity || signature < 0 || slots_[slot].live) {
			invalidate();
			return false;
		}
		slots_[slot] = { signature, true };
		return true;
	}
	void begin_frame(unsigned frame, int level, int player, int signature, unsigned generation, long long clock)
	{
		if (!configured_ || invalid_ || phase_ != inactive || frame != frame_ + 1 || frame > frame_limit_ ||
		    level != level_ || player != player_ || signature != signature_ || generation != generation_ ||
		    (frame_ && clock < last_clock_raw_)) {
			invalidate();
			return;
		}
		frame_ = frame;
		bracket_start_raw_ = clock;
		phase_clock_raw_ = clock;
		frames_[frame - 1].start_raw = clock;
		phase_ = weapon_input;
	}
	void set_phase(Phase phase, long long clock)
	{
		if (invalid_ || (phase_ == weapon_input && phase != game_time_advance) ||
		    (phase_ == game_time_advance && phase != world_update) ||
		    (phase_ != weapon_input && phase_ != game_time_advance) || clock < phase_clock_raw_) {
			invalidate();
			return;
		}
		phase_ = phase;
		phase_clock_raw_ = clock;
		if (phase == game_time_advance) frames_[frame_ - 1].after_weapon_input_raw = clock;
		else frames_[frame_ - 1].after_game_time_raw = clock;
	}
	void end_frame(long long clock)
	{
		if (invalid_ || phase_ != world_update || clock < phase_clock_raw_) {
			invalidate();
			return;
		}
		last_clock_raw_ = clock;
		frames_[frame_ - 1].end_raw = clock;
		phase_ = inactive;
	}
	void observe(const Sample &sample)
	{
		if (invalid_ || phase_ == inactive || sample.level != level_ ||
		    sample.game_time_raw < bracket_start_raw_ || sample.game_time_raw < phase_clock_raw_ ||
		    sample.object < 0 || unsigned(sample.object) >= slot_capacity || sample.signature < 0) {
			invalidate();
			return;
		}
		auto &slot = slots_[sample.object];
		if (sample.kind == 1) {
			if (slot.live || sample.parent_type || sample.parent_object || sample.parent_signature) {
				invalidate();
				return;
			}
			slot = { sample.signature, true };
		} else if (!slot.live || slot.signature != sample.signature || sample.kind < 2 || sample.kind > 6) {
			invalidate();
			return;
		}
		if (sample.kind == 6) slot.live = false;
		if (event_count_ == event_limit_) {
			truncated_ = true;
			invalidate();
			return;
		}
		events_[event_count_++] = { sample, frame_, generation_, phase_, bracket_start_raw_ };
	}
	bool valid() const
	{
		return configured_ && frame_ > 0 && phase_ == inactive && !invalid_ && !truncated_;
	}
	bool truncated() const
	{
		return truncated_;
	}
	unsigned count() const
	{
		return event_count_;
	}
	unsigned frame_count() const
	{
		return frame_;
	}
	const Frame &frame(unsigned i) const
	{
		return frames_[i];
	}
	unsigned frame_limit() const
	{
		return frame_limit_;
	}
	unsigned event_limit() const
	{
		return event_limit_;
	}
	const Event &event(unsigned i) const
	{
		return events_[i];
	}

  private:
	struct Slot {
		int signature = -1;
		bool live = false;
	};
	std::array<Slot, slot_capacity> slots_{};
	std::array<Event, event_capacity> events_{};
	std::array<Frame, frame_capacity> frames_{};
	unsigned generation_ = 0, frame_limit_ = 0, event_limit_ = 0, frame_ = 0, event_count_ = 0;
	int level_ = 0, player_ = -1, signature_ = -1;
	long long bracket_start_raw_ = 0, phase_clock_raw_ = 0, last_clock_raw_ = 0;
	Phase phase_ = inactive;
	bool configured_ = false, invalid_ = false, truncated_ = false;
};
} // namespace pumo_weapon_lifecycle_observer
