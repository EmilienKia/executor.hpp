/* -*- Mode: C++; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * thread_exec.cpp
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
	exec::thread_executor exe;

	// Execute a global function
	auto future_global = exe.execute(test);

	// Execute a functor
	functor f;
	auto future_functor = exe.execute(f);

	// Execute member binding
	object o;
	auto future_bind = exe.execute(std::bind(&object::sleep, &o, 2));

	// Execute a lambda
	std::future<void> future_lambda = exe.execute([]()
		{
			std::this_thread::sleep_for(std::chrono::seconds(2));
		});


	// Wait barrier
	future_global.wait();
	future_functor.wait();
	future_bind.wait();
	future_lambda.wait();

	return 0;
}