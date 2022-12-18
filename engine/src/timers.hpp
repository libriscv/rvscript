#pragma once
#include <functional>
#include <map>
#include <vector>

struct SystemTimer
{
	typedef double duration_t;
	typedef std::function<void(int)> handler_t;

	SystemTimer(duration_t p, int rep, handler_t cb)
	  : period(p), callback(std::move(cb)), repeats(rep),
		deferred_destruct(false)
	{
	}

	SystemTimer(SystemTimer&& other)
	  : period(other.period), callback(std::move(other.callback)),
		repeats(other.repeats), deferred_destruct(other.deferred_destruct)
	{
	}

	bool is_alive() const noexcept
	{
		return deferred_destruct == false;
	}

	bool is_oneshot() const noexcept
	{
		return repeats == 0;
	}

	void reset()
	{
		callback		  = nullptr;
		deferred_destruct = false;
	}

	duration_t period;
	handler_t callback;
	int repeats			   = 0;
	bool deferred_destruct = false;
};

class Timers
{
  public:
	typedef SystemTimer::duration_t duration_t;
	typedef SystemTimer::handler_t handler_t;

	inline int oneshot(duration_t when, handler_t handler);
	int periodic(duration_t when, duration_t period, handler_t handler);
	void stop(int id);

	void set_repeats(int id, int repeats)
	{
		timers.at(id).repeats = repeats;
	}

	int get_repeats(int id)
	{
		return timers.at(id).repeats;
	}

	// returns number of active timers
	size_t active() const noexcept
	{
		return this->scheduled.size() - dead_timers;
	}

	duration_t next() const;

	void handle_events();

  private:
	void sched_timer(duration_t when, int id);

	bool is_running = false;
	int dead_timers = 0;
	std::vector<SystemTimer> timers;
	std::vector<int> free_timers;
	// timers sorted by timestamp
	std::multimap<duration_t, int> scheduled;
};

inline int Timers::oneshot(duration_t when, handler_t handler)
{
	return periodic(when, 0.0, handler);
}
