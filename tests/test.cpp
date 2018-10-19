/* -*- Mode: CPP; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * tests/test.cpp
 * Copyright (C) 2018 Emilien Kia <emilien.kia+dev@gmail.com>
 */


#include "executor.hpp"

#include <iostream>

int main()
{
	std::cout << "Test:" << std::endl;

	constexpr size_t count = 8;

	bool invoked[count];
	std::chrono::steady_clock::time_point tp;
	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};

	for(size_t id = 0; id<count; ++id)
		invoked[id] = false;

	auto process = [&](size_t idx)
	{
		std::cout << idx << " : start" << std::endl;
		std::this_thread::sleep_for(wait);
		std::cout << idx << " : finish" << std::endl;
		invoked[idx] = true;
	};

	exec::thread_pool_executor ex(3);
	std::future<void> future[8];

	for(size_t id = 0; id<count; ++id)
		future[id] = ex.execute(std::bind(process, id));

	for(size_t id = 0; id<count; ++id)
		future[id].wait();


	return 0;
}
