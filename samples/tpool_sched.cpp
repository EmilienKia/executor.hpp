/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * tpool_sched.cpp
 * Copyright (C) 2018 Emilien KIA <emilien.kia+dev@gmail.com>
 *
 * Distributed under the Boost Software License, Version 1.0. (See accompanying
 * file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include "executor.hpp"

#include <thread>

void test()
{
	std::this_thread::sleep_for(std::chrono::seconds(2));
}

struct functor
{
	void operator()()
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
};

struct object
{
	void sleep(int secs)
	{
		std::this_thread::sleep_for(std::chrono::seconds(secs));
	}
};

int main()
{
	// Define a thread pool scheduler with two underlaying threads
	exec::thread_pool_schedule_executor<std::chrono::steady_clock> exe(2);

	// Schedule a global function
	exe.schedule(test, std::chrono::steady_clock::now() + std::chrono::seconds(2));

	// Schedule 4 recurrent functor calls in 2 seconds, every 3 seconds
	functor f;
	exe.schedule_rate(f, std::chrono::seconds(2), std::chrono::seconds(3), 4);

	// Schedule an indefinite binding call with a delay of 2 seconds
	object o;
	exe.schedule_delay(std::bind(&object::sleep, o, 1), std::chrono::steady_clock::now(), std::chrono::seconds(2), -1);

	return 0;
}