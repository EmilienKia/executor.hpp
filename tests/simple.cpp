/* -*- Mode: CPP; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*-  */
/*
 * tests/example.cpp
 * Copyright (C) 2018 Emilien Kia <emilien.kia+dev@gmail.com>
 */

#include "catch.hpp"

#include "executor.hpp"

static int invoc_test = 0;

void test()
{
	invoc_test = 1;
}

TEST_CASE( "Function direct executor", "[executor][direct]" )
{
	exec::direct_executor ex;
	std::future<void> future = ex.execute(test);

	future.wait();

	CHECK( invoc_test > 0 );
}


static int invoc_functor = 0;

struct functor
{
	void operator() ()
	{
		invoc_functor = 1;
	}
};

TEST_CASE( "Functor direct runnable executor", "[executor][direct]" )
{
	exec::direct_executor ex;
	functor fct;
	std::future<void> future = ex.execute(fct);

	future.wait();

	CHECK( invoc_functor > 0 );
}



static int invoc_static = 0;
static int invoc_nonstatic = 0;
static int invoc_other = 0;

struct object
{
	static void static_function()
	{
		invoc_static = 1;
	}

	void nonstatic_function()
	{
		invoc_nonstatic = 1;
	}

	void other_function(int)
	{
		invoc_other = 1;
	}
};

TEST_CASE( "Static function direct executor", "[executor][direct]" )
{
	exec::direct_executor ex;
	functor fct;
	std::future<void> future = ex.execute(object::static_function);

	future.wait();

	CHECK( invoc_static > 0 );
}

TEST_CASE( "Non-static function direct executor", "[executor][direct]" )
{
	exec::direct_executor ex;
	object obj;
	std::future<void> future = ex.execute(std::bind(&object::nonstatic_function, obj));

	future.wait();

	CHECK( invoc_nonstatic > 0 );
}

TEST_CASE( "Non-static function with args simple executor", "[executor][direct]" )
{
	exec::direct_executor ex;
	object obj;
	std::future<void> future = ex.execute(std::bind(&object::other_function, obj, 3));

	future.wait();

	CHECK( invoc_other > 0 );
}

TEST_CASE( "Lambda direct executor", "[executor][direct]" )
{
	bool invoked = false;
	exec::direct_executor ex;
	std::future<void> future = ex.execute([&](){invoked = true;});
	future.wait();

	CHECK( invoked );
}





TEST_CASE( "Lambda thread executor", "[executor][thread]" )
{
	bool invoked = false;
	exec::thread_executor ex;
	std::future<void> future = ex.execute([&](){invoked = true;});
	future.wait();

	CHECK( invoked );
}



TEST_CASE( "Lambda thread pool executor", "[executor][thread_pool]" )
{
	constexpr size_t taskcount = 8;
	constexpr size_t threadcount = 3;
	bool invoked[taskcount];;
	std::chrono::steady_clock::time_point tp;
	std::chrono::milliseconds wait    {1000};
	std::chrono::milliseconds epsillon {100};

	auto process = [&](size_t idx)
	{
		std::this_thread::sleep_for(wait);
		invoked[idx] = true;
	};

	for(size_t id = 0; id<taskcount; ++id)
		invoked[id] = false;

	exec::thread_pool_executor ex(threadcount);
	std::future<void> future[taskcount];

	for(size_t id = 0; id<taskcount; ++id)
		future[id] = ex.execute(std::bind(process, id));

	for(size_t id = 0; id<taskcount; ++id)
		future[id].wait();

	for(size_t id = 0; id<taskcount; ++id)
		CHECK( invoked[id] );
}
