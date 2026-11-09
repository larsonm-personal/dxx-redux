#pragma once

#include <algorithm>
#include <array>

// Developer-probe storage only. No engine callback owns or mutates game objects.
namespace pumo_player_fvi_observer
{
template <typename Sample>
class PlanBound
{
  public:
	static constexpr unsigned segment_capacity = 16;
	static constexpr unsigned sample_capacity = 256;
	struct Event {
		Sample sample;
		unsigned frame;
		unsigned load_generation;
	};

	bool configure(int level, int object, int signature, unsigned generation,
	               const int *segments, unsigned segment_count, unsigned frames, unsigned samples)
	{
		reset();
		if (level < 1 || object < 0 || signature < 0 || !segments || !segment_count ||
		    segment_count > segment_capacity || !frames || !samples || samples > sample_capacity) return false;
		for (unsigned i = 0; i < segment_count; ++i) {
			if (segments[i] < 0 || std::find(segments, segments + i, segments[i]) != segments + i)
				return false;
			allowed_segments_[i] = segments[i];
		}
		level_ = level;
		object_ = object;
		signature_ = signature;
		generation_ = generation;
		segment_count_ = segment_count;
		frame_limit_ = frames;
		sample_limit_ = samples;
		configured_ = true;
		return true;
	}

	void reset()
	{
		*this = {};
	}
	void invalidate()
	{
		active_ = false;
		invalidated_ = true;
	}
	void begin_frame(unsigned frame, int level, unsigned generation)
	{
		if (active_) invalidate();
		active_ = false;
		if (!configured_ || invalidated_) return;
		if (level != level_ || generation != generation_ || frame != frame_ + 1 || frame > frame_limit_) {
			invalidate();
			return;
		}
		frame_ = frame;
		frame_event_count_ = 0;
		active_ = true;
	}
	void end_frame()
	{
		if (active_ && frame_event_count_ == 0) invalidate();
		active_ = false;
		frame_event_count_ = 0;
	}
	void observe(const Sample &sample)
	{
		if (!active_ || sample.player_object != object_ || sample.player_signature != signature_) return;
		if (sample.level != level_ ||
		    std::find(allowed_segments_.begin(), allowed_segments_.begin() + segment_count_, sample.start_segment) ==
		        allowed_segments_.begin() + segment_count_) {
			invalidate();
			return;
		}
		++matched_count_;
		++frame_event_count_;
		if (event_count_ == sample_limit_) {
			truncated_ = true;
			return;
		}
		events_[event_count_++] = { sample, frame_, generation_ };
	}
	bool complete() const
	{
		return configured_ && frame_ > 0 && !active_ && !invalidated_ && !truncated_;
	}
	bool truncated() const
	{
		return truncated_ || invalidated_;
	}
	unsigned count() const
	{
		return event_count_;
	}
	unsigned matched_count() const
	{
		return matched_count_;
	}
	unsigned sample_limit() const
	{
		return sample_limit_;
	}
	unsigned frame_limit() const
	{
		return frame_limit_;
	}
	const Event &event(unsigned index) const
	{
		return events_[index];
	}

  private:
	std::array<int, segment_capacity> allowed_segments_{};
	std::array<Event, sample_capacity> events_{};
	unsigned segment_count_ = 0, frame_limit_ = 0, sample_limit_ = 0;
	unsigned frame_ = 0, frame_event_count_ = 0, event_count_ = 0, matched_count_ = 0, generation_ = 0;
	int level_ = 0, object_ = -1, signature_ = -1;
	bool configured_ = false, active_ = false, invalidated_ = false, truncated_ = false;
};
} // namespace pumo_player_fvi_observer
