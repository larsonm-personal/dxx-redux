#pragma once

#include <array>

// The native callback supplies post-consumer health, not a requested-hit estimate.
namespace pumo_robot_damage_observer
{
template <typename Sample>
class PlanBound
{
  public:
	static constexpr unsigned event_capacity = 64;
	static constexpr unsigned frame_capacity = 10000;
	struct Event {
		Sample sample;
		unsigned frame, load_generation;
	};

	bool configure(int level, int player, int player_signature, int player_type, int target, int target_signature,
	               int target_id, unsigned generation, unsigned frames, unsigned events)
	{
		reset();
		if (level < 1 || player < 0 || player_signature < 0 || player_type < 0 || target < 0 || target_signature < 0 ||
		    target_id < 0 || !frames || frames > frame_capacity || !events || events > event_capacity)
			return false;
		level_ = level;
		player_ = player;
		player_signature_ = player_signature;
		player_type_ = player_type;
		target_ = target;
		target_signature_ = target_signature;
		target_id_ = target_id;
		generation_ = generation;
		frame_limit_ = frames;
		event_limit_ = events;
		configured_ = true;
		return true;
	}
	void reset()
	{
		level_ = player_ = player_signature_ = player_type_ = target_ = target_signature_ = target_id_ = -1;
		generation_ = frame_limit_ = event_limit_ = frame_ = count_ = 0;
		configured_ = active_ = invalid_ = truncated_ = false;
	}
	void invalidate()
	{
		invalid_ = true;
		active_ = false;
	}
	void begin_frame(unsigned frame, int level, int player, int signature, unsigned generation)
	{
		if (!configured_ || invalid_ || active_ || frame != frame_ + 1 || frame > frame_limit_ ||
		    level != level_ || player != player_ || signature != player_signature_ || generation != generation_) {
			invalidate();
			return;
		}
		frame_ = frame;
		active_ = true;
	}
	void end_frame()
	{
		if (!active_ || invalid_) {
			invalidate();
			return;
		}
		active_ = false;
	}
	void observe(const Sample &sample)
	{
		if (!active_ || invalid_ || sample.level != level_) {
			invalidate();
			return;
		}
		if (sample.target != target_ || sample.parent_type != player_type_ || sample.parent_object != player_)
			return;
		if (sample.target_signature != target_signature_ || sample.target_id != target_id_ ||
		    !sample.target_valid || sample.parent_signature != player_signature_ ||
		    sample.parent_actual_type != player_type_ || sample.parent_actual_signature != player_signature_ ||
		    !sample.parent_valid ||
		    !sample.weapon_valid || sample.weapon < 0 || sample.weapon_signature < 0 || sample.weapon_id < 0 ||
		    sample.accepted_delta_raw !=
		        static_cast<long long>(sample.before_raw) - static_cast<long long>(sample.after_raw)) {
			invalidate();
			return;
		}
		if (sample.accepted_delta_raw < 0 || (sample.accepted_delta_raw > 0 && sample.requested_raw <= 0)) {
			invalidate();
			return;
		}
		if (sample.accepted_delta_raw == 0) return;
		if (count_ == event_limit_) {
			truncated_ = true;
			invalidate();
			return;
		}
		events_[count_++] = { sample, frame_, generation_ };
	}
	bool complete() const { return configured_ && frame_ > 0 && !active_ && !invalid_ && !truncated_; }
	bool truncated() const { return truncated_; }
	unsigned count() const { return count_; }
	unsigned frame_count() const { return frame_; }
	unsigned frame_limit() const { return frame_limit_; }
	unsigned event_limit() const { return event_limit_; }
	const Event &event(unsigned index) const { return events_[index]; }

  private:
	std::array<Event, event_capacity> events_{};
	int level_ = -1, player_ = -1, player_signature_ = -1, player_type_ = -1;
	int target_ = -1, target_signature_ = -1, target_id_ = -1;
	unsigned generation_ = 0, frame_limit_ = 0, event_limit_ = 0, frame_ = 0, count_ = 0;
	bool configured_ = false, active_ = false, invalid_ = false, truncated_ = false;
};
} // namespace pumo_robot_damage_observer
