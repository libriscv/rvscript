#include "timers.hpp"
#include <cassert>

int Timers::periodic(duration_t when, duration_t period, handler_t handler)
{
	int repeats = (period > 0.0) ? -1 : 0;
	int id;

	if (this->free_timers.empty())
	{
		if (this->dead_timers)
		{
			// look for dead timer
			auto it = this->scheduled.begin();
			while (it != this->scheduled.end())
			{
				// take over this timer, if dead
				id = it->second;

				if (this->timers[id].deferred_destruct)
				{
					this->dead_timers--;
					// remove from schedule
					this->scheduled.erase(it);
					// reset timer
					this->timers[id].reset();
					// reuse timer
					new (&this->timers[id])
						SystemTimer(period, repeats, handler);

					sched_timer(when, id);
					return id;
				}
				++it;
			}
		}
		id = this->timers.size();
		// occupy new slot
		this->timers.emplace_back(period, repeats, handler);
	}
	else
	{
		// get free timer slot
		id = this->free_timers.back();
		this->free_timers.pop_back();

		// occupy free slot
		new (&this->timers[id]) SystemTimer(period, repeats, handler);
	}

	// immediately schedule timer
	this->sched_timer(when, id);
	return id;
}

void Timers::stop(int id)
{
	if (timers.at(id).deferred_destruct == false)
	{
		// mark as dead already
		this->timers[id].deferred_destruct = true;
		// free resources immediately
		this->timers[id].callback = nullptr;
		this->dead_timers++;
	}
}

/// time functions ///

#include <time.h>

static inline timespec clocktime_now()
{
	timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	return t;
}

static inline double now() noexcept
{
	const auto ts = clocktime_now();
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

/// scheduling ///

void Timers::handle_events()
{
	this->is_running = false;

	while (!this->scheduled.empty())
	{
		const auto it	= this->scheduled.begin();
		const auto when = it->first;
		const int id	= it->second;

		// remove dead timers
		if (this->timers[id].deferred_destruct)
		{
			this->dead_timers--;
			// remove from schedule
			this->scheduled.erase(it);
			// delete timer
			this->timers[id].reset();
			this->free_timers.push_back(id);
		}
		else
		{
			const auto ts_now = now();

			if (ts_now >= when)
			{
				// erase immediately
				this->scheduled.erase(it);

				// reduce repeat count
				if (timers[id].repeats > 0)
					timers[id].repeats--;
				// call the users callback function
				this->timers[id].callback(id);
				// if the timers struct was modified in callback, eg. due to
				// creating a timer, then the timer reference below would have
				// been invalidated, hence why its BELOW, AND MUST STAY THERE
				auto& timer = this->timers[id];

				// oneshot timers are automatically freed
				if (timer.deferred_destruct || timer.is_oneshot())
				{
					timer.reset();
					if (timer.deferred_destruct)
						this->dead_timers--;
					this->free_timers.push_back(id);
				}
				else
				{
					// if the timer is recurring, we will simply reschedule it
					// NOTE: we are carefully using (when + period) to avoid
					// drift
					this->scheduled.emplace(
						std::piecewise_construct,
						std::forward_as_tuple(when + timer.period),
						std::forward_as_tuple(id));
				}

			} // ready
			else
			{
				// since we aren't doing hardware calls here, just return
				return;
			}
		} // timer not dead
	}	  // queue not empty
}

Timers::duration_t Timers::next() const
{
	if (!scheduled.empty())
	{
		auto it	  = scheduled.begin();
		auto when = it->first;
		auto diff = when - now();
		// avoid returning zero or negative diff
		if (diff < 1.0e-9)
			return 1.0e-9;
		return diff;
	}
	return 0.0;
}

void Timers::sched_timer(duration_t when, int id)
{
	assert(when > 0.0 && "Duration must be positive number");

	this->scheduled.emplace(
		std::piecewise_construct, std::forward_as_tuple(now() + when),
		std::forward_as_tuple(id));
}
