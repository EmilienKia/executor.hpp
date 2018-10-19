/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * executor.hpp
 * Copyright (C) 2018 Emilien KIA <emilien.kia+dev@gmail.com>
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 */
/** \mainpage executor.hpp
 *
 * executor.hpp is an opensource header-only task executor library.
 * It provides convenient classes for asynchronous task execution and periodic scheduling.
 *
 * To integrate it in your code, only the src/executor.hpp file is necessary.
 *
 * Executors and schedulers
 * ========================
 *
 * Executors allows to execute [*callable*](https://en.cppreference.com/w/cpp/named_req/Callable) targets as soon as possible (and immediatly if possible).
 * Schedulers allows to schedule execution of *callable* at specified time or with specified delay and evenutally multiple times.
 * Depending of choosen executor or scheduler class, they can execute callables in one or multiple threads.
 *
 * <table>
 * <tr><td></td><th>Executor</th><th>Scheduler</th></tr>
 * <tr><th>Base interface</th><td>exec::executor</td><td>exec::schedule_executor / exec::periodic_schedule_executor</td></tr>
 * <tr><th>Direct execution (synchronous)</th><td>exec::direct_executor</td><td></td></tr>
 * <tr><th>One thread per task</th><td>exec::thread_executor</td><td>exec::thread_schedule_executor</td></tr>
 * <tr><th>Thread pool</th><td>exec::thread_pool_executor</td><td>exec::thread_pool_schedule_executor</td></tr>
 * </table>
 *
 * Examples
 * ========
 *
 * Thread executor
 * ---------------
 *
 * Execute some functions asynchronously in dedicated threads:
 * \see samples/thread_exec.cpp
 *
 * ~~~~~~~~~~~~~~~~~~~~~ .cpp
 * exec::thread_executor exe;
 * // Execute function
 * exe.execute(function);
 *
 * // Execute lambda
 * exe.execute([](){  });
 *
 * // Execute binding and get corresponding future
 * std::future<void> future = exe.execute([]() { });
 * future.get();
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * Thread pool scheduler executor
 * ------------------------------
 *
 * Execute some functions with delay or multiple times in a thread pool:
 * \see samples/tpool_sched.cpp
 *
 * ~~~~~~~~~~~~~~~~~~~~~ .cpp
 * // Define a thread pool scheduler with two underlaying threads
 * exec::thread_pool_schedule_executor<std::chrono::steady_clock> exe(2);
 *
 * // Schedule a global function
 * exe.schedule(test, std::chrono::steady_clock::now() + std::chrono::seconds(2));
 *
 * // Schedule 4 recurrent functor calls in 2 seconds, every 3 seconds
 * functor f;
 * exe.schedule_rate(f, std::chrono::seconds(2), std::chrono::seconds(3), 4);
 *
 * // Schedule an indefinite binding call with a delay of 2 seconds
 * object o;
 * exe.schedule_delay(std::bind(&object::sleep, o, 1), std::chrono::steady_clock::now(), std::chrono::seconds(2), -1);
 *
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * \example samples/thread_exec.cpp
 * \example samples/tpool_sched.cpp
 *
 */
#include <functional>
#include <thread>
#include <future>
#include <chrono>
#include <memory>
#include <future>
#include <queue>
#include <utility>
#include <condition_variable>
#include <algorithm>

/**
 * Executor namespace
 */
namespace exec
{

/**
 * A runnable is a C++ Callable without argument nor result.
 */
typedef std::function<void()> runnable;


/**
 * Interface of object able to execute a runnable command.
 */
class executor
{
public:
	/**
	 * Request execution of the runnable.
	 * \param command Runnable to invoke.
	 * \return Future corresponding to the invocation request.
	 */
	virtual std::future<void> execute(runnable command) =0;
};

/**
 * Executor which simply execute the requested runnable task.
 */
class direct_executor : public executor
{
public:
	virtual std::future<void> execute(runnable command) override
	{
		std::promise<void> promise;
		std::future<void> future = promise.get_future();
		try
		{
			command();
			promise.set_value();
		}
		catch(...)
		{
			promise.set_exception(std::current_exception());
		}
		return future;
	}
};

/**
 * Executor which only create a detached thread to execute the requested runnable task.
 */
class thread_executor : public executor
{
public:
	virtual std::future<void> execute(runnable command) override
	{
		std::promise<void> promise;
		std::future<void> future = promise.get_future();

		auto proxy = [](std::promise<void>&& promise, runnable command){
			try
			{
				command();
				promise.set_value();
			}
			catch(...)
			{
				promise.set_exception(std::current_exception());
			}
		};

		std::thread th(proxy, std::move(promise), command);
		th.detach();

		return future;
	}
};

/**
 * Executor which execute the requested runnable task in a thread pool.
 */
class thread_pool_executor : public executor
{
private:
	std::vector< std::thread > _workers;
	std::queue< runnable > _tasks;
    std::mutex _mutex;
    std::condition_variable _condition;
    bool _stop = false;

public:
	/**
	 * Construct a thread pool executor.
	 * \param thread_count Number of thread provisionned in the pool.
	 */
	thread_pool_executor(size_t thread_count)
	{
		for(size_t i = 0;i<thread_count;++i)
		{
			_workers.emplace_back(
				[this]
				{
					while(true)
					{
						std::function<void()> task;
						{
							std::unique_lock<std::mutex> lock(this->_mutex);
							this->_condition.wait(lock,
								[this]{ return this->_stop || !this->_tasks.empty(); });
							if(this->_stop && this->_tasks.empty())
								return;
							task = std::move(this->_tasks.front());
							this->_tasks.pop();
						}
						task();
					}
				}
			);
		}
	}

	/**
	 * Request to stop the thread pool.
	 * All the pending task will be executed.
	 * New execution requests will be rejected.
	 */
	void stop()
	{
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_stop = true;
		}
		_condition.notify_all();
		for(std::thread &worker: _workers)
			worker.join();
		_workers.clear();
	}

	virtual ~thread_pool_executor()
	{
		stop();
	}

	virtual std::future<void> execute(runnable command) override
	{
		std::shared_ptr<std::packaged_task<void()>> task = std::make_shared<std::packaged_task<void()>>(command);
		std::future<void> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(_mutex);
			if(_stop)
				throw std::runtime_error("enqueue on stopped thread_pool_executor");

			_tasks.emplace([task](){ (*task)(); });
		}
		_condition.notify_one();
		return res;
	}


};



/**
 * Status of a scheduled task.
 */
enum scheduled_status
{
	/** Task is scheduled. */
	scheduled,
	/** Task is currently running. */
	running,
	/** Task have been executed and no other schedule is planned. */
	done,
	/** The task have been cancelled. */
	cancelled,
	/** The task have been forsaken. Executor have been stopped before the end of the task schedulling. */
	forsaken
};

/**
 * Base scheduled task tracker.
 * Used to retrieve task status or request its cancellation.
 */
template<typename TP = std::chrono::steady_clock::time_point>
class schedule
{
public:
	typedef TP time_point;

	/**
	 * Retrieve the status of the task.
	 * \return Task status.
	 */
	virtual scheduled_status get_status() const =0;
	/**
	 * Request task cancellation.
	 */
	virtual void cancel() =0;

	/**
	 * Retrieve the time_point of the next execution of the task.
	 * \note Is undefined if the task is cancelled or done.
	 */
	virtual time_point scheduling() const =0;

	bool operator == (const schedule& sched) const { return scheduling() == sched.scheduling();}
	bool operator != (const schedule& sched) const { return scheduling() != sched.scheduling();}
	bool operator < (const schedule& sched) const { return scheduling() < sched.scheduling();}
	bool operator > (const schedule& sched) const { return scheduling() > sched.scheduling();}
	bool operator <= (const schedule& sched) const { return scheduling() <= sched.scheduling();}
	bool operator >= (const schedule& sched) const { return scheduling() >= sched.scheduling();}

	bool operator == (const time_point& sched) const { return scheduling() == sched;}
	bool operator != (const time_point& sched) const { return scheduling() != sched;}
	bool operator <  (const time_point& sched) const { return scheduling() < sched;}
	bool operator >  (const time_point& sched) const { return scheduling() > sched;}
	bool operator <= (const time_point& sched) const { return scheduling() <= sched;}
	bool operator >= (const time_point& sched) const { return scheduling() >= sched;}
};


/**
 * Scheduled task tracker.
 */
template<typename TP = std::chrono::steady_clock::time_point>
class scheduled_task : public schedule<TP>
{
public:
	typedef TP time_point;

	/**
	 * Retrieve the future corresponding to the task.
	 * Used to wait for task ending.
	 * \return Corresponding task future.
	 */
	virtual std::future<void> get_future() =0;
};


/**
 * Interface for schedule executors.
 */
template<typename Clock = std::chrono::steady_clock, typename TP = typename Clock::time_point, typename D = typename Clock::duration>
class schedule_executor
{
public:
	typedef Clock clock;
	typedef TP time_point;
	typedef D duration;

	/**
	 * Schedule a task execution at specified time point.
	 * \param command Runnable to schedule.
	 * \param timepoint Time when the command will be executed.
	 * \return Scheduled task tracker.
	 */
	virtual std::shared_ptr<scheduled_task<time_point>> schedule(runnable command, time_point timepoint) =0;

	/**
	 * Schedule a task execution with a specified delay (from now).
	 * \param command Runnable to schedule.
	 * \param initial_delay Delay from now when the command will be executed.
	 * \return Scheduled task tracker.
	 */
	inline std::shared_ptr<scheduled_task<time_point>> schedule(runnable command, duration initial_delay)
	{
		return schedule(command, clock::now() + initial_delay);
	}
};

/**
 * Tracker for periodic scheduled tasks.
 */
template<typename TP = std::chrono::steady_clock::time_point>
class periodic_task : public schedule<TP>
{
public:
	typedef TP time_point;

	/**
	 * Retrieve the number of execution of the task.
	 * \return Task execution count.
	 */
	virtual unsigned long count() const =0;

	/**
	 * Retrieve the number of execution remaining for the current task.
	 * \return Remaining execution count, negative if indefinite.
	 */
	virtual long remaining() const =0;
};

/**
 * Interface for periodic scheduler executors.
 * This executor is able to schedule task multiple times by delay or rate.
 */
template<typename Clock = std::chrono::steady_clock, typename TP = typename Clock::time_point, typename D = typename Clock::duration>
class periodic_schedule_executor : public schedule_executor<Clock, TP, D>
{
public:
	typedef Clock clock;
	typedef TP time_point;
	typedef D duration;

	typedef schedule_executor<Clock, TP, D> parent_type;

	using parent_type::schedule;

	/**
	 * Schedule a periodic task with a rate (relative to begin of last task execution).
	 * \param command Runnable to execute.
	 * \param initial Initial execution time.
	 * \param period Period between two execution (begin to begin).
	 * \param count Number of schedule execution. Negative (-1) for infinite.
	 * \return Corresponding periodic task tracker.
	 */
	virtual std::shared_ptr<periodic_task<time_point>> schedule_rate(runnable command, time_point initial, duration period, long count = -1) =0;
	/**
	 * Schedule a periodic task with a rate (relative to begin of last task execution).
	 * \param command Runnable to execute.
	 * \param initial_delay Initial execution delay (from now).
	 * \param period Period between two execution (begin to begin).
	 * \param count Number of schedule execution. Negative (-1) for infinite.
	 * \return Corresponding periodic task tracker.
	 */
	inline std::shared_ptr<periodic_task<time_point>> schedule_rate(runnable command, duration initial_delay, duration period, long count = -1)
	{
		return schedule_rate(command, clock::now() + initial_delay, period, count);
	}

	/**
	 * Schedule a periodic task with a delay (relative to end of last task execution).
	 * \param command Runnable to execute.
	 * \param initial Initial execution time.
	 * \param period Period between two execution (end to begin).
	 * \param count Number of schedule execution. Negative (-1) for infinite.
	 * \return Corresponding periodic task tracker.
	 */
	virtual std::shared_ptr<periodic_task<time_point>> schedule_delay(runnable command, time_point initial, duration delay, long count = -1) =0;
		/**
	 * Schedule a periodic task with a delay (relative to end of last task execution).
	 * \param command Runnable to execute.
	 * \param initial_delay Initial execution delay (from now).
	 * \param period Period between two execution (end to begin).
	 * \param count Number of schedule execution. Negative (-1) for infinite.
	 * \return Corresponding periodic task tracker.
	 */
	inline std::shared_ptr<periodic_task<time_point>> schedule_delay(runnable command, duration initial_delay, duration delay, long count = -1)
	{
		return schedule_delay(command, clock::now() + initial_delay, delay, count);
	}
};


/**
 * Thread base scheduler executor.
 * Each task is executed in its own thread.
 * \note In period scheduling, if the task is longer than the period delay, tasks will be executed as soon as possible and risk to stack.
 */
template<typename Clock = std::chrono::steady_clock, typename TP = typename Clock::time_point, typename D = typename Clock::duration>
class thread_schedule_executor : public periodic_schedule_executor<Clock, TP, D>, public thread_executor
{
public:
	typedef Clock clock;
	typedef TP time_point;
	typedef D duration;

	typedef periodic_schedule_executor<Clock, TP, D> parent_type;

private:

	struct tse_task
	{
		runnable _run;
		scheduled_status _status = scheduled_status::scheduled;
		bool _cancelling = false;
		time_point _next;

		tse_task(runnable run, time_point startpoint):
		_run(run),
		_next(startpoint)
		{
		}
	};

	struct tse_scheduled_task : public scheduled_task<time_point>, public tse_task
	{
		using tse_task::_run;
		using tse_task::_status;
		using tse_task::_cancelling;
		using tse_task::_next;

		std::future<void> _future;

		tse_scheduled_task() = delete;

		tse_scheduled_task(runnable run, time_point startpoint):
		tse_task(run, startpoint)
		{
		}

		virtual scheduled_status get_status() const override
		{
			return _status;
		}

		virtual void cancel() override
		{
			_cancelling = true;
		}

		virtual time_point scheduling() const override
		{
			return _next;
		}

		virtual std::future<void> get_future() override
		{
			return std::move(_future);
		}

		virtual void exec()
		{
			std::this_thread::sleep_until(_next);
			if(_cancelling)
			{
				_status = scheduled_status::cancelled;
				return;
			}
			_status = scheduled_status::running;
			_run();
			_status = scheduled_status::done;
		}

	};

	struct tse_periodic_task : public periodic_task<time_point>, public tse_task
	{
		using tse_task::_run;
		using tse_task::_status;
		using tse_task::_cancelling;
		using tse_task::_next;
		duration _duration;
		unsigned long _count = 0;
		long _remaining;

		tse_periodic_task(runnable run, time_point startpoint, duration duration, int count):
		tse_task(run, startpoint),
		_duration(duration),
		_remaining(count)
		{
		}

		virtual unsigned long count() const override
		{
			return _count;
		}

		virtual long remaining() const override
		{
			return _remaining;
		}

		virtual scheduled_status get_status() const override
		{
			return _status;
		}

		virtual void cancel() override
		{
			_cancelling = true;
		}

		virtual time_point scheduling() const
		{
			return _next;
		}

	};

	struct tse_rate_scheduled_task : public tse_periodic_task
	{
		tse_rate_scheduled_task() = delete;

		tse_rate_scheduled_task(runnable run, time_point startpoint, duration period, int count):
		tse_periodic_task(run, startpoint, period, count)
		{
		}

		using tse_periodic_task::_next;
		using tse_periodic_task::_status;
		using tse_periodic_task::_run;
		using tse_periodic_task::_count;
		using tse_periodic_task::_remaining;
		using tse_periodic_task::_duration;
		using tse_periodic_task::_cancelling;

		void exec()
		{
			while(_remaining!=0) // remaining < 0  (or ==-1)  ||  remaining > 0
			{
				std::this_thread::sleep_until(_next);
				_next = clock::now() + _duration;
				if(_cancelling)
				{
					_status = scheduled_status::cancelled;
					return;
				}
				_status = scheduled_status::running;
				++_count;
				if(_remaining>0) --_remaining;
				_run();
				_status = scheduled_status::scheduled;
			}
			_status = scheduled_status::done;
		}
	};

	struct tse_delay_scheduled_task : public tse_periodic_task
	{
		tse_delay_scheduled_task() = delete;

		tse_delay_scheduled_task(runnable run, time_point startpoint, duration delay, int count):
		tse_periodic_task(run, startpoint, delay, count)
		{
		}

		using tse_periodic_task::_next;
		using tse_periodic_task::_status;
		using tse_periodic_task::_run;
		using tse_periodic_task::_count;
		using tse_periodic_task::_remaining;
		using tse_periodic_task::_duration;
		using tse_periodic_task::_cancelling;

		void exec()
		{
			std::this_thread::sleep_until(_next);
			while(_remaining!=0) // remaining < 0  (or ==-1)  ||  remaining > 0
			{
				if(_cancelling)
				{
					_status = scheduled_status::cancelled;
					return;
				}
				_status = scheduled_status::running;
				++_count;
				if(_remaining>0) --_remaining;
				_run();
				_status = scheduled_status::scheduled;
				std::this_thread::sleep_for(_duration);
			}
			_status = scheduled_status::done;
		}
	};

public:
	using parent_type::schedule;
	using parent_type::schedule_rate;
	using parent_type::schedule_delay;

	thread_schedule_executor() = default;

	virtual std::shared_ptr<scheduled_task<time_point>> schedule(runnable command, time_point initial) override
	{
		std::shared_ptr<tse_scheduled_task> task = std::make_shared<tse_scheduled_task>(command, initial);
		task->_future = execute( std::bind(&tse_scheduled_task::exec, task.get()) ) ;
		return task;
	}

	virtual std::shared_ptr<periodic_task<time_point>> schedule_rate(runnable command, time_point initial, duration period, long count = -1) override
	{
		std::shared_ptr<tse_rate_scheduled_task> task = std::make_shared<tse_rate_scheduled_task>(command, initial, period, count);
		execute( std::bind( &tse_rate_scheduled_task::exec, task.get()) );
		return task;
	}

	virtual std::shared_ptr<periodic_task<time_point>> schedule_delay(runnable command, time_point initial, duration delay, long count = -1) override
	{
		std::shared_ptr<tse_delay_scheduled_task> task = std::make_shared<tse_delay_scheduled_task>(command, initial, delay, count);
		execute( std::bind( &tse_delay_scheduled_task::exec, task.get()) );
		return task;
	}
};



/**
 * Thread pool based scheduler executor.
 * All tasks share a common thread pool for their execution.
 * \note If the pool is underallocated or the underlying hardware is not suffisent, task could be started with a delay.
 */
template<typename Clock = std::chrono::steady_clock, typename TP = typename Clock::time_point, typename D = typename Clock::duration>
class thread_pool_schedule_executor : public periodic_schedule_executor<Clock, TP, D>
{
public:
	typedef Clock clock;
	typedef TP time_point;
	typedef D duration;

	typedef periodic_schedule_executor<Clock, TP, D> parent_type;

private:

	struct tpse_task
	{
		runnable _run;
		scheduled_status _status = scheduled_status::scheduled;
		bool _cancelling = false;
		time_point _next;

		tpse_task() = delete;

		tpse_task(runnable run, time_point startpoint):
		_run(run),
		_next(startpoint)
		{
		}

		virtual bool exec() = 0;
	};

	struct tpse_scheduled_task : public scheduled_task<time_point>, public tpse_task
	{
		using tpse_task::_run;
		using tpse_task::_status;
		using tpse_task::_cancelling;
		using tpse_task::_next;

		std::packaged_task<void()> _pack_task;

		tpse_scheduled_task() = delete;

		tpse_scheduled_task(runnable run, time_point startpoint):
		tpse_task(run, startpoint),
		_pack_task(run)
		{
			_pack_task;
		}

		virtual scheduled_status get_status() const override
		{
			return _status;
		}

		virtual void cancel() override
		{
			_cancelling = true;
		}

		virtual time_point scheduling() const override
		{
			return _next;
		}

		virtual std::future<void> get_future() override
		{
			return _pack_task.get_future();
		}

		// Return true if resceduled.
		virtual bool exec() override
		{
			std::this_thread::sleep_until(_next);
			if(_cancelling)
			{
				_status = scheduled_status::cancelled;
				return false;
			}
			_status = scheduled_status::running;
			_pack_task();
			_status = scheduled_status::done;
			return false;
		}

	};

	struct tpse_periodic_task : public periodic_task<time_point>, public tpse_task
	{
		using tpse_task::_run;
		using tpse_task::_status;
		using tpse_task::_cancelling;
		using tpse_task::_next;

		duration _duration;
		unsigned long _count = 0;
		long _remaining;

		tpse_periodic_task(runnable run, time_point startpoint, duration duration, int count):
		tpse_task(run, startpoint),
		_duration(duration),
		_remaining(count)
		{
		}

		virtual unsigned long count() const override
		{
			return _count;
		}

		virtual long remaining() const override
		{
			return _remaining;
		}

		virtual scheduled_status get_status() const override
		{
			return _status;
		}

		virtual void cancel() override
		{
			_cancelling = true;
		}

		virtual time_point scheduling() const
		{
			return _next;
		}
	};

	struct tpse_rate_scheduled_task : public tpse_periodic_task
	{
		tpse_rate_scheduled_task() = delete;

		tpse_rate_scheduled_task(runnable run, time_point startpoint, duration period, int count):
		tpse_periodic_task(run, startpoint, period, count)
		{
		}

		using tpse_periodic_task::_next;
		using tpse_periodic_task::_status;
		using tpse_periodic_task::_run;
		using tpse_periodic_task::_count;
		using tpse_periodic_task::_remaining;
		using tpse_periodic_task::_duration;
		using tpse_periodic_task::_cancelling;

		virtual bool exec() override
		{
			if(_remaining==0) // Already done
				return false;

			if(_cancelling) // Ask to cancel the task
			{
				_status = scheduled_status::cancelled;
				return false;
			}

			time_point next = _next + _duration;

			_status = scheduled_status::running;
			++_count;
			if(_remaining>0) --_remaining;

			_run();

			if(_remaining==0) // Finished
			{
				_status = scheduled_status::done;
				return false;
			}
			else
			{
				_status = scheduled_status::scheduled;
				_next = next;
				return true;
			}
		}
	};

	struct tpse_delay_scheduled_task : public tpse_periodic_task
	{
		tpse_delay_scheduled_task() = delete;

		tpse_delay_scheduled_task(runnable run, time_point startpoint, duration delay, int count):
		tpse_periodic_task(run, startpoint, delay, count)
		{
		}

		using tpse_periodic_task::_next;
		using tpse_periodic_task::_status;
		using tpse_periodic_task::_run;
		using tpse_periodic_task::_count;
		using tpse_periodic_task::_remaining;
		using tpse_periodic_task::_duration;
		using tpse_periodic_task::_cancelling;

		virtual bool exec() override
		{
			if(_remaining==0) // Already done
				return false;

			if(_cancelling) // Ask to cancel the task
			{
				_status = scheduled_status::cancelled;
				return false;
			}

			_status = scheduled_status::running;
			++_count;
			if(_remaining>0) --_remaining;

			_run();

			if(_remaining==0) // Finished
			{
				_status = scheduled_status::done;
				return false;
			}
			else
			{
				_status = scheduled_status::scheduled;
				_next = clock::now() + _duration;
				return true;
			}
		}
	};

private:

	static bool shrptr_scheduled_task_comparator(const std::shared_ptr<tpse_task>& a, const std::shared_ptr<tpse_task>& b)
	{
		// An undefined task is considered less prioritized than the other
		if(!a) return true;
		if(!b) return false;
//		if(a->get_status()==scheduled_status::cancelled) return true;
//		if(b->get_status()==scheduled_status::cancelled) return false;
		return a->_next < b->_next;
	}

	static inline bool inv_shrptr_scheduled_task_comparator(const std::shared_ptr<tpse_task>& a, const std::shared_ptr<tpse_task>& b)
	{
		return shrptr_scheduled_task_comparator(a, b);
	}

	std::vector< std::thread > _workers;
	std::vector<std::shared_ptr<tpse_task>> _tasks; // Task vector shall always be sorted reverse time (farrest to closest)
    std::mutex _mutex;
    std::condition_variable _condition;
    bool _stop = false;

	void push(std::shared_ptr<tpse_task> task)
	{
		{
			std::unique_lock<std::mutex> lock(this->_mutex);
			auto it = std::find_if_not(this->_tasks.begin(), this->_tasks.end(),
				[&](const std::shared_ptr<tpse_task>& t)->bool
				{ return inv_shrptr_scheduled_task_comparator(t, task); });

			this->_tasks.insert(it, task);
		}
		this->_condition.notify_one();
	}

public:
	using parent_type::schedule;
	using parent_type::schedule_rate;
	using parent_type::schedule_delay;

	/**
	 * Construct a thread pool scheduler.
	 * \param thread_count Number of thread provisionned in the pool.
	 */
	thread_pool_schedule_executor(size_t thread_count)
	{
		for(size_t i = 0;i<thread_count;++i)
		{
			_workers.emplace_back(
				[this]
				{
					while(true)
					{
						std::shared_ptr<tpse_task> task;
						{
							std::unique_lock<std::mutex> lock(this->_mutex);
							if(!this->_tasks.empty())
							{
								// Already have pending task, wait for the first available.
								auto next = this->_tasks.back()->_next;
								this->_condition.wait_until(lock, next,
									[this]{ return this->_stop || !this->_tasks.empty(); });
							}
							else
							{
								// No scheduled pending task, wait for an insertion
								this->_condition.wait(lock,
									[this]{ return this->_stop || !this->_tasks.empty(); });
							}
							if(this->_stop)
								return;
							task = this->_tasks.back();
							if(task->_next>clock::now())
							{
								// Sceduled for later, wait a little bit more.
								continue;
							}
							this->_tasks.pop_back();
						}
						if(task->exec()==true)
						{
							push(task);
						}
					}
				}
			);
		}
	}

	/**
	 * Request to stop the thread pool.
	 * All the pending task will be flagged as forsaken.
	 * New execution requests will be rejected.
	 */
	void stop()
	{
		{
			std::unique_lock<std::mutex> lock(_mutex);
			_stop = true;
			for(auto& task : _tasks)
				task->_status = scheduled_status::forsaken;
		}
		_condition.notify_all();
		for(std::thread &worker: _workers)
			worker.join();
		_workers.clear();
	}

	virtual ~thread_pool_schedule_executor()
	{
		stop();
	}

	virtual std::shared_ptr<scheduled_task<time_point>> schedule(runnable command, time_point initial) override
	{
		std::shared_ptr<tpse_scheduled_task> task = std::make_shared<tpse_scheduled_task>(command, initial);
		push( task ) ;
		return task;
	}

	virtual std::shared_ptr<periodic_task<time_point>> schedule_rate(runnable command, time_point initial, duration period, long count = -1) override
	{
		std::shared_ptr<tpse_rate_scheduled_task> task = std::make_shared<tpse_rate_scheduled_task>(command, initial, period, count);
		push( task ) ;
		return task;
	}

	virtual std::shared_ptr<periodic_task<time_point>> schedule_delay(runnable command, time_point initial, duration delay, long count = -1) override
	{
		std::shared_ptr<tpse_delay_scheduled_task> task = std::make_shared<tpse_delay_scheduled_task>(command, initial, delay, count);
		push( task ) ;
		return task;
	}
};







} // namespace exec
